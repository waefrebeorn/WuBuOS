/*
 * wubu_metal_audio.h -- Audio backend module interface.
 *
 * Split from wubu_metal.c (the ALSA/Pulse/PipeWire backends).
 * The wubu_audio_* dispatch in wubu_metal.c calls these; this
 * header is the compiler-enforced boundary between the two TUs.
 * C11 opaque-struct: depends only on wubu_metal.h public types.
 */
#ifndef WUBU_METAL_AUDIO_H
#define WUBU_METAL_AUDIO_H

#include "wubu_metal.h"   /* WubuAudio, AUDIO_* enum, g_audio extern */

/* Backend entry points (real + dlopen-stub variants).
 * Declared non-static so wubu_metal.c's wubu_audio_* dispatch
 * links against this separate TU. No other TU should call these. */
int  wubu_alsa_init(int sample_rate, int channels, int buffer_frames);
void wubu_alsa_shutdown(void);
void wubu_alsa_submit(const float *buf, int frames);
double wubu_alsa_cpu_load(void);

int  wubu_pulse_init(int sample_rate, int channels, int buffer_frames);
void wubu_pulse_shutdown(void);
void wubu_pulse_submit(const float *buf, int frames);
double wubu_pulse_cpu_load(void);

int  wubu_pipewire_init(int sample_rate, int channels, int buffer_frames);
void wubu_pipewire_shutdown(void);
void wubu_pipewire_submit(const float *buf, int frames);
double wubu_pipewire_cpu_load(void);

#endif /* WUBU_METAL_AUDIO_H */
