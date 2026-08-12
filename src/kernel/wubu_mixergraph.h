/*
 * wubu_mixergraph.h -- kernel-owned audio mixer graph routing.
 */
#ifndef WUBU_MIXERGRAPH_H
#define WUBU_MIXERGRAPH_H

#include <stddef.h>

void wubu_mixergraph_probe(void);
int  wubu_mixergraph_present(void);
int  wubu_mixergraph_pb(void);
int  wubu_mixergraph_cap(void);
int  wubu_mixergraph_mon(void);
int  wubu_mixergraph_groups(void);
const char *wubu_mixergraph_driver(void);
const char *wubu_mixergraph_path_for(const char *p);
const char *wubu_mixergraph_group_for(const char *g);
int wubu_mixergraph_summary(char *out, size_t cap);

#endif
