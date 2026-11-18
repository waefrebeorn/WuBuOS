/*
 * test_wubu_gui_metrics.c -- the metric invariants, proven.
 *
 * If a surface ever starts inventing its own spacing these assertions are what
 * break, because the test pins the Longhorn-derived relationships: grid, ramp
 * ordering, minimum hit/touch targets and snap behaviour.
 */
#include "wubu_gui_metrics.h"

#include <stdio.h>
#include <stdlib.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); g_fails++; } } while (0)

int main(void)
{
    WubuGuiMetrics m;
    wubu_gui_metrics_default(&m);

    /* the 4 px grid is the root of every other number */
    CHECK(m.unit == 4);
    CHECK(m.margin_xs % m.unit == 0 && m.margin_sm % m.unit == 0);
    CHECK(m.margin_md % m.unit == 0 && m.margin_lg % m.unit == 0);
    CHECK(m.gap_xs % m.unit == 0 && m.gap_sm % m.unit == 0);
    CHECK(m.gap_md % m.unit == 0 && m.gap_lg % m.unit == 0);
    CHECK(m.row_h % m.unit == 0 && m.control_h % m.unit == 0);
    CHECK(m.touch_h % m.unit == 0 && m.titlebar_h % m.unit == 0);

    /* margins strictly ascend; no ad-hoc values can squeeze between them */
    CHECK(m.margin_xs < m.margin_sm);
    CHECK(m.margin_sm < m.margin_md);
    CHECK(m.margin_md < m.margin_lg);
    CHECK(m.gap_xs <= m.gap_sm && m.gap_sm <= m.gap_md && m.gap_md <= m.gap_lg);

    /* hit / touch floors: this is what fixes cramped, mis-clicked controls */
    CHECK(m.control_h >= 32);
    CHECK(m.touch_h   >= 40);
    CHECK(m.touch_h   >= m.control_h);
    CHECK(m.scrollbar_w >= 12);

    /* type ramp ascends */
    CHECK(m.type_caption < m.type_body);
    CHECK(m.type_body    < m.type_lead);
    CHECK(m.type_lead    < m.type_title);
    CHECK(m.type_title   < m.type_hero);

    /* snapping keeps layout on the grid */
    CHECK(wubu_gui_snap(13, m.unit) == 12);
    CHECK(wubu_gui_snap(14, m.unit) == 16);
    CHECK(wubu_gui_snap(15, m.unit) == 16);
    CHECK(wubu_gui_snap(0,  m.unit) == 0);
    CHECK(wubu_gui_snap(-7, m.unit) == -8);
    CHECK(wubu_gui_snap(13, 0)  == 13);   /* degenerate unit is a no-op */

    /* control heights clamp up, never down */
    CHECK(wubu_gui_clamp_control(10, &m) == m.control_h);
    CHECK(wubu_gui_clamp_control(m.control_h, &m) == m.control_h);
    CHECK(wubu_gui_clamp_control(64, &m) == 64);
    CHECK(wubu_gui_clamp_control(10, NULL) == 10);

    /* hit testing is honest about size */
    CHECK(!wubu_gui_hit_ok(20, 20, &m));
    CHECK( wubu_gui_hit_ok(m.control_h, m.control_h, &m));
    CHECK(!wubu_gui_touch_ok(m.control_h, m.control_h, &m));
    CHECK( wubu_gui_touch_ok(m.touch_h, m.touch_h, &m));
    CHECK(!wubu_gui_hit_ok(40, 40, NULL));

    /* NULL metrics must not crash and must yield nothing */
    wubu_gui_metrics_default(NULL);
    CHECK(wubu_gui_clamp_control(999, NULL) == 999);

    if (g_fails) {
        printf("wubu_gui_metrics: %d FAILED\n", g_fails);
        return 1;
    }
    printf("ALL GUI METRIC TESTS PASSED\n");
    return 0;
}
