/*
 * wubu_hdr.h -- kernel-owned display HDR + audio jack detection routing.
 */
#ifndef WUBU_HDR_H
#define WUBU_HDR_H

#include <stddef.h>

/* W1: probe the HDR/jack topology. */
void wubu_hdr_probe(void);

/* W2: accessors */
int  wubu_hdr_hdr10(void);
int  wubu_hdr_hdr10p(void);
int  wubu_hdr_dv(void);
int  wubu_hdr_sink(void);
int  wubu_hdr_jack(void);
const char *wubu_hdr_driver(void);

/* W3: HDR/jack routing. */
const char *wubu_hdr_meta_for(const char *hdr);
const char *wubu_hdr_jack_for(const char *jack);

/* W4: summary fragment. */
int wubu_hdr_summary(char *out, size_t cap);

#endif /* WUBU_HDR_H */
