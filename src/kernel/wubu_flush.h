/*
 * wubu_flush.h -- kernel-owned storage cache flush/barrier routing.
 */
#ifndef WUBU_FLUSH_H
#define WUBU_FLUSH_H

#include <stddef.h>

/* W1: probe the flush topology. */
void wubu_flush_probe(void);

/* W2: accessors */
int  wubu_flush_supported(void);
int  wubu_flush_barrier(void);
int  wubu_flush_wbcache(void);
int  wubu_flush_fsync(void);
int  wubu_flush_nvme(void);
const char *wubu_flush_driver(void);

/* W3: flush routing. */
const char *wubu_flush_mode_for(const char *mode);
const char *wubu_flush_op_for(const char *op);

/* W4: summary fragment. */
int wubu_flush_summary(char *out, size_t cap);

#endif /* WUBU_FLUSH_H */
