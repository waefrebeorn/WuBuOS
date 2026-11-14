/*
 * hosted_display.h -- the guest's model of the displays attached to it.
 *
 * WHY THIS EXISTS
 *   The compositor advertises wl_output events and the guest listened to them -- but
 *   the handlers only printed to stderr and threw the values away.  Meanwhile the
 *   compositor advertised exactly ONE hardcoded output with zeroed geometry.  So
 *   neither side had a notion of "which displays exist, where they sit, or what
 *   resolution they are at", which is everything window tiling across monitors and a
 *   resolution menu would need.
 *
 *   This module is that notion, kept free of Wayland types so it can be unit-tested
 *   without a compositor and reused by anything that needs geometry: snap layouts,
 *   the overview, the window manager, a future resolution control.
 *
 * WHAT IT IS NOT
 *   It does not talk to the protocol and does not decide modes.  The Wayland listener
 *   feeds it (geometry/mode/scale events), it stores and answers questions.  Keeping
 *   those apart means the geometry logic -- bounds, ordering, hit-testing -- is
 *   testable and cannot be broken by a protocol change.
 *
 * COORDINATE SYSTEM
 *   Positions are in the COMPOSITOR'S VIRTUAL DESKTOP space, exactly as wl_output
 *   reports them: a second monitor to the right of the first has x == width of the
 *   first.  A window at x=1920 on a 1920-wide primary is therefore on the second
 *   monitor.  No scaling is applied here; `scale` is carried separately because
 *   clients must multiply by it themselves.
 */
#ifndef WUBU_HOSTED_DISPLAY_H
#define WUBU_HOSTED_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#define HOSTED_MAX_DISPLAYS 8

/* One physical output.  Fields mirror what wl_output tells us; `present` is false
 * until the output has sent its first `done`, because geometry and mode arrive as
 * separate events before that and a half-described display is worse than none. */
typedef struct {
    int      x, y;              /* position in the virtual desktop */
    int      w, h;              /* current mode, in logical pixels */
    int      refresh_mhz;       /* as reported (milli-Hz / 1000) */
    int      scale;             /* integer factor, 1 when unknown */
    bool     present;           /* has completed at least one event burst */
    uint32_t name;              /* wl_registry global name, for identity */
    char     make[32];
    char     model[32];
} hosted_display_t;

typedef struct {
    hosted_display_t d[HOSTED_MAX_DISPLAYS];
    int count;
} hosted_displays_t;

/* Empty model.  Called once at startup; also clears any stale outputs. */
void hosted_display_reset(hosted_displays_t *ds);

/* Find-or-create the slot for a wl_registry global name.  Returns NULL when the table
 * is full -- the caller should stop listening rather than overwrite a real display.
 * Creating a slot does NOT make it present; commit() does. */
hosted_display_t *hosted_display_slot(hosted_displays_t *ds, uint32_t name);

/* Mark a display complete after its `done` event.  Idempotent. */
void hosted_display_commit(hosted_display_t *d);

/* Number of displays that have actually reported themselves. */
int hosted_display_present(const hosted_displays_t *ds);

/* Display whose top-left region contains the point, or NULL when the point is outside
 * every display.  This is how "which monitor is this window on" gets answered. */
hosted_display_t *hosted_display_at(const hosted_displays_t *ds, int px, int py);

/* Bounding rectangle covering every present display.  For a single monitor this is
 * just that monitor's geometry; for two side-by-side it spans both, which is the
 * virtual-desktop size any full-screen surface must cover. */
bool hosted_display_bounds(const hosted_displays_t *ds,
                           int *bx, int *by, int *bw, int *bh);

/* Index of the primary display: the one containing the origin (0,0), falling back to
 * the first present one.  Windows calls this "Make this my main display"; every
 * taskbar/overview decision needs it. */
int hosted_display_primary(const hosted_displays_t *ds);

#endif /* WUBU_HOSTED_DISPLAY_H */
