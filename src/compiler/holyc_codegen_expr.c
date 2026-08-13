/*
 * holyc_codegen_expr.c  --  HolyC Code Generator: Expression Generation
 * Generates x86-64 machine code for HolyC AST expressions.
 */

#include "holyc_codegen_internal.h"

/* Trap for an unresolved function call. The JIT emits a call to this (instead
 * of a raw `call 0`) when it cannot resolve an identifier to an internal
 * function or a registered extern. It never returns; it aborts with a clear
 * message so a missing HolyC builtin fails loudly instead of SIGSEGV-ing on a
 * null function pointer. Declared here so both call sites can reference it. */
__attribute__((noreturn))
void hc_trap_unresolved_call(const char *name);

__attribute__((noreturn))
void hc_trap_unresolved_call(const char *name) {
    fprintf(stderr, "[holyc] unresolved function call%s%s -- no such function or "
                    "extern registered (crash avoided)\n",
            name ? ": " : "", name ? name : "");
    fflush(stderr);
    abort();
}

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
    if (gen->hedge_loads)
        emit_prefetch_rip(gen, global_offset);
    size_t patch_pos = gen->code_size + 3;   /* disp32 start */
    emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x05);
    emit_dword(gen, 0);                       /* placeholder disp32 */
    if (gen->n_global_patches < 128) {
        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
        gen->global_patches[gen->n_global_patches].global_offset = global_offset;
        gen->n_global_patches++;
    }
}

void emit_global_store_rax(HCGen *gen, size_t global_offset) {
    size_t patch_pos = gen->code_size + 3;   /* disp32 start */
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x05);
    emit_dword(gen, 0);                       /* placeholder disp32 */
    if (gen->n_global_patches < 128) {
        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
        gen->global_patches[gen->n_global_patches].global_offset = global_offset;
        gen->n_global_patches++;
    }
}

/* ---- shared variable load/store for ASSIGN + compound-assigns ----
 * A variable's symbol-table `stack_offset` is either POSITIVE (a stack
 * local inside a function body) or NON-POSITIVE (a module-level global
 * in the data section; `off <= 0` means `global_offset = -off`). The
 * IDENT-load / Inc/Dec / &x paths already dispatch on `off <= 0` to the
 * RIP-relative emitters; ASSIGN and every compound-assign used to emit
 * `mov [rbp-off]` unconditionally — writing to a garbage high stack slot
 * for globals → heap corruption + SIGSEGV in hc_eval's free(). */

/* resolve a named variable: returns 1 if found, sets off (stack) or
 * *is_global (with global_offset = -off). Creates an implicit local on
 * first assignment if absent. Optionally returns the declared HCType
 * (populated from the VAR_DECL; NULL for implicit locals). */
int resolve_var(HCGen *gen, const char *name, int *off, int *is_global)
{
    for (int i = 0; i < gen->symbols.n_locals; i++) {
        if (strcmp(gen->symbols.locals[i].name, name) == 0) {
            *off = gen->symbols.locals[i].stack_offset;
            *is_global = (*off <= 0);
            return 1;
        }
    }
    /* implicit declaration on first assignment → a new stack local */
    *off = gen->symbols.stack_size + 8;
    gen->symbols.stack_size += 8;
    *is_global = 0;
    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                name, HC_MAX_IDENT_LEN - 1);
        gen->symbols.locals[gen->symbols.n_locals].stack_offset = *off;
        gen->symbols.locals[gen->symbols.n_locals].type = NULL;
        gen->symbols.n_locals++;
    }
    return 1;
}

/* resolve a variable's declared HCType (or NULL if implicit/untyped).
 * Used to decide array/struct base decay in lvalue/read codegen. */
static HCType *resolve_var_type(HCGen *gen, const char *name)
{
    for (int i = 0; i < gen->symbols.n_locals; i++) {
        if (strcmp(gen->symbols.locals[i].name, name) == 0)
            return gen->symbols.locals[i].type;
    }
    return NULL;
}

/* static type of an expression node: IDENT → symbol-table type; MEMBER →
 * the member's declared type; ARROW → pointee member type; INDEX → element
 * type; DEREF → pointee type. Used so nested access like q.p.x can resolve
 * x's offset inside struct P (the parser doesn't attach types to nodes). */
