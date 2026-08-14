/*
 * test_mir_regalloc.c -- verify the MIR linear-scan register allocator.
 *
 * Tests:
 *   1. Build a MIR program with many virtual registers.
 *   2. Run the allocator.
 *   3. Verify no two simultaneously-live vrs share a physical register.
 *   4. Verify the assignment count matches max_vr + 1.
 *
 * Compile:
 *   cc -std=c11 -Wall -Wextra -I src/compiler \
 *      src/compiler/wubu_mir.c src/compiler/wubu_mir_regalloc.c \
 *      tools/test_mir_regalloc.c -o test_mir_regalloc
 * Run:
 *   ./test_mir_regalloc
 */

#include "wubu_mir.h"
#include "wubu_mir_regalloc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers: opcode classification (mirrors the allocator's view)     */
/* ------------------------------------------------------------------ */

static int mir_has_dst(wubu_mir_op_t op)
{
    switch (op) {
    case MIR_CONST:
    case MIR_ADD: case MIR_SUB: case MIR_MUL: case MIR_DIV: case MIR_MOD:
    case MIR_AND: case MIR_OR: case MIR_XOR:
    case MIR_SHL: case MIR_SHR:
    case MIR_NEG: case MIR_NOT:
    case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE:
    case MIR_MOV:
        return 1;
    default:
        return 0;
    }
}

static int mir_num_srcs(wubu_mir_op_t op)
{
    switch (op) {
    case MIR_CONST:
    case MIR_LABEL:
    case MIR_JMP:
        return 0;
    case MIR_NEG: case MIR_NOT:
    case MIR_MOV:
    case MIR_JZ: case MIR_RET:
        return 1;
    default:
        return 2;
    }
}

/* ------------------------------------------------------------------ */
/* Dynamic integer set for liveness tracking                           */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t *vrs;
    size_t    n;
    size_t    cap;
} vr_set_t;

static void vrset_init(vr_set_t *s)
{
    s->cap = 64;
    s->n   = 0;
    s->vrs = (uint32_t *)malloc(s->cap * sizeof(uint32_t));
}

static void vrset_add(vr_set_t *s, uint32_t vr)
{
    for (size_t i = 0; i < s->n; i++)
        if (s->vrs[i] == vr) return;
    if (s->n >= s->cap) {
        s->cap *= 2;
        s->vrs = (uint32_t *)realloc(s->vrs, s->cap * sizeof(uint32_t));
    }
    s->vrs[s->n++] = vr;
}

/* ------------------------------------------------------------------ */
/* Compute first_def and last_use for all vrs in the program          */
/* ------------------------------------------------------------------ */

static void compute_live_ranges(const wubu_mir_prog_t *p,
                                 uint32_t max_vr,
                                 int32_t *first_def,
                                 int32_t *last_use)
{
    size_t n_vr = (size_t)max_vr + 1;
    for (size_t v = 0; v < n_vr; v++) {
        first_def[v] = -1;
        last_use[v]  = -1;
    }

    size_t n_ins = p->n;
    for (size_t i = 0; i < n_ins; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];

        if (mir_has_dst(in->op)) {
            if (first_def[in->dst] < 0)
                first_def[in->dst] = (int32_t)i;
        }

        int ns = mir_num_srcs(in->op);
        if (ns >= 1) last_use[in->a] = (int32_t)i;
        if (ns >= 2) last_use[in->b] = (int32_t)i;
    }

    /* For vrs that are defined but never used as a source, their
     * last_use defaults to their first_def (live only at the def point). */
    for (uint32_t v = 0; v <= max_vr; v++) {
        if (first_def[v] >= 0 && last_use[v] < 0)
            last_use[v] = first_def[v];
    }
}

static uint32_t find_max_vr(const wubu_mir_prog_t *p)
{
    uint32_t max_vr = 0;
    size_t n_ins = p->n;
    for (size_t i = 0; i < n_ins; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (mir_has_dst(in->op) && in->dst > max_vr) max_vr = in->dst;
        int ns = mir_num_srcs(in->op);
        if (ns >= 1 && in->a > max_vr) max_vr = in->a;
        if (ns >= 2 && in->b > max_vr) max_vr = in->b;
    }
    /* Always include v0 in the result array (return register) */
    if (max_vr < 1) max_vr = 0;
    return max_vr;
}

