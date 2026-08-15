/*
 * wubu_camera.c -- kernel-owned V4L2 camera/ISP driver routing.
 *
 * Cameras feed everything: video calls, streaming, computer vision, and
 * the AGI's vision. The kernel must route the camera to the right V4L2
 * driver and expose the media device topology (sensor -> ISP -> video
 * node). "Runs on everything" includes every webcam, MIPI-CSI sensor,
 * and embedded camera.
 *
 * Camera drivers:
 *   - USB webcams: uvcvideo (UVC class), /dev/video0
 *   - MIPI-CSI sensors: imx219, imx290, ov5640, ov9281 (per-sensor)
 *   - ISP: rkisp1 (Rockchip), imx8-isi (NXP), tegra-vi (NVIDIA),
 *     amlogic, csi drivers
 *   - Virtual: vimc (V4L2 virtual media controller, for testing),
 *     vivid (V4L2 virtual test driver)
 *   - capture: v4l2loopback (virtual output)
 *
 * WuBuOS owns this: detect the camera pipeline (video nodes + media
 * devices), route to the right V4L2 driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the camera frontier):
 *   - V4L2 core + media controller: /dev/videoN, /dev/media0
 *   - uvcvideo: USB Video Class (webcams)
 *   - rkisp1: Rockchip ISP (embedded/SoC cameras)
 *   - imx219/ov5640: MIPI-CSI sensor drivers
 *   - vimc/vivid: virtual media controllers (testing)
 */
#include "wubu_camera.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_v4l2 = 0;
static int  g_uvc = 0;
static int  g_isp = 0;
static int  g_mipi = 0;
static int  g_video_nodes = 0;
static int  g_media_devices = 0;
static char g_camera_drv[32] = "";

/* ---- W1: probe the camera topology ---- */
void wubu_camera_probe(void)
{
    g_v4l2 = 0; g_uvc = 0; g_isp = 0; g_mipi = 0;
    g_video_nodes = 0; g_media_devices = 0;
    g_camera_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* Count /dev/videoN nodes. */
    char path[64];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/video%d", i);
        if (access(path, R_OK) == 0) g_video_nodes++;
    }
    /* Count /dev/mediaN devices. */
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/media%d", i);
        if (access(path, R_OK) == 0) g_media_devices++;
    }
    g_v4l2 = (g_video_nodes > 0);

    /* uvcvideo loaded? */
    if (access("/sys/bus/usb/drivers/uvcvideo", R_OK) == 0) {
        g_uvc = 1;
        g_camera_drv[0] = '\0';
        strcpy(g_camera_drv, "uvcvideo");
    }
    /* ISP present (rkisp1 or media controller with subdevs)? */
    if (access("/sys/bus/platform/drivers/rkisp1", R_OK) == 0) {
        g_isp = 1;
        strcpy(g_camera_drv, "rkisp1");
    }
    if (g_media_devices > 0 && !g_isp) {
        g_isp = 1;  /* media controller implies an ISP pipeline */
        if (!g_camera_drv[0]) strcpy(g_camera_drv, "media-controller");
    }
    /* MIPI-CSI (sensor) drivers present? */
    g_mipi = (access("/sys/bus/i2c/drivers/imx219", R_OK) == 0) ||
             (access("/sys/bus/i2c/drivers/ov5640", R_OK) == 0) ||
             (access("/sys/bus/i2c/drivers/imx290", R_OK) == 0) ||
             (access("/sys/bus/i2c/drivers/ov9281", R_OK) == 0);
#endif
}

/* ---- W2: accessors ---- */
int  wubu_camera_present(void)  { return g_v4l2; }
int  wubu_camera_has_uvc(void)  { return g_uvc; }
int  wubu_camera_has_isp(void)  { return g_isp; }
int  wubu_camera_has_mipi(void) { return g_mipi; }
int  wubu_camera_video_nodes(void) { return g_video_nodes; }
int  wubu_camera_media_devices(void){ return g_media_devices; }
const char *wubu_camera_driver(void){ return g_camera_drv[0] ? g_camera_drv : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_camera_pipeline_driver(const char *device)
{
    if (!device) return NULL;
    if (strstr(device, "uvc"))      return "uvcvideo";
    if (strstr(device, "rkisp"))    return "rkisp1";
    if (strstr(device, "imx219"))   return "imx219";
    if (strstr(device, "imx290"))   return "imx290";
    if (strstr(device, "ov5640"))   return "ov5640";
    if (strstr(device, "ov9281"))   return "ov9281";
    if (strstr(device, "imx8-isi")) return "imx8-isi";
    if (strstr(device, "vimc"))     return "vimc";
    if (strstr(device, "vivid"))    return "vivid";
    return "v4l2";
}

/* ---- W4: summary ---- */
int wubu_camera_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "camera[v4l2=%d uvc=%d isp=%d mipi=%d video=%d media=%d drv=%s]",
        g_v4l2, g_uvc, g_isp, g_mipi, g_video_nodes, g_media_devices,
        wubu_camera_driver() ? wubu_camera_driver() : "none");
}
