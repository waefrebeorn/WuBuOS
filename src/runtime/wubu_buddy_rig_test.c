/* wubu_buddy_rig_test.c -- gate for the WuBu Buddy pixel rig renderer.
 *
 * Asserts the baked rig actually draws the mascot: rest pose has mass,
 * walk/wave frames MOVE parts, rendering is deterministic, and every facial
 * expression is distinct. Run: ./test_buddy_rig  (prints "N pass, M fail").
 */
#include "wubu_buddy_rig.h"
#include "wubu_buddy_rig_data.h"   /* cell dimensions (BUDDY_W/H) */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CW BUDDY_W
#define CH BUDDY_H

static int g_pass, g_fail;

static void check(int cond, const char *what)
{
    if (cond) { ++g_pass; }
    else { ++g_fail; printf("[wubu_buddy_rig] FAIL: %s\n", what); }
}

static uint32_t g_buf[CW * CH];

static int opaque(void)
{
    int n = 0;
    for (int i = 0; i < CW * CH; ++i)
        if ((g_buf[i] & 0xFFu) != 0) ++n;      /* 0xRRGGBBAA -> alpha = low byte */
    return n;
}

static int diff_pixels(const uint32_t *a, const uint32_t *b)
{
    int n = 0;
    for (int i = 0; i < CW * CH; ++i)
        if (a[i] != b[i]) ++n;
    return n;
}

static void render(const wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_], int face)
{
    wubu_buddy_rig_draw(xf, face, g_buf, CW, CH);
}

int main(void)
{
    wubu_buddy_xform_t rest[WUBU_BUDDY_PART_COUNT_];
    wubu_buddy_xform_t a[WUBU_BUDDY_PART_COUNT_], b[WUBU_BUDDY_PART_COUNT_];

    /* rest pose: character present, deterministic */
    wubu_buddy_rig_pose_rest(rest);
    render(rest, WUBU_BUDDY_FACE_NONE);
    int n_rest = opaque();
    uint32_t first[CW * CH];
    memcpy(first, g_buf, sizeof(g_buf));
    render(rest, WUBU_BUDDY_FACE_NONE);
    check(memcmp(first, g_buf, sizeof(g_buf)) == 0, "rest render deterministic");
    check(n_rest > 800, "rest pose has real mass");

    /* walk: frames move parts, loop wraps */
    wubu_buddy_rig_pose_walk(0, a);
    wubu_buddy_rig_pose_walk(2, b);
    render(a, WUBU_BUDDY_FACE_NONE);
    uint32_t wa[CW * CH]; memcpy(wa, g_buf, sizeof(g_buf));
    render(b, WUBU_BUDDY_FACE_NONE);
    check(diff_pixels(wa, g_buf) > 40, "walk frames move parts");
    wubu_buddy_rig_pose_walk(0, a);
    wubu_buddy_rig_pose_walk(8, b);
    check(memcmp(a, b, sizeof(a)) == 0, "walk cycle wraps at 8");

    /* wave: arm moves */
    wubu_buddy_rig_pose_wave(0, a);
    wubu_buddy_rig_pose_wave(4, b);
    render(a, WUBU_BUDDY_FACE_NONE);
    memcpy(wa, g_buf, sizeof(g_buf));
    render(b, WUBU_BUDDY_FACE_NONE);
    check(diff_pixels(wa, g_buf) > 20, "wave frames move arm");

    /* faces: each expression distinct from open */
    int faces[] = {WUBU_BUDDY_FACE_OPEN, WUBU_BUDDY_FACE_BLINK,
                   WUBU_BUDDY_FACE_HAPPY, WUBU_BUDDY_FACE_WINK,
                   WUBU_BUDDY_FACE_SAD,   WUBU_BUDDY_FACE_SMILE};
    int nf = (int)(sizeof(faces) / sizeof(faces[0]));
    render(rest, faces[0]);
    uint32_t base[CW * CH]; memcpy(base, g_buf, sizeof(g_buf));
    for (int i = 1; i < nf; ++i) {
        render(rest, faces[i]);
        check(diff_pixels(base, g_buf) > 10, "expression differs from open");
    }
    /* blink eyes differ from open eyes specifically */
    render(rest, WUBU_BUDDY_FACE_OPEN);
    memcpy(base, g_buf, sizeof(g_buf));
    render(rest, WUBU_BUDDY_FACE_BLINK);
    check(diff_pixels(base, g_buf) > 4, "blink changes the eyes");

    printf("[wubu_buddy_rig] %d pass, %d fail (rest opaque=%d)\n",
           g_pass, g_fail, n_rest);
    return g_fail ? 1 : 0;
}
