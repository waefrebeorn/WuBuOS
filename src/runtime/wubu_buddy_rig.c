/* wubu_buddy_rig.c -- WuBu Buddy pixel rig renderer (C11).
 *
 * One base sprite + anatomical part map (baked) -> any pose, any face.
 * Pixel-exact: every frame writes the sprite's own palette pixels moved by a
 * per-part offset. No redraw, no duplicated art. */
#include "wubu_buddy_rig.h"
#include "wubu_buddy_rig_data.h"

#include <string.h>

/* Z-order: torso, head, cap, ears, then arms and legs on top. */
static const uint8_t kDrawOrder[WUBU_BUDDY_PART_COUNT_] = {
    WUBU_BUDDY_PART_TORSO, WUBU_BUDDY_PART_HEAD, WUBU_BUDDY_PART_CAP,
    WUBU_BUDDY_PART_EARS,  WUBU_BUDDY_PART_ARM_L, WUBU_BUDDY_PART_ARM_R,
    WUBU_BUDDY_PART_LEG_L, WUBU_BUDDY_PART_LEG_R,
};

static void put(uint32_t *buf, int w, int h, int x, int y, uint32_t c)
{
    if (x >= 0 && x < w && y >= 0 && y < h)
        buf[(size_t)y * w + x] = c;
}

void wubu_buddy_rig_pose_rest(wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_])
{
    for (int i = 0; i < WUBU_BUDDY_PART_COUNT_; ++i) {
        xf[i].dx = 0;
        xf[i].dy = 0;
    }
}

void wubu_buddy_rig_pose_walk(int frame,
                              wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_])
{
    int f = ((frame % 8) + 8) % 8;
    int bob  = (f % 2 == 0) ? 1 : 0;
    int step = (f == 0 || f == 1 || f == 6 || f == 7) ? 2 : -2;
    wubu_buddy_rig_pose_rest(xf);
    xf[WUBU_BUDDY_PART_HEAD].dy = (int8_t)bob;
    xf[WUBU_BUDDY_PART_CAP].dy  = (int8_t)bob;
    xf[WUBU_BUDDY_PART_EARS].dy = (int8_t)bob;
    xf[WUBU_BUDDY_PART_TORSO].dy = (int8_t)bob;
    xf[WUBU_BUDDY_PART_ARM_L].dy = (int8_t)(bob + step);
    xf[WUBU_BUDDY_PART_ARM_R].dy = (int8_t)(bob - step);
    xf[WUBU_BUDDY_PART_LEG_L].dy = (int8_t)(-step);
    xf[WUBU_BUDDY_PART_LEG_R].dy = (int8_t)(step);
}

void wubu_buddy_rig_pose_wave(int frame,
                              wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_])
{
    int f = ((frame % 8) + 8) % 8;
    int bob  = (f % 2 == 0) ? 1 : 0;
    int wave = (f == 0 || f == 1 || f == 4 || f == 5) ? 2 : -2;
    int tilt = (f <= 2) ? 2 : ((f >= 5) ? -2 : 0);
    wubu_buddy_rig_pose_rest(xf);
    xf[WUBU_BUDDY_PART_HEAD].dy = (int8_t)bob;
    xf[WUBU_BUDDY_PART_HEAD].dx = (int8_t)tilt;
    xf[WUBU_BUDDY_PART_CAP].dy  = (int8_t)bob;
    xf[WUBU_BUDDY_PART_CAP].dx  = (int8_t)tilt;
    xf[WUBU_BUDDY_PART_EARS].dy = (int8_t)bob;
    xf[WUBU_BUDDY_PART_EARS].dx = (int8_t)tilt;
    xf[WUBU_BUDDY_PART_TORSO].dy = (int8_t)bob;
    xf[WUBU_BUDDY_PART_ARM_L].dy = (int8_t)(bob - wave);
    xf[WUBU_BUDDY_PART_ARM_R].dy = (int8_t)(bob + wave);
}

/* ---- facial rig: redraw canonical dot eyes + mouth in the head region --- */
static const uint32_t kFaceDark = 0x181616FFu;

static void face_disc(uint32_t *buf, int w, int h, int cx, int cy, int r)
{
    for (int dx = -r; dx <= r; ++dx)
        for (int dy = -r; dy <= r; ++dy)
            if (dx * dx + dy * dy <= r * r + 1)
                put(buf, w, h, cx + dx, cy + dy, kFaceDark);
}

