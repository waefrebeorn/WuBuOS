/*
 * wubu_audio.h -- kernel-owned audio driver routing interface.
 *
 * The kernel detects all audio devices (HDA, HDMI, USB, Bluetooth A2DP)
 * and generates the PipeWire/WirePlumber/ALSA config that fixes the gaps
 * Linux audio has natively. The user never touches pavucontrol.
 */
#ifndef WUBU_AUDIO_H
#define WUBU_AUDIO_H

/* W1: probe all audio controllers via PCI scan. Call from wubu_hw_detect(). */
void wubu_audio_probe(void);

/* W2: accessors */
int          wubu_audio_present(void);  /* any audio device found */
int          wubu_audio_is_hdmi(void);   /* HDMI/DisplayPort audio active */
int          wubu_audio_is_usb(void);    /* USB audio class present */
int          wubu_audio_has_bt(void);    /* Bluetooth A2DP sink active */
const char *wubu_audio_driver(void);     /* "snd_sof" | "snd_hda_intel" | ... */
const char *wubu_audio_path(void);       /* "/dev/snd/hwC0D0" or NULL */

/* W3: PipeWire/WirePlumber config string (write to pipewire.conf.d). */
const char *wubu_audio_pipewire_config(void);

/* W4: Bluetooth A2DP codec + latency config (write to wireplumber.d). */
const char *wubu_audio_bt_config(void);

/* W5: ALSA mixer state template. */
const char *wubu_audio_alsa_state(void);

/* Bluetooth A2DP sink detection. */
int wubu_audio_has_bt_a2dp(void);

#endif /* WUBU_AUDIO_H */
