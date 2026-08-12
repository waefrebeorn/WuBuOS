/*
 * wubu_ns.h -- kernel-owned NVMe namespace/multipath routing.
 */
#ifndef WUBU_NS_H
#define WUBU_NS_H

#include <stddef.h>

/* W1: probe the namespace topology. */
void wubu_ns_probe(void);

/* W2: accessors */
int  wubu_ns_nvme(void);
int  wubu_ns_namespace(void);
int  wubu_ns_multipath(void);
int  wubu_ns_ana(void);
int  wubu_ns_cli(void);
const char *wubu_ns_driver(void);

/* W3: namespace routing. */
const char *wubu_ns_path_for(const char *mode);
const char *wubu_ns_state_for(const char *state);

/* W4: summary fragment. */
int wubu_ns_summary(char *out, size_t cap);

#endif /* WUBU_NS_H */
