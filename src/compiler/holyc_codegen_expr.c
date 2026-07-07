/*
 * holyc_codegen_expr.c  --  HolyC Code Generator: Expression Generation
 * Generates x86-64 machine code for HolyC AST expressions.
 */

#include "holyc_codegen_internal.h"

/* -- Global (data-section) variable access helpers ---------------- */
/* A module-level var_decl lives in the data section (see VAR_DECL in
 * holyc_codegen_stmt.c). Its symbol offset is negative; the magnitude is the
 * byte offset into the data section (global_offset). We access it RIP-relative
 * and leave a patch point so the loader can relocate disp32 to
 * exec + code_size + global_offset. The load/store instructions are
 *   48 8B 05 disp32   (mov rax, [rip+disp32])
 *   48 89 05 disp32   (mov [rip+disp32], rax)
 * (7 bytes each; the disp32 starts at code_size + 3). The loader computes
 *   disp32 = code_size + global_offset - (code_patch_pos + 4)
 * where code_patch_pos is the disp32 start, i.e. code_size + 3, so the
 * effective -4 cancels the 7-byte instruction length to give a -7 RIP
 * adjustment — matching holyd's patch loop. */

void emit_global_load_rax(HCGen *gen, size_t global_offset) {
    size_t patch_pos = gen->code_size + 3;   /* disp32 start */
    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x05);
    emit_dword(gen, 0);                       /* placeholder disp32 */
    if (gen->n_global_patches < 32) {
        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
        gen->global_patches[gen->n_global_patches].global_offset = global_offset;
        gen->n_global_patches++;
    }
}

void emit_global_store_rax(HCGen *gen, size_t global_offset) {
    size_t patch_pos = gen->code_size + 3;   /* disp32 start */
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x05);
    emit_dword(gen, 0);                       /* placeholder disp32 */
    if (gen->n_global_patches < 32) {
        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
        gen->global_patches[gen->n_global_patches].global_offset = global_offset;
        gen->n_global_patches++;
    }
}

/* ====================================================================
 * EXPRESSION GENERATION
 * ==================================================================== */

