/*
 * wubu_nvmehotplug.h -- kernel-owned NVMe hotplug routing.
 */
#ifndef WUBU_NVMEHOTPLUG_H
#define WUBU_NVMEHOTPLUG_H

#include <stddef.h>

void wubu_nvmehotplug_probe(void);
int  wubu_nvmehotplug_present(void);
int  wubu_nvmehotplug_stable(int events, int interval_ms);
int  wubu_nvmehotplug_is_event(int prev, int curr);
void wubu_nvmehotplug_summary(char *out, size_t cap);

#endif
