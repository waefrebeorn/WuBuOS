/*
 * wubufx.c -- WuBuFX: WuBuOS Application Framework (implementation)
 *
 * Self-contained. Composition root wires the LIVE HolyC daemon (wubu_holyd)
 * and the EDR engine (wubu_edr.*) into a namespace-first app model.
 *
 * Isolation (the .NET "DLL hell" antidote): each mounted app gets its OWN
 * HolyC session inside the holyd daemon, keyed by the app's content hash.
 * Globals defined in one app's session cannot collide with another's -- the
 * TempleOS REPL persistence is per-namespace, not global.
 *
 * EDR disclosure: every eval (human or AGI) is logged. The AGI path uses
 * edr_log_agent_action; there is no bypass.
 *
 * C11, minimal includes, opaque structs (see wubufx.h for the public face).
 */

#include "wubufx.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../runtime/wubu_holyd.h"   /* LIVE HolyC daemon + per-session eval */
#include "../runtime/wubu_edr.h"     /* EDR disclosure surface */

/* -- Internal (opaque) types ---------------------------------------- */

struct WubuFxNode {
    char      path[256];
    WubuFxCap caps;
    char      state[1024];   /* explicit, inspectable (plan point 28) */
    bool      state_set;
};

struct WubuFxApp {
    char       id[256];
    uint32_t   id_hash;       /* content address (no DLL-lottery, point 2) */
    WubuFxCap  caps;          /* least-privilege, point 47/81 */
    char       session[64];   /* per-app HolyC session name (isolation) */
    WubuFxNode nodes[32];
    int        node_count;
    bool       mounted;
};

/* One daemon for the whole framework; sessions are per-app. */
static WubuHoly *g_fx_holyd = NULL;

/* -- Small content hash (FNV-1a) -- capability addressing ------------ */
static uint32_t fx_hash(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

/* -- Error strings --------------------------------------------------- */
const char *wubufx_strerr(WubuFxErr e) {
    switch (e) {
        case WUBUFX_OK:           return "ok";
        case WUBUFX_ERR_PARAM:    return "bad parameter";
        case WUBUFX_ERR_NOMOUNT:  return "namespace not mounted";
        case WUBUFX_ERR_NOENT:    return "node not found";
        case WUBUFX_ERR_CAP:      return "capability denied (EDR)";
        case WUBUFX_ERR_EVAL:     return "HolyC eval failed";
        case WUBUFX_ERR_SIGN:     return "signature/attestation failed";
        case WUBUFX_ERR_LIMIT:    return "resource limit exceeded";
        case WUBUFX_ERR_INTERNAL: return "internal error";
    }
    return "unknown";
}

/* -- Composition root ------------------------------------------------ */

WubuFxErr wubufx_init(void) {
    if (g_fx_holyd) return WUBUFX_OK;   /* idempotent */

    WubuHoly *d = (WubuHoly *)calloc(1, sizeof(WubuHoly));
    if (!d) return WUBUFX_ERR_INTERNAL;

    WubuHolyConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.sessions_path, "/tmp/wubu/fx", sizeof(cfg.sessions_path) - 1);
    strncpy(cfg.socket_path,   "/tmp/wubu/fx.sock", sizeof(cfg.socket_path) - 1);
    strncpy(cfg.log_path,      "/tmp/wubu/fx.log", sizeof(cfg.log_path) - 1);
    cfg.log_level = 0;

    if (wubu_holyd_init(d, &cfg) != 0) { free(d); return WUBUFX_ERR_INTERNAL; }
    g_fx_holyd = d;

    edr_log_event((uint16_t)0, 0, 0, 0, 0, 0, "wubufx: framework initialized");
    return WUBUFX_OK;
}

void wubufx_shutdown(void) {
    edr_log_event((uint16_t)0, 0, 0, 0, 0, 0, "wubufx: framework shutdown");
    if (g_fx_holyd) { wubu_holyd_shutdown(g_fx_holyd); free(g_fx_holyd); g_fx_holyd = NULL; }
}

/* -- Lifecycle ------------------------------------------------------- */

WubuFxErr wubufx_mount(const char *id, WubuFxCap caps, WubuFxApp **out_app) {
    if (!id || !*id || !out_app) return WUBUFX_ERR_PARAM;
    *out_app = NULL;
    if (!g_fx_holyd && wubufx_init() != WUBUFX_OK) return WUBUFX_ERR_INTERNAL;

    uint32_t h = fx_hash(id);

    /* Attestation hook (point 5/53): a real mount verifies the signature via
     * EDR here. We record the intent as a disclosed event. */
    edr_log_event(EDR_EV_IMAGE_LOAD, 0, 0, h, 0, 0, id);

    WubuFxApp *app = (WubuFxApp *)calloc(1, sizeof(*app));
    if (!app) return WUBUFX_ERR_INTERNAL;
    strncpy(app->id, id, sizeof(app->id) - 1);
    app->id_hash = h;
    app->caps    = caps & WUBUFX_CAP_ALL;
    app->mounted = true;

    /* Per-app HolyC session: globals are isolated to THIS namespace. */
    snprintf(app->session, sizeof(app->session), "fx_%08x", h);
    wubu_holyd_session_create(g_fx_holyd, app->session, 800, 600);

    /* Seed conventional nodes (real inspectable nodes, no magic). */
    const char *seed[] = { "/win", "/main", "/state", "/sys" };
    for (size_t i = 0; i < sizeof(seed)/sizeof(seed[0]) && app->node_count < 32; i++) {
        WubuFxNode *n = &app->nodes[app->node_count++];
        strncpy(n->path, seed[i], sizeof(n->path) - 1);
        n->caps = caps & WUBUFX_CAP_ALL;
        n->state_set = false;
    }

    *out_app = app;
    return WUBUFX_OK;
}

