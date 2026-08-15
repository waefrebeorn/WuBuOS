/*
 * wubu_mir_regalloc.c -- linear scan register allocator for MIR programs.
 *
 * Algorithm (linear scan on SSA):
 *   1. Scan instructions forward to compute live ranges [first_def, last_use]
 *      for each virtual register.
 *   2. Sort intervals by start position (first_def).
 *   3. Walk intervals linearly:
 *      - Expire intervals whose last_use < current start (free their regs).
 *      - If a free physical register exists, assign it.
 *      - Otherwise, spill the interval whose last_use is furthest in the
 *        future (the "farthest" heuristic). If the current interval has a
 *        later end than the victim, spill the current one instead.
 *   4. Pre-assign v0 -> physical reg 0 (return), v1..n_args -> reg 1..n_args.
 *
 * Key properties:
 *   - MIR is SSA: each vr is defined exactly once, so intervals are simple.
 *   - v0 is the return register (always physical reg 0).
 *   - Argument vrs (v1..n_args) are pre-assigned to physical regs 1..n_args.
 *   - If n_phys_regs is too small, some vrs spill (reg=-1, stack=offset).
 *
 * C11, self-contained.
 */

#include "wubu_mir_regalloc.h"
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Helpers: classify MIR opcodes                                      */
/* ------------------------------------------------------------------ */

static int op_has_dst(wubu_mir_op_t op)
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

static int op_num_srcs(wubu_mir_op_t op)
{
    switch (op) {
    case MIR_CONST:
    case MIR_LABEL:
    case MIR_JMP:
        return 0;
    case MIR_NEG: case MIR_NOT:
    case MIR_MOV:
    case MIR_JZ:
    case MIR_RET:
        return 1;
    default:
        return 2; /* binops */
    }
}

/* ------------------------------------------------------------------ */
/* Interval (live range) for a single virtual register               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t vr;
    int32_t  start;   /* first_def position */
    int32_t  end;     /* last_use position */
} interval_t;

