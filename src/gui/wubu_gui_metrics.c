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
    m->unit       = WUBU_METRIC_UNIT;

    m->margin_xs  = WUBU_METRIC_MARGIN_XS;
    m->margin_sm  = WUBU_METRIC_MARGIN_SM;
    m->margin_md  = WUBU_METRIC_MARGIN_MD;
    m->margin_lg  = WUBU_METRIC_MARGIN_LG;

    m->gap_xs     = WUBU_METRIC_GAP_XS;
    m->gap_sm     = WUBU_METRIC_GAP_SM;
    m->gap_md     = WUBU_METRIC_GAP_MD;
    m->gap_lg     = WUBU_METRIC_GAP_LG;

    m->row_h      = WUBU_METRIC_ROW_H;
    m->control_h  = WUBU_METRIC_CONTROL_H;
    m->touch_h    = WUBU_METRIC_TOUCH_H;
    m->titlebar_h = WUBU_METRIC_TITLEBAR_H;
    m->scrollbar_w = WUBU_METRIC_SCROLLBAR_W;

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
