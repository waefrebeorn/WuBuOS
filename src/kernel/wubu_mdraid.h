/*
 * wubu_mdraid.h -- kernel-owned storage MD RAID routing.
 */
#ifndef WUBU_MDRAID_H
#define WUBU_MDRAID_H

#include <stddef.h>

void wubu_mdraid_probe(void);
int  wubu_mdraid_present(void);
const char *wubu_mdraid_level_str(int level);
int  wubu_mdraid_degraded(int total, int active);
void wubu_mdraid_summary(char *out, size_t cap);

#endif
