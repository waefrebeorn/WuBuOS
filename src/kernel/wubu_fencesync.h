/*
 * wubu_fencesync.h -- kernel-owned GPU sync/fence routing.
 */
#ifndef WUBU_FENCESYNC_H
#define WUBU_FENCESYNC_H

#include <stddef.h>

void wubu_fencesync_probe(void);
int  wubu_fencesync_present(void);
int  wubu_fencesync_timeline(void);
int  wubu_fencesync_wait(void);
int  wubu_fencesync_timeout(void);
int  wubu_fencesync_signal(void);
const char *wubu_fencesync_driver(void);
const char *wubu_fencesync_op_for(const char *o);
const char *wubu_fencesync_type_for(const char *t);
int wubu_fencesync_summary(char *out, size_t cap);

#endif