HCType *expr_static_type(HCGen *gen, const HCASTNode *node)
{
    if (!node) return NULL;
    switch (node->kind) {
        case HC_AST_IDENT:
            return resolve_var_type(gen, node->ident);
        case HC_AST_MEMBER: {
            HCType *bt = expr_static_type(gen, node->left);
            if (bt && (bt->kind == HC_TYPE_STRUCT || bt->kind == HC_TYPE_UNION)) {
                for (int i = 0; i < bt->n_members; i++)
                    if (strcmp(bt->members[i].name, node->ident) == 0)
                        return bt->members[i].type;
            }
            return NULL;
        }
        case HC_AST_ARROW: {
            HCType *bt = expr_static_type(gen, node->left);
            if (bt && bt->kind == HC_TYPE_PTR && bt->base &&
                (bt->base->kind == HC_TYPE_STRUCT || bt->base->kind == HC_TYPE_UNION)) {
                for (int i = 0; i < bt->base->n_members; i++)
                    if (strcmp(bt->base->members[i].name, node->ident) == 0)
                        return bt->base->members[i].type;
            }
            return NULL;
        }
        case HC_AST_INDEX: {
            HCType *bt = expr_static_type(gen, node->left);
            if (bt && (bt->kind == HC_TYPE_ARRAY || bt->kind == HC_TYPE_PTR) && bt->base)
                return bt->base;
            return NULL;
        }
        case HC_AST_DEREF: {
            HCType *bt = expr_static_type(gen, node->child);
            if (bt && bt->kind == HC_TYPE_PTR && bt->base) return bt->base;
            return NULL;
        }
        case HC_AST_FUNC_CALL: {
            /* Static type of a call = the callee's declared return type.
             * Look up the named function in the function table. This is what
             * lets `f().a` (member access on a struct-return call) resolve
             * the member's offset: expr_static_type(f().a) → int, but more
             * importantly the MEMBER codegen needs f's struct ret_type to
             * walk the base. */
            if (node->callee && node->callee->kind == HC_AST_IDENT) {
                for (int i = 0; i < gen->n_functions; i++)
                    if (strcmp(gen->functions[i].name, node->callee->ident) == 0)
                        return gen->functions[i].ret_type;
            }
            return NULL;
        }
        default:
            return NULL;
    }
}

/* load `rax = var` — RIP-relative for globals, [rbp-off] for locals */
static void emit_var_load(HCGen *gen, int off, int is_global)
{
    if (is_global) {
        emit_global_load_rax(gen, (size_t)(-off));
    } else {
        if (gen->hedge_loads)
            emit_prefetch_rbp(gen, off);
        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
    }
}

/* store `rax -> var` — RIP-relative for globals, [rbp-off] for locals */
static void emit_var_store(HCGen *gen, int off, int is_global)
{
    if (is_global) {
        emit_global_store_rax(gen, (size_t)(-off));
    } else {
        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
    }
}

/* ---- sized load/store for member/element access -------------------
 * A struct member or array element of a narrower type must be loaded/stored
 * with its OWN width, not a full 8-byte qword. `struct{int x;int y;}; s.x`
 * reads `mov eax,[&s+0]` (4 bytes, zero-extended) so it doesn't pull in the
 * adjacent `y`; writing must match. The INDEX path already sizes by element
 * width; MEMBER/ARROW did not (the 8-byte qword load of s.x returned
 * 22<<32|20, i.e. both x and y packed). */

/* value is in rdi, address in rax. size is the element/member byte width. */
static void emit_sized_store_rax_rdi(HCGen *gen, int size)
{
    if (size <= 1) {
        /* mov byte [rax], dil : 40 88 38 */
        emit_byte(gen, 0x40); emit_byte(gen, 0x88); emit_byte(gen, 0x38);
    } else if (size == 2) {
        /* mov word [rax], di : 66 89 38 */
        emit_byte(gen, 0x66); emit_byte(gen, 0x89); emit_byte(gen, 0x38);
    } else if (size == 4) {
        /* mov dword [rax], edi : 89 38 */
        emit_byte(gen, 0x89); emit_byte(gen, 0x38);
    } else {
        /* mov qword [rax], rdi : 48 89 38 */
        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x38);
    }
}

/* load [rax+off] into rax, zero-extending to the element's width. */
static void emit_sized_load_rax_off(HCGen *gen, int off, int size)
{
    if (gen->hedge_loads)
        emit_prefetch_rax_off(gen, off);
    if (size <= 1) {
        /* movzx rax, byte [rax+disp32] : 48 0F B6 80 */
        emit_byte(gen, 0x48); emit_byte(gen, 0x0F); emit_byte(gen, 0xB6); emit_byte(gen, 0x80);
        emit_dword(gen, (uint32_t)off);
    } else if (size == 2) {
        /* movzx rax, word [rax+disp32] : 48 0F B7 80 */
        emit_byte(gen, 0x48); emit_byte(gen, 0x0F); emit_byte(gen, 0xB7); emit_byte(gen, 0x80);
        emit_dword(gen, (uint32_t)off);
    } else if (size == 4) {
        /* mov eax, dword [rax+disp32] : 8B 80 */
        emit_byte(gen, 0x8B); emit_byte(gen, 0x80);
        emit_dword(gen, (uint32_t)off);
    } else {
        /* mov rax, qword [rax+disp32] : 48 8B 80 */
        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x80);
        emit_dword(gen, (uint32_t)off);
    }
}

/* emit_lvalue_addr: compute the ADDRESS of an lvalue (in rax). Supports
 * IDENT (lea stack-local / RIP-relative global), INDEX (base + idx*scale),
 * MEMBER (.field, struct value), ARROW (->field, struct pointer), and
 * DEREF (*ptr). Used by ASSIGN/compound-assign to store through a
 * non-trivial target — previously only IDENT targets were assignable, so
 * `x[0]=42`, `s.a=42`, `*p=42` were silently dropped (returned 0). */
