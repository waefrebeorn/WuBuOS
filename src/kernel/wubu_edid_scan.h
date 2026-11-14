/*
 * wubu_edid_scan.h -- find EDID blobs for attached connectors.
 *
 * WHY THIS EXISTS
 *   wubu_edid.c parses an EDID block, but nothing in the tree actually HANDS it one.
 *   On hosted Linux the kernel already exposes every connector's EDID at
 *   /sys/class/drm/<card>-<connector>/edid -- the same bytes the firmware read over
 *   DDC/CI -- so identity is available without touching the GPU bus at all.
 *
 *   This scanner turns that directory into a list of (connector, EDID facts) pairs.
 *   It is deliberately separate from both the parser (pure logic, testable with a
 *   captured blob) and from wubu_ddcci (which routes which kernel DRIVER owns DDC).
 *   Three concerns, three leaves.
 *
 * WHAT IT DOES NOT DO
 *   It does not read DDC directly over I2C -- on bare metal that is the fallback when
 *   no sysfs exists, and it needs the bus access wubu_ddcci describes.  Porting this
 *   scanner to that path means replacing only the "read these bytes" step.
 */
#ifndef WUBU_EDID_SCAN_H
#define WUBU_EDID_SCAN_H

#include <stdbool.h>
#include <stddef.h>

#include "../kernel/wubu_edid.h"

#define WUBU_EDID_MAX_CONNECTORS 8
#define WUBU_EDID_NAME          64
/* Base EDID is 128 bytes; allow one extension block, which covers CTA-861. */
#define WUBU_EDID_RAW_MAX       256

typedef struct {
    char        dir[64];                /* directory name exactly as found */
    char        name[WUBU_EDID_NAME];   /* connector part: "eDP-1", "HDMI-A-1" */
    bool        connected;              /* connector status, not just EDID presence */
    bool        enabled;                /* kernel's `enabled` flag for the connector */
    bool        parsed;                 /* EDID read AND parsed */
    wubu_edid_t edid;
    uint8_t     raw[WUBU_EDID_RAW_MAX];
    size_t      raw_len;
} wubu_connector_edid_t;

/* Scan `drm_root` (normally "/sys/class/drm") for connector directories.
 *
 * Returns the number of connector entries written, or -1 when the root cannot be read
 * (headless, non-Linux, container).  Individual failures are SKIPPED, never fatal: a
 * zero-length EDID, a connector with no EDID file, or an unparseable blob must not hide
 * the other monitors that are fine.
 *
 * Disconnected connectors are reported with connected=false and parsed=false rather than
 * omitted, because "this port exists but nothing is plugged in" is information a display
 * panel should show; advertising them as displays would be wrong.
 *
 * NAME AGNOSTICISM: a directory counts as a CONNECTOR when it contains any of the
 * kernel's per-connector files (status, enabled, modes, edid), NOT by matching a
 * "cardN-*" pattern. Naming conventions have shifted across kernel releases and GPU
 * vendors, so anything that keys off the exact shape of the name eventually misses real
 * hardware. The connector part is derived by dropping a leading "<card>-" prefix when
 * present, and `dir` keeps the original for callers that need it.
 *
 * OTHER SOURCES: this scanner covers the sysfs path. Established practice for the
 * remaining environments is a separate backend behind the same result type -- direct
 * KMS enumeration (what libdrm's modetest does), the UEFI GOP mode list plus EDID over
 * DDC/CI for bare metal, and system_profiler on macOS. None of those are wired here yet;
 * the descriptor shape is what they will all feed.
 *
 * `raw` keeps the original bytes so callers can re-parse with a newer parser without
 * reading the bus again.
 */
int wubu_edid_scan(const char *drm_root,
                   wubu_connector_edid_t *out, int cap);

#endif /* WUBU_EDID_SCAN_H */
