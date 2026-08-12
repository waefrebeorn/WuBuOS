/*
 * wubu_spdif.h -- kernel-owned audio SPDIF/HDMI passthrough routing.
 */
#ifndef WUBU_SPDIF_H
#define WUBU_SPDIF_H

#include <stddef.h>

/* W1: probe the SPDIF topology. */
void wubu_spdif_probe(void);

/* W2: accessors */
int  wubu_spdif_present(void);
int  wubu_spdif_hdmi(void);
int  wubu_spdif_iec61937(void);
int  wubu_spdif_passthru(void);
int  wubu_spdif_i2s(void);
const char *wubu_spdif_driver(void);

/* W3: SPDIF routing. */
const char *wubu_spdif_codec_for(const char *codec);
const char *wubu_spdif_fmt_for(const char *fmt);

/* W4: summary fragment. */
int wubu_spdif_summary(char *out, size_t cap);

#endif /* WUBU_SPDIF_H */
