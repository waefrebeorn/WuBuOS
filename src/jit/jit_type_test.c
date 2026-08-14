/*
 * jit_type_test.c -- Verify the Mini-C type system (Subsystem A):
 * struct field reordering (#19) packs to the minimal size, and member offsets
 * are computed consistently.
 *
 * #19 correctness: the classic case — a struct with members {char, long, char}
 * declared in source order packs to 16 bytes under naive layout (1+7 pad+8+1+7
 * pad) but to 24? No — reorder {long,char,char} = 8 + 1 + 1 = 10 -> align to
 * 16 = 16 bytes. Compare a case where reordering actually saves space:
 *   struct { char c1; long x; char c2; }  -> naive 24, reordered 16.
 * The DISCRIMINATOR is sizeof: reorder must shrink 24 -> 16 (or the minimal
 * packing), and member offsets must match the reordered layout exactly.
 */
#include "jit_internal.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;
#define CHECK(cond, msg) do { if (cond) pass++; else { fail++; printf("FAIL: %s\n", msg); } } while(0)

int main(void) {
    MinicTypeRegistry r;
    minic_type_registry_init(&r);
    /* types: 0=I64,1=U8,2=void* */

    /* Build struct S { char c1; long x; char c2; } */
    int s = minic_type_new(&r);
    MinicType *st = &r.types[s];
    st->kind = MTY_STRUCT;
    strcpy(st->name, "S");
    strcpy(st->members[0].name, "c1"); st->members[0].mty = 1;  /* U8 */
    strcpy(st->members[1].name, "x");  st->members[1].mty = 0;  /* I64 */
    strcpy(st->members[2].name, "c2"); st->members[2].mty = 1;  /* U8 */
    st->n_members = 3;
    minic_type_layout(&r, st);

    /* #19: reordered layout { x(0..7), c1(8), c2(9) }, size aligned to 16 */
    CHECK(minic_type_size(&r, s) == 16, "S reordered size == 16 (was 24 naive)");
    CHECK(minic_type_member_offset(&r, s, "x") == 0,  "x at offset 0 (reordered first)");
    CHECK(minic_type_member_offset(&r, s, "c1") == 8, "c1 at offset 8");
    CHECK(minic_type_member_offset(&r, s, "c2") == 9, "c2 at offset 9");

    /* A second struct with only same-alignment members keeps decl order */
    int t = minic_type_new(&r);
    MinicType *tt = &r.types[t];
    tt->kind = MTY_STRUCT; strcpy(tt->name, "T");
    strcpy(tt->members[0].name, "a"); tt->members[0].mty = 1;
    strcpy(tt->members[1].name, "b"); tt->members[1].mty = 1;
    tt->n_members = 2;
    minic_type_layout(&r, tt);
    CHECK(minic_type_member_offset(&r, t, "a") == 0, "T.a at 0");
    CHECK(minic_type_member_offset(&r, t, "b") == 1, "T.b at 1");
    CHECK(minic_type_size(&r, t) == 2, "T size == 2");

    /* pointer type is 8 bytes, aligned 8 */
    CHECK(minic_type_size(&r, 2) == 8, "void* size == 8");
    CHECK(minic_type_align(&r, 2) == 8, "void* align == 8");
    CHECK(minic_type_is_ptr(&r, 2), "type 2 is ptr");

    /* robustness: re-running layout is idempotent (size unchanged) */
    minic_type_layout(&r, st);
    CHECK(minic_type_size(&r, s) == 16, "S re-layout idempotent (still 16)");

    /* member offset of a nonexistent member returns -1 */
    CHECK(minic_type_member_offset(&r, s, "nope") == -1, "missing member -> -1");

    printf("=== jit_type_test: %d passed, %d failed ===\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