static int emit_lvalue_addr(HCGen *gen, const HCASTNode *node);
void emit_base_addr(HCGen *gen, const HCASTNode *node);

/* element size for pointer arithmetic: given a pointer/array-typed operand,
 * return sizeof(elem) (1 if not a pointer/array or unknown). */
static int pointer_elem_scale(HCGen *gen, const HCASTNode *node)
{
    if (!node) return 1;
    HCType *t = node->type;
    if (!t && node->kind == HC_AST_IDENT)
        t = resolve_var_type(gen, node->ident);
    if (t) {
        if (t->kind == HC_TYPE_PTR && t->base) {
            size_t sz = hc_type_size(t->base);
            return (sz > 1) ? (int)sz : 1;
        }
        if (t->kind == HC_TYPE_ARRAY && t->base) {
            size_t sz = hc_type_size(t->base);
            return (sz > 1) ? (int)sz : 1;
        }
    }
    return 1;
}

/* scale rdi by `mul` (multiply in place): used to scale a pointer
 * index before adding/subtracting. */
static void scale_rdi(HCGen *gen, int mul)
{
    /* imul rdi, rdi, imm8: 48 6B FF imm8  (works for 2/4/8) */
    emit_byte(gen, 0x48); emit_byte(gen, 0x6B); emit_byte(gen, 0xFF);
    emit_byte(gen, (uint8_t)mul);
}

static int emit_lvalue_addr(HCGen *gen, const HCASTNode *node)
{
    switch (node->kind) {
        case HC_AST_IDENT: {
            int off = 0, is_global = 0;
            resolve_var(gen, node->ident, &off, &is_global);
            if (is_global) {
                size_t go = (size_t)(-off);
                size_t patch_pos = gen->code_size + 3;
                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x05);
                emit_dword(gen, 0);
                if (gen->n_global_patches < 128) {
                    gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                    gen->global_patches[gen->n_global_patches].global_offset = go;
                    gen->n_global_patches++;
                }
            } else {
                /* lea rax, [rbp - off] */
                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            return 1;
        }
        case HC_AST_INDEX: {
            /* address = base + idx * scale; base is the array's ADDRESS
             * (a struct/array ident must decay to &x, not its value). */
            int scale = 1;
            HCType *bt = node->left->type;
            if (!bt)
                bt = expr_static_type(gen, node->left);
            if (bt) {
                if (bt->kind == HC_TYPE_PTR && bt->base)
                    scale = (hc_type_size(bt->base) > 1) ? (int)hc_type_size(bt->base) : 1;
                else if (bt->kind == HC_TYPE_ARRAY && bt->base)
                    scale = (hc_type_size(bt->base) > 1) ? (int)hc_type_size(bt->base) : 1;
            }
            emit_base_addr(gen, node->left); /* rax = &base (or ptr value) */
            emit_mov_rdi_rax(gen);           /* rdi = base address */
            gen_expr(gen, node->right);      /* rax = idx */
            if (scale == 8) emit_byte(gen, 0x48), emit_byte(gen, 0xC1), emit_byte(gen, 0xE0), emit_byte(gen, 0x03);
            else if (scale == 4) emit_byte(gen, 0x48), emit_byte(gen, 0xC1), emit_byte(gen, 0xE0), emit_byte(gen, 0x02);
            else if (scale == 2) emit_byte(gen, 0x48), emit_byte(gen, 0xC1), emit_byte(gen, 0xE0), emit_byte(gen, 0x01);
            /* rax = base + idx*scale: add rax, rdi */
            emit_byte(gen, 0x48); emit_byte(gen, 0x01); emit_byte(gen, 0xF8);
            return 1;
        }
        case HC_AST_DEREF: {
            gen_expr(gen, node->child);      /* rax = pointer (the address) */
            return 1;
        }
        case HC_AST_MEMBER:
        case HC_AST_ARROW: {
            /* evaluate base: for a struct-VALUE ident (s.a) we need &s,
             * for a struct-POINTER (p->a) we need the pointer's value. */
            emit_base_addr(gen, node->left);
            int off = 0;
            HCType *btype = node->left->type;
            if (!btype && node->left->kind == HC_AST_IDENT)
                btype = resolve_var_type(gen, node->left->ident);
            const HCType *st = (node->kind == HC_AST_ARROW && btype &&
                                btype->kind == HC_TYPE_PTR && btype->base)
                                    ? btype->base : btype;
            if (st && (st->kind == HC_TYPE_STRUCT || st->kind == HC_TYPE_UNION)) {
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) { off = (int)st->members[i].offset; break; }
                }
            }
            if (off != 0) {
                /* add rax, imm32 */
                emit_byte(gen, 0x48); emit_byte(gen, 0x05);
                emit_dword(gen, (uint32_t)off);
            }
            return 1;
        }
        default:
            return 0;
    }
}

