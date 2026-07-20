/*
 * wubu_holyc_agi.c -- Live ring-0 compiler AGI layer (TempleOS "God compiler")
 *
 * See wubu_holyc_agi.h. Owns an in-process holyd daemon + persistent
 * "default" session; bridges the HolyC Terminal and the AGI to the real
 * HolyC JIT compiler. Every AGI eval is disclosed to EDR.
 */

#include "wubu_holyc_agi.h"
#include "wubu_holyd.h"
#include "wubu_edr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* The single in-process daemon instance (lazy-initialized). */
static WubuHoly *g_agi_holyd = NULL;
static char      g_agi_session[WUBU_HOLYD_MAX_SESSION_NAME] = "default";

int wubu_holyc_agi_init(void) {
    if (g_agi_holyd) return 0;

    WubuHoly *d = (WubuHoly *)calloc(1, sizeof(WubuHoly));
    if (!d) return -1;

    WubuHolyConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.sessions_path, "/tmp/wubu/holyc", sizeof(cfg.sessions_path) - 1);
    strncpy(cfg.socket_path,  "/tmp/wubu/holyc.sock", sizeof(cfg.socket_path) - 1);
    strncpy(cfg.log_path,     "/tmp/wubu/holyc.log",  sizeof(cfg.log_path) - 1);
    cfg.log_level = 0;

    if (wubu_holyd_init(d, &cfg) != 0) { free(d); return -1; }
    if (wubu_holyd_session_create(d, g_agi_session, 800, 600) != 0) {
        /* Session may already exist from a prior init in this process; that's
         * fine -- the REPL state simply persists. */
    }
    g_agi_holyd = d;
    return 0;
}

static int do_eval(const char *src, char *out, size_t out_size, int log_to_edr) {
    if (!g_agi_holyd) wubu_holyc_agi_init();
    if (!g_agi_holyd) {
        snprintf(out, out_size, "HolyC AGI layer not initialized");
        return -1;
    }

    int ret = wubu_holyd_eval(g_agi_holyd, g_agi_session, src, out, out_size);

    if (log_to_edr) {
        /* Disclose the agent's compile+run as a first-class EDR action. The
         * detail carries the actual source so a human can audit exactly what
         * code the operating system executed on the agent's behalf. */
        char detail[256];
        snprintf(detail, sizeof(detail), "holyc: %s", src);
        edr_log_agent_action(EDR_AGENT_KEY, 0, 0, 0, 0, detail);
    }
    return ret;
}

int wubu_holyc_eval(const char *src, char *out, size_t out_size) {
    return do_eval(src, out, out_size, 0);
}

int wubu_holyc_agent_eval(const char *src, char *out, size_t out_size) {
    return do_eval(src, out, out_size, 1);
}

void wubu_holyc_agi_shutdown(void) {
    if (g_agi_holyd) {
        wubu_holyd_shutdown(g_agi_holyd);
        free(g_agi_holyd);
        g_agi_holyd = NULL;
    }
}
