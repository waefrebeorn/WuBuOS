/*
 * wubu_holyd_exec.c  --  WuBuOS HolyC DOS Daemon: Exec
 */

#include "wubu_holyd_internal.h"

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

    /* Get or create persistent compiler for this session */
    HCCompiler *compiler = holyd_get_compiler(s, d);
    if (!compiler) {
        snprintf(output, out_size, "Compiler initialization failed");
        s->state = SESSION_STATE_ACTIVE;
        return -1;
    }

    /* Use the persistent compiler state */
    HCLexer *lex = &compiler->lex;
    HCParser *parse = &compiler->parse;
    HCGen *gen = &compiler->gen;

    /* Re-lex with the new code, keeping the existing compiler state */
    hc_lex_init(lex, code);

    if (lex->has_error) {
        snprintf(output, out_size, "Lexer error: %s", lex->error);
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }

    HCParser *p = parse;
    p->lex = lex;
    p->has_error = false;
    p->n_errors = 0;

    /* Parse with persistent symbol table */
    HCASTNode *ast = hc_parse_expr(p);

    if (p->has_error || (hc_parse_peek(p) != HC_TOK_EOF && hc_parse_peek(p) != HC_TOK_SEMI)) {
        hc_ast_free(ast);
        p->has_error = false;
        p->n_errors = 0;
        hc_lex_init(lex, code);
        hc_parse_init(p, lex);

        const char *p_src = code;
        while (*p_src && (*p_src == ' ' || *p_src == '\t' || *p_src == '\n' || *p_src == '\r')) p_src++;
        bool starts_with_keyword = false;
        if (strncmp(p_src, "if ", 3) == 0 || strncmp(p_src, "while ", 6) == 0 ||
            strncmp(p_src, "for ", 4) == 0 || strncmp(p_src, "do ", 3) == 0 ||
            strncmp(p_src, "return", 6) == 0 || strncmp(p_src, "break", 5) == 0 ||
            strncmp(p_src, "continue", 8) == 0 || *p_src == '{') {
            starts_with_keyword = true;
        }

        bool has_semicolon = false;
        const char *p2 = code;
        while (*p2) {
            if (*p2 == ';') { has_semicolon = true; break; }
            p2++;
        }

        if (has_semicolon && !starts_with_keyword) {
            size_t len = strlen(code);
            char *wrapped = malloc(len + 3);
            sprintf(wrapped, "{ %s }", code);
            hc_lex_init(lex, wrapped);
            hc_parse_init(parse, lex);
            ast = hc_parse_block(parse);
            free(wrapped);
        } else {
            hc_ast_free(ast);
            ast = hc_parse_stmt(p);
        }
    }

    if (p->has_error || !ast) {
        hc_ast_free(ast);
        snprintf(output, out_size, "Parse error");
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }

    /* Use the persistent HCGen with accumulated symbols/functions */
    hc_gen_init(gen);  /* Reset code buffer but keep symbols/functions */

    emit_prologue(&compiler->gen);

    if (ast->kind == HC_AST_BLOCK) {
        gen_stmt(&compiler->gen, ast);
    } else if (ast->kind == HC_AST_EXPR_STMT || ast->kind == HC_AST_RETURN ||
        ast->kind == HC_AST_IF || ast->kind == HC_AST_WHILE ||
        ast->kind == HC_AST_FOR || ast->kind == HC_AST_DO_WHILE ||
        ast->kind == HC_AST_VAR_DECL || ast->kind == HC_AST_FUNC_DECL) {
        gen_stmt(&compiler->gen, ast);
    } else {
        gen_expr(&compiler->gen, ast);
    }
    emit_epilogue(&compiler->gen);

    hc_ast_free(ast);

    if (compiler->gen.code_size == 0 || compiler->gen.has_error) {
        free(compiler->gen.code);
        free(compiler->gen.data);
        snprintf(output, out_size, "Codegen error: %s", compiler->gen.error);
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }

    void *exec = jit_alloc_exec(compiler->gen.code_size + compiler->gen.data_size);
    if (!exec) {
        free(compiler->gen.code);
        free(compiler->gen.data);
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
        *(int32_t*)((uint8_t *)exec + patch_pos) = disp32;
    }

    int64_t result = ((int64_t(*)(void))exec)();

    /* Copy runtime data section back to compiler's data section for persistence */
    if (compiler->gen.data_size > 0) {
        memcpy(compiler->gen.data, (uint8_t *)exec + compiler->gen.code_size, compiler->gen.data_size);
    }

    /* Free the executable memory but keep the compiler state */
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