static void face_line(uint32_t *buf, int w, int h, int cx, int cy, int half)
{
    for (int dx = -half; dx <= half; ++dx)
        put(buf, w, h, cx + dx, cy, kFaceDark);
}

static void face_arc(uint32_t *buf, int w, int h, int cx, int cy, int r, int up)
{
    int k = (r > 2) ? 6 : 4;                 /* slope x10, integer-only */
    for (int dx = -r; dx <= r; ++dx) {
        int dy = (dx < 0 ? -dx : dx) * k / 10;
        int y = up ? cy - dy : cy + dy;
        put(buf, w, h, cx + dx, y, kFaceDark);
    }
}

static void apply_face(uint32_t *buf, int w, int h, int expr)
{
    const wubu_buddy_part_def_t *hd = &kBuddyParts[WUBU_BUDDY_PART_HEAD];
    int ex = hd->x + hd->w / 2;
    int ey = hd->y + hd->h * 48 / 100;
    int gap = hd->w * 17 / 100; if (gap < 2) gap = 2;
    int r_dot = hd->w / 22; if (r_dot < 1) r_dot = 1; if (r_dot > 2) r_dot = 2;
    int r_wide = hd->w / 16; if (r_wide < 2) r_wide = 2; if (r_wide > 3) r_wide = 3;
    int my = ey + hd->h * 24 / 100;
    int mw = hd->w * 14 / 100; if (mw < 2) mw = 2;

    switch (expr) {
    case WUBU_BUDDY_FACE_BLINK:
        face_line(buf, w, h, ex - gap, ey, gap);
        face_line(buf, w, h, ex + gap, ey, gap);
        break;
    case WUBU_BUDDY_FACE_HAPPY:
        face_arc(buf, w, h, ex - gap, ey + r_dot, r_dot + 1, 1);
        face_arc(buf, w, h, ex + gap, ey + r_dot, r_dot + 1, 1);
        break;
    case WUBU_BUDDY_FACE_WINK:
        face_line(buf, w, h, ex - gap, ey, gap);
        face_disc(buf, w, h, ex + gap, ey, r_dot);
        break;
    default:                                     /* open, sad, wide */
        face_disc(buf, w, h, ex - gap, ey, expr == WUBU_BUDDY_FACE_WIDE ? r_wide : r_dot);
        face_disc(buf, w, h, ex + gap, ey, expr == WUBU_BUDDY_FACE_WIDE ? r_wide : r_dot);
        break;
    }

    if (expr == WUBU_BUDDY_FACE_SMILE || expr == WUBU_BUDDY_FACE_HAPPY) {
        face_arc(buf, w, h, ex, my, mw, 1);      /* corners rise -> smile */
    } else if (expr == WUBU_BUDDY_FACE_SAD) {
        face_arc(buf, w, h, ex, my, mw, 0);      /* corners drop -> frown */
    } else if (expr == WUBU_BUDDY_FACE_OPEN) {
        for (int dx = -mw / 2; dx <= mw / 2; ++dx)
            for (int dy = 0; dy < 3; ++dy)
                put(buf, w, h, ex + dx, my + dy, kFaceDark);
    } else {
        face_line(buf, w, h, ex, my, mw / 2);
    }
}

void wubu_buddy_rig_draw(const wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_],
                         int face,
                         uint32_t *buf, int w, int h)
{
    /* clear to transparent */
    memset(buf, 0, (size_t)w * h * sizeof(uint32_t));

    /* paste parts in z-order */
    for (int oi = 0; oi < WUBU_BUDDY_PART_COUNT_; ++oi) {
        int p = kDrawOrder[oi];
        const wubu_buddy_part_def_t *def = &kBuddyParts[p];
        int dx = xf[p].dx, dy = xf[p].dy;
        for (uint16_t i = 0; i < def->n; ++i) {
            int off = def->px[i];
            int px = off % BUDDY_W + dx;
            int py = off / BUDDY_W + dy;
            uint8_t ci = kBuddyIdx[off];
            if (ci == 0xFF) continue;
            put(buf, w, h, px, py, kBuddyPalette[ci]);
        }
    }

    if (face >= 0 && face < WUBU_BUDDY_FACE_COUNT_)
        apply_face(buf, w, h, face);
}
