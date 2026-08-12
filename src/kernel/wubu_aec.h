/*
 * wubu_aec.h -- kernel-owned audio AEC + noise suppression routing.
 */
#ifndef WUBU_AEC_H
#define WUBU_AEC_H

#include <stddef.h>

/* W1: probe the AEC topology. */
void wubu_aec_probe(void);

/* W2: accessors */
int  wubu_aec_present(void);
int  wubu_aec_ns(void);
int  wubu_aec_webrtc(void);
int  wubu_aec_pw(void);
int  wubu_aec_pa(void);
const char *wubu_aec_driver(void);

/* W3: AEC routing. */
const char *wubu_aec_method_for(const char *method);
const char *wubu_aec_level_for(const char *level);

/* W4: summary fragment. */
int wubu_aec_summary(char *out, size_t cap);

#endif /* WUBU_AEC_H */
