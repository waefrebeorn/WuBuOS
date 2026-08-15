/*
 * wubu_gamepaddz.c -- kernel-owned gamepad deadzone routing.
 *
 * Gamepad deadzone filters stick drift by ignoring small movements.
 * "Runs on everything" includes correct deadzone routing on all
 * input devices.
 *
 * Impl routing:
 *   - /sys/class/input/js0: gamepad presence
 *   - /sys/class/input/js0/device: gamepad device
 */
#include "wubu_gamepaddz.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gamepaddz_present = 0;
static int g_gamepaddz_axes = 0;

void wubu_gamepaddz_probe(void)
{
#ifdef WUBU_HOSTED
    g_gamepaddz_present = (access("/sys/class/input/js0", R_OK) == 0) ? 1 : 0;
    g_gamepaddz_axes = (access("/sys/class/input/js0/device", R_OK) == 0) ? 1 : 0;
#else
    g_gamepaddz_present = g_gamepaddz_axes = 0;
#endif
}

int wubu_gamepaddz_present(void)
{
#ifdef WUBU_HOSTED
    return g_gamepaddz_present;
#else
    return 0;
#endif
}

int wubu_gamepaddz_filter(int value, int deadzone)
{
    /* Deadzone: values within [-dz, +dz] become 0. */
    if (value >= -deadzone && value <= deadzone) return 0;
    return value;
}

int wubu_gamepaddz_is_drift(int value, int deadzone)
{
    /* True if value is within deadzone (drift). */
    return (value >= -deadzone && value <= deadzone) ? 1 : 0;
}

void wubu_gamepaddz_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gamepaddz[dev=%d axes=%d]", g_gamepaddz_present, g_gamepaddz_axes);
}
