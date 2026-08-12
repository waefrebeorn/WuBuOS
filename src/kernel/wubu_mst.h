/*
 * wubu_mst.h -- kernel-owned DisplayPort MST + audio SRC routing.
 */
#ifndef WUBU_MST_H
#define WUBU_MST_H

#include <stddef.h>

/* W1: probe the MST/SRC topology. */
void wubu_mst_probe(void);

/* W2: accessors */
int  wubu_mst_dp(void);
int  wubu_mst_topology(void);
int  wubu_mst_dsc(void);
int  wubu_mst_src(void);
int  wubu_mst_resample(void);
const char *wubu_mst_driver(void);

/* W3: MST/SRC routing. */
const char *wubu_mst_payload_for(const char *mode);
const char *wubu_mst_src_for(const char *rate);

/* W4: summary fragment. */
int wubu_mst_summary(char *out, size_t cap);

#endif /* WUBU_MST_H */
