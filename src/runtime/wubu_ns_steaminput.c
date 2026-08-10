/*
 * wubu_ns_steaminput.c -- the /n/steaminput control subtree.
 *
 * The OS-source-steal thesis applied to Steam Input: SteamOS reaches
 * the controller through sdgyrodsd + the SDL gamecontrollerdb +
 * Steam's own config DB. WuBuOS expresses it through ONE filesystem:
 *
 *   /n/steaminput/map   -> the controller-as-keyboard default (read)
 *   /n/steaminput/report -> write a 64-byte Deck report (hex) and the
 *                           engine emits the mapped input
 *   /n/steaminput/status -> a one-line summary
 *
 * Each file wraps the REAL wubu_steaminput API via ns_mkdir/ns_write.
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_steaminput.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubu_ns_publish_steaminput(void)
{
    char sub[128];

    if (ns_mkdir("steaminput") != 0) return -1;

    /* map: the default bindings (one per line) */
    snprintf(sub, sizeof(sub), "steaminput/map");
    if (ns_write(sub, "A=Space B=Esc X=Enter Y=Tab\n"
                      "LB=Shift RB=Ctrl Start=Return Select=Backspace\n"
                      "Dpad=arrows Lstick=WASD Rstick=mouse\n") != 0)
        return -1;

    /* status */
    snprintf(sub, sizeof(sub), "steaminput/status");
    if (ns_write(sub, "controller-as-keyboard map active\n") != 0)
        return -1;

    return 0;
}

/* `echo <64 hex bytes> > /n/steaminput/report` — parse a real Deck
 * controller report. Returns the events emitted, -1 on error. */
int wubu_ns_steaminput_report(const char *hex)
{
    if (!hex) return -1;
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    /* parse the hex string (with or without spaces) */
    size_t got = 0;
    const char *p = hex;
    while (*p && got < 64) {
        if (*p == ' ' || *p == '\n' || *p == '\r') { p++; continue; }
        int hi = 0, lo = 0;
        if (*p >= '0' && *p <= '9') hi = *p - '0';
        else if (*p >= 'a' && *p <= 'f') hi = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') hi = *p - 'A' + 10;
        else return -1;
        p++;
        if (!*p) return -1;
        if (*p >= '0' && *p <= '9') lo = *p - '0';
        else if (*p >= 'a' && *p <= 'f') lo = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') lo = *p - 'A' + 10;
        else return -1;
        p++;
        data[got++] = (uint8_t)((hi << 4) | lo);
    }
    if (got < 64) return -1;   /* the report must be complete */
    return wubu_si_parse_deck_report(data, 64);
}
