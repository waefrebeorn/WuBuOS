/* jit_minic_type.c -- Mini-C type system for the WuBuOS JIT.
 *
 * Subsystem A: a real type model (structs, pointers, arrays) with a layout
 * pass. Delivers optimization #19 (struct field reordering): members are
 * sorted by descending alignment before assigning offsets, which packs the
 * struct to its minimum size (the largest-alignment-first packing rule).
 *
 * The reorder is SAFE for the compiler because the compiler owns the layout:
 * every member access goes through the same member->offset table, so a reorder
 * is transparent to generated code. (This is NOT a C-ABI struct — it is the
 * compiler's own layout, which is exactly what makes reordering valid.)
 */
#include "jit_internal.h"
#include <stdlib.h>

/* -- registry helpers ------------------------------------------- */

int minic_type_new(MinicTypeRegistry *r) {
    if (r->n_types >= MINIC_MAX_TYPES) return -1;
    int i = r->n_types++;
    memset(&r->types[i], 0, sizeof(r->types[i]));
    r->types[i].size = 0;
    r->types[i].align = 1;
    r->types[i].elem = -1;
    r->types[i].defined = 1;   /* primitives/anonymous are pre-defined */
    return i;
}

/* Primitive types are always indices 0..2. */
void minic_type_registry_init(MinicTypeRegistry *r) {
    memset(r, 0, sizeof(*r));
    /* 0 = I64 (long/int) */
    r->types[0].kind = MTY_I64; r->types[0].size = 8; r->types[0].align = 8; r->types[0].defined = 1;
    /* 1 = U8 */
    r->types[1].kind = MTY_U8; r->types[1].size = 1; r->types[1].align = 1; r->types[1].defined = 1;
    /* 2 = void * placeholder (pointer, 8 bytes) */
    r->types[2].kind = MTY_PTR; r->types[2].size = 8; r->types[2].align = 8; r->types[2].elem = 0; r->types[2].defined = 1;
    r->n_types = 3;
}

MinicType *minic_type_find(MinicTypeRegistry *r, const char *name) {
    for (int i = 0; i < r->n_types; i++)
        if (r->types[i].name[0] && strcmp(r->types[i].name, name) == 0)
            return &r->types[i];
    return NULL;
}

int minic_type_index(MinicTypeRegistry *r, MinicType *t) {
    return (int)(t - r->types);
}

static int mty_align(MinicTypeRegistry *r, int idx) {
    if (idx < 0 || idx >= r->n_types) return 1;
    return r->types[idx].align;
}
static int64_t mty_size(MinicTypeRegistry *r, int idx) {
    if (idx < 0 || idx >= r->n_types) return 0;
    return r->types[idx].size;
}

/* -- struct field reordering (#19) --------------------------------
 * Sort members by descending alignment, then by original order for equal
 * alignments (stable), then assign offsets with natural alignment padding.
 * This yields the minimal packed size (largest-alignment-first). Because the
 * compiler owns the layout, member accesses stay correct after the reorder. */
static int member_align_cmp(const void *a, const void *b) {
    const MinicMember *ma = (const MinicMember*)a, *mb = (const MinicMember*)b;
    if (mb->align != ma->align) return mb->align - ma->align;  /* desc align */
    return ma->offset - mb->offset;                            /* stable by decl order */
}

void minic_type_layout(MinicTypeRegistry *r, MinicType *s) {
    /* Compute each member's size/align from its type FIRST (the sort
     * comparator needs them), and snapshot decl-order for the stable sort. */
    for (int i = 0; i < s->n_members; i++) {
        MinicMember *m = &s->members[i];
        m->offset = i;                        /* decl order (stability key) */
        m->size = mty_size(r, m->mty);
        m->align = mty_align(r, m->mty);
    }

    /* #19: sort by descending alignment to minimize padding */
    qsort(s->members, (size_t)s->n_members, sizeof(MinicMember), member_align_cmp);

    int64_t off = 0;
    int max_align = 1;
    for (int i = 0; i < s->n_members; i++) {
        MinicMember *m = &s->members[i];
        if (m->align > max_align) max_align = m->align;
        off = (off + m->align - 1) & ~((int64_t)m->align - 1);  /* align up */
        m->offset = (int32_t)off;
        off += m->size;
    }
    s->size = (off + max_align - 1) & ~((int64_t)max_align - 1);
    s->align = max_align;
    s->defined = 1;
}

/* -- query API for the compiler ---------------------------------- */

int minic_type_member_offset(MinicTypeRegistry *r, int type_idx, const char *member) {
    if (type_idx < 0 || type_idx >= r->n_types) return -1;
    MinicType *t = &r->types[type_idx];
    if (t->kind != MTY_STRUCT) return -1;
    for (int i = 0; i < t->n_members; i++)
        if (strcmp(t->members[i].name, member) == 0)
            return t->members[i].offset;
    return -1;
}

/* Byte size of a named struct member, or 0 if not found. */
int minic_type_member_size(MinicTypeRegistry *r, int type_idx, const char *member) {
    if (type_idx < 0 || type_idx >= r->n_types) return 0;
    MinicType *t = &r->types[type_idx];
    if (t->kind != MTY_STRUCT) return 0;
    for (int i = 0; i < t->n_members; i++)
        if (strcmp(t->members[i].name, member) == 0)
            return (int)t->members[i].size;
    return 0;
}

int minic_type_size(MinicTypeRegistry *r, int type_idx) {
    return (int)mty_size(r, type_idx);
}
int minic_type_align(MinicTypeRegistry *r, int type_idx) {
    return mty_align(r, type_idx);
}
int minic_type_is_struct(MinicTypeRegistry *r, int type_idx) {
    return type_idx >= 0 && type_idx < r->n_types && r->types[type_idx].kind == MTY_STRUCT;
}
int minic_type_is_ptr(MinicTypeRegistry *r, int type_idx) {
    return type_idx >= 0 && type_idx < r->n_types && r->types[type_idx].kind == MTY_PTR;
}
int minic_type_is_array(MinicTypeRegistry *r, int type_idx) {
    return type_idx >= 0 && type_idx < r->n_types && r->types[type_idx].kind == MTY_ARRAY;
}
int minic_type_elem(MinicTypeRegistry *r, int type_idx) {
    return type_idx >= 0 && type_idx < r->n_types ? r->types[type_idx].elem : -1;
}
