/*
 * wubu_fusefs.h -- kernel-owned storage FUSE routing.
 */
#ifndef WUBU_FUSEFS_H
#define WUBU_FUSEFS_H

#include <stddef.h>

void wubu_fusefs_probe(void);
int  wubu_fusefs_present(void);
int  wubu_fusefs_dev(void);
int  wubu_fusefs_mount(void);
int  wubu_fusefs_ctl(void);
int  wubu_fusefs_conn(void);
const char *wubu_fusefs_driver(void);
const char *wubu_fusefs_impl_for(const char *i);
const char *wubu_fusefs_op_for(const char *o);
int wubu_fusefs_summary(char *out, size_t cap);

#endif