/* ------------------------------------------------------------------ */
/* Verify: no two simultaneously-live vrs share a physical register   */
/* ------------------------------------------------------------------ */

static int verify_no_conflicts(const wubu_mir_prog_t *p,
                                const wubu_reg_assign_t *assign,
                                size_t count,
                                int n_phys_regs)
{
    size_t n_ins = p->n;
    if (n_ins == 0) return 1;

    uint32_t max_vr = find_max_vr(p);
    size_t n_vr = (size_t)max_vr + 1;

    int32_t *first_def = (int32_t *)malloc(n_vr * sizeof(int32_t));
    int32_t *last_use  = (int32_t *)malloc(n_vr * sizeof(int32_t));
    if (!first_def || !last_use) {
        free(first_def);
        free(last_use);
        return 0;
    }
    compute_live_ranges(p, max_vr, first_def, last_use);

    /* Forward liveness: at each instruction, check conflicts among
     * currently-live vrs, then update the live set. */
    vr_set_t live;
    vrset_init(&live);
    int ok = 1;

    for (size_t i = 0; i < n_ins && ok; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];

        /* Conflict check on the live set BEFORE this instruction */
        for (size_t a = 0; a < live.n && ok; a++) {
            for (size_t b = a + 1; b < live.n && ok; b++) {
                uint32_t va = live.vrs[a];
                uint32_t vb = live.vrs[b];
                if (va >= count || vb >= count) continue;
                int32_t ra = assign[va].reg;
                int32_t rb = assign[vb].reg;
                if (ra >= 0 && ra < n_phys_regs &&
                    rb >= 0 && rb < n_phys_regs && ra == rb) {
                    fprintf(stderr,
                        "CONFLICT at ins %zu: v%u (reg %d) and v%u (reg %d) "
                        "are simultaneously live\n",
                        i, va, ra, vb, rb);
                    ok = 0;
                }
            }
        }

        /* Kill: remove vrs whose last_use is at this position.
         * A vr used (as src) here is consumed; if not redefined here,
         * it dies. (SSA: a new def here means it stays alive.) */
        unsigned has_dst = mir_has_dst(in->op);
        wubu_vr_t def_dst = has_dst ? in->dst : 0xFFFFFFFFu;

        int write = 0;
        for (size_t j = 0; j < live.n; j++) {
            uint32_t av = live.vrs[j];
            if (last_use[av] == (int32_t)i && av != def_dst) {
                /* consumed and not redefined here */
                continue;
            }
            live.vrs[write++] = av;
        }
        live.n = write;

        /* Define: add the new vr (if any) to the live set */
        if (has_dst) vrset_add(&live, in->dst);
    }

    free(live.vrs);
    free(first_def);
    free(last_use);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Test programs                                                       */
/* ------------------------------------------------------------------ */

/*
 * Test 1: Basic allocation with args.
 * v1, v2 = args (pre-assigned to reg 0, 1).
 * v3 = v1+v2, v4 = v3+v1, v5 = v3-v2, ret v5.
 * With 6 phys regs, nothing should spill.
 */