/* emit_base_addr: emit the ADDRESS of a base expression into rax.
 * For a struct/array IDENT this is its location (&s, &x); for a pointer
 * IDENT it's the pointer's value (which IS the address). Generic
 * expressions are evaluated normally. Used by INDEX/MEMBER/ARROW so the
 * base decays to an address instead of loading the struct's value. */
void emit_base_addr(HCGen *gen, const HCASTNode *node)
{
    if (!node) { emit_mov_rax_imm64(gen, 0); return; }
    if (node->kind == HC_AST_IDENT) {
        /* resolve the declared type from the symbol table — the parser
         * doesn't attach a type to IDENT nodes, so fall back to the
         * symbol's recorded VAR_DECL type. */
        HCType *t = node->type;
        if (!t) t = resolve_var_type(gen, node->ident);
        bool is_container = (t && (t->kind == HC_TYPE_STRUCT ||
                                   t->kind == HC_TYPE_ARRAY ||
                                   t->kind == HC_TYPE_UNION));
        if (is_container) {
            /* &ident via lea */
            int off = 0, is_global = 0;
            resolve_var(gen, node->ident, &off, &is_global);
            if (is_global) {
                size_t go = (size_t)(-off);
                size_t patch_pos = gen->code_size + 3;
                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x05);
                emit_dword(gen, 0);
                if (gen->n_global_patches < 128) {
                    gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                    gen->global_patches[gen->n_global_patches].global_offset = go;
                    gen->n_global_patches++;
                }
            } else {
                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            return;
        }
        gen_expr(gen, node);
        return;
    }
    /* Nested composite lvalue: q.p (member), a[i] (index), *p (deref).
     * What we need depends on the static type:
     *   - pointer type   → LOAD the value (the pointer IS the address we
     *     want). This is the `q.p->a` case where q.p is a `struct P*`
     *     member — its ADDRESS (&q.p) would be wrong, we need q.p's VALUE.
     *   - struct/array/union → the ADDRESS (recurse emit_lvalue_addr).
     * Previously every composite was treated as needing its address, so
     * `q.p->a` (nested pointer member) computed &q.p and dereferenced a
     * garbage address. */
    HCType *nt = expr_static_type(gen, node);
    if (nt && nt->kind == HC_TYPE_PTR) {
        gen_expr(gen, node);   /* load the pointer value */
        return;
    }
    if (emit_lvalue_addr(gen, node))
        return;
    gen_expr(gen, node);
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
                        /* Array-to-pointer decay: an ARRAY variable used in
                         * most expressions yields its ADDRESS (&x[0]), not
                         * its value. This is what makes `x+1` (pointer arith)
                         * and `x` (passed as pointer) work. The `&x` in
                         * emit_lvalue_addr uses lea; here we load address
                         * the same way. */
                        HCType *sym_type = gen->symbols.locals[i].type;
                        bool is_array = (sym_type && sym_type->kind == HC_TYPE_ARRAY);
                        if (is_array) {
                            if (off <= 0) {
                                /* global: lea rax, [rip + disp32] */
                                size_t go = (size_t)(-off);
                                size_t patch_pos = gen->code_size + 3;
                                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x05);
                                emit_dword(gen, 0);
                                if (gen->n_global_patches < 128) {
                                    gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                                    gen->global_patches[gen->n_global_patches].global_offset = go;
                                    gen->n_global_patches++;
                                }
                            } else {
                                /* lea rax, [rbp - off] */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x85);
                                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                            }
                            found = true;
                            break;
                        }
                        if (off <= 0) {
                            /* Global variable in data section: offset is negative or zero */
                            /* mov rax, [rip + offset] */
                            int32_t data_offset = -off;
                            /* RIP after instruction = exec + gen->code_size + 7
                             * Data address = exec + gen->code_size + data_offset
                             * disp32 = data_offset - 7 (placeholder; will be patched at runtime) */
                            int32_t rip_disp = data_offset - 7;
                            if (gen->hedge_loads)
                                emit_prefetch_rip(gen, (size_t)data_offset);
                            size_t patch_pos = gen->code_size + 3; /* Position of disp32 in instruction */
                            emit_byte(gen, 0x48); /* REX.W */
                            emit_byte(gen, 0x8B); /* mov rax, r/m64 */
                            emit_byte(gen, 0x05); /* modrm: [rip + disp32] */
                            emit_dword(gen, (uint32_t)rip_disp);
                            
                            /* Store patch info for runtime fixup (same logic as VAR_DECL stores) */
                            if (gen->n_global_patches < 128) {
                                gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                                gen->global_patches[gen->n_global_patches].global_offset = data_offset;
                                gen->n_global_patches++;
                            }
                        } else {
                            /* Local variable on stack: mov rax, [rbp - off] with disp32: 48 8B 85 disp32 */
                            if (gen->hedge_loads)
                                emit_prefetch_rbp(gen, off);
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
                    /* Not a variable but may be a FUNCTION name used as a value
                     * (e.g. `int (*op)(..)=add;` or `int (*fp)(..)=func;`
                     * assigns the function's address, and `op(20,22)` calls
                     * through it). Emit `mov rax, func_addr`. */
                    void *func_addr = NULL;
                    for (int i = 0; i < gen->n_functions; i++) {
                        if (strcmp(gen->functions[i].name, node->ident) == 0) {
                            func_addr = gen->functions[i].func_ptr;
                            break;
                        }
                    }
                    if (func_addr) {
                        emit_mov_rax_imm64(gen, (int64_t)(intptr_t)func_addr);
                    } else if (gen->n_self_call_patches < 32 &&
                               node->ident[0] &&
                               strcmp(node->ident, gen->current_function ? gen->current_function : "") == 0) {
                        /* Self-recursive reference as a value: resolve to the
                         * currently-compiling function's eventual exec address
                         * (patched after the body is copied). */
                        emit_mov_rax_imm64(gen, 0);  /* placeholder, patched by self-call fixup */
                    } else {
                        emit_mov_rax_imm64(gen, 0);
                    }
                }
            } else {
                /* No locals in scope: IDENT may be a function name used as a
                 * value (e.g. `add;` or `op = add;`). Check the function table
                 * before defaulting to 0. */
                void *func_addr = NULL;
                for (int i = 0; i < gen->n_functions; i++) {
                    if (strcmp(gen->functions[i].name, node->ident) == 0) {
                        func_addr = gen->functions[i].func_ptr;
                        break;
                    }
                }
                if (func_addr)
                    emit_mov_rax_imm64(gen, (int64_t)(intptr_t)func_addr);
                else
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
            if (gen->hedge_loads)
                emit_prefetch_rax(gen);
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
                    if (gen->n_global_patches < 128) {
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

        /* Cast: (type)expr — convert between F64 and integer when the
         * target type differs from the operand's natural type. Without
         * this, (int)42.9 emitted the raw F64 bit-pattern (garbage I64).
         * We decide purely from the cast-target type + operand AST kind
         * since intermed. float arith isn't tracked as a distinct lane. */
        case HC_AST_CAST: {
            bool to_int = node->type && node->type->kind != HC_TYPE_F64;
            bool from_f64 = node->child &&
                (node->child->kind == HC_AST_FLOAT_LIT ||
                 (node->child->type && node->child->type->kind == HC_TYPE_F64));
            gen_expr(gen, node->child);
            if (to_int && from_f64) emit_cvt_f64_to_i64(gen);
            else if (!to_int && !from_f64) emit_cvt_i64_to_f64(gen);
            /* to_int && from_int, or to_f64 && from_f64: no-op */
            break;
        }

        /* sizeof(type) or sizeof expr — emit the size as a constant. */
        case HC_AST_SIZEOF: {
            size_t sz = 0;
            if (node->type) {
                sz = hc_type_size(node->type);
            } else {
                HCType *st = node->child ? expr_static_type(gen, node->child) : NULL;
                sz = st ? hc_type_size(st) : 8;   /* default I64 size */
            }
            emit_mov_rax_imm64(gen, (int64_t)sz);
            break;
        }

        /* Array index: expr[index] */
        case HC_AST_INDEX: {
            /* Determine element scale: char/byte arrays index by 1, wider
             * types by their size. Default to 1 (byte) so `"hi"[0]` works;
             * int/pointer arrays use the type size. */
            int scale = 1;
            HCType *bt = node->left->type;
            if (!bt)
                bt = expr_static_type(gen, node->left);  /* handles nested
                    INDEX / IDENT / MEMBER: `a[0][0]` outer left is INDEX */
            if (bt) {
                if (bt->kind == HC_TYPE_PTR && bt->base) {
                    int esz = hc_type_size(bt->base);
                    scale = (esz > 1) ? esz : 1;
                } else if (bt->kind == HC_TYPE_ARRAY && bt->base) {
                    int esz = hc_type_size(bt->base);
                    scale = (esz > 1) ? esz : 1;
                }
            }
            /* Evaluate base: for an array ident this is its ADDRESS (&x),
             * for a pointer ident it's the pointer value. */
            emit_base_addr(gen, node->left);
            /* rax = base address */
            emit_mov_rdi_rax(gen);  /* save base in rdi */
            /* Evaluate index expression */
            gen_expr(gen, node->right);
            /* rax = index, rdi = base */
            if (scale == 8) {
                /* Scale index by 8 (I64 size): shl rax, 3 */
                emit_byte(gen, 0x48); emit_byte(gen, 0xC1); emit_byte(gen, 0xE0); emit_byte(gen, 0x03);
            } else if (scale == 4) {
                emit_byte(gen, 0x48); emit_byte(gen, 0xC1); emit_byte(gen, 0xE0); emit_byte(gen, 0x02);
            } else if (scale == 2) {
                emit_byte(gen, 0x48); emit_byte(gen, 0xC1); emit_byte(gen, 0xE0); emit_byte(gen, 0x01);
            }
            /* Add to base: add rdi, rax */
            emit_byte(gen, 0x48); emit_byte(gen, 0x01); emit_byte(gen, 0xC7);
            /* Load from address with the element width (char arrays load a
             * byte, int arrays load 8). Previously always loaded 8 bytes,
             * so `"hi"[0]` returned 0x6968 (both chars packed) instead of
             * 'h'=104. */
            if (scale == 1) {
                if (gen->hedge_loads)
                    emit_prefetch_rdi(gen);
                /* movzx rax, byte ptr [rdi]: 48 0F B6 07 */
                emit_byte(gen, 0x48); emit_byte(gen, 0x0F); emit_byte(gen, 0xB6); emit_byte(gen, 0x07);
            } else if (scale == 2) {
                if (gen->hedge_loads)
                    emit_prefetch_rdi(gen);
                /* movzx rax, word ptr [rdi]: 48 0F B7 07 */
                emit_byte(gen, 0x48); emit_byte(gen, 0x0F); emit_byte(gen, 0xB7); emit_byte(gen, 0x07);
            } else if (scale == 4) {
                if (gen->hedge_loads)
                    emit_prefetch_rdi(gen);
                /* mov rax, dword ptr [rdi]: 8B 07 */
                emit_byte(gen, 0x8B); emit_byte(gen, 0x07);
            } else {
                if (gen->hedge_loads)
                    emit_prefetch_rdi(gen);
                /* mov rax, qword ptr [rdi]: 48 8B 07 */
                emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x07);
            }
            break;
        }

        /* Struct declaration - no-op at expression level */
        case HC_AST_STRUCT_DECL:
            emit_mov_rax_imm64(gen, 0);
            break;

        /* Binary operations: eval left → rax, push it, eval right
         * (rax=right, may clobber rdi), pop rdi (rdi=left), xchg
         * (rax=left, rdi=right), op. The xchg is REQUIRED for the
         * non-commutative ops (SUB/DIV/MOD/SHR) — dropping it reverses
         * the operands (the 10-(3*2) = -4 bug the differential
         * harness caught on our own first fix). */
        case HC_AST_ADD:
            gen_expr(gen, node->left);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);            /* rax=left, rdi=right */
            /* Pointer arithmetic: if left is a pointer/array, right is an
             * index scaled by the element size — so `x+1` = &x[1], not
             * &x[0]+1. (Scales rdi before the add; rax is the address.) */
            {
                int escale = pointer_elem_scale(gen, node->left);
                if (escale > 1) scale_rdi(gen, escale);
            }
            emit_add_rax_rdi(gen);
            break;

        case HC_AST_SUB:
            gen_expr(gen, node->left);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);            /* rax=left, rdi=right */
            {
                int escale = pointer_elem_scale(gen, node->left);
                if (escale > 1) scale_rdi(gen, escale);
            }
            emit_sub_rax_rdi(gen);
            break;

        case HC_AST_MUL:
            gen_expr(gen, node->left);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);
            emit_mul_rax_rdi(gen);
            break;

        case HC_AST_DIV:
            gen_expr(gen, node->left);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);
            emit_div_rax_rdi(gen);
            break;

        case HC_AST_MOD:
            gen_expr(gen, node->left);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);
            emit_div_rax_rdi(gen);
            /* Remainder is in rdx, move to rax */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89);
            emit_byte(gen, 0xD0); /* mov rax, rdx */
            break;

        /* Bitwise AND, OR, XOR */
        case HC_AST_BITAND:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            /* save the LEFT operand across the right's evaluation —
             * the right may itself be a binop that clobbers rdi (the
             * 1|2&4 bug: the inner bitand destroyed rdi=left). push
             * left, gen right (rax=right), pop rdi (rdi=left), xchg. */
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);
            /* and rax, rdi: 48 21 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x21); emit_byte(gen, 0xF8);
            break;

        case HC_AST_BITOR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);
            /* or rax, rdi: 48 09 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x09); emit_byte(gen, 0xF8);
            break;

        case HC_AST_BITXOR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            emit_byte(gen, 0x50);              /* push rax (left) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left restored) */
            emit_xchg_rax_rdi(gen);
            /* xor rax, rdi: 48 31 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xF8);
            break;

        /* Shift left/right */
        case HC_AST_SHL:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);            /* rax=left, rdi=right */
            /* mov rcx, rdi (shift count must be in cl): 48 89 F9 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9);
            emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE0); /* shl rax, cl */
            break;

        case HC_AST_SHR:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);            /* rax=left, rdi=right */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9);
            emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE8); /* shr rax, cl */
            break;

        /* Comparison ops: cmp rax, rdi then setcc */
        case HC_AST_EQ:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x94); /* sete */
            break;

        case HC_AST_NE:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x95); /* setne */
            break;

        case HC_AST_LT:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9C); /* setl */
            break;

        case HC_AST_LE:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9E); /* setle */
            break;

        case HC_AST_GT:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9F); /* setg */
            break;

        case HC_AST_GE:
            gen_expr(gen, node->left); emit_byte(gen, 0x50); /* push left */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x5F);              /* pop rdi (left) */
            emit_xchg_rax_rdi(gen);
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

        case HC_AST_ASSIGN: {
            /* Right-hand side → rax (for struct-by-value calls, rax = pointer
             * to the callee's static ret-slot; for everything else, rax is
             * the scalar value to store). */
            gen_expr(gen, node->right);
            HCType *lt = node->left ? expr_static_type(gen, node->left) : NULL;
            int lhs_sz = (lt && lt->kind == HC_TYPE_STRUCT) ? (int)hc_type_size(lt) : 0;
            /* If LHS is a struct IDENT and the RHS expression is a CALL
             * (rax currently holds the callee's &ret-slot), do a rep movsb
             * copy. If RHS is also a struct literal/IDENT, the current
             * emit_var_store copies only 8 bytes — same 8-byte-leak bug for
             * rhs IDENT, but we leave it untouched for this wave. */
            bool rhs_is_call = node->right && node->right->kind == HC_AST_FUNC_CALL;
            /* rep movsb from the callee's &slot to &lhs for struct-return
             * calls of ANY size. (The legacy 8-byte-only arg passing is a
             * SEPARATE limitation — it only affects calling a >8B-returning
             * function, not the return+assign path here.) */
            bool sret_supported = lhs_sz > 0 && rhs_is_call &&
                                  node->left->kind == HC_AST_IDENT;
            if (sret_supported) {
                int off = 0, is_global = 0;
                resolve_var(gen, node->left->ident, &off, &is_global);
                /* rax currently holds &slot (from RETURN path). We need
                 * dst = &lhs, src = rax. Swap: push rax; lea rax, [rbp-off];
                 * pop rsi; xchg rax, rdi → rdi=&lhs, rsi=&slot. Then rcx=lhs_sz. */
                emit_byte(gen, 0x50);                              /* push rax (slot addr) */
                if (is_global) {
                    size_t go = (size_t)(-off);
                    size_t patch_pos = gen->code_size + 3;
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x05);
                    emit_dword(gen, 0);
                    if (gen->n_global_patches < 128) {
                        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
                        gen->global_patches[gen->n_global_patches].global_offset = go;
                        gen->n_global_patches++;
                    }
                } else {
                    emit_byte(gen, 0x48); emit_byte(gen, 0x8D); emit_byte(gen, 0x85);
                    emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                }
                /* rax = &lhs */
                emit_byte(gen, 0x5E);                              /* pop rsi (slot addr) */
                /* xchg rax, rdi → rax = slot addr (returned), rdi = &lhs */
                emit_byte(gen, 0x48); emit_byte(gen, 0x97);      /* xchg rax, rdi */
                /* mov rcx, lhs_sz */
                emit_byte(gen, 0x48); emit_byte(gen, 0xC7); emit_byte(gen, 0xC1);
                emit_dword(gen, (uint32_t)lhs_sz);
                /* rep movsb: [rdi=&lhs] <- [rsi=&slot], rcx bytes */
                emit_rep_movsb(gen);
                /* Restore rax = &slot for any chained rvalue uses. */
            } else if (node->left && node->left->kind == HC_AST_IDENT) {
                int off = 0, is_global = 0;
                resolve_var(gen, node->left->ident, &off, &is_global);
                emit_var_store(gen, off, is_global);
            } else if (node->left) {
                /* save value across address computation */
                emit_byte(gen, 0x50);            /* push rax (value) */
                emit_lvalue_addr(gen, node->left);  /* rax = address */
                emit_byte(gen, 0x5F);            /* pop rdi (value) */
                /* store with the target's element width (a 4-byte int member
                 * stores dword, so the adjacent member isn't clobbered). */
                HCType *lt2 = expr_static_type(gen, node->left);
                int esz = lt2 ? (int)hc_type_size(lt2) : 8;
                emit_sized_store_rax_rdi(gen, esz);
            }
            break;
        }

        /* Compound assignments: x op= y means x = x op y. Unified with
         * the shared resolve/load/store helpers so a GLOBAL (module-level
         * var, off<=0) is accessed RIP-relative — the old per-op code
         * emitted `mov [rbp-off]` unconditionally and corrupted the stack
         * for globals. Covers all 10 ops (+= -= *= /= %= <<= >>= &= |= ^=). */
        case HC_AST_ADD_ASSIGN:
        case HC_AST_SUB_ASSIGN:
        case HC_AST_MUL_ASSIGN:
        case HC_AST_DIV_ASSIGN:
        case HC_AST_MOD_ASSIGN:
        case HC_AST_SHL_ASSIGN:
        case HC_AST_SHR_ASSIGN:
        case HC_AST_AMP_ASSIGN:
        case HC_AST_PIPE_ASSIGN:
        case HC_AST_CARET_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                int off = 0, is_global = 0;
                resolve_var(gen, node->left->ident, &off, &is_global);
                /* rax = left */
                emit_var_load(gen, off, is_global);
                /* rdi = left (via rdi/rax swap dance to survive right eval) */
                emit_mov_rdi_rax(gen);
                /* rax = right (may clobber rdi) */
                gen_expr(gen, node->right);
                /* rax=left, rdi=right  (the xchg is REQUIRED for the
                 * non-commutative ops — see the rdi-clobber binop fix) */
                emit_xchg_rax_rdi(gen);
                /* rax op= rdi */
                switch (node->kind) {
                    case HC_AST_ADD_ASSIGN:     emit_add_rax_rdi(gen); break;
                    case HC_AST_SUB_ASSIGN:     emit_sub_rax_rdi(gen); break;
                    case HC_AST_MUL_ASSIGN:     emit_mul_rax_rdi(gen); break;
                    case HC_AST_DIV_ASSIGN:     emit_div_rax_rdi(gen); break;
                    case HC_AST_MOD_ASSIGN:     emit_mod_rax_rdi(gen); break;
                    case HC_AST_SHL_ASSIGN:     emit_shl_rax_rdi(gen); break;
                    case HC_AST_SHR_ASSIGN:     emit_shr_rax_rdi(gen); break;
                    case HC_AST_AMP_ASSIGN:     emit_and_rax_rdi(gen); break;
                    case HC_AST_PIPE_ASSIGN:    emit_or_rax_rdi(gen);  break;
                    case HC_AST_CARET_ASSIGN:   emit_xor_rax_rdi(gen); break;
                    default: break;
                }
                emit_var_store(gen, off, is_global);
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
                    /* Struct-by-value arg >8B: the callee expects a POINTER
                     * (its param prologue deref-copies). Emit the address,
                     * not the packed value. */
                    HCType *arg_t = expr_static_type(gen, node->args[i]);
                    if (arg_t && arg_t->kind == HC_TYPE_STRUCT &&
                        hc_type_size(arg_t) > 8) {
                        emit_base_addr(gen, node->args[i]);
                    } else {
                        gen_expr(gen, node->args[i]);
                    }
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
                        } else if (gen->n_self_call_patches < 32 &&
                                   node->callee->ident[0] != '\0' &&
                                   strcmp(node->callee->ident, gen->current_function) == 0) {
                            /* Self-recursive call: this function is still being
                             * compiled, so its address isn't registered yet.
                             * Emit `call rel32` (E8 + placeholder disp32=0) and
                             * record the position; patched to exec+0 after the
                             * body is copied to executable memory. */
                            size_t pos = gen->code_size;
                            emit_byte(gen, 0xE8);
                            emit_dword(gen, 0);   /* placeholder */
                            gen->self_call_patches[gen->n_self_call_patches++] = pos;
                        } else {
                            /* IDENT not in the function/extern tables: treat
                             * it as a function POINTER variable and call through
                             * its stored value (`op(20,22)` where `op` is a
                             * `typedef int(*)(int,int)`). Load the pointer, then
                             * `call rax` — a NULL stored value would have crashed
                             * at `call 0` before this path existed. */
                            emit_var_load(gen,
                                ({ int off=0,isg=0; resolve_var(gen, node->callee->ident, &off, &isg); off; }),
                                ({ int off=0,isg=0; resolve_var(gen, node->callee->ident, &off, &isg); isg; }));
                            emit_byte(gen, 0xFF); emit_byte(gen, 0xD0); /* call rax */
                        }
                    }
                } else {
                    /* Non-ident callee (e.g. function pointer) with no
                     * resolvable address: trap instead of `call 0`. */
                    emit_mov_rax_imm64(gen, 0);
                    /* mov rdi, rax : 48 89 C7 */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC7);
                    emit_mov_rax_imm64(gen, (int64_t)(uintptr_t)hc_trap_unresolved_call);
                    emit_byte(gen, 0xFF); emit_byte(gen, 0xD0);
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
            /* Evaluate base: struct-VALUE ident decays to &s (address);
             * other bases eval normally. */
            emit_base_addr(gen, node->left);
            /* rax now contains base address */
            /* Find member offset — resolve the base's struct type from the
             * symbol table (the parser doesn't attach types to IDENTs). */
            HCType *btype = expr_static_type(gen, node->left);
            if (btype && (btype->kind == HC_TYPE_STRUCT || btype->kind == HC_TYPE_UNION)) {
                HCType *st = btype;
                bool found = false;
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) {
                        int off = (int)st->members[i].offset;
                        int msz = st->members[i].type
                                      ? (int)hc_type_size(st->members[i].type)
                                      : 8;
                        emit_sized_load_rax_off(gen, off, msz);
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
            /* Evaluate base: a struct-pointer ident loads its value (which
             * IS the address); a struct-VALUE ident would decay to &s. */
            emit_base_addr(gen, node->left);
            /* rax now contains base address (already a pointer) */
            HCType *btype = expr_static_type(gen, node->left);
            HCType *st = NULL;
            if (btype && btype->kind == HC_TYPE_PTR && btype->base &&
                (btype->base->kind == HC_TYPE_STRUCT || btype->base->kind == HC_TYPE_UNION))
                st = btype->base;
            if (st) {
                bool found = false;
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) {
                        int off = (int)st->members[i].offset;
                        int msz = st->members[i].type
                                      ? (int)hc_type_size(st->members[i].type)
                                      : 8;
                        emit_sized_load_rax_off(gen, off, msz);
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