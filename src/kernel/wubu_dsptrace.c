/*
 * wubu_dsptrace.c -- kernel-owned audio DSP trace/debug routing.
 *
 * DSP firmware exposes trace buffers and debug channels so the kernel
 * can inspect DSP state (load, xruns, overruns, firmware errors). "Runs
 * on everything" includes DSP diagnostics on every audio accelerator.
 *
 * Impl routing:
 *   - /sys/kernel/debug (tracefs debug presence)
 *   - /sys/module/snd_sof/parameters: SOF DSP module
 */
#include "wubu_dsptrace.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_dsptrace_avail = 0;
static int g_dsptrace_errors = 0;

void wubu_dsptrace_probe(void)
{
    /* Detect DSP trace availability. */
#ifdef _GNU_SOURCE
    g_dsptrace_avail = (access("/sys/kernel/debug", R_OK) == 0) ? 1 : 0;
    g_dsptrace_errors = (access("/sys/module/snd_sof/parameters", R_OK) == 0) ? 1 : 0;
#else
    g_dsptrace_avail = g_dsptrace_errors = 0;
#endif
}

int wubu_dsptrace_present(void)
{
#ifdef _GNU_SOURCE
    return g_dsptrace_avail;
#else
    return 0;
#endif
}

int wubu_dsptrace_level(int level)
{
    if (level < 0) return 0;
    if (level > 4) return 4;
    return level;
}

const char *wubu_dsptrace_evt(int code)
{
    switch (code) {
        case 0: return "ok";
        case 1: return "xrun";
        case 2: return "overrun";
        case 3: return "firmware_error";
        default: return "unknown";
    }
}

void wubu_dsptrace_summary(char *out, size_t cap)
{
    snprintf(out, cap, "dsptrace[avail=%d err=%d]",
             g_dsptrace_avail, g_dsptrace_errors);
}
