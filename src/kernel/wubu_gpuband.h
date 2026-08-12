/*
 * wubu_gpuband.h -- kernel-owned GPU scheduler priority bands routing.
 */
#ifndef WUBU_GPUBAND_H
#define WUBU_GPUBAND_H

#include <stddef.h>

/* W1: probe the GPU-band topology. */
void wubu_gpuband_probe(void);

/* W2: accessors */
int  wubu_gpuband_present(void);
int  wubu_gpuband_fair(void);
int  wubu_gpuband_prio(void);
int  wubu_gpuband_entity(void);
int  wubu_gpuband_stats(void);
const char *wubu_gpuband_driver(void);

/* W3: GPU-band routing. */
const char *wubu_gpuband_prio_for(const char *p);
const char *wubu_gpuband_class_for(const char *c);

/* W4: summary fragment. */
int wubu_gpuband_summary(char *out, size_t cap);

#endif /* WUBU_GPUBAND_H */
