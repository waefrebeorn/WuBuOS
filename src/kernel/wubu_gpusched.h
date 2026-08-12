/*
 * wubu_gpusched.h -- kernel-owned GPU compute scheduler routing.
 */
#ifndef WUBU_GPUSCHED_H
#define WUBU_GPUSCHED_H

#include <stddef.h>

/* W1: probe the GPU-sched topology. */
void wubu_gpusched_probe(void);

/* W2: accessors */
int  wubu_gpusched_present(void);
int  wubu_gpusched_guc(void);
int  wubu_gpusched_prio(void);
int  wubu_gpusched_preempt(void);
int  wubu_gpusched_fair(void);
const char *wubu_gpusched_driver(void);

/* W3: GPU-sched routing. */
const char *wubu_gpusched_prio_for(const char *prio);
const char *wubu_gpusched_class_for(const char *cls);

/* W4: summary fragment. */
int wubu_gpusched_summary(char *out, size_t cap);

#endif /* WUBU_GPUSCHED_H */
