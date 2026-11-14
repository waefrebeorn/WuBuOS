/*
 * wubu_edid_kms.h -- display discovery through direct KMS enumeration.
 *
 * WHY THIS SOURCE EXISTS
 *   Layer three of the portability design. sysfs needs the kernel's connector
 *   directories; xrandr needs an X server; neither exists everywhere. Direct KMS --
 *   open /dev/dri/cardN, enumerate connectors, read each one's EDID property -- needs
 *   only the DRM device node itself. That is exactly what libdrm's `modetest -c` does,
 *   so the method is established rather than invented.
 *
 * It also produces the SAME descriptor type as the other two sources, so a caller can
 * try them in order without caring which answered.
 *
 * WHAT IT DOES NOT DO
 *   It does not acquire DRM master, set modes, or touch CRTCs -- enumeration only.
 *   Becoming master would fight whatever is currently driving the display, so a
 *   read-only diagnostic must never do it.
 */
#ifndef WUBU_EDID_KMS_H
#define WUBU_EDID_KMS_H

#include "wubu_edid_scan.h"

/* Enumerate every connector on every readable /dev/dri/cardN device.
 *
 * Returns the number of connector records written, or -1 when no DRM device could be
 * opened at all (no /dev/dri, no permission, headless). Individual failures are SKIPPED:
 * a card that refuses to open or a connector without an EDID blob must not hide the
 * others.
 *
 * Records use `dir` for the card ("card1") and `name` for the connector as libdrm
 * reports it, e.g. "eDP-1". `parsed` is set only when the EDID blob was found AND
 * accepted by wubu_edid_parse.
 */
int wubu_edid_kms_scan(wubu_connector_edid_t *out, int cap);

#endif /* WUBU_EDID_KMS_H */
