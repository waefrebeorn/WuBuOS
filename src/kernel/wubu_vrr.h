/*
 * wubu_vrr.h -- kernel-owned G-Sync/FreeSync variable refresh routing.
 */
#ifndef WUBU_VRR_H
#define WUBU_VRR_H

#include <stddef.h>

void wubu_vrr_probe(void);
int  wubu_vrr_present(void);
int  wubu_vrr_is_freesync(int freesync_available);
int  wubu_vrr_is_gsync(int gsync_available);
void wubu_vrr_summary(char *out, size_t cap);

#endif
