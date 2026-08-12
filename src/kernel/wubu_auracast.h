/*
 * wubu_auracast.h -- kernel-owned Bluetooth Auracast routing.
 */
#ifndef WUBU_AURACAST_H
#define WUBU_AURACAST_H

#include <stddef.h>

void wubu_auracast_probe(void);
int  wubu_auracast_present(void);
int  wubu_auracast_streams(int active);
int  wubu_auracast_is_broadcasting(int broadcaster, int pa);
void wubu_auracast_summary(char *out, size_t cap);

#endif
