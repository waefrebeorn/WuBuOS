/*
 * hosted_display.c -- see hosted_display.h for why this exists.
 *
 * Pure geometry over plain integers: no Wayland types, no I/O, no allocation beyond
 * the caller's struct.  That is deliberate so the rules (what counts as present,
 * which display owns a point, how the virtual desktop spans outputs) can be tested
 * without a compositor and cannot regress silently when protocol code changes.
 */
#include "hosted_display.h"

#include <string.h>

void hosted_display_reset(hosted_displays_t *ds)
{
    if (!ds) return;
    memset(ds, 0, sizeof(*ds));
}

hosted_display_t *hosted_display_slot(hosted_displays_t *ds, uint32_t name)
{
    if (!ds || name == 0) return NULL;

    /* Re-binding the same global after a reset must not create a duplicate slot:
     * identity is the registry name, not object pointer. */
    for (int i = 0; i < ds->count; i++)
        if (ds->d[i].name == name) return &ds->d[i];

    if (ds->count >= HOSTED_MAX_DISPLAYS) return NULL;

    hosted_display_t *d = &ds->d[ds->count];
    memset(d, 0, sizeof(*d));
    d->name  = name;
    d->scale = 1;              /* wl_output defaults to 1 when it does not say */
    ds->count++;
    return d;
}

void hosted_display_commit(hosted_display_t *d)
{
    if (!d) return;
    d->present = true;
}

int hosted_display_present(const hosted_displays_t *ds)
{
    if (!ds) return 0;
    int n = 0;
    for (int i = 0; i < ds->count; i++)
        if (ds->d[i].present) n++;
    return n;
}

hosted_display_t *hosted_display_at(const hosted_displays_t *ds, int px, int py)
{
    if (!ds) return NULL;

    /* Prefer an EXACT hit.  Two displays never overlap in a normal configuration, but
     * if one did, the first present match wins rather than returning something
     * arbitrary. */
    for (int i = 0; i < ds->count; i++) {
        const hosted_display_t *d = &ds->d[i];
        if (!d->present) continue;
        if (px >= d->x && px < d->x + d->w &&
            py >= d->y && py < d->y + d->h)
            return (hosted_display_t *)d;
    }
    return NULL;
}

bool hosted_display_bounds(const hosted_displays_t *ds,
                           int *bx, int *by, int *bw, int *bh)
{
    if (!ds) return false;

    int first = -1;
    for (int i = 0; i < ds->count; i++)
        if (ds->d[i].present) { first = i; break; }

    if (first < 0) return false;

    int x0 = ds->d[first].x, y0 = ds->d[first].y;
    int x1 = x0 + ds->d[first].w, y1 = y0 + ds->d[first].h;

    for (int i = first + 1; i < ds->count; i++) {
        const hosted_display_t *d = &ds->d[i];
        if (!d->present) continue;
        if (d->x < x0) x0 = d->x;
        if (d->y < y0) y0 = d->y;
        if (d->x + d->w > x1) x1 = d->x + d->w;
        if (d->y + d->h > y1) y1 = d->y + d->h;
    }

    if (bx) *bx = x0;
    if (by) *by = y0;
    if (bw) *bw = x1 - x0;
    if (bh) *bh = y1 - y0;
    return true;
}

int hosted_display_primary(const hosted_displays_t *ds)
{
    if (!ds) return -1;

    /* The primary display holds the origin of the virtual desktop.  Real
     * configurations have exactly one such output; if more than one claims it (an
     * overlapping or misreported layout), pick the one whose corner is NEAREST the
     * origin and break remaining ties by index -- so the answer is deterministic
     * instead of depending on enumeration order. */
    int best = -1;
    long best_dist = 0;
    for (int i = 0; i < ds->count; i++) {
        const hosted_display_t *d = &ds->d[i];
        if (!d->present) continue;
        if (d->x <= 0 && d->y <= 0 &&
            0 < d->x + d->w && 0 < d->y + d->h) {
            long dist = (long)(d->x < 0 ? -d->x : d->x) +
                        (long)(d->y < 0 ? -d->y : d->y);
            if (best < 0 || dist < best_dist) { best = i; best_dist = dist; }
        }
    }
    if (best >= 0) return best;

    /* Fall back to the leftmost-topmost present output, which is what Windows treats
     * as main when nothing claims the origin. */
    int corner = -1;
    for (int i = 0; i < ds->count; i++) {
        if (!ds->d[i].present) continue;
        if (corner < 0 ||
            ds->d[i].x < ds->d[corner].x ||
            (ds->d[i].x == ds->d[corner].x && ds->d[i].y < ds->d[corner].y))
            corner = i;
    }
    return corner;
}
