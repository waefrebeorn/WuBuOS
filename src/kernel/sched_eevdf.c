/*
 * sched_eevdf.c -- EEVDF runqueue policy (earliest eligible virtual deadline
 * first), separable from context switching so it can be tested on the host.
 *
 * Each task has a weight, a virtual runtime and a slice. A task is eligible
 * when its vruntime has caught up with the runqueue's virtual clock; among the
 * eligible set the earliest virtual deadline wins. Lag is vruntime minus the
 * weighted fair share, and is the quantity the scheduler is supposed to bound.
 */
#include "sched_eevdf.h"

#include <string.h>

void eevdf_init(eevdf_rq_t *rq)
{
    memset(rq, 0, sizeof *rq);
    rq->vclock = 0;
    rq->min_vruntime = 0;
}

int eevdf_add(eevdf_rq_t *rq, int id, uint32_t weight)
{
    if (!rq || rq->count >= EEVDF_MAX_TASKS || weight == 0) return -1;
    eevdf_task_t *t = &rq->tasks[rq->count++];
    memset(t, 0, sizeof *t);
    t->id = id;
    t->weight = weight;
    t->vruntime = rq->min_vruntime;      /* join at the clock, not behind it */
    t->slice = EEVDF_BASE_SLICE * EEVDF_NICE_0 / weight;
    if (t->slice < 1) t->slice = 1;
    t->deadline = t->vruntime + t->slice;
    t->eligible = 1;
    return 0;
}

/* Weighted share: a task of weight w out of total W is entitled to w/W of the
 * virtual clock's progress. */
static double total_weight(const eevdf_rq_t *rq)
{
    double w = 0.0;
    for (int i = 0; i < rq->count; i++)
        if (rq->tasks[i].weight) w += (double)rq->tasks[i].weight;
    return w;
}

int eevdf_pick(const eevdf_rq_t *rq)
{
    if (!rq || rq->count == 0) return -1;

    int best = -1;
    uint64_t best_dl = ~0ull;
    for (int i = 0; i < rq->count; i++) {
        const eevdf_task_t *t = &rq->tasks[i];
        if (!t->eligible) continue;
        if (t->deadline < best_dl ||
            (t->deadline == best_dl && best >= 0 && t->vruntime < rq->tasks[best].vruntime)) {
            best_dl = t->deadline;
            best = i;
        }
    }
    return best;
}

void eevdf_charge(eevdf_rq_t *rq, int index, uint32_t ticks)
{
    if (!rq || index < 0 || index >= rq->count) return;

    eevdf_task_t *t = &rq->tasks[index];
    /* Virtual time advances inversely with weight. */
    uint64_t delta = (uint64_t)ticks * EEVDF_NICE_0 / t->weight;
    t->vruntime += delta;
    t->ran += ticks;
    rq->vclock += delta;

    if (t->vruntime > rq->min_vruntime) rq->min_vruntime = t->vruntime;
    t->deadline = t->vruntime + t->slice;

    /* Eligibility: a task that has run past its fair share waits until the
     * clock catches up; one that is behind is eligible again. */
    for (int i = 0; i < rq->count; i++) {
        eevdf_task_t *u = &rq->tasks[i];
        u->eligible = (u->vruntime <= rq->min_vruntime);
        if (!u->eligible && u->deadline < rq->min_vruntime)
            u->deadline = rq->min_vruntime;   /* don't leave stale deadlines */
    }
}

int64_t eevdf_lag(const eevdf_rq_t *rq, int index)
{
    if (!rq || index < 0 || index >= rq->count) return 0;

    double W = total_weight(rq);
    if (W <= 0.0) return 0;

    const eevdf_task_t *t = &rq->tasks[index];
    /* Ideal service = share of the total virtual progress. */
    double ideal = (double)rq->vclock * (double)t->weight / W;
    return (int64_t)t->vruntime - (int64_t)ideal;
}
