/*
 * wubu_dspmode.h -- kernel-owned audio codec DSP modes + suspend routing.
 */
#ifndef WUBU_DSPMODE_H
#define WUBU_DSPMODE_H

#include <stddef.h>

/* W1: probe the audio DSP-mode topology. */
void wubu_dspmode_probe(void);

/* W2: accessors */
int  wubu_dspmode_sof(void);
int  wubu_dspmode_pm(void);
int  wubu_dspmode_voice_wake(void);
int  wubu_dspmode_suspend_ok(void);
const char *wubu_dspmode_driver(void);
const char *wubu_dspmode_current(void);

/* W3: DSP mode routing. */
const char *wubu_dspmode_for(const char *mode);

/* W4: summary fragment. */
int wubu_dspmode_summary(char *out, size_t cap);

#endif /* WUBU_DSPMODE_H */
