/*
 * wubu_computectx.c -- kernel-owned GPU compute context routing.
 *
 * GPU compute contexts (amdgpu KFD queues, OpenCL/CUDA contexts) manage
 * kernel execution + queue scheduling. "Runs on everything" includes
 * correct compute on every GPU.
 *
 * Compute context:
 *   - amdgpu KFD (HSA): queue, doorbell, GTT
 *   - OpenCL: cl_context, cl_command_queue
 *   - CUDA: cuCtxCreate, cuCtxDestroy
 *   - /sys/class/kfd proc: KFD process
 *   - queue: compute queue, DMA queue
 *   - priority: high/normal/low
 *
 * WuBuOS owns this: detect compute context + KFD + queue, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the computectx frontier):
 *   - amdgpu KFD queue
 *   - OpenCL context
 *   - CUDA context
 */
#include "wubu_computectx.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_ctx = 0;         /* compute context present */
static int  g_kfd = 0;         /* KFD */
static int  g_queue = 0;       /* queue */
static int  g_opencl = 0;      /* OpenCL */
static int  g_cuda = 0;        /* CUDA */
static char g_ctx_drv[24] = "";

void wubu_computectx_probe(void)
{
    g_ctx = 0; g_kfd = 0; g_queue = 0; g_opencl = 0; g_cuda = 0;
    g_ctx_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/class/kfd", R_OK) == 0 ||
        access("/dev/kfd", R_OK) == 0) {
        g_ctx = 1; g_kfd = 1; g_queue = 1;
        strcpy(g_ctx_drv, "amdgpu-kfd");
    }
    if (access("/usr/lib/libOpenCL.so", R_OK) == 0 ||
        access("/etc/OpenCL", R_OK) == 0) {
        g_ctx = 1; g_opencl = 1;
        if (!g_ctx_drv[0]) strcpy(g_ctx_drv, "opencl");
    }
    if (access("/usr/lib/libcuda.so", R_OK) == 0 ||
        access("/usr/lib/x86_64-linux-gnu/libcuda.so", R_OK) == 0) {
        g_ctx = 1; g_cuda = 1;
        if (!g_ctx_drv[0]) strcpy(g_ctx_drv, "cuda");
    }
#endif
}

int  wubu_computectx_present(void){ return g_ctx; }
int  wubu_computectx_kfd(void)    { return g_kfd; }
int  wubu_computectx_queue(void)  { return g_queue; }
int  wubu_computectx_opencl(void) { return g_opencl; }
int  wubu_computectx_cuda(void)   { return g_cuda; }
const char *wubu_computectx_driver(void){ return g_ctx_drv[0] ? g_ctx_drv : NULL; }

const char *wubu_computectx_queue_for(const char *q)
{
    if (!q) return NULL;
    if (strstr(q, "compute") || strstr(q, "gfx")) return "compute";
    if (strstr(q, "sdma"))  return "sdma";
    if (strstr(q, "dma"))   return "dma";
    if (strstr(q, "copy"))  return "copy";
    return "compute";
}

const char *wubu_computectx_prio_for(const char *p)
{
    if (!p) return NULL;
    if (strstr(p, "high") || strstr(p, "rt"))  return "high";
    if (strstr(p, "low"))   return "low";
    if (strstr(p, "normal"))return "normal";
    return "normal";
}

int wubu_computectx_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "computectx[ctx=%d kfd=%d queue=%d opencl=%d cuda=%d drv=%s]",
        g_ctx, g_kfd, g_queue, g_opencl, g_cuda,
        wubu_computectx_driver() ? wubu_computectx_driver() : "none");
}
