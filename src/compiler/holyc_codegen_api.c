/*
 * holyc_codegen_api.c  --  HolyC Code Generator: Public API
 * Top-level compile/eval functions and public interface.
 */

#include "holyc_codegen_internal.h"

/* ====================================================================
 * PUBLIC API
 * ==================================================================== */

int hc_gen_node(HCGen *gen, const HCASTNode *node) {
    if (!node) return -1;
    if (node->kind == HC_AST_BLOCK || node->kind == HC_AST_FUNC_DECL)
        return gen_stmt(gen, node);
    return gen_expr(gen, node);
}

int hc_gen_function(HCGen *gen, const HCASTNode *func) {
    if (!func || func->kind != HC_AST_FUNC_DECL) return -1;
    return gen_stmt(gen, func);
}

const uint8_t *hc_gen_code(const HCGen *gen, size_t *out_size) {
    if (out_size) *out_size = gen->code_size;
    return gen->code;
}

/* -- Top-Level Compile and Execute -------------------------------- */

void *hc_compile(const char *source, size_t *out_size) {
    HCLexer lex;
    hc_lex_init(&lex, source);

    if (lex.has_error) return NULL;

    HCParser parse;
    hc_parse_init(&parse, &lex);

    HCASTNode *ast = hc_parse_expr(&parse);
    if (parse.has_error) {
        hc_ast_free(ast);
        return NULL;
    }

    HCGen gen;
    hc_gen_init(&gen);
    gen_expr(&gen, ast);
    emit_ret(&gen);

    hc_ast_free(ast);

    if (gen.code_size == 0 || gen.has_error) {
        free(gen.code);
        free(gen.data);
        return NULL;
    }

    /* Allocate executable memory */
    void *exec = jit_alloc_exec(gen.code_size + gen.data_size);
    if (!exec) { free(gen.code); free(gen.data); return NULL; }

    memcpy(exec, gen.code, gen.code_size);
    /* Copy data section after code for string literals */
    if (gen.data_size > 0) {
        memcpy((uint8_t *)exec + gen.code_size, gen.data, gen.data_size);
    }
    if (out_size) *out_size = gen.code_size + gen.data_size;
    free(gen.code);
    free(gen.data);

    return exec;
}

