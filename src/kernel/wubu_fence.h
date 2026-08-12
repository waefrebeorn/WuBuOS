/*
 * wubu_fence.h -- kernel-owned GPU fence routing.
 */
#ifndef WUBU_FENCE_H
#define WUBU_FENCE_H

#include <stddef.h>

void wubu_fence_probe(void);
int  wubu_fence_present(void);
int  wubu_fence_timeout(void);
int  wubu_fence_signal(void);
int  wubu_fence_amd(void);
int  wubu_fence_i915(void);
const char *wubu_fence_driver(void);
const char *wubu_fence_type_for(const char *t);
const char *wubu_fence_action_for(const char *a);
int wubu_fence_summary(char *out, size_t cap);

#endif
