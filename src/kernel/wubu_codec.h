/*
 * wubu_codec.h -- kernel-owned audio codec/DSP driver routing.
 */
#ifndef WUBU_CODEC_H
#define WUBU_CODEC_H

#include <stddef.h>

/* W1: probe the audio codec topology. */
void wubu_codec_probe(void);

/* W2: accessors */
int  wubu_codec_present(void);
int  wubu_codec_has_hda(void);
int  wubu_codec_has_asoc(void);
int  wubu_codec_has_sof_dsp(void);
const char *wubu_codec_driver(void);
const char *wubu_codec_name(void);

/* W3: codec driver routing. */
const char *wubu_codec_hda_driver(const char *vendor);
const char *wubu_codec_asoc_driver(const char *chip);

/* W4: summary fragment. */
int wubu_codec_summary(char *out, size_t cap);

#endif /* WUBU_CODEC_H */
