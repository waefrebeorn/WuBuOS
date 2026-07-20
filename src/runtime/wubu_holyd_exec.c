/*
 * wubu_holyd_exec.c  --  WuBuOS HolyC DOS Daemon: Exec
 */

#include "wubu_holyd_internal.h"
#include <ctype.h>
#include <stdbool.h>

/* -- Code Execution ----------------------------------------------- */

int wubu_holyd_eval(WubuHoly *d, const char *session,
                      const char *code, char *output, size_t out_size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) { snprintf(output, out_size, "Session '%s' not found", session); return -1; }
    if (s->state != SESSION_STATE_ACTIVE) {
        snprintf(output, out_size, "Session '%s' not active (state=%s)",
                 session, wubu_holyd_session_state_str(s->state));
        return -1;
    }

    s->state = SESSION_STATE_RUNNING;
    s->last_active = time(NULL);
    d->evals_performed++;

    holyd_log(d, 2, "Eval in session '%s': %.60s...", session, code);

    /* Get or create persistent compiler for this session. The REPL state we
     * must preserve across evals is the accumulated function table + symbol
     * table (so a function declared in one eval is callable in the next).
     * Those live in HCGen and are preserved by hc_gen_init(). Everything
     * else -- the lexer, parser, and the per-call code/data buffers -- is
     * re-initialized fresh each call so a failed parse cannot leave the
     * persistent compiler in a corrupt state (which previously caused a
     * use-after-free + SIGSEGV on the following eval). */
    HCCompiler *compiler = holyd_get_compiler(s, d);
    if (!compiler) {
        snprintf(output, out_size, "Compiler initialization failed");
        s->state = SESSION_STATE_ACTIVE;
        return -1;
    }

    /* Fresh lexer + parser each call. We parse a *compilation unit* (the
     * same entry point the compiler's own tests use) so a single eval can
     * contain function/var declarations followed by a trailing expression,
     * e.g. "I64 sq(I64 n){ return n*n; } sq(9)". The function/symbol tables
     * in HCGen are preserved across evals by hc_gen_init() below, which is
     * what gives the REPL its persistent state. */
    hc_lex_init(&compiler->lex, code);
    if (compiler->lex.has_error) {
        snprintf(output, out_size, "Lexer error: %s", compiler->lex.error);
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }
    hc_parse_init(&compiler->parse, &compiler->lex);

    HCParser *p = &compiler->parse;
    p->lex = &compiler->lex;
    p->has_error = false;
    /* Parse the eval source as a single *block* of statements. Wrapping the
     * source in braces gives REPL semantics: a declaration (function/var)
     * registers persistently in the compiler's function/symbol tables across
     * evals (preserved by hc_gen_init()), and a trailing bare expression
     * becomes the eval's return value (it is the last statement, so its
     * result sits in RAX when the generated function returns). This single
     * parse path handles bare exprs ("1+2+3"), decl+expr
     * ("I64 sq(I64 n){ return n*n; } 0"), and pure decls uniformly -- no
     * dual-parse heuristic that previously left the parser in a corrupt
     * state and crashed on garbage input (infinite loop -> OOM -> NULL
     * deref).
     *
     * A bare trailing expression must be a well-formed expression statement,
     * so it needs a terminating ';' -- unless the source already ends with
     * ';' or '}' (an already-complete statement or a block). */
    size_t wlen = strlen(code);
    size_t last = wlen;
    while (last > 0 && isspace((unsigned char)code[last - 1])) last--;
    bool need_semi = (last > 0 && code[last - 1] != ';' && code[last - 1] != '}');
    size_t wcap = wlen + 8; /* "{ " + "%s" + "; " + " }" + NUL (worst case) */
    char *wrapped = malloc(wcap);
    if (!wrapped) {
        snprintf(output, out_size, "Out of memory");
        s->state = SESSION_STATE_ACTIVE;
        return -1;
    }
    snprintf(wrapped, wcap, need_semi ? "{ %s; }" : "{ %s }", code);
    hc_lex_init(&compiler->lex, wrapped);
    hc_parse_init(p, &compiler->lex);
    HCASTNode *ast = hc_parse_block(p);
    free(wrapped);

    if (p->has_error || !ast) {
        hc_ast_free(ast);
        snprintf(output, out_size, "Parse error: %s",
                 p->n_errors ? p->errors[0] : "invalid syntax");
        s->state = SESSION_STATE_ACTIVE;
        return -1;
    }

    if (p->has_error || !ast) {
        hc_ast_free(ast);
        snprintf(output, out_size, "Parse error");
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }

    /* Fresh codegen buffer, but keep accumulated functions/symbols. */
    hc_gen_init(&compiler->gen);

    emit_prologue(&compiler->gen);
    gen_stmt(&compiler->gen, ast);   /* BLOCK/decl/expr: decls register, expr returns */
    emit_epilogue(&compiler->gen);

    hc_ast_free(ast);

    if (compiler->gen.code_size == 0 || compiler->gen.has_error) {
        free(compiler->gen.code);
        free(compiler->gen.data);
        compiler->gen.code = NULL;
        compiler->gen.data = NULL;
        compiler->gen.code_size = 0;
        compiler->gen.data_size = 0;
        snprintf(output, out_size, "Codegen error: %s", compiler->gen.error);
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }

    void *exec = jit_alloc_exec(compiler->gen.code_size + compiler->gen.data_size);
    if (!exec) {
        free(compiler->gen.code);
        free(compiler->gen.data);
        compiler->gen.code = NULL;
        compiler->gen.data = NULL;
        compiler->gen.code_size = 0;
        compiler->gen.data_size = 0;
        snprintf(output, out_size, "JIT allocation failed");
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }

    memcpy(exec, compiler->gen.code, compiler->gen.code_size);
    memcpy((uint8_t *)exec + compiler->gen.code_size, compiler->gen.data, compiler->gen.data_size);

    /* Patch global variable RIP-relative addresses
     * disp32 = global_offset - (final_code_size + 4 - code_patch_pos)
     * At runtime: code at exec, data at exec + code_size
     * RIP after instruction at code_patch_pos - 4 = exec + code_patch_pos + 3 (since instruction is 7 bytes)
     * Target = exec + code_size + global_offset
     * disp32 = target - RIP = (exec + code_size + global_offset) - (exec + code_patch_pos + 4) = code_size + global_offset - code_patch_pos - 4 */
    for (int i = 0; i < compiler->gen.n_global_patches; i++) {
        size_t patch_pos = compiler->gen.global_patches[i].code_patch_pos;
        size_t global_offset = compiler->gen.global_patches[i].global_offset;
        int32_t disp32 = (int32_t)(compiler->gen.code_size + global_offset - patch_pos - 4);
        *(int32_t *)((uint8_t *)exec + patch_pos) = disp32;
    }

    int64_t result = ((int64_t(*)(void))exec)();

    /* Copy runtime data section back to compiler's data section for persistence */
    if (compiler->gen.data_size > 0) {
        memcpy(compiler->gen.data, (uint8_t *)exec + compiler->gen.code_size, compiler->gen.data_size);
    }

    /* Free the executable memory and the per-call code buffer, but PRESERVE
     * the data section. The data section holds global variables (e.g. a
     * module-level `I64 acc = 10;`) whose values must survive across evals to
     * give the REPL its persistent state. hc_gen_init() already saves/restores
     * gen.data across the fresh init below, so the allocation must stay valid.
     * Only the freshly-generated code buffer (and its now-unmapped exec copy)
     * is discarded; the data buffer is reused by the next eval. */
    jit_free_exec(exec, compiler->gen.code_size + compiler->gen.data_size);
    free(compiler->gen.code);
    compiler->gen.code = NULL;
    compiler->gen.code_size = 0;
    compiler->gen.code_cap = 0;

    snprintf(output, out_size, "%ld", (long)result);
    s->state = SESSION_STATE_ACTIVE;
    s->exit_code = (int)(result & 0xFF);
    wubu_holyd_publish_event(d, "eval_complete", session, NULL);
    return 0;
}

