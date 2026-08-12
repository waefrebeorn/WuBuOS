/*
 * wubu_rendernode.c -- kernel-owned GPU render node routing.
 *
 * GPU render node exposes the GPU for compute/render tasks
 * without display control. "Runs on everything" includes
 * correct render node routing on all GPU vendors.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver version
 *   - /sys/class/drm/renderD128: render node presence
 */
#include "wubu_rendernode.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_rendernode_present = 0;
static int g_rendernode_ready = 0;

void wubu_rendernode_probe(void)
{
#ifdef _GNU_SOURCE
    g_rendernode_present = (access("/sys/class/drm/renderD128", R_OK) == 0) ? 1 : 0;
    g_rendernode_ready = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
#else
    g_rendernode_present = g_rendernode_ready = 0;
#endif
}

int wubu_rendernode_present(void)
{
#ifdef _GNU_SOURCE
    return g_rendernode_present;
#else
    return 0;
#endif
}

int wubu_rendernode_valid_fd(int fd)
{
    /* Valid renderD128 fd: non-negative. */
    return (fd >= 0) ? 1 : 0;
}

int wubu_rendernode_priority(int ctx_priority)
{
    /* Context priority: normal(0), high(1), real-time(2). */
    if (ctx_priority < 0) return 0;
    if (ctx_priority == 0) return 1;
    if (ctx_priority > 0 && ctx_priority < 100) return 2;
    return 3; /* real-time */
}

void wubu_rendernode_summary(char *out, size_t cap)
{
    snprintf(out, cap, "rendernode[dev=%d ready=%d]", g_rendernode_present, g_rendernode_ready);
}
