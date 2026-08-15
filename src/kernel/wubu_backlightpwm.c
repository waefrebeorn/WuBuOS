/*
 * wubu_backlightpwm.c -- kernel-owned display backlight PWM routing.
 *
 * Backlight PWM controls display brightness by modulating the backlight
 * LED duty cycle. "Runs on everything" includes correct brightness.
 *
 * Backlight:
 *   - sysfs /sys/class/backlight brightness
 *   - ACPI: acpi_video0
 *   - Intel: intel_backlight (i915)
 *   - AMD: amdgpu_bl
 *   - PWM: raw PWM for LED backlight
 *
 * WuBuOS owns this: detect backlight type + PWM + brightness range, route
 * to the right driver, and expose the topology.
 */
#include "wubu_backlightpwm.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_bl = 0;          /* backlight present */
static int  g_pwm = 0;         /* PWM control */
static int  g_sysfs = 0;       /* sysfs interface */
static int  g_acpi = 0;        /* ACPI video */
static int  g_intel = 0;       /* intel_backlight */
static char g_bl_drv[24] = "";

/* ---- W1: probe the backlight topology ---- */
void wubu_backlightpwm_probe(void)
{
    g_bl = 0; g_pwm = 0; g_sysfs = 0; g_acpi = 0; g_intel = 0;
    g_bl_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* /sys/class/backlight present? */
    if (access("/sys/class/backlight", R_OK) == 0) {
        g_bl = 1; g_sysfs = 1;
        strcpy(g_bl_drv, "sysfs-backlight");
    }
    /* PWM fan/backlight controller? */
    if (access("/sys/class/pwm", R_OK) == 0) {
        g_pwm = 1;
        if (!g_bl_drv[0]) strcpy(g_bl_drv, "pwm-backlight");
    }
    /* intel_backlight? */
    if (access("/sys/module/i915", R_OK) == 0) {
        g_intel = 1;
    }
    /* ACPI video? */
    if (access("/sys/class/backlight/acpi_video0", R_OK) == 0) {
        g_acpi = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_backlightpwm_present(void){ return g_bl; }
int  wubu_backlightpwm_pwm(void)    { return g_pwm; }
int  wubu_backlightpwm_sysfs(void)  { return g_sysfs; }
int  wubu_backlightpwm_acpi(void)   { return g_acpi; }
int  wubu_backlightpwm_intel(void)  { return g_intel; }
const char *wubu_backlightpwm_driver(void){ return g_bl_drv[0] ? g_bl_drv : NULL; }

/* ---- W3: backlight routing ---- */
const char *wubu_backlightpwm_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "sysfs"))   return "sysfs";
    if (strstr(t, "acpi"))    return "acpi-video";
    if (strstr(t, "intel"))   return "intel-backlight";
    if (strstr(t, "amd"))     return "amdgpu-bl";
    if (strstr(t, "pwm"))     return "pwm-raw";
    return "backlight";
}

const char *wubu_backlightpwm_brightness_for(const char *b)
{
    if (!b) return NULL;
    if (strstr(b, "max"))    return "max";
    if (strstr(b, "min") || strstr(b, "off")) return "min";
    if (strstr(b, "50") || strstr(b, "half")) return "50";
    return "auto";
}

/* ---- W4: summary ---- */
int wubu_backlightpwm_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "backlightpwm[bl=%d pwm=%d sysfs=%d acpi=%d intel=%d drv=%s]",
        g_bl, g_pwm, g_sysfs, g_acpi, g_intel,
        wubu_backlightpwm_driver() ? wubu_backlightpwm_driver() : "none");
}
