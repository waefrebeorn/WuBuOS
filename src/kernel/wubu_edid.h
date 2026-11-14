/*
 * wubu_edid.h -- read a monitor's EDID blob into plain facts.
 *
 * WHY THIS EXISTS
 *   On the bootable image there is no X server and no wayland host to ask what a
 *   display is.  The information that matters -- who made it, its physical size, the
 *   resolution and refresh it prefers -- lives in the monitor itself, in the EDID
 *   block the GPU firmware reads over DDC/CI (I2C).  Nothing in this tree parsed it:
 *   wubu_ddcci only routes which DRIVER handles DDC, and wubu_display picks the first
 *   connected connector without ever reading its identity.
 *
 * WHAT THIS IS
 *   Pure parsing of the 128-byte base block into integers.  No I/O, no protocol, no
 *   allocation beyond the caller's struct -- so it runs identically on hosted Linux
 *   (reading the kernel's per-connector edid blob), on bare metal over the GPU
 *   bus, and in a unit
 *   test with a captured blob.
 *
 * FAILURE POLICY
 *   Everything fails closed: a short buffer, a wrong header, a bad checksum or an
 *   unsupported version returns -1 and leaves `out` zeroed.  A half-parsed EDID is
 *   worse than none, because a caller would build a desktop around a phantom display.
 *
 * WHAT IT DOES NOT DO
 *   It does not enumerate modes beyond the preferred one, does not parse CTA-861
 *   extensions (HDMI/DP timing lists), and does not touch the bus.  Scope discipline:
 *   the base block answers "which monitor is this and what does it prefer", which is
 *   what display routing needs first.
 */
#ifndef WUBU_EDID_H
#define WUBU_EDID_H

#include <stdbool.h>
#include <stddef.h>   /* size_t */
#include <stdint.h>

typedef struct {
    char     manuf[4];            /* three-letter ID, NUL-terminated */
    uint16_t product;             /* vendor's product code */
    uint32_t serial;              /* 32-bit serial, 0 when unset */
    int      week;                /* manufacture week, 0 when unspecified */
    int      year;                /* manufacture year (absolute, e.g. 2030) */
    int      mm_w, mm_h;          /* physical size in millimetres, 0 when unknown */
    char     name[16];            /* monitor name descriptor, "" when absent */
    int      pref_w, pref_h;      /* preferred detailed timing, 0 when unreadable */
    int      pref_refresh_mhz;    /* rounded to the nearest Hz */
    bool     valid;
} wubu_edid_t;

/* Parse a raw EDID block.  Returns 0 on success, -1 on any refusal. */
int wubu_edid_parse(const uint8_t *data, size_t len, wubu_edid_t *out);

#endif /* WUBU_EDID_H */
