/*
 * wubu_edid_xrandr.h -- display discovery through `xrandr`, when sysfs yields nothing.
 *
 * WHY THIS SOURCE EXISTS
 *   Established practice for cross-environment display tools is layered sources: read
 *   /sys/class/drm natively and FALL BACK to `xrandr --current` when nothing is found
 *   there (retch-sysinfo's display module does exactly this). On a headless host, a
 *   container without DRM device access, or a session where the kernel exposes no
 *   connector directories, sysfs returns zero connectors and the tool would otherwise
 *   report "no displays" on a machine that plainly has them.
 *
 * WHAT IT READS
 *   `xrandr -q --props` gives, per output: connection state, primary flag, the mode
 *   list with `*`/`+` markers, and -- crucially -- the EDID blob in hex under an
 *   `EDID:` property.  Feeding those bytes back through wubu_edid_parse means both
 *   sources produce the SAME descriptor type, so callers do not need to know which one
 *   answered.
 *
 * WHAT IT DOES NOT DO
 *   It does not claim a display exists when xrandr cannot be run: failure returns -1,
 *   never a fabricated entry.  And it does not try to interpret the mode markers
 *   beyond recording the current/preferred resolution, because the descriptor's
 *   preferred timing already comes from EDID.
 */
#ifndef WUBU_EDID_XRANDR_H
#define WUBU_EDID_XRANDR_H

#include <stdio.h>

#include "wubu_edid_scan.h"

/* Parse an already-open `xrandr -q --props` stream.  Split out from the popen wrapper
 * so the parsing logic can be tested against a captured fixture instead of requiring a
 * live X server.  Returns connectors written, or -1 on a NULL stream. */
int wubu_edid_xrandr_parse(FILE *fp,
                           wubu_connector_edid_t *out, int cap);

/* Run `xrandr -q --props` and parse it.  Returns connectors found, or -1 when xrandr is
 * absent, the display is unreachable, or it produced nothing usable -- callers should
 * treat that as "this source has no answer", not as "there are no displays". */
int wubu_edid_xrandr_scan(wubu_connector_edid_t *out, int cap);

#endif /* WUBU_EDID_XRANDR_H */
