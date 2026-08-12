/*
 * wubu_loudness.h -- kernel-owned audio loudness normalization routing.
 */
#ifndef WUBU_LOUDNESS_H
#define WUBU_LOUDNESS_H

#include <stddef.h>

/* W1: probe the loudness topology. */
void wubu_loudness_probe(void);

/* W2: accessors */
int  wubu_loudness_present(void);
int  wubu_loudness_replaygain(void);
int  wubu_loudness_r128(void);
int  wubu_loudness_lufs(void);
int  wubu_loudness_pw(void);
const char *wubu_loudness_driver(void);

/* W3: loudness routing. */
const char *wubu_loudness_mode_for(const char *mode);
const char *wubu_loudness_target_for(const char *target);

/* W4: summary fragment. */
int wubu_loudness_summary(char *out, size_t cap);

#endif /* WUBU_LOUDNESS_H */
