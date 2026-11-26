/*
 * wubu_gui_metrics.h -- the WuBuOS GUI metric system (Longhorn-derived).
 *
 * Longhorn's design language (2003-2007 Microsoft research) is what Windows
 * Vista/7 shipped from: generous, CONSISTENT margins; a strict type ramp; a
 * chrome-to-content relationship where window furniture never crowds content;
 * and an OOBE that walks the user through setup instead of dumping a desktop.
 *
 * The failure mode this replaces: every surface inventing its own spacing
 * numbers, so margins disagree between the greeter, the start menu, the settings
 * panels and the applets. Fixing "bad GUI" means one owner for metrics, and
 * every drawing site asking for them.
 *
 * Rules enforced here (not by taste):
 *   - everything sits on a 4 px grid;
 *   - margins are a fixed ascending ramp, never ad-hoc;
 *   - interactive controls are >= 32 px tall, touch targets >= 40 px;
 *   - the type ramp is fixed (no arbitrary font sizes at call sites).
 */
#ifndef WUBU_GUI_METRICS_H
#define WUBU_GUI_METRICS_H

/* Canonical compile-time values. Headers that need the same numbers without a
 * runtime struct use these; wubu_gui_metrics_default() copies them, so there is
 * exactly one place where the numbers are decided. */
#define WUBU_METRIC_UNIT        4
#define WUBU_METRIC_GAP_XS      4
#define WUBU_METRIC_GAP_SM      8
#define WUBU_METRIC_GAP_MD      12
#define WUBU_METRIC_GAP_LG      16
#define WUBU_METRIC_MARGIN_XS   4
#define WUBU_METRIC_MARGIN_SM   8
#define WUBU_METRIC_MARGIN_MD   16
#define WUBU_METRIC_MARGIN_LG   24
#define WUBU_METRIC_ROW_H       24
/* Window open animation (Win11): 140ms ease-out. A window must not pop: on Windows it grows
 * into place. The duration is a metric like every other number in the shell. */
#define WUBU_ANIM_OPEN_MS       140
#define WUBU_ANIM_OPEN_FROM_PCT 85   /* opens from 85% of its height */

#define WUBU_METRIC_CONTROL_H   32

/* Modal dialog: the Print dialog's size. Owned here like every other surface number, so the
 * dialog's layout and the WM's idea of it cannot disagree. */
#define WUBU_METRIC_DIALOG_W    460
#define WUBU_METRIC_DIALOG_H    340
#define WUBU_METRIC_TOUCH_H     40
#define WUBU_METRIC_TITLEBAR_H  32   /* window caption (Win11-faithful) */
#define WUBU_METRIC_TASKBAR_H    48   /* taskbar height */
#define WUBU_METRIC_TITLE_H      32   /* Fluent title bar (Win11); legacy is 24 */
#define WUBU_METRIC_BORDER_HAIRLINE 1 /* Fluent border: a hairline, never a frame */
#define WUBU_METRIC_SCROLLBAR_W 12
#define WUBU_METRIC_BORDER_W      2   /* hairline frame width */
#define WUBU_METRIC_RADIUS_CONTROL 4
#define WUBU_METRIC_RADIUS_WINDOW  8

/* Motion and pointer thresholds. These are Win11's numbers, not preferences:
 * a click that has not moved DRAG_THRESHOLD px is still a click (SM_CXDRAG),
 * and a snapped window leaves SNAP_GAP px to its neighbour so two halves read
 * as two windows instead of one seam. Before this, the WM had no threshold at
 * all -- every 1 px tremor during a press started a drag, which is why windows
 * "jumped" when the user only meant to click. */
#define WUBU_METRIC_DRAG_THRESHOLD 4
#define WUBU_METRIC_SNAP_GAP       8

#include <stdint.h>

typedef struct WubuGuiMetrics {
    int unit;                    /* base grid, always 4 */

    int margin_xs;               /* 4  - inline icon/text breathing room */
    int margin_sm;               /* 8  - stacked tight controls          */
    int margin_md;               /* 16 - standard content inset         */
    int margin_lg;               /* 24 - window/section gutter          */

    int gap_xs;                  /* 2  */
    int gap_sm;                  /* 4  */
    int gap_md;                  /* 8  */
    int gap_lg;                  /* 16 */

    int row_h;                   /* list row              (>= 24) */
    int control_h;               /* button/input          (>= 32) */
    int touch_h;                 /* pointer+touch target  (>= 40) */
    int titlebar_h;              /* window caption        (>= 28) */
    int taskbar_h;               /* taskbar band          (>= 40) */
    int scrollbar_w;             /*                      (>= 12) */

    int radius;                  /* Longhorn chrome is square-ish: 2 */

    int type_caption;            /* 9  */
    int type_body;               /* 12 */
    int type_lead;               /* 14 */
    int type_title;              /* 20 */
    int type_hero;               /* 34 */

    int snap_tol;                /* rounding tolerance for layout math */
} WubuGuiMetrics;

/* Populate the canonical set. All values are Longhorn-derived, not tuned. */
void wubu_gui_metrics_default(WubuGuiMetrics *m);

/* Round a pixel value to the nearest grid multiple (ties toward zero). */
int  wubu_gui_snap(int v, int unit);

/* Height for a control: the requested value, clamped up to control_h. */
int  wubu_gui_clamp_control(int h, const WubuGuiMetrics *m);

/* 1 when a rect is big enough to be hit reliably, else 0. */
int  wubu_gui_hit_ok(int w, int h, const WubuGuiMetrics *m);

/* 1 when the rect satisfies the stricter touch target, else 0. */
int  wubu_gui_touch_ok(int w, int h, const WubuGuiMetrics *m);

#endif /* WUBU_GUI_METRICS_H */
