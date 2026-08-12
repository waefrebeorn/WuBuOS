/*
 * wubu_compress.h -- kernel-owned storage transparent compression routing.
 */
#ifndef WUBU_COMPRESS_H
#define WUBU_COMPRESS_H

#include <stddef.h>

/* W1: probe the compression topology. */
void wubu_compress_probe(void);

/* W2: accessors */
int  wubu_compress_present(void);
int  wubu_compress_btrfs(void);
int  wubu_compress_zfs(void);
int  wubu_compress_zstd(void);
int  wubu_compress_lz4(void);
const char *wubu_compress_driver(void);

/* W3: compression routing. */
const char *wubu_compress_algo_for(const char *algo);
const char *wubu_compress_mode_for(const char *mode);

/* W4: summary fragment. */
int wubu_compress_summary(char *out, size_t cap);

#endif /* WUBU_COMPRESS_H */
