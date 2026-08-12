/*
 * wubu_ioprio.h -- kernel-owned storage I/O priority routing.
 */
#ifndef WUBU_IOPRIO_H
#define WUBU_IOPRIO_H

#include <stddef.h>

void wubu_ioprio_probe(void);
int  wubu_ioprio_present(void);
int  wubu_ioprio_rt(void);
int  wubu_ioprio_be(void);
int  wubu_ioprio_idle(void);
int  wubu_ioprio_sched(void);
const char *wubu_ioprio_driver(void);
const char *wubu_ioprio_class_for(const char *c);
const char *wubu_ioprio_sched_for(const char *s);
int wubu_ioprio_summary(char *out, size_t cap);

#endif