static int interval_cmp(const void *a, const void *b)
{
    const interval_t *ia = (const interval_t *)a;
    const interval_t *ib = (const interval_t *)b;
    if (ia->start < ib->start) return -1;
    if (ia->start > ib->start) return 1;
    if (ia->end < ib->end) return -1;
    if (ia->end > ib->end) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Allocator                                                           */
/* ------------------------------------------------------------------ */

wubu_reg_assign_t *wubu_mir_alloc_regs(const wubu_mir_prog_t *p,
                                        int n_phys_regs,
                                        size_t *out_count)
{
    if (!p || n_phys_regs <= 0) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    size_t n_ins = p->n;

    /* ---- Step 0: find max vr in the program ---- */
    uint32_t max_vr = 0;
    for (size_t i = 0; i < n_ins; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (op_has_dst(in->op) && in->dst > max_vr) max_vr = in->dst;
        int ns = op_num_srcs(in->op);
        if (ns >= 1 && in->a > max_vr) max_vr = in->a;
        if (ns >= 2 && in->b > max_vr) max_vr = in->b;
    }
    /* Always include v0 in the result array (return register) */
    if (max_vr < 1) max_vr = 0;

    size_t n_vr = (size_t)max_vr + 1;

    /* ---- Step 1: compute live ranges [first_def, last_use] ---- */
    int32_t *first_def = (int32_t *)malloc(n_vr * sizeof(int32_t));
    int32_t *last_use  = (int32_t *)malloc(n_vr * sizeof(int32_t));
    if (!first_def || !last_use) {
        free(first_def);
        free(last_use);
        if (out_count) *out_count = 0;
        return NULL;
    }

    for (size_t v = 0; v < n_vr; v++) {
        first_def[v] = -1;
        last_use[v]  = -1;
    }

    for (size_t i = 0; i < n_ins; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (op_has_dst(in->op)) {
            if (first_def[in->dst] < 0)
                first_def[in->dst] = (int32_t)i;
            last_use[in->dst] = (int32_t)i;
        }
        int ns = op_num_srcs(in->op);
        if (ns >= 1) last_use[in->a] = (int32_t)i;
        if (ns >= 2) last_use[in->b] = (int32_t)i;
    }

    /* ---- Step 2: build sorted interval list ---- */
    int n_intervals = 0;
    for (size_t v = 0; v < n_vr; v++)
        if (first_def[v] >= 0) n_intervals++;

    interval_t *intervals = (interval_t *)malloc(
        (n_intervals > 0 ? n_intervals : 1) * sizeof(interval_t));
    if (!intervals) {
        free(first_def);
        free(last_use);
        if (out_count) *out_count = 0;
        return NULL;
    }

    int idx = 0;
    for (size_t v = 0; v < n_vr; v++) {
        if (first_def[v] >= 0) {
            intervals[idx].vr    = (uint32_t)v;
            intervals[idx].start = first_def[v];
            intervals[idx].end   = (last_use[v] >= 0) ? last_use[v] : first_def[v];
            idx++;
        }
    }

    qsort(intervals, n_intervals, sizeof(interval_t), interval_cmp);

    /* ---- Step 3: allocate and initialize result ---- */
    wubu_reg_assign_t *assign = (wubu_reg_assign_t *)calloc(n_vr, sizeof(wubu_reg_assign_t));
    if (!assign) {
        free(intervals);
        free(first_def);
        free(last_use);
        if (out_count) *out_count = 0;
        return NULL;
    }
    for (size_t v = 0; v < n_vr; v++) {
        assign[v].reg   = -1;
        assign[v].stack = 0;
    }

    /* ---- Step 4: pre-assign argument registers ---- */
    /* v1..n_args -> physical regs 0..n_args-1 (capped at 6 arg regs).
     * Note: v0 is NOT pre-assigned — it's the implicit return register
     * that the RET instruction reads from. The allocator will assign it
     * normally if it's used in the program. */
    uint32_t n_args = p->n_args;
    if (n_args > 6) n_args = 6;
    for (uint32_t a = 1; a <= n_args && a < n_vr; a++) {
        int32_t phys = (int32_t)(a - 1);  /* v1 -> reg 0, v2 -> reg 1, ... */
        if (phys < n_phys_regs) {
            assign[a].reg = phys;
        }
    }

    /* ---- Step 5: linear scan ---- */
    /* reg_vr[r] = vr currently in physical register r, or -1 */
    int32_t *reg_vr = (int32_t *)malloc(n_phys_regs * sizeof(int32_t));
    /* active[]: list of vrs currently holding a physical register */
    uint32_t *active = (uint32_t *)malloc((n_intervals + 1) * sizeof(uint32_t));
    int32_t next_spill_slot = 0;
    int active_count = 0;

    if (!reg_vr || !active) {
        free(reg_vr);
        free(active);
        free(assign);
        free(intervals);
        free(first_def);
        free(last_use);
        if (out_count) *out_count = 0;
        return NULL;
    }

    for (int r = 0; r < n_phys_regs; r++)
        reg_vr[r] = -1;

    /* Seed the active set with pre-assigned arg vrs.
     * Args -> regs 0..n_args-1.
     * Their registers are occupied until their live ranges expire. */
    for (uint32_t a = 1; a <= n_args && a < n_vr; a++) {
        int32_t phys = (int32_t)(a - 1);  /* v1 -> reg 0, v2 -> reg 1, ... */
        if (phys >= 0 && phys < n_phys_regs) {
            reg_vr[phys] = (int32_t)a;
            active[active_count++] = a;
        }
    }

    for (int i = 0; i < n_intervals; i++) {
        uint32_t vr  = intervals[i].vr;
        int32_t  pos = intervals[i].start;

        /* Skip pre-assigned vrs (args already placed above) */
        if (vr >= 1 && vr <= n_args) continue;

        /* Expire active intervals whose last_use < pos */
        int write = 0;
        for (int j = 0; j < active_count; j++) {
            uint32_t av = active[j];
            int32_t aend = (last_use[av] >= 0) ? last_use[av] : first_def[av];
            if (aend < pos) {
                int32_t ar = assign[av].reg;
                if (ar >= 0 && ar < n_phys_regs)
                    reg_vr[ar] = -1;
                continue;  /* drop from active */
            }
            active[write++] = av;
        }
        active_count = write;

        /* Find a free physical register */
        int32_t chosen = -1;
        for (int r = 0; r < n_phys_regs; r++) {
            if (reg_vr[r] < 0) {
                chosen = r;
                break;
            }
        }

        if (chosen >= 0) {
            /* Assign register */
            assign[vr].reg   = chosen;
            assign[vr].stack = 0;
            reg_vr[chosen]   = (int32_t)vr;
            active[active_count++] = vr;
        } else {
            /* Pool exhausted — find the active interval with the furthest end */
            int32_t victim_vr = -1;
            int32_t victim_end = -1;
            int victim_idx = -1;
            for (int j = 0; j < active_count; j++) {
                uint32_t av = active[j];
                int32_t ae = (last_use[av] >= 0) ? last_use[av] : first_def[av];
                if (victim_vr < 0 || ae > victim_end) {
                    victim_end = ae;
                    victim_vr = (int32_t)av;
                    victim_idx = j;
                }
            }

            int32_t cur_end = intervals[i].end;

            if (victim_vr >= 0 && victim_end > cur_end) {
                /* Spill the victim; give its register to current vr */
                int32_t vreg = assign[victim_vr].reg;
                assign[victim_vr].reg   = -1;
                assign[victim_vr].stack = -(next_spill_slot + 1) * 8;
                next_spill_slot++;

                assign[vr].reg   = vreg;
                assign[vr].stack = 0;
                reg_vr[vreg]     = (int32_t)vr;
                active[victim_idx] = vr;  /* replace victim in active */
            } else {
                /* Spill current vr */
                assign[vr].reg   = -1;
                assign[vr].stack = -(next_spill_slot + 1) * 8;
                next_spill_slot++;
            }
        }
    }

    free(active);
    free(reg_vr);
    free(intervals);
    free(first_def);
    free(last_use);

    if (out_count) *out_count = n_vr;
    return assign;
}

void wubu_mir_free_alloc(wubu_reg_assign_t *alloc)
{
    free(alloc);
}