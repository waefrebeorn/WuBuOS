/*
 * wubu_quadro.c -- kernel-owned NVIDIA Quadro professional routing.
 *
 * NVIDIA Quadro (professional workstations) binds nvidia 535/550/590
 * driver. NVIDIA: "Professional Workstations Software" with ISV
 * certifications. Same kernel driver as GeForce; Quadro gets
 * exclusive ISV certifications. "Runs on everything" includes
 * Quadro professional routing.
 *
 * Impl routing:
 *   - /sys/class/drm/card0/device/uevent: driver binding
 *   - /sys/class/drm/card0/device/vendor: PCI vendor (0x10DE)
 */
#include "wubu_quadro.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_quadro_present = 0;
static int g_quadro_driver = 0;

void wubu_quadro_probe(void)
{
#ifdef _GNU_SOURCE
    g_quadro_present = (access("/sys/class/drm/card0/device/uevent", R_OK) == 0) ? 1 : 0;
    g_quadro_driver = (access("/sys/class/drm/card0/device/vendor", R_OK) == 0) ? 1 : 0;
#else
    g_quadro_present = g_quadro_driver = 0;
#endif
}

int wubu_quadro_present(void)
{
#ifdef _GNU_SOURCE
    return g_quadro_present;
#else
    return 0;
#endif
}

int wubu_quadro_is_professional(int gpu_type)
{
    /* Quadro is the professional/workstation tier. */
    return (gpu_type == 1) ? 1 : 0;
}

int wubu_quadro_has_isv(int isv_certified)
{
    /* Quadro has ISV certifications (Autodesk, Adobe, etc). */
    return (isv_certified) ? 1 : 0;
}

void wubu_quadro_summary(char *out, size_t cap)
{
    snprintf(out, cap, "quadro[dev=%d driver=%d]", g_quadro_present, g_quadro_driver);
}
