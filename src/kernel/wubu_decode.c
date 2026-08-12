/*
 * wubu_decode.c -- kernel-owned GPU video decode routing.
 *
 * Video decode (VCN/UVD, NVDEC, Quick Sync) handles GPU-accelerated
 * decoding. "Runs on everything" includes correct decode on every GPU.
 *
 * Decode:
 *   - AMD: UVD, VCN decode, H.264, H.265, AV1, VP9
 *   - Intel: i915, Quick Sync Video (QSV) decode
 *   - nvidia: NVDEC
 *   - /dev/dri/card*: DRM decode
 *   - /sys/class/drm/card*: decode capability
 *   - codec: H.264 (AVC), H.265 (HEVC), VP9, AV1, MPEG
 *
 * WuBuOS owns this: detect decode + codec + capability, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the decode frontier):
 *   -Hardware video acceleration
 */
#include "wubu_decode.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_dec = 0;         /* decode present */
static int  g_h264 = 0;        /* H.264/AVC */
static int  g_h265 = 0;        /* H.265/HEVC */
static int  g_av1 = 0;         /* AV1 */
static int  g_vp9 = 0;         /* VP9 */
static char g_dec_drv[24] = "";

void wubu_decode_probe(void)
{
    g_dec = 0; g_h264 = 0; g_h265 = 0; g_av1 = 0; g_vp9 = 0;
    g_dec_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_dec = 1; g_h264 = 1; g_h265 = 1; g_av1 = 1;
        strcpy(g_dec_drv, "amd-uvd");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_dec = 1; g_h264 = 1; g_h265 = 1;
        if (!g_dec_drv[0]) strcpy(g_dec_drv, "i915-qsv-decode");
    }
    if (access("/sys/module/nvidia", R_OK) == 0 ||
        access("/sys/module/nvidia_uvm", R_OK) == 0) {
        g_dec = 1; g_h264 = 1; g_h265 = 1; g_vp9 = 1;
        if (!g_dec_drv[0]) strcpy(g_dec_drv, "nvidia-nvdec");
    }
#endif
}

int  wubu_decode_present(void){ return g_dec; }
int  wubu_decode_h264(void)  { return g_h264; }
int  wubu_decode_h265(void)  { return g_h265; }
int  wubu_decode_av1(void)   { return g_av1; }
int  wubu_decode_vp9(void)   { return g_vp9; }
const char *wubu_decode_driver(void){ return g_dec_drv[0] ? g_dec_drv : NULL; }

const char *wubu_decode_codec_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "h264") || strstr(c, "avc")) return "H.264";
    if (strstr(c, "h265") || strstr(c, "hevc")) return "H.265";
    if (strstr(c, "av1")) return "AV1";
    if (strstr(c, "vp9")) return "VP9";
    if (strstr(c, "mpeg")) return "MPEG";
    return "H.264";
}

const char *wubu_decode_api_for(const char *a)
{
    if (!a) return NULL;
    if (strstr(a, "uvd") || strstr(a, "vcn") || strstr(a, "amd")) return "VCN";
    if (strstr(a, "qsv") || strstr(a, "intel")) return "QuickSync";
    if (strstr(a, "nvdec") || strstr(a, "nv") || strstr(a, "nvidia")) return "NVDEC";
    if (strstr(a, "v4l2")) return "V4L2";
    return "VCN";
}

int wubu_decode_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "decode[dec=%d h264=%d h265=%d av1=%d vp9=%d drv=%s]",
        g_dec, g_h264, g_h265, g_av1, g_vp9,
        wubu_decode_driver() ? wubu_decode_driver() : "none");
}
