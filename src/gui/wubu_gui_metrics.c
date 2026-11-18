/*
 * wubu_gui_metrics.c -- the canonical GUI metric set (see wubu_gui_metrics.h).
 *
 * C11, minimal includes, self-contained. One owner for every spacing and type
 * number the desktop draws with; surfaces call in instead of inventing values.
 */
#include "wubu_gui_metrics.h"

void wubu_gui_metrics_default(WubuGuiMetrics *m)
{
    if (!m) return;
    m->unit       = 4;

    m->margin_xs  = 4;
    m->margin_sm  = 8;
    m->margin_md  = 16;
    m->margin_lg  = 24;

    m->gap_xs     = 4;
    m->gap_sm     = 8;
    m->gap_md     = 12;
    m->gap_lg     = 16;

    m->row_h      = 24;
    m->control_h  = 32;
    m->touch_h    = 40;
    m->titlebar_h = 28;
    m->scrollbar_w = 12;

    m->radius     = 2;

    m->type_caption = 9;
    m->type_body    = 12;
    m->type_lead    = 14;
    m->type_title   = 20;
    m->type_hero    = 34;

    m->snap_tol   = 1;
}

int wubu_gui_snap(int v, int unit)
{
    if (unit <= 0) return v;
    int q = v / unit;
    int r = v - q * unit;
    if (r < 0) r = -r;           /* magnitude: ties round away from zero */
    if (r * 2 >= unit) q += (v >= 0) ? 1 : -1;
    return q * unit;
}

int wubu_gui_clamp_control(int h, const WubuGuiMetrics *m)
{
    if (!m || h >= m->control_h) return h;
    return m->control_h;
}

int wubu_gui_hit_ok(int w, int h, const WubuGuiMetrics *m)
{
    if (!m) return 0;
    return w >= m->control_h && h >= m->control_h;
}

int wubu_gui_touch_ok(int w, int h, const WubuGuiMetrics *m)
{
    if (!m) return 0;
    return w >= m->touch_h && h >= m->touch_h;
}
