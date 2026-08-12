/*
 * wubu_codecgraph.h -- kernel-owned audio codec graph routing.
 */
#ifndef WUBU_CODECGRAPH_H
#define WUBU_CODECGRAPH_H

#include <stddef.h>

/* W1: probe the codec-graph topology. */
void wubu_codecgraph_probe(void);

/* W2: accessors */
int  wubu_codecgraph_present(void);
int  wubu_codecgraph_graph(void);
int  wubu_codecgraph_amp(void);
int  wubu_codecgraph_widgets(void);
int  wubu_codecgraph_dapm(void);
const char *wubu_codecgraph_driver(void);

/* W3: codec-graph routing. */
const char *wubu_codecgraph_widget_for(const char *w);
const char *wubu_codecgraph_verb_for(const char *verb);

/* W4: summary fragment. */
int wubu_codecgraph_summary(char *out, size_t cap);

#endif /* WUBU_CODECGRAPH_H */
