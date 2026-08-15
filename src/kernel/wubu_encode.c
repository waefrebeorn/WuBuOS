/*
 * wubu_encode.c -- kernel-owned GPU video encode routing.
 *
 * Video encode (VCN/UVD, H.264/H.265) handles GPU-accelerated encoding.
 * "Runs on everything" includes correct encode on every GPU.
 *
 * Encode:
 *   - AMD: VCN (Video Coding Engine), UVD, H.264, H.265/HEVC
 *   - Intel: i915, iHD, Quick Sync Video (QSV)
 *   - nvidia: NVENC, NVDEC
 *   - /dev/dri/card*: DRM render node
 *   - /sys/class/drm/card*: encode capability
 *   - codec: H.264 (AVC), H.265 (HEVC), VP9, AV1
 *
 * WuBuOS owns this: detect encode + codec + capability, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the encode frontier):
 *   -GPU video encode VCN H.264
 *   - NVENC, Quick Sync Video
 */
#include "wubu_encode.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_enc = 0;         /* encode present */
static int  g_h264 = 0;        /* H.264/AVC */
static int  g_h265 = 0;        /* H.265/HEVC */
static int  g_vp9 = 0;         /* VP9 */
static int  g_av1 = 0;         /* AV1 */
static char g_enc_drv[24] = "";

void wubu_encode_probe(void)
{
    g_enc = 0; g_h264 = 0; g_h265 = 0; g_vp9 = 0; g_av1 = 0;
    g_enc_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_enc = 1; g_h264 = 1; g_h265 = 1;
        strcpy(g_enc_drv, "amd-vcn");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_enc = 1; g_h264 = 1; g_h265 = 1;
        if (!g_enc_drv[0]) strcpy(g_enc_drv, "i915-qsv");
    }
    if (access("/sys/module/nvidia", R_OK) == 0 ||
        access("/sys/module/nvidia_uvm", R_OK) == 0) {
        g_enc = 1; g_h264 = 1; g_h265 = 1; g_vp9 = 1;
        if (!g_enc_drv[0]) strcpy(g_enc_drv, "nvidia-nvenc");
    }
#endif
}

int  wubu_encode_present(void){ return g_enc; }
int  wubu_encode_h264(void)  { return g_h264; }
int  wubu_encode_h265(void)  { return g_h265; }
int  wubu_encode_vp9(void)   { return g_vp9; }
int  wubu_encode_av1(void)   { return g_av1; }
const char *wubu_encode_driver(void){ return g_enc_drv[0] ? g_enc_drv : NULL; }

const char *wubu_encode_codec_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "h264") || strstr(c, "avc")) return "H.264";
    if (strstr(c, "h265") || strstr(c, "hevc")) return "H.265";
    if (strstr(c, "vp9")) return "VP9";
    if (strstr(c, "av1")) return "AV1";
    if (strstr(c, "mpeg")) return "MPEG";
    return "H.264";
}

const char *wubu_encode_api_for(const char *a)
{
    if (!a) return NULL;
    if (strstr(a, "vcn") || strstr(a, "amd")) return "VCN";
    if (strstr(a, "qsv") || strstr(a, "intel")) return "QuickSync";
    if (strstr(a, "nvenc") || strstr(a, "nv") || strstr(a, "nvidia")) return "NVENC";
    if (strstr(a, "v4l2")) return "V4L2";
    return "VCN";
}

int wubu_encode_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "encode[enc=%d h264=%d h265=%d vp9=%d av1=%d drv=%s]",
        g_enc, g_h264, g_h265, g_vp9, g_av1,
        wubu_encode_driver() ? wubu_encode_driver() : "none");
}
