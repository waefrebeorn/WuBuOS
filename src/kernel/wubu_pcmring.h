/*
 * wubu_pcmring.h -- kernel-owned audio PCM ring buffer routing.
 */
#ifndef WUBU_PCMRING_H
#define WUBU_PCMRING_H

#include <stddef.h>

void wubu_pcmring_probe(void);
int  wubu_pcmring_present(void);
const char *wubu_pcmring_format_for(const char *ext);
int  wubu_pcmring_latency_us(int rate, int period, int buf);
void wubu_pcmring_summary(char *out, size_t cap);

#endif