int64_t hc_eval(const char *source) {
    HCLexer lex;
    hc_lex_init(&lex, source);

    if (lex.has_error) return 0;

    HCParser parse;
    hc_parse_init(&parse, &lex);

    /* Try parsing as expression first */
    HCASTNode *ast = hc_parse_expr(&parse);

    /* If expression parsing failed or didn't consume all input, try block parsing */
    if (parse.has_error || (hc_parse_peek(&parse) != HC_TOK_EOF && hc_parse_peek(&parse) != HC_TOK_SEMI)) {
        hc_ast_free(ast);
        parse.has_error = false;
        parse.n_errors = 0;
        /* Re-lex from start */
        hc_lex_init(&lex, source);
        hc_parse_init(&parse, &lex);
        
        /* Check if source starts with a control keyword */
        const char *p = source;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        bool starts_with_keyword = false;
        if (strncmp(p, "if ", 3) == 0 || strncmp(p, "while ", 6) == 0 ||
            strncmp(p, "for ", 4) == 0 || strncmp(p, "do ", 3) == 0 ||
            strncmp(p, "return", 6) == 0 || strncmp(p, "break", 5) == 0 ||
            strncmp(p, "continue", 8) == 0 || *p == '{') {
            starts_with_keyword = true;
        }
        
        /* Wrap in braces to parse as block if it contains semicolons AND doesn't start with keyword */
        bool has_semicolon = false;
        p = source;
        while (*p) {
            if (*p == ';') { has_semicolon = true; break; }
            p++;
        }
        
        if (has_semicolon && !starts_with_keyword) {
            /* Create a temporary buffer with braces */
            size_t len = strlen(source);
            char *wrapped = malloc(len + 3);
            sprintf(wrapped, "{ %s }", source);
            hc_lex_init(&lex, wrapped);
            hc_parse_init(&parse, &lex);
            /* hc_lex_init already primes the first token (which is `{`) */
            ast = hc_parse_block(&parse);
            free(wrapped);
        } else {
            ast = hc_parse_stmt(&parse);
        }
    }

    if (parse.has_error || !ast) {
        hc_ast_free(ast);
        return 0;
    }

    HCGen gen;
    hc_gen_init(&gen);
    /* hc_eval compiles a self-contained expression/block: each call is an
     * isolated scope, so do NOT inherit the REPL-persisted symbol/function
     * tables or data section from a previous eval. (hc_gen_init preserves
     * them for the interactive REPL; standalone eval must reset to avoid
     * stale stack offsets, cross-call symbol leakage, and a double-free of
     * the shared data buffer across successive evals.) */
    memset(&gen.symbols, 0, sizeof(gen.symbols));
    gen.n_functions = 0;
    gen.n_global_patches = 0;
    gen.data = NULL;
    gen.data_size = 0;
    gen.data_cap = 0;
    emit_prologue(&gen);

    if (ast->kind == HC_AST_BLOCK) {
        gen_stmt(&gen, ast);
    } else if (ast->kind == HC_AST_EXPR_STMT || ast->kind == HC_AST_RETURN ||
        ast->kind == HC_AST_IF || ast->kind == HC_AST_WHILE ||
        ast->kind == HC_AST_FOR || ast->kind == HC_AST_DO_WHILE ||
        ast->kind == HC_AST_VAR_DECL || ast->kind == HC_AST_FUNC_DECL) {
        gen_stmt(&gen, ast);
    } else {
        gen_expr(&gen, ast);
    }
    emit_epilogue(&gen);

    hc_ast_free(ast);

    if (gen.code_size == 0 || gen.has_error) {
        free(gen.code);
        free(gen.data);
        return 0;
    }

    void *exec = jit_alloc_exec(gen.code_size + gen.data_size);
    if (!exec) { free(gen.code); free(gen.data); return 0; }

    memcpy(exec, gen.code, gen.code_size);
    /* Copy data section after code - string literals need to be readable */
    if (gen.data_size > 0) {
        memcpy((uint8_t *)exec + gen.code_size, gen.data, gen.data_size);
    }

    /* Patch global variable RIP-relative addresses (module-level vars
     * declared in the data section). Matches wubu_holyd_eval's fixup. */
    for (int i = 0; i < gen.n_global_patches; i++) {
        size_t patch_pos = gen.global_patches[i].code_patch_pos;
        size_t global_offset = gen.global_patches[i].global_offset;
        int32_t disp32 = (int32_t)(gen.code_size + global_offset - patch_pos - 4);
        *(int32_t *)((uint8_t *)exec + patch_pos) = disp32;
    }

    /* Make memory executable. The data section holds global variables that
     * are written at runtime (e.g. `x = 5` stores into it, and the REPL copies
     * it back), so when a data section exists the page must remain writable.
     * Only drop write permission for pure-code buffers (no globals), matching
     * the holyd eval path which never locks. Locking an RWX page to RX would
     * turn runtime global stores into SIGSEGVs. */
    if (gen.data_size == 0) {
        jit_lock_exec(exec, gen.code_size + gen.data_size);
    }

    int64_t result = JIT_CALL(exec);
    jit_free_exec(exec, gen.code_size + gen.data_size);
    free(gen.code);
    free(gen.data);

    return result;
}

