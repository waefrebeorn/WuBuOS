/*
 * wubu_gamepadbm.c -- kernel-owned gamepad button map routing.
 *
 * Gamepad button maps define physical-to-logical button mapping
 * for consistent input across controllers. "Runs on everything"
 * includes correct button map routing on all gamepads.
 *
 * Impl routing:
 *   - /sys/class/input/js0: gamepad presence
 *   - /sys/class/input/js0/device: gamepad device
 */
#include "wubu_gamepadbm.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_gamepadbm_present = 0;
static int g_gamepadbm_buttons = 0;

void wubu_gamepadbm_probe(void)
{
#ifdef WUBU_HOSTED
    g_gamepadbm_present = (access("/sys/class/input/js0", R_OK) == 0) ? 1 : 0;
    g_gamepadbm_buttons = (access("/sys/class/input/js0/device", R_OK) == 0) ? 1 : 0;
#else
    g_gamepadbm_present = g_gamepadbm_buttons = 0;
#endif
}

int wubu_gamepadbm_present(void)
{
#ifdef WUBU_HOSTED
    return g_gamepadbm_present;
#else
    return 0;
#endif
}

int wubu_gamepadbm_map(int button)
{
    /* Standard button map: 0=A, 1=B, 2=X, 3=Y, 4=LB, 5=RB, 6=BACK, 7=START. */
    if (button >= 0 && button <= 15) return button;
    return -1;
}

int wubu_gamepadbm_is_pressed(int value)
{
    return (value != 0) ? 1 : 0;
}

void wubu_gamepadbm_summary(char *out, size_t cap)
{
    snprintf(out, cap, "gamepadbm[dev=%d btns=%d]", g_gamepadbm_present, g_gamepadbm_buttons);
}
