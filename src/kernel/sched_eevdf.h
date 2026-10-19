/* sched_eevdf.h -- EEVDF policy, host-testable (see sched_eevdf.c). */
#ifndef WUBU_SCHED_EEVDF_H
#define WUBU_SCHED_EEVDF_H

#include <stdint.h>

#define EEVDF_MAX_TASKS  16
#define EEVDF_NICE_0     1024u
#define EEVDF_BASE_SLICE 4u

typedef struct {
    int      id;
    uint32_t weight;
    uint32_t slice;
    uint64_t vruntime;
    uint64_t deadline;
    uint64_t ran;
    int      eligible;
} eevdf_task_t;

typedef struct {
    eevdf_task_t tasks[EEVDF_MAX_TASKS];
    int          count;
    uint64_t     vclock;
    uint64_t     min_vruntime;
} eevdf_rq_t;

void    eevdf_init(eevdf_rq_t *rq);
int     eevdf_add(eevdf_rq_t *rq, int id, uint32_t weight);
int     eevdf_pick(const eevdf_rq_t *rq);          /* index, or -1 */
void    eevdf_charge(eevdf_rq_t *rq, int index, uint32_t ticks);
int64_t eevdf_lag(const eevdf_rq_t *rq, int index);

#endif