int gen_expr(HCGen *gen, const HCASTNode *node) {
    if (!node) return 0;

    switch (node->kind) {
        /* Literals → mov rax, imm64 */
        case HC_AST_INT_LIT:
            emit_mov_rax_imm64(gen, node->int_val);
            break;

        case HC_AST_FLOAT_LIT:
            /* For now, store as I64 bit pattern */
            {
                union { double d; int64_t i; } u;
                u.d = node->float_val;
                emit_mov_rax_imm64(gen, u.i);
            }
            break;

        case HC_AST_BOOL_LIT:
            emit_mov_rax_imm64(gen, node->int_val ? 1 : 0);
            break;

        case HC_AST_CHAR_LIT:
            /* Character literal 'c' -> its ASCII value as I64 */
            emit_mov_rax_imm64(gen, (int64_t)(uint8_t)node->str_val[0]);
            break;

        case HC_AST_STRING_LIT:
            /* Store string in data section and emit pointer */
            {
                size_t str_len = strlen(node->str_val);
                size_t str_offset = gen->data_size;
                /* Emit string bytes + null terminator */
                for (size_t i = 0; i < str_len; i++) {
                    emit_data_byte(gen, (uint8_t)node->str_val[i]);
                }
                emit_data_byte(gen, 0); /* null terminator */
                /* Align to 8 bytes */
                while (gen->data_size % 8 != 0) {
                    emit_data_byte(gen, 0);
                }
                /* mov rax, data_section_base + str_offset */
                emit_mov_rax_imm64(gen, (int64_t)(size_t)(gen->data + str_offset));
            }
            break;

        /* Identifiers  --  for now, look up in symbol table */
        case HC_AST_IDENT:
            if (gen->symbols.n_locals > 0) {
                /* Look up variable in symbol table */
                bool found = false;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->ident) == 0) {
                        int off = gen->symbols.locals[i].stack_offset;
                        if (off <= 0) {
                            /* Global variable in data section: offset is negative or zero */
                            /* mov rax, [rip + offset] */
                            int32_t data_offset = -off;
                            /* RIP after instruction = exec + gen->code_size + 7
                             * Data address = exec + gen->code_size + data_offset
                             * disp32 = data_offset - 7 (placeholder; will be patched at runtime) */
                            int32_t rip_disp = data_offset - 7;
                            size_t patch_pos = gen->code_size + 3; /* Position of disp32 in instruction */
                            emit_byte(gen, 0x48); /* REX.W */
                            emit_byte(gen, 0x8B); /* mov rax, r/m64 */
                            emit_byte(gen, 0x05); /* modrm: [rip + disp32] */
                            emit_dword(gen, (uint32_t)rip_disp);
                            
                            /* Store patch info for runtime fixup (same logic as VAR_DECL stores) */
                            if (gen->n_global_patches < 32) {
                                gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                                gen->global_patches[gen->n_global_patches].global_offset = data_offset;
                                gen->n_global_patches++;
                            }
                        } else {
                            /* Local variable on stack: mov rax, [rbp - off] with disp32: 48 8B 85 disp32 */
                            emit_byte(gen, 0x48); /* REX.W */
                            emit_byte(gen, 0x8B); /* mov rax, r/m64 */
                            emit_byte(gen, 0x85); /* modrm: disp32 with rbp */
                            emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* Implicit variable - not yet assigned. Return 0. */
                    emit_mov_rax_imm64(gen, 0);
                }
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;

        /* Negation */
        case HC_AST_NEG:
            gen_expr(gen, node->child);
            emit_neg_rax(gen);
            break;

        /* Logical NOT: test rax, rax; setz al; movzx rax, al */
        case HC_AST_NOT:
            gen_expr(gen, node->child);
            emit_test_rax_rax(gen);
            emit_setcc(gen, 0x94); /* sete */
            break;

        /* Bitwise NOT */
        case HC_AST_BITNOT:
            gen_expr(gen, node->child);
            emit_not_rax(gen);
            break;

        /* Pre-increment: ++expr */
        case HC_AST_PRE_INC: {
            if (node->child && node->child->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->child->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                    break;
                }
                if (off <= 0) {  /* global in data section */
                    size_t go = (size_t)(-off);
                    emit_global_load_rax(gen, go);   /* rax = *x */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC0); /* inc rax */
                    emit_global_store_rax(gen, go);  /* *x = rax */
                } else {        /* stack local */
                    /* mov rax, [rbp - off] */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* inc rax */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC0);
                    /* mov [rbp - off], rax */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                }
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;
        }

        /* Pre-decrement: --expr */
        case HC_AST_PRE_DEC: {
            if (node->child && node->child->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->child->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                    break;
                }
                if (off <= 0) {  /* global in data section */
                    size_t go = (size_t)(-off);
                    emit_global_load_rax(gen, go);   /* rax = *x */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC8); /* dec rax */
                    emit_global_store_rax(gen, go);  /* *x = rax */
                } else {        /* stack local */
                    /* mov rax, [rbp - off] */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* dec rax */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC8);
                    /* mov [rbp - off], rax */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                }
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;
        }

        /* Post-increment: expr++ */
        case HC_AST_POST_INC: {
            if (node->child && node->child->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->child->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                    break;
                }
                if (off <= 0) {  /* global in data section */
                    size_t go = (size_t)(-off);
                    emit_global_load_rax(gen, go);     /* rax = old */
                    emit_mov_rdi_rax(gen);             /* rdi = old */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC0); /* inc rax */
                    emit_global_store_rax(gen, go);    /* *x = rax (new) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF8); /* rax = rdi (old) */
                } else {        /* stack local */
                    /* mov rax, [rbp - off] (return old value) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* mov rdi, rax (save old value) */
                    emit_mov_rdi_rax(gen);
                    /* inc rax */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC0);
                    /* mov [rbp - off], rax (store new value) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* mov rax, rdi (return old value) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF8);
                }
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;
        }

        /* Post-decrement: expr-- */
        case HC_AST_POST_DEC: {
            if (node->child && node->child->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->child->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                    break;
                }
                if (off <= 0) {  /* global in data section */
                    size_t go = (size_t)(-off);
                    emit_global_load_rax(gen, go);     /* rax = old */
                    emit_mov_rdi_rax(gen);             /* rdi = old */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC8); /* dec rax */
                    emit_global_store_rax(gen, go);    /* *x = rax (new) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF8); /* rax = rdi (old) */
                } else {        /* stack local */
                    /* mov rax, [rbp - off] (return old value) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* mov rdi, rax (save old value) */
                    emit_mov_rdi_rax(gen);
                    /* dec rax */
                    emit_byte(gen, 0x48); emit_byte(gen, 0xFF); emit_byte(gen, 0xC8);
                    /* mov [rbp - off], rax (store new value) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                    /* mov rax, rdi (return old value) */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF8);
                }
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;
        }

        /* Dereference: *expr */
        case HC_AST_DEREF: {
            gen_expr(gen, node->child);
            /* rax now contains pointer value. Load from it: mov rax, [rax] */
            emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x00); /* mov rax, [rax] */
            break;
        }

        /* Address-of: &expr */
        case HC_AST_ADDR: {
            if (node->child && node->child->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->child->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                    break;
                }
                if (off <= 0) {  /* global: load its runtime address via RIP-relative lea */
                    size_t go = (size_t)(-off);
                    size_t patch_pos = gen->code_size + 3;   /* disp32 start */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x05);
                    emit_dword(gen, 0);                       /* placeholder disp32 */
                    if (gen->n_global_patches < 32) {
                        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                        gen->global_patches[gen->n_global_patches].global_offset = go;
                        gen->n_global_patches++;
                    }
                } else {        /* stack local */
                    /* lea rax, [rbp - off] */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                }
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;
        }

        /* Cast: (type)expr - for now just evaluate expr (no-op at codegen level for I64) */
        case HC_AST_CAST:
            gen_expr(gen, node->child);
            break;

        /* Array index: expr[index] */
        case HC_AST_INDEX: {
            /* Evaluate base expression (array pointer) */
            gen_expr(gen, node->left);
            /* rax = base pointer */
            emit_mov_rdi_rax(gen);  /* save base in rdi */
            /* Evaluate index expression */
            gen_expr(gen, node->right);
            /* rax = index, rdi = base */
            /* Scale index by 8 (I64 size): shl rax, 3 */
            emit_byte(gen, 0x48); emit_byte(gen, 0xC1); emit_byte(gen, 0xE0); emit_byte(gen, 0x03);
            /* Add to base: add rdi, rax */
            emit_byte(gen, 0x48); emit_byte(gen, 0x01); emit_byte(gen, 0xC7);
            /* Load from address: mov rax, [rdi] */
            emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x07);
            break;
        }

        /* Struct declaration - no-op at expression level */
        case HC_AST_STRUCT_DECL:
            emit_mov_rax_imm64(gen, 0);
            break;

        /* Binary operations: eval left → rax, save to rdi, eval right → rax, swap, op */
        case HC_AST_ADD:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_add_rax_rdi(gen);
            break;

        case HC_AST_SUB:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_sub_rax_rdi(gen);
            break;

        case HC_AST_MUL:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_mul_rax_rdi(gen);
            break;

        case HC_AST_DIV:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_div_rax_rdi(gen);
            break;

        case HC_AST_MOD:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_div_rax_rdi(gen);
            /* Remainder is in rdx, move to rax */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89);
            emit_byte(gen, 0xD0); /* mov rax, rdx */
            break;

        /* Bitwise AND, OR, XOR */
        case HC_AST_BITAND:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* and rax, rdi: 48 21 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x21); emit_byte(gen, 0xF8);
            break;

        case HC_AST_BITOR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* or rax, rdi: 48 09 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x09); emit_byte(gen, 0xF8);
            break;

        case HC_AST_XOR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* xor rax, rdi: 48 31 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xF8);
            break;

        /* Shift left/right */
        case HC_AST_SHL:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* mov rcx, rdi (shift count must be in cl): 48 89 F9 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9);
            emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE0); /* shl rax, cl */
            break;

        case HC_AST_SHR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9);
            emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE8); /* shr rax, cl */
            break;

        /* Comparison ops: cmp rax, rdi then setcc */
        case HC_AST_EQ:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x94); /* sete */
            break;

        case HC_AST_NE:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x95); /* setne */
            break;

        case HC_AST_LT:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9C); /* setl */
            break;

        case HC_AST_LE:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9E); /* setle */
            break;

        case HC_AST_GT:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9F); /* setg */
            break;

        case HC_AST_GE:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9D); /* setge */
            break;

        /* Logical AND with short-circuit and proper backpatching:
         *   eval left → rax
         *   test rax, rax
         *   jz false_label          (5 bytes, placeholder)
         *   eval right → rax
         *   test rax, rax
         *   setne al; movzx rax, al  (convert to bool)
         *   jmp end_label            (5 bytes, placeholder)
         * false_label:
         *   xor rax, rax             (rax = 0)
         * end_label:
         */
        case HC_AST_AND: {
            gen_expr(gen, node->left);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz false */
            gen_expr(gen, node->right);
            emit_test_rax_rax(gen);
            emit_setcc(gen, 0x95); /* setne al; movzx rax, al → bool result */
            size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
            size_t false_label = gen->code_size;
            emit_xor_rax_rax(gen);
            size_t end_label = gen->code_size;
            patch_rel32(gen, jz_patch, false_label);
            patch_rel32(gen, jmp_patch, end_label);
            break;
        }

        /* Logical OR with short-circuit and proper backpatching:
         *   eval left → rax
         *   test rax, rax
         *   jnz true_label           (5 bytes, placeholder)
         *   eval right → rax
         *   test rax, rax
         *   setne al; movzx rax, al   (convert to bool)
         *   jmp end_label             (5 bytes, placeholder)
         * true_label:
         *   mov rax, 1                (rax = 1)
         * end_label:
         */
        case HC_AST_OR: {
            gen_expr(gen, node->left);
            emit_test_rax_rax(gen);
            size_t jnz_patch = emit_jcc_placeholder(gen, CC_NE); /* jnz true */
            gen_expr(gen, node->right);
            emit_test_rax_rax(gen);
            emit_setcc(gen, 0x95); /* setne al; movzx → bool */
            size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
            size_t true_label = gen->code_size;
            emit_mov_rax_1(gen);
            size_t end_label = gen->code_size;
            patch_rel32(gen, jnz_patch, true_label);
            patch_rel32(gen, jmp_patch, end_label);
            break;
        }

        case HC_AST_ASSIGN:
            /* Right-hand side → rax, then store to left-hand side */
            gen_expr(gen, node->right);
            if (node->left && node->left->kind == HC_AST_IDENT) {
                /* Look up variable in symbol table */
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* Implicit variable declaration on first assignment */
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                /* mov [rbp - off], rax: 48 89 85 disp32 */
                emit_byte(gen, 0x48); /* REX.W */
                emit_byte(gen, 0x89); /* mov */
                emit_byte(gen, 0x85); /* [rbp+disp32] */
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        /* Compound assignments: x += y means x = x + y */
        case HC_AST_ADD_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                /* Load current value of left var (with implicit declaration) */
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        /* mov rax, [rbp - off] with disp32: 48 8B 85 disp32 */
                        emit_byte(gen, 0x48);
                        emit_byte(gen, 0x8B);
                        emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* Implicit declaration - initialize to 0 */
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                    emit_mov_rax_imm64(gen, 0);
                }
                /* Add right-hand side */
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_add_rax_rdi(gen);
                /* Store back */
                /* mov [rbp - off], rax: 48 89 85 disp32 */
                emit_byte(gen, 0x48); /* REX.W */
                emit_byte(gen, 0x89); /* mov */
                emit_byte(gen, 0x85); /* [rbp+disp32] */
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        case HC_AST_SUB_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_sub_rax_rdi(gen);
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        case HC_AST_MUL_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_mul_rax_rdi(gen);
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        case HC_AST_DIV_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_div_rax_rdi(gen);
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        /* Function call */
        case HC_AST_FUNC_CALL:
            /* System V AMD64 ABI: args in rdi, rsi, rdx, rcx, r8, r9 */
            {
                int n_args = node->n_args;
                /* System V AMD64 ABI: args 0-5 in registers, 6+ on stack (right-to-left) */
                if (n_args > 6) {
                    /* Evaluate and push stack args (6th, 7th, ...) right-to-left */
                    for (int i = n_args - 1; i >= 6; i--) {
                        gen_expr(gen, node->args[i]);
                        /* push rax: 50 */
                        emit_byte(gen, 0x50);
                    }
                }
                /* Evaluate register args right-to-left to avoid clobbering */
                for (int i = (n_args < 6 ? n_args - 1 : 5); i >= 0; i--) {
                    gen_expr(gen, node->args[i]);
                    switch (i) {
                        case 0: emit_mov_rdi_rax(gen); break;  /* arg0 → rdi */
                        case 1: /* mov rsi, rax: 48 89 C6 */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC6); break;
                        case 2: /* mov rdx, rax: 48 89 C2 */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC2); break;
                        case 3: /* mov rcx, rax: 48 89 C1 */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC1); break;
                        case 4: /* mov r8, rax: 49 89 C0 */
                                emit_byte(gen, 0x49); emit_byte(gen, 0x89); emit_byte(gen, 0xC0); break;
                        case 5: /* mov r9, rax: 49 89 C1 */
                                emit_byte(gen, 0x49); emit_byte(gen, 0x89); emit_byte(gen, 0xC1); break;
                    }
                }
                /* Get function address from callee (should be ident or function pointer) */
                if (node->callee && node->callee->kind == HC_AST_IDENT) {
                    /* Look up function in function table */
                    void *func_addr = NULL;
                    for (int i = 0; i < gen->n_functions; i++) {
                        if (strcmp(gen->functions[i].name, node->callee->ident) == 0) {
                            func_addr = gen->functions[i].func_ptr;
                            break;
                        }
                    }
                    if (func_addr) {
                        /* mov rax, func_addr */
                        emit_mov_rax_imm64(gen, (int64_t)func_addr);
                        /* call rax: FF D0 */
                        emit_byte(gen, 0xFF); emit_byte(gen, 0xD0);
                    } else {
                        /* Check extern C functions */
                        for (int i = 0; i < gen->n_extern_funcs; i++) {
                            if (strcmp(gen->extern_funcs[i].c_name, node->callee->ident) == 0) {
                                func_addr = gen->extern_funcs[i].func_addr;
                                break;
                            }
                        }
                        if (func_addr) {
                            emit_mov_rax_imm64(gen, (int64_t)func_addr);
                            emit_byte(gen, 0xFF); emit_byte(gen, 0xD0);
                        } else {
                            /* Function not found - emit call to 0 (will crash at runtime) */
                            emit_mov_rax_imm64(gen, 0);
                            emit_byte(gen, 0xFF); emit_byte(gen, 0xD0);
                        }
                    }
                } else {
                    emit_mov_rax_imm64(gen, 0);
                }
            }
            break;

        /* Ternary: cond ? then : else */
        case HC_AST_TERNARY: {
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz else */
            gen_expr(gen, node->then_branch);
            size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
            size_t else_label = gen->code_size;
            gen_expr(gen, node->else_branch);
            size_t end_label = gen->code_size;
            patch_rel32(gen, jz_patch, else_label);
            patch_rel32(gen, jmp_patch, end_label);
            break;
        }

        /* Member access: expr.member */
        case HC_AST_MEMBER: {
            /* Evaluate base expression (struct pointer or value) */
            gen_expr(gen, node->left);
            /* rax now contains base address */
            /* Find member offset */
            if (node->left->type && node->left->type->kind == HC_TYPE_STRUCT) {
                HCType *st = node->left->type;
                bool found = false;
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) {
                        int off = st->members[i].offset;
                        /* mov rax, [rax + off]: 48 8B 80 disp32 */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x8B); /* mov */
                        emit_byte(gen, 0x80); /* modrm: rax, [rax+disp32] */
                        emit_dword(gen, (uint32_t)off);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                }
            }
            break;
        }

        /* Arrow access: expr->member (ptr to struct) */
        case HC_AST_ARROW: {
            /* Evaluate base expression (pointer to struct) */
            gen_expr(gen, node->left);
            /* rax now contains base address (already a pointer) */
            if (node->left->type && node->left->type->kind == HC_TYPE_PTR &&
                node->left->type->base && node->left->type->base->kind == HC_TYPE_STRUCT) {
                HCType *st = node->left->type->base;
                bool found = false;
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) {
                        int off = st->members[i].offset;
                        /* mov rax, [rax + off]: 48 8B 80 disp32 */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x8B); /* mov */
                        emit_byte(gen, 0x80); /* modrm: rax, [rax+disp32] */
                        emit_dword(gen, (uint32_t)off);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                }
            }
            break;
        }

        default:
            /* Unknown expression  --  emit 0 */
            emit_mov_rax_imm64(gen, 0);
            break;
    }

    return 0;
}