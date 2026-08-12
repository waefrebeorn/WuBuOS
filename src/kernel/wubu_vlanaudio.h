/*
 * wubu_vlanaudio.h -- kernel-owned NIC VLAN + audio DSP routing.
 */
#ifndef WUBU_VLANAUDIO_H
#define WUBU_VLANAUDIO_H

#include <stddef.h>

/* W1: probe the VLAN/audio-DSP topology. */
void wubu_vlanaudio_probe(void);

/* W2: accessors */
int  wubu_vlanaudio_vlan(void);
int  wubu_vlanaudio_vlan_offload(void);
int  wubu_vlanaudio_pipewire(void);
int  wubu_vlanaudio_alsa(void);
int  wubu_vlanaudio_sof(void);
const char *wubu_vlanaudio_dsp_driver(void);

/* W3: driver routing. */
const char *wubu_vlanaudio_vlan_for(const char *nic);
const char *wubu_vlanaudio_dsp_for(const char *dsp);

/* W4: summary fragment. */
int wubu_vlanaudio_summary(char *out, size_t cap);

#endif /* WUBU_VLANAUDIO_H */