int wubu_holyd_repl_start(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->compiler) return 0; /* Already started */
    
    HCCompiler *compiler = holyd_get_compiler(s, d);
    if (!compiler) return -1;
    
    s->state = SESSION_STATE_ACTIVE;
    holyd_log(d, 2, "REPL started for session '%s'", session);
    wubu_holyd_publish_event(d, "repl_started", session, NULL);
    return 0;
}

int wubu_holyd_repl_eval(WubuHoly *d, const char *session,
                          const char *code, char *output, size_t out_size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) { snprintf(output, out_size, "Session '%s' not found", session); return -1; }
    if (s->state != SESSION_STATE_ACTIVE) {
        snprintf(output, out_size, "Session '%s' not active (state=%s)",
                 session, wubu_holyd_session_state_str(s->state));
        return -1;
    }
    
    HCCompiler *compiler = holyd_get_compiler(s, d);
    if (!compiler) { snprintf(output, out_size, "Compiler initialization failed"); return -1; }
    
    s->state = SESSION_STATE_RUNNING;
    s->last_active = time(NULL);
    d->evals_performed++;
    
    holyd_log(d, 2, "REPL eval in session '%s': %.60s...", session, code);
    
    /* Use the persistent compiler state */
    HCLexer *lex = &compiler->lex;
    HCParser *parse = &compiler->parse;
    HCGen *gen = &compiler->gen;
    
    /* Re-lex with the new code, keeping the existing compiler state */
    hc_lex_init(lex, code);
    
    if (lex->has_error) {
        snprintf(output, out_size, "Lexer error: %s", lex->error);
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }
    
    HCParser *p = parse;
    p->lex = lex;
    p->has_error = false;
    p->n_errors = 0;
    
    /* Parse with persistent symbol table */
    HCASTNode *ast = hc_parse_expr(p);
    
    if (p->has_error || (hc_parse_peek(p) != HC_TOK_EOF && hc_parse_peek(p) != HC_TOK_SEMI)) {
        hc_ast_free(ast);
        p->has_error = false;
        p->n_errors = 0;
        hc_lex_init(lex, code);
        hc_parse_init(p, lex);
        
        const char *p_src = code;
        while (*p_src && (*p_src == ' ' || *p_src == '\t' || *p_src == '\n' || *p_src == '\r')) p_src++;
        bool starts_with_keyword = false;
        if (strncmp(p_src, "if ", 3) == 0 || strncmp(p_src, "while ", 6) == 0 ||
            strncmp(p_src, "for ", 4) == 0 || strncmp(p_src, "do ", 3) == 0 ||
            strncmp(p_src, "return", 6) == 0 || strncmp(p_src, "break", 5) == 0 ||
            strncmp(p_src, "continue", 8) == 0 || *p_src == '{') {
            starts_with_keyword = true;
        }
        
        bool has_semicolon = false;
        const char *p2 = code;
        while (*p2) {
            if (*p2 == ';') { has_semicolon = true; break; }
            p2++;
        }
        
        if (has_semicolon && !starts_with_keyword) {
            size_t len = strlen(code);
            char *wrapped = malloc(len + 3);
            sprintf(wrapped, "{ %s }", code);
            hc_lex_init(lex, wrapped);
            hc_parse_init(parse, lex);
            HCASTNode *ast = hc_parse_block(parse);
            free(wrapped);
        } else {
            hc_ast_free(ast);
            ast = hc_parse_stmt(p);
        }
    }
    
    if (parse->has_error || !ast) {
        hc_ast_free(ast);
        snprintf(output, out_size, "Parse error");
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }
    
    /* Use the persistent HCGen with accumulated symbols/functions */
    hc_gen_init(gen);  /* Reset code buffer but keep symbols/functions */
    
    emit_prologue(&compiler->gen);
    
    if (ast->kind == HC_AST_BLOCK) {
        gen_stmt(&compiler->gen, ast);
    } else if (ast->kind == HC_AST_EXPR_STMT || ast->kind == HC_AST_RETURN ||
        ast->kind == HC_AST_IF || ast->kind == HC_AST_WHILE ||
        ast->kind == HC_AST_FOR || ast->kind == HC_AST_DO_WHILE ||
        ast->kind == HC_AST_VAR_DECL || ast->kind == HC_AST_FUNC_DECL) {
        gen_stmt(&compiler->gen, ast);
    } else {
        gen_expr(&compiler->gen, ast);
    }
    emit_epilogue(&compiler->gen);
    
    hc_ast_free(ast);
    
    if (compiler->gen.code_size == 0 || compiler->gen.has_error) {
        free(compiler->gen.code);
        free(compiler->gen.data);
        snprintf(output, out_size, "Codegen error: %s", compiler->gen.error);
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }
    
    void *exec = jit_alloc_exec(compiler->gen.code_size + compiler->gen.data_size);
    if (!exec) { 
        free(compiler->gen.code); 
        free(compiler->gen.data); 
        snprintf(output, out_size, "JIT allocation failed");
        s->state = SESSION_STATE_ACTIVE;
        return 0;
    }
    
    memcpy(exec, compiler->gen.code, compiler->gen.code_size);
    memcpy((uint8_t *)exec + compiler->gen.code_size, compiler->gen.data, compiler->gen.data_size);
    
    int64_t result = ((int64_t(*)(void))exec)();
    
    /* Free the executable memory but keep the compiler state */
    jit_free_exec(exec, compiler->gen.code_size + compiler->gen.data_size);
    free(compiler->gen.code);
    free(compiler->gen.data);
    compiler->gen.code = NULL;
    compiler->gen.data = NULL;
    compiler->gen.code_size = 0;
    compiler->gen.data_size = 0;
    
    snprintf(output, out_size, "%ld", (long)result);
    s->state = SESSION_STATE_ACTIVE;
    s->exit_code = (int)(result & 0xFF);
    wubu_holyd_publish_event(d, "repl_eval_complete", session, NULL);
    return 0;
}

