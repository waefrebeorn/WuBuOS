/*
 * wubu_mixgraph.h -- kernel-owned audio mixing graph routing.
 */
#ifndef WUBU_MIXGRAPH_H
#define WUBU_MIXGRAPH_H

#include <stddef.h>

/* W1: probe the audio graph topology. */
void wubu_mixgraph_probe(void);

/* W2: accessors */
int  wubu_mixgraph_pipewire(void);
int  wubu_mixgraph_pulse(void);
int  wubu_mixgraph_jack(void);
int  wubu_mixgraph_alsa(void);
int  wubu_mixgraph_wireplumber(void);
const char *wubu_mixgraph_driver(void);

/* W3: graph driver routing. */
const char *wubu_mixgraph_driver_for(const char *graph);

/* W4: summary fragment. */
int wubu_mixgraph_summary(char *out, size_t cap);

#endif /* WUBU_MIXGRAPH_H */
