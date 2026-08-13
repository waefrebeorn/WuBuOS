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
    
    /* Tailslayer DRAM hedge is ON by default: all compiled code gets a
     * software prefetch before every load. The runtime/API can turn it
     * off (gen.hedge_loads = false) for microbenchmarking the overhead. */
    gen->hedge_loads = true;
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

        case HC_AST_RETURN: {
            HCType *ret_t = node->child ? expr_static_type(gen, node->child) : NULL;
            int ret_sz = (ret_t && ret_t->kind == HC_TYPE_STRUCT) ? (int)hc_type_size(ret_t) : 0;
            /* Struct-by-value return: memcpy the local struct into a
             * 16-byte-aligned .data slot, return rax=&slot. The caller does
             * the matching rep movsb on ASSIGN. Works for ANY struct size
             * now that local struct slots are sized correctly. */
            bool can_sret = ret_sz > 0 && node->child &&
                            node->child->kind == HC_AST_IDENT &&
                            gen->symbols.n_locals > 0;
            if (can_sret) {
                /* Reserve a 16-byte-aligned slot in .data for the returned
                 * struct copy. The caller treats rax as the pointer to it
                 * (a tiny memory-return ABI). */
                int pad = (16 - (gen->data_size % 16)) % 16;
                for (int p_ = 0; p_ < pad; p_++) emit_data_byte(gen, 0);
                int slot_off = (int)gen->data_size;
                for (int p_ = 0; p_ < ret_sz; p_++) emit_data_byte(gen, 0);
                int off = 0, is_global = 0;
                resolve_var(gen, node->child->ident, &off, &is_global);
                /* lea rsi, [rbp - off] (src = local) */
                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0xB5);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                /* mov rdi, slot_addr (dst) */
                emit_mov_rax_imm64(gen, (int64_t)(size_t)(gen->data + slot_off));
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC7);
                /* mov rcx, ret_sz */
                emit_byte(gen, 0x48); emit_byte(gen, 0xC7); emit_byte(gen, 0xC1);
                emit_dword(gen, (uint32_t)ret_sz);
                /* rep movsb */
                emit_rep_movsb(gen);
                /* rax = &slot (return value) */
                emit_mov_rax_imm64(gen, (int64_t)(size_t)(gen->data + slot_off));
            } else if (node->child) {
                gen_expr(gen, node->child);
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            emit_epilogue(gen);
            break;
        }

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
            /* init — can be an expression OR a var-declaration statement
             * (`for(int i=0; ...)`). Dispatch on kind. */
            if (node->init_expr) {
                if (node->init_expr->kind == HC_AST_VAR_DECL)
                    gen_stmt(gen, node->init_expr);
                else
                    gen_expr(gen, node->init_expr);
            }

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
                /* Top-level variable: store in data section as global. Always
                 * allocate (even without an init) and size by the declared
                 * type, so `struct S s;` (no init) still reserves memory and
                 * records a symbol — previously the no-init case was skipped
                 * entirely, so later `s.a` created an implicit stack local and
                 * pointed at garbage (crash). */
                {
                    int size = 8;
                    if (node->type) {
                        size_t tsz = hc_type_size(node->type);
                        if (tsz > 0) size = (int)tsz;
                    }
                    size_t global_offset = gen->data_size;
                    while (gen->data_size % 8 != 0) emit_data_byte(gen, 0);
                    /* reserve `size` bytes (round up to 8) for the global */
                    int qwords = (size + 7) / 8;
                    for (int q = 0; q < qwords; q++) emit_data_qword(gen, 0);

                    /* Record in symbol table (negative offset = global) */
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = -(int)global_offset;
                        gen->symbols.locals[gen->symbols.n_locals].type = node->type;
                        gen->symbols.n_locals++;
                    }

                    if (node->init) {
                        HCType *decl_t = node->type;
                        HCType *init_t = expr_static_type(gen, node->init);
                        bool decay = decl_t && decl_t->kind == HC_TYPE_PTR &&
                                     init_t && init_t->kind == HC_TYPE_ARRAY;
                        if (decay) emit_base_addr(gen, node->init);
                        else       gen_expr(gen, node->init);
                        /* mov [rip + disp32], rax (patched after code_size known) */
                        size_t patch_pos = gen->code_size;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x05);
                        emit_dword(gen, 0);
                        if (gen->n_global_patches < 128) {
                            gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos + 3;
                            gen->global_patches[gen->n_global_patches].global_offset = global_offset;
                            gen->n_global_patches++;
                        }
                    }
                }
            } else {
                /* Local variable inside function: store on stack */
                {
                    /* Always allocate the slot (even without an init) so an
                     * uninitialized struct/array still has memory to point at.
                     * Sized by the declared type: an int x[3] array needs
                     * 24 bytes, a struct needs sizeof(struct). Previously
                     * every var got one 8-byte slot, so struct S s; s.a=42
                     * pointed at garbage and crashed. */
                    int size = 8;
                    if (node->type) {
                        size_t tsz = hc_type_size(node->type);
                        if (tsz > 0) size = (int)tsz;
                    }
                    /* A struct/array local must reserve its FULL size, not
                     * 8 bytes, or a member beyond offset 8 (`s.c`) writes
                     * past the slot into the saved rbp at [rbp+0] → corrupts
                     * the frame → crash on leave/ret. Round the reservation
                     * up to 8 so the +8 base pad stays consistent. The
                     * offset must ALSO grow by the slot size — a 12-byte
                     * struct at offset 8 occupies [rbp-8..rbp+3], stomping
                     * the saved rbp at [rbp+0]. */
                    int slot = (size < 8) ? 8 : ((size + 7) & ~7);
                    int offset = gen->symbols.stack_size + slot;
                    gen->symbols.stack_size += slot;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = offset;
                        gen->symbols.locals[gen->symbols.n_locals].type = node->type;
                        gen->symbols.n_locals++;
                    }
                    if (node->init) {
                        /* Array-to-pointer decay: `int* p = a;` or
                         * `int* p = a[0];` must store the ADDRESS of the
                         * array (or row), not load its first element.
                         * gen_expr would load the value — detect the case
                         * (declared type is PTR, init static type is ARRAY)
                         * and emit the base address instead. */
                        HCType *decl_t = node->type;
                        HCType *init_t = expr_static_type(gen, node->init);
                        bool decay = decl_t && decl_t->kind == HC_TYPE_PTR &&
                                     init_t && init_t->kind == HC_TYPE_ARRAY;
                        if (decay) emit_base_addr(gen, node->init);
                        else       gen_expr(gen, node->init);
                        /* mov [rbp - offset], rax: 48 89 85 disp32 */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x89); /* mov */
                        emit_byte(gen, 0x85); /* [rbp+disp32] */
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                    }
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
                        /* Record this function's own name so recursive calls
                         * can emit a rel32 placeholder patched after copy. */
                        strncpy(gen->current_function, node->ident, HC_MAX_IDENT_LEN - 1);
                        gen->current_function[HC_MAX_IDENT_LEN - 1] = '\0';
                        gen->n_self_call_patches = 0;
           
                        emit_prologue(gen);
           
                        /* Add function parameters to symbol table before compiling body */
            for (int i = 0; i < node->n_params; i++) {
                /* A struct param reserves its FULL size (rounded to 8), so
                 * a member beyond offset 8 has a slot to land in; scalars
                 * keep the 8-byte slot. */
                size_t psz = 8;
                if (node->param_types && node->param_types[i]) {
                    size_t ts = hc_type_size(node->param_types[i]);
                    if (ts > 8) psz = (ts + 7) & ~(size_t)7;
                }
                int offset = gen->symbols.stack_size + (int)psz;
                gen->symbols.stack_size += (int)psz;
                if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                    strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                            node->param_names[i], HC_MAX_IDENT_LEN - 1);
                    gen->symbols.locals[gen->symbols.n_locals].stack_offset = offset;
                    gen->symbols.locals[gen->symbols.n_locals].type =
                        (node->param_types && node->param_types[i])
                            ? node->param_types[i] : NULL;
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
            /* SECOND PASS — struct-by-value params >8B arrive as POINTERS
             * (the call site emits the struct's address). After ALL register
             * stores are done (first pass), dereference-copy each into its
             * stack slot so the body sees a by-value copy. Doing this inside
             * the first loop would clobber the later param registers (rsi/rdx/…)
             * that haven't been stored yet. For each struct param:
             *   lea rdi,[rbp-off]; mov rsi,[rdi]; mov rcx,psz; rep movsb
             * → [&slot] <- [incoming addr] (the address was stored at &slot). */
            for (int i = 0; i < node->n_params; i++) {
                if (!(node->param_types && node->param_types[i])) continue;
                HCType *pt = node->param_types[i];
                if (pt && pt->kind == HC_TYPE_STRUCT && hc_type_size(pt) > 8) {
                    int off = gen->symbols.locals[i].stack_offset;
                    size_t psz = (hc_type_size(pt) + 7) & ~(size_t)7;
                    /* lea rdi, [rbp - off] */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0xBD);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* rsi = [rdi]  (incoming address) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x37);
                    /* mov rcx, psz */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xC7); emit_byte(gen, 0xC1);
                    emit_dword(gen, (uint32_t)psz);
                    /* rep movsb */
                    emit_rep_movsb(gen);
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
                    /* Patch self-recursive calls: each placeholder was a
                     * `call rel32` inside this body. After copy, the call's
                     * target is exec+0 (the function's own start). disp32 is
                     * relative to the instruction AFTER the call, so
                     *   disp32 = -((size_t)patch_pos + 5)
                     * (5 = len of E8 + disp32). Patch into the exec copy. */
                    for (int si = 0; si < gen->n_self_call_patches; si++) {
                        size_t pos = gen->self_call_patches[si];
                        uint8_t *p = (uint8_t*)exec + pos;
                        if (pos + 5 <= gen->code_size) {
                            int32_t disp = (int32_t)(-(int32_t)(pos + 5));
                            p[0] = 0xE8;                      /* call rel32 */
                            memcpy(&p[1], &disp, sizeof(disp));
                        }
                    }
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
                    gen->functions[gen->n_functions].ret_type = node->type;
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

        /* switch(expr){ case V: ... default: ... }
         *   eval expr -> rax
         *   push rax                    ; [rsp] = switch value
         *   ; dispatch chain — one per case:
         *   ;   eval case value -> rax
         *   ;   mov rdi, [rsp]          ; rdi = switch value
         *   ;   cmp rdi, rax            ; equality (order irrelevant)
         *   ;   je case_body_i          ; (patched to inline body)
         *   ; (no match)
         *   ;   jmp default_body  (or end if no default)
         *   case_body_0: ... (contiguous, fallthrough naturally)
         *   case_body_1: ...
         *   default_body: ...
         *   end: add rsp, 8             ; pop switch value
         * Break inside a case jumps to end (via break_patches at this depth).
         */
        case HC_AST_SWITCH: {
            int depth = gen->loop_depth;
            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;

            gen_expr(gen, node->cond);      /* rax = switch value */
            emit_push_rax(gen);             /* [rsp] = switch value */

            HCASTNode *body = node->body;   /* block of CASE nodes */
            int n_cases = body ? body->n_stmts : 0;
            size_t je_patches[64];
            int n_je = 0;

            /* Dispatch chain — one compare+je per case. Keep the switch
             * value on [rsp] (the push above). Per case:
             *   gen case value -> rax; mov rdi, rax; mov rax,[rsp]; cmp rdi,rax
             * The case value may be a full expression (clobbers rax), so we
             * move it to rdi BEFORE reloading the switch value into rax. */
            for (int i = 0; i < n_cases; i++) {
                HCASTNode *c = body->stmts[i];
                if (c->kind == HC_AST_CASE && c->cond) {
                    gen_expr(gen, c->cond);     /* rax = case value */
                    emit_mov_rdi_rax(gen);      /* rdi = case value */
                    emit_mov_rax_mem_rsp(gen);  /* rax = switch value */
                    emit_cmp_rax_rdi(gen);      /* cmp rdi, rax (equality) */
                    size_t je = emit_jcc_placeholder(gen, CC_E);
                    if (n_je < 64) je_patches[n_je++] = je;
                }
            }

            /* no match: default or end */
            int default_idx = -1;
            for (int i = 0; i < n_cases; i++)
                if (body->stmts[i]->kind == HC_AST_CASE && !body->stmts[i]->cond)
                    default_idx = i;
            size_t default_jmp = emit_jmp_placeholder(gen);  /* jmp default/end */

            /* Pass 2: emit each case body inline (contiguous). Record the
             * body start so the dispatch je can target it. */
            size_t body_start[64];
            for (int i = 0; i < n_cases; i++) {
                body_start[i] = gen->code_size;
                HCASTNode *c = body->stmts[i];
                if (c->body)
                    for (int s = 0; s < c->body->n_stmts; s++)
                        gen_stmt(gen, c->body->stmts[s]);
            }

            size_t switch_end = gen->code_size;
            emit_add_rsp_8(gen);            /* pop switch value */
            switch_end = gen->code_size;    /* break targets AFTER the pop so
                                             * every exit path balances rsp */

            /* patch je targets */
            int j = 0;
            for (int i = 0; i < n_cases; i++)
                if (body->stmts[i]->kind == HC_AST_CASE && body->stmts[i]->cond) {
                    if (j < n_je) patch_rel32(gen, je_patches[j], body_start[i]);
                    j++;
                }
            /* patch the no-match jmp -> default body or end */
            patch_rel32(gen, default_jmp,
                        (default_idx >= 0) ? body_start[default_idx] : switch_end);
            /* patch breaks to switch_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++)
                patch_rel32(gen, gen->break_patches[depth][i], switch_end);
            gen->loop_depth--;
            break;
        }

        case HC_AST_CASE:
            /* CASE nodes are only handled inside SWITCH; a stray one emits
             * nothing (its body is emitted by the switch pass). */
            break;

        case HC_AST_GOTO:
            /* goto label; — emit a jmp placeholder. If the label was ALREADY
             * placed (backward goto) patch it now; else record it for the
             * forward patch when the label is placed. */
            if (gen->n_labels < 128 && gen->n_label_patches < 512) {
                int idx = -1;
                for (int i = 0; i < gen->n_labels; i++)
                    if (strcmp(gen->labels[i].name, node->ident) == 0) { idx = i; break; }
                if (idx < 0) {
                    idx = gen->n_labels++;
                    strncpy(gen->labels[idx].name, node->ident, HC_MAX_IDENT_LEN - 1);
                    gen->labels[idx].offset = -1;   /* unknown yet */
                }
                size_t patch = emit_jmp_placeholder(gen);
                if (gen->labels[idx].offset >= 0)
                    patch_rel32(gen, patch, gen->labels[idx].offset);
                else {
                    gen->label_patches[gen->n_label_patches].patch_pos = patch;
                    gen->label_patches[gen->n_label_patches].label_idx = idx;
                    gen->n_label_patches++;
                }
            }
            break;

        case HC_AST_LABEL:
            /* label: — record current code position; patch any pending
             * forward gotos that target this label. */
            if (gen->n_labels < 128) {
                int idx = -1;
                for (int i = 0; i < gen->n_labels; i++)
                    if (strcmp(gen->labels[i].name, node->ident) == 0) { idx = i; break; }
                if (idx < 0) {
                    idx = gen->n_labels++;
                    strncpy(gen->labels[idx].name, node->ident, HC_MAX_IDENT_LEN - 1);
                }
                gen->labels[idx].offset = (int)gen->code_size;
                /* patch any pending forward gotos */
                for (int i = 0; i < gen->n_label_patches; i++)
                    if (gen->label_patches[i].label_idx == idx)
                        patch_rel32(gen, gen->label_patches[i].patch_pos,
                                    gen->labels[idx].offset);
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