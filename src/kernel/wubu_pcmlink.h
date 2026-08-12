/*
 * wubu_pcmlink.h -- kernel-owned audio PCM link routing.
 */
#ifndef WUBU_PCMLINK_H
#define WUBU_PCMLINK_H

#include <stddef.h>

void wubu_pcmlink_probe(void);
int  wubu_pcmlink_present(void);
const char *wubu_pcmlink_dir_str(int is_playback);
const char *wubu_pcmlink_state_str(int active);
int  wubu_pcmlink_channels(const char *fmt);
void wubu_pcmlink_summary(char *out, size_t cap);

#endif