static void test_basic_allocation(void)
{
    printf("[test_basic_allocation] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);
    wubu_mir_set_n_args(&p, 2);

    wubu_vr_t v1 = wubu_mir_const(&p, 10);
    wubu_vr_t v2 = wubu_mir_const(&p, 20);
    wubu_vr_t v3 = wubu_mir_binop(&p, MIR_ADD, v1, v2);
    wubu_vr_t v4 = wubu_mir_binop(&p, MIR_ADD, v3, v1);
    wubu_vr_t v5 = wubu_mir_binop(&p, MIR_SUB, v3, v2);
    wubu_mir_ret(&p, v5);

    size_t count = 0;
    wubu_reg_assign_t *a = wubu_mir_alloc_regs(&p, 6, &count);
    assert(a != NULL);
    assert(count == 6);  /* v0..v5 */

    /* Pre-assigned: v0 -> reg 0 (return), v1 -> reg 1, v2 -> reg 2 */
    assert(a[0].reg == 0);   /* v0 = return = reg 0 */
    assert(a[v1].reg == 1);  /* v1 = arg 0 -> reg 1 */
    assert(a[v2].reg == 2);  /* v2 = arg 1 -> reg 2 */

    /* v3, v4, v5 should have valid regs (6 regs, no spill expected) */
    assert(a[v3].reg >= 0);
    assert(a[v4].reg >= 0);
    assert(a[v5].reg >= 0);

    /* No conflicts */
    assert(verify_no_conflicts(&p, a, count, 6));

    wubu_mir_free_alloc(a);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

/*
 * Test 2: Spilling with few registers.
 * 8 const vrs used in a chain, only 4 phys regs.
 * Some vrs must spill.
 */
static void test_spilling(void)
{
    printf("[test_spilling] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);

    wubu_vr_t v[8];
    for (int i = 0; i < 8; i++) {
        v[i] = wubu_mir_const(&p, i + 1);
    }
    wubu_vr_t acc = v[0];
    for (int i = 1; i < 8; i++) {
        acc = wubu_mir_binop(&p, MIR_ADD, acc, v[i]);
    }
    wubu_mir_ret(&p, acc);

    size_t count = 0;
    int n_phys = 4;
    wubu_reg_assign_t *a = wubu_mir_alloc_regs(&p, n_phys, &count);
    assert(a != NULL);

    /* Count spills */
    int spilled = 0;
    for (size_t i = 0; i < count; i++) {
        if (a[i].reg < 0) spilled++;
    }
    printf("  spilled %d of %zu vrs (n_phys=%d)\n", spilled, count, n_phys);
    assert(spilled > 0);

    /* No two simultaneously-live vrs share a physical reg */
    assert(verify_no_conflicts(&p, a, count, n_phys));

    /* v0 pre-assigned */
    assert(a[0].reg == 0);

    wubu_mir_free_alloc(a);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

/*
 * Test 3: All 6 args pre-assigned to regs 0-5.
 */
static void test_args_preassigned(void)
{
    printf("[test_args_preassigned] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);
    wubu_mir_set_n_args(&p, 6);

    wubu_vr_t args[6];
    for (int i = 0; i < 6; i++) {
        args[i] = wubu_mir_const(&p, 100 + i);
    }
    wubu_vr_t tmp = wubu_mir_binop(&p, MIR_ADD, args[0], args[1]);
    tmp = wubu_mir_binop(&p, MIR_ADD, tmp, args[2]);
    wubu_mir_ret(&p, tmp);

    size_t count = 0;
    wubu_reg_assign_t *a = wubu_mir_alloc_regs(&p, 8, &count);
    assert(a != NULL);

    /* All 6 args should be pre-assigned to regs 1..6 */
    for (int i = 0; i < 6; i++) {
        printf("  v%u -> reg %d\n", args[i], a[args[i]].reg);
        assert(a[args[i]].reg == i + 1);  /* v1 -> reg 1, v2 -> reg 2, etc. */
    }

    assert(verify_no_conflicts(&p, a, count, 8));

    wubu_mir_free_alloc(a);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

/*
 * Test 4: Assignment count = max_vr + 1.
 */
static void test_count_matches_max_vr(void)
{
    printf("[test_count_matches_max_vr] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);

    /* Creates vrs 1..15 (acc starts at 1, then 14 binops each producing a new vr) */
    wubu_vr_t acc = wubu_mir_const(&p, 1);
    for (int i = 0; i < 14; i++) {
        wubu_vr_t c = wubu_mir_const(&p, i + 2);
        acc = wubu_mir_binop(&p, MIR_ADD, acc, c);
    }
    wubu_mir_ret(&p, acc);

    size_t count = 0;
    wubu_reg_assign_t *a = wubu_mir_alloc_regs(&p, 10, &count);
    assert(a != NULL);

    uint32_t max_vr = find_max_vr(&p);
    assert(count == max_vr + 1);
    printf("  max_vr=%u, count=%zu (match)\n", max_vr, count);

    assert(verify_no_conflicts(&p, a, count, 10));

    wubu_mir_free_alloc(a);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

/*
 * Test 5: Program with args + branches (ternary-like).
 */
static void test_with_args_and_branches(void)
{
    printf("[test_with_args_and_branches] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);
    wubu_mir_set_n_args(&p, 3);

    wubu_vr_t a = wubu_mir_const(&p, 1);
    wubu_vr_t b = wubu_mir_const(&p, 2);
    wubu_vr_t c = wubu_mir_const(&p, 3);

    wubu_vr_t z = wubu_mir_binop(&p, MIR_NE, a, wubu_mir_const(&p, 0));
    uint32_t l_false = wubu_mir_new_label(&p);
    uint32_t l_end   = wubu_mir_new_label(&p);
    wubu_mir_jz(&p, z, l_false);
    wubu_vr_t merge = wubu_mir_const(&p, 0);
    wubu_mir_mov_to(&p, merge, b);
    wubu_mir_jmp(&p, l_end);
    wubu_mir_place_label(&p, l_false);
    wubu_mir_mov_to(&p, merge, c);
    wubu_mir_place_label(&p, l_end);
    wubu_mir_ret(&p, merge);

    size_t count = 0;
    wubu_reg_assign_t *as = wubu_mir_alloc_regs(&p, 6, &count);
    assert(as != NULL);

    /* v1, v2, v3 pre-assigned to regs 1, 2, 3 */
    assert(as[a].reg == 1);
    assert(as[b].reg == 2);
    assert(as[c].reg == 3);

    printf("  Program with branches + args:\n");
    wubu_mir_dump(&p);

    assert(verify_no_conflicts(&p, as, count, 6));

    wubu_mir_free_alloc(as);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

/*
 * Test 6: Edge case — program with no instructions beyond ret.
 * Only v0 exists.
 */
static void test_minimal_program(void)
{
    printf("[test_minimal_program] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);
    wubu_mir_set_n_args(&p, 0);
    wubu_mir_ret(&p, 0);

    size_t count = 0;
    wubu_reg_assign_t *a = wubu_mir_alloc_regs(&p, 8, &count);
    assert(a != NULL);
    assert(count == 1);  /* only v0 */
    assert(a[0].reg == 0);

    assert(verify_no_conflicts(&p, a, count, 8));

    wubu_mir_free_alloc(a);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

/*
 * Test 7: More regs than needed — everything in register, no spills.
 */
static void test_no_spill_abundant(void)
{
    printf("[test_no_spill_abundant] ...\n");
    wubu_mir_prog_t p;
    wubu_mir_init(&p);

    /* 10 const vrs all live simultaneously (defined first, then used),
     * but with 16 phys regs that's plenty. No spills expected. */
    wubu_vr_t v[10];
    for (int i = 0; i < 10; i++) {
        v[i] = wubu_mir_const(&p, i);
    }
    wubu_vr_t acc = wubu_mir_binop(&p, MIR_ADD, v[0], v[1]);
    for (int i = 2; i < 10; i++) {
        acc = wubu_mir_binop(&p, MIR_ADD, acc, v[i]);
    }
    wubu_mir_ret(&p, acc);
    (void)v;

    size_t count = 0;
    wubu_reg_assign_t *a = wubu_mir_alloc_regs(&p, 16, &count);
    assert(a != NULL);

    int spilled = 0;
    for (size_t i = 0; i < count; i++) {
        if (a[i].reg < 0) spilled++;
    }
    printf("  spilled %d of %zu vrs (n_phys=16)\n", spilled, count);
    assert(spilled == 0);

    assert(verify_no_conflicts(&p, a, count, 16));

    wubu_mir_free_alloc(a);
    wubu_mir_free(&p);
    printf("  PASS\n");
}

int main(void)
{
    printf("=== MIR Register Allocator Tests ===\n\n");

    test_basic_allocation();
    test_spilling();
    test_args_preassigned();
    test_count_matches_max_vr();
    test_with_args_and_branches();
    test_minimal_program();
    test_no_spill_abundant();

    printf("\n=== All tests passed ===\n");
    return 0;
}
