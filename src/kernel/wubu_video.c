/*
 * wubu_video.c -- kernel-owned video encode/decode (VA-API / codec) routing.
 *
 * Hardware video acceleration offloads H.264/H.265/VP9/AV1 encode + decode
 * to the GPU or a dedicated media block. "Runs on everything" includes
 * smooth playback + streaming. The kernel must route the codec engine to
 * the right VA-API driver and expose the supported codec set.
 *
 * VA-API drivers (by GPU vendor):
 *   - Intel: i965/iHD (intel-media-driver) for HD Graphics + Arc; libva
 *   - AMD: radeonsi (mesa VAAPI) / amdgpu
 *   - NVIDIA: vdpau via nvidia-vdpau-driver (VDPAU), NVDEC via vaapi
 *   - Qualcomm: venus (v4l2 m2m) for Snapdragon
 *   - Broadcom: rpi-hw-accel (Raspberry Pi)
 *   - Allwinner/Amlogic/Rockchip: v4l2 m2m (media codec)
 *
 * Codecs: H.264 (avc), H.265 (hevc), VP8, VP9, AV1 (4K/8K), MJPEG
 *
 * WuBuOS owns this: detect the video codec engine (VA-API/v4l2 m2m),
 * route to the right driver, and expose the codec topology.
 *
 * Research (Kevin-Bacon 7-hop on the video-codec frontier):
 *   - VA-API: libva + intel-media-driver (iHD) / i965 / radeonsi
 *   - VDPAU: NVIDIA vdpau driver
 *   - v4l2 m2m: media codec for embedded (hantro, cedrus, rkvdec, venus)
 *   - AV1: the modern royalty-free codec (Intel Arc, AMD RDNA2+)
 */
#include "wubu_video.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_vaapi = 0;
static int  g_vdpau = 0;
static int  g_v4l2_m2m = 0;
static int  g_av1 = 0;      /* AV1 decode/encode */
static int  g_hevc = 0;     /* H.265 */
static int  g_vp9 = 0;
static char g_video_drv[32] = "";
static char g_video_engine[32] = "";

/* ---- W1: probe the video codec topology ---- */
void wubu_video_probe(void)
{
    g_vaapi = 0; g_vdpau = 0; g_v4l2_m2m = 0;
    g_av1 = 0; g_hevc = 0; g_vp9 = 0;
    g_video_drv[0] = '\0'; g_video_engine[0] = '\0';

#ifdef WUBU_HOSTED
    /* VA-API drivers present? */
    if (access("/usr/lib/x86_64-linux-gnu/dri/iHD_drv_video.so", R_OK) == 0 ||
        access("/usr/lib/x86_64-linux-gnu/dri/i965_drv_video.so", R_OK) == 0 ||
        access("/usr/lib/x86_64-linux-gnu/dri/radeonsi_drv_video.so", R_OK) == 0) {
        g_vaapi = 1;
        strcpy(g_video_drv, "vaapi");
        if (access("/usr/lib/x86_64-linux-gnu/dri/iHD_drv_video.so", R_OK) == 0)
            strcpy(g_video_engine, "Intel iHD");
        else if (access("/usr/lib/x86_64-linux-gnu/dri/i965_drv_video.so", R_OK) == 0)
            strcpy(g_video_engine, "Intel i965");
        else
            strcpy(g_video_engine, "radeonsi");
    }
    /* VDPAU (NVIDIA) present? */
    if (access("/usr/lib/x86_64-linux-gnu/libvdpau_nvidia.so", R_OK) == 0 ||
        access("/usr/lib/libvdpau_nvidia.so", R_OK) == 0) {
        g_vdpau = 1;
        if (!g_video_drv[0]) { strcpy(g_video_drv, "vdpau"); strcpy(g_video_engine, "NVIDIA"); }
    }
    /* v4l2 m2m codec (embedded)? */
    if (access("/sys/bus/platform/drivers/rkvdec", R_OK) == 0 ||
        access("/sys/bus/platform/drivers/hantro", R_OK) == 0 ||
        access("/sys/bus/platform/drivers/cedrus", R_OK) == 0 ||
        access("/sys/bus/platform/drivers/venus", R_OK) == 0) {
        g_v4l2_m2m = 1;
        if (!g_video_drv[0]) { strcpy(g_video_drv, "v4l2-m2m"); strcpy(g_video_engine, "media codec"); }
    }
    /* Codec support: modern GPUs all do AV1/HEVC/VP9. */
    if (g_vaapi || g_v4l2_m2m) { g_av1 = 1; g_hevc = 1; g_vp9 = 1; }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_video_vaapi(void)     { return g_vaapi; }
int  wubu_video_vdpau(void)     { return g_vdpau; }
int  wubu_video_v4l2_m2m(void)  { return g_v4l2_m2m; }
int  wubu_video_av1(void)       { return g_av1; }
int  wubu_video_hevc(void)      { return g_hevc; }
int  wubu_video_vp9(void)       { return g_vp9; }
int  wubu_video_present(void)   { return (g_vaapi || g_vdpau || g_v4l2_m2m); }
const char *wubu_video_driver(void){ return g_video_drv[0] ? g_video_drv : NULL; }
const char *wubu_video_engine(void){ return g_video_engine[0] ? g_video_engine : NULL; }

/* ---- W3: codec driver routing ---- */
const char *wubu_video_driver_for(const char *gpu)
{
    if (!gpu) return NULL;
    if (strstr(gpu, "intel"))  return "iHD";
    if (strstr(gpu, "amd") || strstr(gpu, "radeon")) return "radeonsi";
    if (strstr(gpu, "nvidia") || strstr(gpu, "nouveau")) return "vdpau";
    if (strstr(gpu, "qualcomm") || strstr(gpu, "venus")) return "venus";
    if (strstr(gpu, "rockchip") || strstr(gpu, "rkvdec")) return "rkvdec";
    if (strstr(gpu, "broadcom") || strstr(gpu, "rpi")) return "rpi-hw-accel";
    return "v4l2-m2m";
}

/* ---- W4: summary ---- */
int wubu_video_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "video[vaapi=%d vdpau=%d m2m=%d av1=%d hevc=%d vp9=%d drv=%s eng=%s]",
        g_vaapi, g_vdpau, g_v4l2_m2m, g_av1, g_hevc, g_vp9,
        wubu_video_driver() ? wubu_video_driver() : "none",
        wubu_video_engine() ? wubu_video_engine() : "-");
}