void wubufx_close(WubuFxApp *app) { (void)app; }

void wubufx_unmount(WubuFxApp *app) {
    if (!app) return;
    if (g_fx_holyd) wubu_holyd_session_destroy(g_fx_holyd, app->session);
    edr_log_event(EDR_EV_PROCESS_EXIT, 0, 0, app->id_hash, 0, 0, app->id);
    free(app);
}

WubuFxNode *wubufx_open(WubuFxApp *app, const char *path) {
    if (!app || !app->mounted || !path) return NULL;
    for (int i = 0; i < app->node_count; i++)
        if (strcmp(app->nodes[i].path, path) == 0) return &app->nodes[i];

    /* Lazily create unknown nodes: a namespace grows by use, not by manifest
     * bloat (point 9). Bounded. */
    if (app->node_count < 32) {
        WubuFxNode *n = &app->nodes[app->node_count++];
        strncpy(n->path, path, sizeof(n->path) - 1);
        n->caps = app->caps;
        n->state_set = false;
        return n;
    }
    return NULL;
}

void wubufx_node_close(WubuFxNode *node) { (void)node; }

/* -- Execution (LIVE HolyC, namespace-isolated) --------------------- */

WubuFxErr wubufx_eval(WubuFxApp *app, const char *src, char *out, size_t n) {
    if (!app || !app->mounted || !src || !out || n == 0) return WUBUFX_ERR_PARAM;
    if (!(app->caps & WUBUFX_CAP_EXEC)) {
        edr_log_event(EDR_EV_AGENT_ACTION, 0, 0, app->id_hash, 0, 0, "wubufx_eval: cap denied");
        return WUBUFX_ERR_CAP;
    }
    if (!g_fx_holyd) return WUBUFX_ERR_INTERNAL;

    edr_log_event(EDR_EV_SCRIPT_EXECUTION, 0, 0, app->id_hash, 0, 0, app->id);

    int r = wubu_holyd_eval(g_fx_holyd, app->session, src, out, n);
    if (r != 0) {
        if (strlen(out) == 0) { strncpy(out, "eval error", n - 1); out[n-1]='\0'; }
        return WUBUFX_ERR_EVAL;
    }
    return WUBUFX_OK;
}

WubuFxErr wubufx_agent_eval(WubuFxApp *app, const char *src, char *out, size_t n) {
    if (!app || !app->mounted || !src || !out || n == 0) return WUBUFX_ERR_PARAM;
    if (!(app->caps & WUBUFX_CAP_AGI)) {
        edr_log_event(EDR_EV_AGENT_ACTION, 0, 0, app->id_hash, 0, 0, "wubufx_agent_eval: cap denied");
        return WUBUFX_ERR_CAP;
    }
    if (!g_fx_holyd) return WUBUFX_ERR_INTERNAL;

    /* The ONLY path an AGI may execute through. EDR-disclosed by construction;
     * no bypass (point 44/52). */
    edr_log_agent_action(EDR_AGENT_KEY, 0, 0, 0, 0, app->id);

    int r = wubu_holyd_eval(g_fx_holyd, app->session, src, out, n);
    if (r != 0) {
        if (strlen(out) == 0) { strncpy(out, "agent eval error", n - 1); out[n-1]='\0'; }
        return WUBUFX_ERR_EVAL;
    }
    return WUBUFX_OK;
}

/* -- State (explicit, inspectable) ----------------------------------- */

WubuFxErr wubufx_state_get(WubuFxNode *node, char *out, size_t n) {
    if (!node || !out || n == 0) return WUBUFX_ERR_PARAM;
    if (node->state_set) { strncpy(out, node->state, n - 1); out[n-1] = '\0'; }
    else out[0] = '\0';
    return WUBUFX_OK;
}

WubuFxErr wubufx_state_set(WubuFxNode *node, const char *value) {
    if (!node || !value) return WUBUFX_ERR_PARAM;
    if (!(node->caps & WUBUFX_CAP_WRITE)) return WUBUFX_ERR_CAP;
    strncpy(node->state, value, sizeof(node->state) - 1);
    node->state[sizeof(node->state) - 1] = '\0';
    node->state_set = true;
    return WUBUFX_OK;
}
