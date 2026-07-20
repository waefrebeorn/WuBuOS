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

    /* -- Read snapshot API (the disclosure surface) -- */
    edr_analytics_set_enabled(true);
    wubu_ui_click(300, 300, 1);   /* ensure at least one fresh agent event */
    EdrEventView buf[64];
    int n = edr_recent_events(buf, 64, 0, 0);
    T(n > 0, "edr_recent_events() returns a non-empty snapshot");
    /* Find an agent action in the snapshot. */
    int saw_agent = 0;
    for (int i = 0; i < n; i++)
        if (buf[i].type == EDR_EV_AGENT_ACTION) { saw_agent = 1; break; }
    T(saw_agent, "snapshot contains a real EDR_EV_AGENT_ACTION event");

    /* Filtering by type window 26..26 yields ONLY agent events. */
    EdrEventView fbuf[64];
    int fn = edr_recent_events(fbuf, 64, 26, 26);
    int only_agent = 1;
    for (int i = 0; i < fn; i++)
        if (fbuf[i].type != EDR_EV_AGENT_ACTION) { only_agent = 0; break; }
    T(fn > 0 && only_agent, "type-filtered snapshot returns only agent actions");

    /* Names are human-readable (for the dashboard). */
    T(strcmp(edr_event_type_name(EDR_EV_AGENT_ACTION), "AgentAction") == 0,
      "edr_event_type_name maps the agent type");
    T(strcmp(edr_agent_action_name(EDR_AGENT_MOUSE_DOWN), "MouseDown") == 0,
      "edr_agent_action_name maps the sub-type");

    /* Snapshot is NON-destructive: a second call returns the same data. */
    EdrEventView buf2[64];
    int n2 = edr_recent_events(buf2, 64, 0, 0);
    T(n2 == n, "edr_recent_events is a non-destructive snapshot (count stable)");

    dosgui_wm_shutdown();
    vbe_shutdown();
    edr_stop();
    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
