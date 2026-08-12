/*
 * wubu_dappath.h -- kernel-owned audio DAPM path routing.
 */
#ifndef WUBU_DAPPATH_H
#define WUBU_DAPPATH_H

#include <stddef.h>

void wubu_dappath_probe(void);
int  wubu_dappath_present(void);
int  wubu_dappath_pb(void);
int  wubu_dappath_cap(void);
int  wubu_dappath_mux(void);
int  wubu_dappath_mix(void);
const char *wubu_dappath_driver(void);
const char *wubu_dappath_type_for(const char *t);
const char *wubu_dappath_widget_for(const char *w);
int wubu_dappath_summary(char *out, size_t cap);

#endif