int wubu_holyd_repl_stop(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->compiler) {
        free(s->compiler);
        s->compiler = NULL;
        s->compiler_initialized = false;
    }
    holyd_log(d, 2, "REPL stopped for session '%s'", session);
    wubu_holyd_publish_event(d, "repl_stopped", session, NULL);
    return 0;
}

/* Simple macro storage (name -> value) */
#define MAX_MACROS 64
typedef struct {
    char name[64];
    char value[1024];
} HolyMacro;

static HolyMacro session_macros[WUBU_HOLYD_MAX_SESSIONS][MAX_MACROS];
static int macro_counts[WUBU_HOLYD_MAX_SESSIONS] = {0};

int wubu_holyd_macro_define(WubuHoly *d, const char *session,
                            const char *name, const char *value) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s || !name || !value) return -1;
    
    int idx = (int)(s - d->sessions);
    if (idx < 0 || idx >= WUBU_HOLYD_MAX_SESSIONS) return -1;
    
    /* Check if already exists */
    for (int i = 0; i < macro_counts[idx]; i++) {
        if (strcmp(session_macros[idx][i].name, name) == 0) {
            strncpy(session_macros[idx][i].value, value, sizeof(session_macros[idx][i].value) - 1);
            holyd_log(d, 2, "Macro '%s' updated in session '%s'", name, session);
            return 0;
        }
    }
    
    if (macro_counts[idx] >= MAX_MACROS) return -1;
    
    strncpy(session_macros[idx][macro_counts[idx]].name, name, sizeof(session_macros[idx][0].name) - 1);
    strncpy(session_macros[idx][macro_counts[idx]].value, value, sizeof(session_macros[idx][0].value) - 1);
    macro_counts[idx]++;
    holyd_log(d, 2, "Macro '%s' defined in session '%s'", name, session);
    return 0;
}

int wubu_holyd_macro_undef(WubuHoly *d, const char *session,
                            const char *name) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s || !name) return -1;
    
    int idx = (int)(s - d->sessions);
    if (idx < 0 || idx >= WUBU_HOLYD_MAX_SESSIONS) return -1;
    
    for (int i = 0; i < macro_counts[idx]; i++) {
        if (strcmp(session_macros[idx][i].name, name) == 0) {
            /* Remove by shifting remaining macros */
            for (int j = i; j < macro_counts[idx] - 1; j++) {
                session_macros[idx][j] = session_macros[idx][j + 1];
            }
            macro_counts[idx]--;
            holyd_log(d, 2, "Macro '%s' undefined in session '%s'", name, session);
            return 0;
        }
    }
    return -1; /* Not found */
}