void *hc_compile_func(const char *source, const char *func_name) {
    HCLexer lex;
    hc_lex_init(&lex, source);
    if (lex.has_error) return NULL;

    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) {
        hc_ast_free(ast);
        return NULL;
    }

    /* Collect all function declarations */
    HCASTNode *funcs[HC_MAX_FUNCTIONS];
    int n_funcs = 0;
    HCASTNode *target_func = NULL;
    for (int i = 0; i < ast->n_stmts && n_funcs < HC_MAX_FUNCTIONS; i++) {
        if (ast->stmts[i]->kind == HC_AST_FUNC_DECL) {
            funcs[n_funcs++] = ast->stmts[i];
            if (func_name && func_name[0] && ast->stmts[i]->ident) {
                if (strcmp(ast->stmts[i]->ident, func_name) == 0) {
                    target_func = ast->stmts[i];
                }
            }
        }
    }

    /* If no functions found, treat as expression */
    if (n_funcs == 0) {
        hc_ast_free(ast);
        HCLexer lex2;
        hc_lex_init(&lex2, source);
        HCParser parse2;
        hc_parse_init(&parse2, &lex2);
        HCASTNode *expr = hc_parse_expr(&parse2);

        HCGen gen;
        hc_gen_init(&gen);
        emit_prologue(&gen);
        gen_expr(&gen, expr);
        emit_epilogue(&gen);
        hc_ast_free(expr);

        if (gen.code_size == 0) { free(gen.code); return NULL; }
        void *exec = jit_alloc_exec(gen.code_size);
        if (!exec) { free(gen.code); return NULL; }
        memcpy(exec, gen.code, gen.code_size);
        free(gen.code);
        return exec;
    }

    /* If func_name specified but not found, fail */
    if (func_name && func_name[0] && !target_func) {
        hc_ast_free(ast);
        return NULL;
    }

    /* If no func_name specified, use first function */
    if (!target_func) target_func = funcs[0];

    /* Generate all functions into a single code buffer so they can call each other */
    HCGen gen;
    hc_gen_init(&gen);
    /* Register all function names in the gen's function table for cross-referencing */
    for (int i = 0; i < n_funcs; i++) {
        if (funcs[i]->ident && gen.n_functions < HC_MAX_FUNCTIONS) {
            strncpy(gen.functions[gen.n_functions].name, funcs[i]->ident, 63);
            gen.functions[gen.n_functions].name[63] = '\0';
            /* func_ptr will be patched after code generation */
            gen.functions[gen.n_functions].func_ptr = NULL;
            gen.n_functions++;
        }
    }

    /* Generate each function, recording its code offset */
    typedef struct { char name[64]; size_t code_offset; } func_offset_t;
    func_offset_t foffsets[HC_MAX_FUNCTIONS];
    int n_foffsets = 0;

    for (int i = 0; i < n_funcs; i++) {
        size_t offset_before = gen.code_size;
        hc_gen_function(&gen, funcs[i]);
        if (funcs[i]->ident) {
            strncpy(foffsets[n_foffsets].name, funcs[i]->ident, 63);
            foffsets[n_foffsets].name[63] = '\0';
            foffsets[n_foffsets].code_offset = offset_before;
            n_foffsets++;
        }
    }

    hc_ast_free(ast);

    if (gen.code_size == 0) { free(gen.code); free(gen.data); return NULL; }

    void *exec = jit_alloc_exec(gen.code_size + gen.data_size);
    if (!exec) { free(gen.code); free(gen.data); return NULL; }
    memcpy(exec, gen.code, gen.code_size);
    if (gen.data_size > 0) {
        memcpy((uint8_t *)exec + gen.code_size, gen.data, gen.data_size);
    }

    /* Patch function table pointers to actual addresses */
    for (int i = 0; i < gen.n_functions; i++) {
        for (int j = 0; j < n_foffsets; j++) {
            if (strcmp(gen.functions[i].name, foffsets[j].name) == 0) {
                gen.functions[i].func_ptr = (uint8_t *)exec + foffsets[j].code_offset;
                break;
            }
        }
    }

    /* If a specific function was requested, return a thunk that adjusts offset */
    if (target_func && target_func->ident) {
        /* Find the target function's offset */
        for (int j = 0; j < n_foffsets; j++) {
            if (strcmp(foffsets[j].name, target_func->ident) == 0) {
                /* Return the exec base; caller can use offset if needed.
                 * For JIT_CALL compatibility, we return the target function address directly. */
                free(gen.code);
                free(gen.data);
                return (uint8_t *)exec + foffsets[j].code_offset;
            }
        }
    }

    free(gen.code);
    free(gen.data);
    return exec;
}