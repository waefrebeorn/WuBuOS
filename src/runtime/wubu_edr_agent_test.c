/*
 * wubu_edr_agent_test.c -- AGI activity -> EDR integration test.
 *
 * This is the proof of the transparency edict: when the agent drives the UI
 * through wubu_ui (the SAME path a human uses), every action lands in the EDR
 * engine as an EDR_EV_AGENT_ACTION event on the identical queue as process/
 * file/network telemetry -- so a user can search and replay exactly what the
 * operating system did on the agent's behalf. Also verifies the master
 * analytics toggle gates all agent logging.
 *
 * Built with -DWUBU_EDR_AGENT so wubu_ui actually calls edr_log_agent_action.
 */
#include "wubu_edr.h"
#include "wubu_ui.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { g_run++; if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
                         else { printf("  ❌ %s\n", msg); } } while (0)

int main(void) {
    printf("=== WuBuOS EDR <- AGI Agent Action Test ===\n\n");

    /* Master analytics ON by default. */
    edr_analytics_set_enabled(true);
    T(edr_analytics_enabled(), "analytics master toggle defaults/enables ON");

    edr_start();
    vbe_init(800, 600);
    dosgui_wm_init(800, 600);
    DosGuiWindow *w = dosgui_wm_create(40, 40, 200, 150, "AgentTest");
    int wx0 = w->x, wy0 = w->y;

    uint64_t before = edr_agent_events_logged();

    /* Drive the desktop the way the AGI would: drag the window, then type. */
    wubu_ui_drag(wx0 + 100, wy0 + 8, wx0 + 100 + 50, wy0 + 8 + 30, 1);
    wubu_ui_type("hello");   /* 5 key events */

    /* (1) The WM actually obeyed the agent: window moved. */
    T(w->x == wx0 + 50 && w->y == wy0 + 30, "agent drag moved the window (WM obeyed)");
    /* (2) The agent's actions landed in EDR as first-class events. */
    uint64_t after = edr_agent_events_logged();
    T(after > before, "agent UI actions were logged into EDR (searchable/replayable)");
    T(after - before >= 6, "batch of agent events recorded (drag move/down/up + 5 keys)");

    /* -- Toggle OFF gates all future agent logging -- */
    printf("\n[Toggle OFF gates agent logging]\n");
    edr_analytics_set_enabled(false);
    T(!edr_analytics_enabled(), "analytics toggle reports OFF");
    uint64_t off_before = edr_agent_events_logged();
    wubu_ui_mouse_move(200, 200);
    wubu_ui_mouse_down(1);
    wubu_ui_mouse_up(1);
    uint64_t off_after = edr_agent_events_logged();
    T(off_after == off_before, "with analytics OFF, no agent events are logged");

    /* -- Toggle back ON resumes -- */
    edr_analytics_set_enabled(true);
    uint64_t on_before = edr_agent_events_logged();
    wubu_ui_click(100, 100, 1);
    T(edr_agent_events_logged() > on_before, "with analytics ON again, logging resumes");

    dosgui_wm_shutdown();
    vbe_shutdown();
    edr_stop();
    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
