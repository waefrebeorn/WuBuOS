/*
 * holyc_codegen_stmt.c  --  HolyC Code Generator: Statement Generation
 * Generates x86-64 machine code for HolyC AST statements.
 */

#include "holyc_codegen_internal.h"

/* ====================================================================
 * CODE GEN INIT
 * ==================================================================== */

void hc_gen_init(HCGen *gen) {
    /* Preserve symbol table, function table, and data section across evaluations for REPL persistence */
    HCSymTab saved_symbols = gen->symbols;
    HCFunction saved_functions[HC_MAX_FUNCTIONS];
    int saved_n_functions = gen->n_functions;
    memcpy(saved_functions, gen->functions, sizeof(HCFunction) * HC_MAX_FUNCTIONS);
    
    /* Preserve data section (globals) */
    uint8_t *saved_data = gen->data;
    size_t saved_data_size = gen->data_size;
    size_t saved_data_cap = gen->data_cap;
    
    /* Do NOT preserve global_patches - they are ephemeral per-compilation */
    
    memset(gen, 0, sizeof(*gen));
    
    /* Restore symbol table, function table, and data section */
    gen->symbols = saved_symbols;
    gen->n_functions = saved_n_functions;
    memcpy(gen->functions, saved_functions, sizeof(HCFunction) * HC_MAX_FUNCTIONS);
    
    gen->data = saved_data;
    gen->data_size = saved_data_size;
    gen->data_cap = saved_data_cap;
    
    /* global_patches start fresh for each compilation - n_global_patches = 0 */
}

/* ====================================================================
 * STATEMENT GENERATION
 * ==================================================================== */

