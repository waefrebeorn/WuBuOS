/*
 * wubu_dspgraph.h -- kernel-owned audio DSP graph routing.
 */
#ifndef WUBU_DSPGRAPH_H
#define WUBU_DSPGRAPH_H

#include <stddef.h>

void wubu_dspgraph_probe(void);
int  wubu_dspgraph_present(void);
int  wubu_dspgraph_dapm(void);
int  wubu_dspgraph_widget(void);
int  wubu_dspgraph_path(void);
int  wubu_dspgraph_route(void);
const char *wubu_dspgraph_driver(void);
const char *wubu_dspgraph_widget_for(const char *w);
const char *wubu_dspgraph_path_for(const char *p);
int wubu_dspgraph_summary(char *out, size_t cap);

#endif
