/* wubu_buddy_rig.h -- WuBu Buddy pixel rig renderer (C11, opaque API).
 *
 * Draws the canonical mascot from baked part data (wubu_buddy_rig_data.h):
 * one base sprite, segmented into anatomical parts, moved by per-part
 * transforms. The facial rig redraws eyes/mouth inside the head region.
 * Any pose x any expression renders on the sprite's OWN pixels -- no redraw,
 * no second copy of the art. */
#ifndef WUBU_BUDDY_RIG_H
#define WUBU_BUDDY_RIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Part indices -- match the baked order in wubu_buddy_rig_data.h. */
enum {
    WUBU_BUDDY_PART_EARS = 0,
    WUBU_BUDDY_PART_CAP,
    WUBU_BUDDY_PART_HEAD,
    WUBU_BUDDY_PART_TORSO,
    WUBU_BUDDY_PART_ARM_L,
    WUBU_BUDDY_PART_ARM_R,
    WUBU_BUDDY_PART_LEG_L,
    WUBU_BUDDY_PART_LEG_R,
    WUBU_BUDDY_PART_COUNT_
};

/* Facial expressions. -1 = leave the sprite's baked face untouched. */
enum {
    WUBU_BUDDY_FACE_NONE = -1,
    WUBU_BUDDY_FACE_OPEN = 0,
    WUBU_BUDDY_FACE_BLINK,
    WUBU_BUDDY_FACE_HAPPY,
    WUBU_BUDDY_FACE_WINK,
    WUBU_BUDDY_FACE_SAD,
    WUBU_BUDDY_FACE_SMILE,
    WUBU_BUDDY_FACE_WIDE,
    WUBU_BUDDY_FACE_COUNT_
};

typedef struct {
    int8_t dx, dy;      /* per-part pixel offset this frame */
} wubu_buddy_xform_t;

/* Blit the buddy into an RGBA buffer (w*h*4 bytes, 0xRRGGBBAA pixels).
 * xf[BUDDY_PART_N] holds each part's offset; face selects the expression
 * layer (-1 keeps the baked face). Parts paste in anatomical z-order. */
void wubu_buddy_rig_draw(const wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_],
                         int face,
                         uint32_t *buf, int w, int h);

/* Fill a 96x96 transform set for an 8-frame walk cycle (frame & 7). */
void wubu_buddy_rig_pose_walk(int frame,
                              wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_]);

/* Fill a 96x96 transform set for an 8-frame wave cycle (frame & 7). */
void wubu_buddy_rig_pose_wave(int frame,
                              wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_]);

/* Zero every transform (rest pose). */
void wubu_buddy_rig_pose_rest(wubu_buddy_xform_t xf[WUBU_BUDDY_PART_COUNT_]);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_BUDDY_RIG_H */
