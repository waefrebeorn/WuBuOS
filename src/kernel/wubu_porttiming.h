/*
 * wubu_porttiming.h -- kernel-owned display port timing routing.
 */
#ifndef WUBU_PORTTIMING_H
#define WUBU_PORTTIMING_H

#include <stddef.h>

/* W1: probe the port-timing topology. */
void wubu_porttiming_probe(void);

/* W2: accessors */
int  wubu_porttiming_mode(void);
int  wubu_porttiming_cvt(void);
int  wubu_porttiming_rb(void);
int  wubu_porttiming_link(void);
int  wubu_porttiming_preferred(void);
const char *wubu_porttiming_driver(void);

/* W3: port-timing routing. */
const char *wubu_porttiming_std_for(const char *std);
const char *wubu_porttiming_link_for(const char *link);

/* W4: summary fragment. */
int wubu_porttiming_summary(char *out, size_t cap);

#endif /* WUBU_PORTTIMING_H */
