/*
 * wubu_video.h -- kernel-owned video encode/decode (VA-API/codec) routing.
 */
#ifndef WUBU_VIDEO_H
#define WUBU_VIDEO_H

#include <stddef.h>

/* W1: probe the video codec topology. */
void wubu_video_probe(void);

/* W2: accessors */
int  wubu_video_vaapi(void);
int  wubu_video_vdpau(void);
int  wubu_video_v4l2_m2m(void);
int  wubu_video_av1(void);
int  wubu_video_hevc(void);
int  wubu_video_vp9(void);
int  wubu_video_present(void);
const char *wubu_video_driver(void);
const char *wubu_video_engine(void);

/* W3: codec driver routing. */
const char *wubu_video_driver_for(const char *gpu);

/* W4: summary fragment. */
int wubu_video_summary(char *out, size_t cap);

#endif /* WUBU_VIDEO_H */
