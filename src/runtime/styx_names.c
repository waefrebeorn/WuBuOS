/*
 * styx_names.c -- Styx/9P2000 message-name table + lookup.
 *
 * Self-contained: only depends on styx.h for the type range. Minimal
 * includes. Originally part of the styx.c monolith; split out so the
 * protocol's debug-name surface lives in its own translation unit.
 */

#include "styx.h"
#include <string.h>

/* -- Message name table ------------------------------------------- */

static const char *g_msg_names[] = {
    [100] = "Tversion", [101] = "Rversion",
    [102] = "Tauth",    [103] = "Rauth",
    [104] = "Tattach",  [105] = "Rattach",
    [106] = "Terror",   [107] = "Rerror",
    [108] = "Tflush",   [109] = "Rflush",
    [110] = "Twalk",    [111] = "Rwalk",
    [112] = "Topen",    [113] = "Ropen",
    [114] = "Tcreate",  [115] = "Rcreate",
    [116] = "Tread",    [117] = "Rread",
    [118] = "Twrite",   [119] = "Rwrite",
    [120] = "Tclunk",   [121] = "Rclunk",
    [122] = "Tremove",  [123] = "Rremove",
    [124] = "Tstat",    [125] = "Rstat",
    [126] = "Twstat",   [127] = "Rwstat",
};

/* -- Message-name registry (the Revolver Doctrine) -----------------
 * The 9P2000 standard table above is the SEED; the live registry is a
 * runtime copy so extension message types (9P2000.e, custom server
 * dialects) can be named without a recompile. styx_msg_name reads the
 * LIVE copy, so a register is visible immediately. */
#define STYX_MSG_REGISTRY_MAX 256
static const char *g_styx_msg_registry[STYX_MSG_REGISTRY_MAX];
static int g_styx_msg_registry_seeded = 0;

static void styx_msg_registry_seed(void)
{
    if (g_styx_msg_registry_seeded) return;
    for (int i = 0; i < 128 && i < STYX_MSG_REGISTRY_MAX; i++)
        g_styx_msg_registry[i] = (i < 100 || i > 127) ? NULL : g_msg_names[i];
    g_styx_msg_registry_seeded = 1;
}

int styx_msg_name_register(uint8_t type, const char *name)
{
    if (!name) return -1;
    if (type >= STYX_MSG_REGISTRY_MAX) return -1;
    styx_msg_registry_seed();
    g_styx_msg_registry[type] = name;
    return 0;
}

const char *styx_msg_name(uint8_t type) {
    styx_msg_registry_seed();
    if (type < STYX_MSG_REGISTRY_MAX && g_styx_msg_registry[type])
        return g_styx_msg_registry[type];
    return "Unknown";
}