int gen_stmt(HCGen *gen, const HCASTNode *node) {
    if (!node) return 0;

    switch (node->kind) {
        case HC_AST_EXPR_STMT:
            return gen_expr(gen, node->child);

        case HC_AST_EXTERN_DECL:
            /* Extern declarations are no-ops at codegen time.
             * They register the function name and C name for the function call handler. */
            break;

        case HC_AST_RETURN:
            if (node->child)
                gen_expr(gen, node->child);
            else
                emit_mov_rax_imm64(gen, 0);
            emit_epilogue(gen);
            break;

        case HC_AST_BLOCK:
            for (int i = 0; i < node->n_stmts; i++)
                gen_stmt(gen, node->stmts[i]);
            break;

        /* if (cond) then [else else]
         *   eval cond → rax
         *   test rax, rax
         *   jz else_label             (5 bytes, placeholder)
         *   gen then_branch
         *   jmp end_label             (5 bytes, placeholder)  [if else exists]
         * else_label:
         *   gen else_branch           [if exists]
         * end_label:
         */
        case HC_AST_IF: {
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz else */
            gen_stmt(gen, node->then_branch);
            if (node->else_branch) {
                size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
                size_t else_label = gen->code_size;
                gen_stmt(gen, node->else_branch);
                size_t end_label = gen->code_size;
                patch_rel32(gen, jz_patch, else_label);
                patch_rel32(gen, jmp_patch, end_label);
            } else {
                size_t end_label = gen->code_size;
                patch_rel32(gen, jz_patch, end_label);
            }
            break;
        }

        /* while (cond) body
         * loop_top:
         *   eval cond → rax
         *   test rax, rax
         *   jz loop_end               (5 bytes, placeholder)
         *   gen body
         *   jmp loop_top              (5 bytes, back jump)
         * loop_end:
         */
        case HC_AST_WHILE: {
            size_t loop_top = gen->code_size;
            int depth = gen->loop_depth;
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz loop_end */
            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;
            gen_stmt(gen, node->body);
            /* Continue target is loop_top (condition check) */
            size_t continue_label = loop_top;
            /* jmp loop_top (back jump) */
            size_t jmp_patch = emit_jmp_placeholder(gen);
            patch_rel32(gen, jmp_patch, loop_top);
            size_t loop_end = gen->code_size;
            patch_rel32(gen, jz_patch, loop_end);
            /* Patch all break statements in this loop to jump to loop_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++) {
                patch_rel32(gen, gen->break_patches[depth][i], loop_end);
            }
            /* Patch all continue statements to jump to continue_label */
            for (int i = 0; i < gen->n_continue_patches[depth]; i++) {
                patch_rel32(gen, gen->continue_patches[depth][i], continue_label);
            }
            gen->loop_depth--;
            break;
        }

        /* do body while (cond)
         * loop_top:
         *   gen body
         *   eval cond → rax
         *   test rax, rax
         *   jnz loop_top              (5 bytes, back jump)
         * loop_end:                   (break target)
         */
        case HC_AST_DO_WHILE: {
            size_t loop_top = gen->code_size;
            int depth = gen->loop_depth;
            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;
            gen_stmt(gen, node->body);
            /* Continue target is the condition check */
            size_t continue_label = gen->code_size;
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jnz_patch = emit_jcc_placeholder(gen, CC_NE); /* jnz loop_top */
            patch_rel32(gen, jnz_patch, loop_top);
            size_t loop_end = gen->code_size;
            /* Patch all break statements in this loop to jump to loop_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++) {
                patch_rel32(gen, gen->break_patches[depth][i], loop_end);
            }
            /* Patch all continue statements to jump to continue_label */
            for (int i = 0; i < gen->n_continue_patches[depth]; i++) {
                patch_rel32(gen, gen->continue_patches[depth][i], continue_label);
            }
            gen->loop_depth--;
            break;
        }

        /* for (init; cond; update) body
         *   gen init
         * loop_top:
         *   eval cond → rax
         *   test rax, rax
         *   jz loop_end               (5 bytes, placeholder)
         *   gen body
         * continue_label:
         *   gen update
         *   jmp loop_top              (5 bytes, back jump)
         * loop_end:
         */
        case HC_AST_FOR: {
            int depth = gen->loop_depth;
            /* init */
            if (node->init_expr)
                gen_expr(gen, node->init_expr);

            size_t loop_top = gen->code_size;

            /* condition */
            if (node->cond)
                gen_expr(gen, node->cond);
            else
                emit_mov_rax_imm64(gen, 1); /* no cond = infinite loop (true) */

            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz loop_end */

            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;

            /* body */
            if (node->body)
                gen_stmt(gen, node->body);

            /* continue target = update */
            size_t continue_label = gen->code_size;

            /* update */
            if (node->update)
                gen_expr(gen, node->update);

            /* jmp loop_top */
            size_t jmp_patch = emit_jmp_placeholder(gen);
            patch_rel32(gen, jmp_patch, loop_top);

            size_t loop_end = gen->code_size;
            patch_rel32(gen, jz_patch, loop_end);
            /* Patch all break statements in this loop to jump to loop_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++) {
                patch_rel32(gen, gen->break_patches[depth][i], loop_end);
            }
            /* Patch all continue statements to jump to continue_label */
            for (int i = 0; i < gen->n_continue_patches[depth]; i++) {
                patch_rel32(gen, gen->continue_patches[depth][i], continue_label);
            }
            gen->loop_depth--;
            break;
        }

        case HC_AST_VAR_DECL:
            /* Module-level var = global in the data section (persists across
             * evals). Inside a function body it's a stack-local. Note emit_prologue()
             * runs before gen_stmt for module evals, so has_prologue is unreliable
             * here — use in_function to tell them apart. */
            bool is_module_level = !gen->in_function;
           
            if (is_module_level) {
                /* Top-level variable: store in data section as global */
                if (node->init) {
                    gen_expr(gen, node->init);
                    /* Reserve space in data section */
                    size_t global_offset = gen->data_size;
                    /* Align to 8 bytes */
                    while (gen->data_size % 8 != 0) {
                        emit_data_byte(gen, 0);
                    }
                    /* Reserve 8 bytes for global */
                    emit_data_qword(gen, 0);
                    
                    /* Record in symbol table (negative offset = global in data section) */
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = -(int)global_offset;
                        gen->symbols.n_locals++;
                    }
                   
                    /* Emit: mov [rip + disp32], rax with placeholder disp32
                     * The actual disp32 will be patched in wubu_holyd_eval after final code_size is known */
                    size_t patch_pos = gen->code_size;
                    emit_byte(gen, 0x48); /* REX.W */
                    emit_byte(gen, 0x89); /* mov r/m64, rax */
                    emit_byte(gen, 0x05); /* [rip + disp32] */
                    emit_dword(gen, 0);   /* placeholder disp32 */
                   
                    /* Store patch info for runtime fixup */
                    if (gen->n_global_patches < 32) {
                        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos + 3; /* Position of disp32 */
                        gen->global_patches[gen->n_global_patches].global_offset = global_offset;
                        gen->n_global_patches++;
                    }
                }
            } else {
                /* Local variable inside function: store on stack */
                if (node->init) {
                    gen_expr(gen, node->init);
                    /* Store rax to stack: mov [rbp - offset], rax */
                    int offset = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = offset;
                        gen->symbols.n_locals++;
                    }
                    /* mov [rbp - offset], rax: 48 89 85 disp32 */
                    emit_byte(gen, 0x48); /* REX.W */
                    emit_byte(gen, 0x89); /* mov */
                    emit_byte(gen, 0x85); /* [rbp+disp32] */
                    emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                }
            }
            break;

        case HC_AST_FUNC_DECL:
                        /* Generate function body and save function pointer */
                        /* Save current code state */
                        uint8_t *saved_code = gen->code;
                        size_t saved_code_size = gen->code_size;
                        size_t saved_code_cap = gen->code_cap;
                        HCSymTab saved_symbols = gen->symbols;
                        int saved_n_functions = gen->n_functions;
                        HCFunction saved_functions[HC_MAX_FUNCTIONS];
                        memcpy(saved_functions, gen->functions, sizeof(HCFunction) * HC_MAX_FUNCTIONS);
           
                        gen->code = NULL;
                        gen->code_size = 0;
                        gen->code_cap = 0;
                        /* Reset symbols but keep functions */
                        memset(&gen->symbols, 0, sizeof(HCSymTab));
           
                        emit_prologue(gen);
           
                        /* Add function parameters to symbol table before compiling body */
            for (int i = 0; i < node->n_params; i++) {
                int offset = gen->symbols.stack_size + 8;
                gen->symbols.stack_size += 8;
                if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                    strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                            node->param_names[i], HC_MAX_IDENT_LEN - 1);
                    gen->symbols.locals[gen->symbols.n_locals].stack_offset = offset;
                    gen->symbols.n_locals++;
                }
                /* Store parameter from register to stack slot */
                switch (i) {
                    case 0: /* rdi -> [rbp - offset] */
                        /* mov [rbp - offset], rdi: 48 89 BD disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xBD);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 1: /* rsi -> [rbp - offset] */
                        /* mov [rbp - offset], rsi: 48 89 B5 disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xB5);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 2: /* rdx -> [rbp - offset] */
                        /* mov [rbp - offset], rdx: 48 89 95 disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x95);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 3: /* rcx -> [rbp - offset] */
                        /* mov [rbp - offset], rcx: 48 89 8D disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x8D);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 4: /* r8 -> [rbp - offset] */
                        /* mov [rbp - offset], r8: 4C 89 85 disp32 */
                        emit_byte(gen, 0x4C); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 5: /* r9 -> [rbp - offset] */
                        /* mov [rbp - offset], r9: 4C 89 8D disp32 */
                        emit_byte(gen, 0x4C); emit_byte(gen, 0x89); emit_byte(gen, 0x8D);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                }
            }
           
            /* Allocate stack frame for locals */
            gen->in_function = true;
            if (node->body)
            gen_stmt(gen, node->body);
            gen->in_function = false;
            emit_epilogue(gen);

            /* Allocate executable memory for this function */
            if (gen->code_size > 0 && gen->n_functions < HC_MAX_FUNCTIONS) {
                void *exec = jit_alloc_exec(gen->code_size);
                if (exec) {
                    memcpy(exec, gen->code, gen->code_size);
                    /* Restore main code buffer */
                    gen->code = saved_code;
                    gen->code_size = saved_code_size;
                    gen->code_cap = saved_code_cap;
                    gen->symbols = saved_symbols;
                    gen->n_functions = saved_n_functions;
                    memcpy(gen->functions, saved_functions, sizeof(HCFunction) * HC_MAX_FUNCTIONS);
                   
                    strncpy(gen->functions[gen->n_functions].name,
                            node->ident, HC_MAX_IDENT_LEN - 1);
                    gen->functions[gen->n_functions].func_ptr = exec;
                    gen->functions[gen->n_functions].n_params = node->n_params;
                    gen->n_functions++;
                } else {
                    /* Restore on failure */
                    free(gen->code);
                    gen->code = saved_code;
                    gen->code_size = saved_code_size;
                    gen->code_cap = saved_code_cap;
                    gen->symbols = saved_symbols;
                }
            }
            break;

        case HC_AST_BREAK:
            /* Emit jump to loop end - will be patched when loop ends */
            if (gen->loop_depth > 0 && gen->loop_depth <= 10) {
                int depth = gen->loop_depth - 1;
                if (gen->n_break_patches[depth] < 16) {
                    size_t patch = emit_jmp_placeholder(gen);
                    gen->break_patches[depth][gen->n_break_patches[depth]++] = patch;
                }
            }
            break;

        case HC_AST_CONTINUE:
            /* Emit jump to loop continue/condition - will be patched when loop ends */
            if (gen->loop_depth > 0 && gen->loop_depth <= 10) {
                int depth = gen->loop_depth - 1;
                if (gen->n_continue_patches[depth] < 16) {
                    size_t patch = emit_jmp_placeholder(gen);
                    gen->continue_patches[depth][gen->n_continue_patches[depth]++] = patch;
                }
            }
            break;

        default:
            /* Try as expression */
            return gen_expr(gen, node);
    }

    return 0;
}