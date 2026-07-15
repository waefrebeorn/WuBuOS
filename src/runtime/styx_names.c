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

const char *styx_msg_name(uint8_t type) {
    if (type >= 100 && type <= 127 && g_msg_names[type])
        return g_msg_names[type];
    return "Unknown";
}
