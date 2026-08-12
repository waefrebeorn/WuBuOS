/*
 * wubu_computectx.h -- kernel-owned GPU compute context routing.
 */
#ifndef WUBU_COMPUTECTX_H
#define WUBU_COMPUTECTX_H

#include <stddef.h>

void wubu_computectx_probe(void);
int  wubu_computectx_present(void);
int  wubu_computectx_kfd(void);
int  wubu_computectx_queue(void);
int  wubu_computectx_opencl(void);
int  wubu_computectx_cuda(void);
const char *wubu_computectx_driver(void);
const char *wubu_computectx_queue_for(const char *q);
const char *wubu_computectx_prio_for(const char *p);
int wubu_computectx_summary(char *out, size_t cap);

#endif
