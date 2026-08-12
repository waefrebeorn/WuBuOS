/*
 * wubu_camera.h -- kernel-owned V4L2 camera/ISP driver routing.
 */
#ifndef WUBU_CAMERA_H
#define WUBU_CAMERA_H

#include <stddef.h>

/* W1: probe the camera topology. */
void wubu_camera_probe(void);

/* W2: accessors */
int  wubu_camera_present(void);
int  wubu_camera_has_uvc(void);
int  wubu_camera_has_isp(void);
int  wubu_camera_has_mipi(void);
int  wubu_camera_video_nodes(void);
int  wubu_camera_media_devices(void);
const char *wubu_camera_driver(void);

/* W3: pipeline driver routing. */
const char *wubu_camera_pipeline_driver(const char *device);

/* W4: summary fragment. */
int wubu_camera_summary(char *out, size_t cap);

#endif /* WUBU_CAMERA_H */
