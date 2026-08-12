/*
 * wubu_flush2.h -- kernel-owned storage flush barrier routing.
 */
#ifndef WUBU_FLUSH2_H
#define WUBU_FLUSH2_H

#include <stddef.h>

void wubu_flush2_probe(void);
int  wubu_flush2_present(void);
int  wubu_flush2_barrier(void);
int  wubu_flush2_fsync(void);
int  wubu_flush2_cache(void);
int  wubu_flush2_flushcmd(void);
const char *wubu_flush2_driver(void);
const char *wubu_flush2_type_for(const char *t);
const char *wubu_flush2_cmd_for(const char *c);
int wubu_flush2_summary(char *out, size_t cap);

#endif