int wubu_holyd_compile(WubuHoly *d, const char *session,
                         const char *code, void **out_binary, size_t *out_size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    *out_binary = hc_compile(code, out_size);
    return (*out_binary) ? 0 : -1;
}

int wubu_holyd_run(WubuHoly *d, const char *session,
                     const void *binary, size_t size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (!binary || size == 0) return -1;
    /* Execute compiled binary via JIT */
    ((int64_t(*)(void))(binary))();
    return 0;
}

int wubu_holyd_stop(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->state == SESSION_STATE_RUNNING) {
        s->state = SESSION_STATE_ACTIVE;
        holyd_log(d, 2, "Session '%s' stopped", session);
    }
    return 0;
}

/* -- Helper: get compiler from session (implementation) ----------- */

HCCompiler *holyd_get_compiler(WubuHolySession *s, WubuHoly *d) {
    if (!s->compiler) {
        s->compiler = calloc(1, sizeof(HCCompiler));
        if (!s->compiler) return NULL;
        /* Initialize the compiler components */
        hc_lex_init(&((HCCompiler *)s->compiler)->lex, "");
        hc_parse_init(&((HCCompiler *)s->compiler)->parse, &((HCCompiler *)s->compiler)->lex);
        hc_gen_init(&((HCCompiler *)s->compiler)->gen);
        s->compiler_initialized = true;
        holyd_log(d, 2, "Compiler initialized for session");
    }
    return (HCCompiler *)s->compiler;
}

/* EOF */


