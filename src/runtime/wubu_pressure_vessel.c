/*
 * wubu_pressure_vessel.c -- the PRESSURE-VESSEL preset (SteamOS
 * EPIC E2 gap: "Pressure Vessel (runtime container) not implemented").
 *
 * Pressure Vessel is how SteamOS runs a game inside a runtime-locked
 * container: the game sees ONLY the Steam Linux Runtime library stack
 * (sniper/soldier) plus its OWN libraries layered on top, so it can
 * never dangle on a host distro library. The real mechanism (FOSDEM
 * 2020 Collabora talk + the pressure-vessel source):
 *   1. a mount namespace with the runtime base ro-bound
 *   2. the game's own lib dirs bound over it
 *   3. a curated LD_LIBRARY_PATH: game libs FIRST, runtime libs
 *      after — the game wins, the runtime fills the gaps
 *   4. the GPU + Wayland + audio surfaces exposed
 *
 * This preset builds the SAME launch on WuBuOS's bwrap container:
 *   - wubu_pv_create(name, runtime_path, game_lib_dir, game_exe):
 *     a WubuCt with the runtime binds + the env already set
 *   - wubu_pv_launch(): start it via the bwrap runtime
 *
 * C11, self-contained.
 */
#include "wubu_host_exec.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* the runtime base + the game lib layer */
typedef struct {
    WubuCt ct;
    char runtime_lib[512];   /* the Steam Linux Runtime lib dir */
    char game_lib[512];      /* the game's own lib dir (may be "") */
    char game_exe[512];      /* the game binary (may be "") */
    char ld_library_path[4096];
    int  ld_set;
} WubuPv;

/* PV1: create the pressure-vessel container. runtime_lib = the Steam
 * Linux Runtime library base (e.g. /usr/lib/steamrt/sniper/lib/x86_64-linux-gnu
 * or an extracted steam-runtime dir); game_lib = the game's bundled
 * lib dir ("" = none); game_exe = the game binary ("" = set later). */
WubuPv *wubu_pv_create(const char *name, const char *runtime_lib,
                       const char *game_lib, const char *game_exe)
{
    if (!name || !runtime_lib) return NULL;
    WubuPv *pv = calloc(1, sizeof(*pv));
    if (!pv) return NULL;
    WubuCt *ct = &pv->ct;
    WubuCt *base = wubu_ct_create(name, "/", CT_STEAMOS);   /* Steam
                 * Runtime + Proton — the enum anticipated this */
    if (!base) { free(pv); return NULL; }
    memcpy(ct, base, sizeof(*ct));
    free(base);   /* the fields are copied; the heap strings were NOT
                   * owned by base (wubu_ct_create uses static/argv
                   * storage) — safe */

    strncpy(pv->runtime_lib, runtime_lib, sizeof(pv->runtime_lib) - 1);
    if (game_lib) strncpy(pv->game_lib, game_lib, sizeof(pv->game_lib) - 1);
    if (game_exe) strncpy(pv->game_exe, game_exe, sizeof(pv->game_exe) - 1);

    /* the runtime base + the game lib layer (the game wins) */
    if (pv->runtime_lib[0] && ct->n_binds < WUBU_CT_MAX_BINDS) {
        strncpy(ct->binds[ct->n_binds].host, pv->runtime_lib,
                sizeof(ct->binds[ct->n_binds].host) - 1);
        snprintf(ct->binds[ct->n_binds].guest, sizeof(ct->binds[ct->n_binds].guest),
                 "%s", pv->runtime_lib);
        ct->binds[ct->n_binds].readonly = true;
        ct->n_binds++;
    }
    if (pv->game_lib[0] && ct->n_binds < WUBU_CT_MAX_BINDS) {
        strncpy(ct->binds[ct->n_binds].host, pv->game_lib,
                sizeof(ct->binds[ct->n_binds].host) - 1);
        snprintf(ct->binds[ct->n_binds].guest, sizeof(ct->binds[ct->n_binds].guest),
                 "%s", pv->game_lib);
        ct->binds[ct->n_binds].readonly = true;   /* the game libs are ro */
        ct->n_binds++;
    }

    /* the curated LD_LIBRARY_PATH: game libs FIRST, runtime after */
    pv->ld_library_path[0] = '\0';
    if (pv->game_lib[0])
        snprintf(pv->ld_library_path, sizeof(pv->ld_library_path),
                 "%s:", pv->game_lib);
    strncat(pv->ld_library_path, pv->runtime_lib,
            sizeof(pv->ld_library_path) - strlen(pv->ld_library_path) - 1);
    pv->ld_set = 1;

    /* the game cmd (when known) — a HEAP string (the teardown frees
     * the argv entries, so a pointer into pv would be freed twice) */
    if (pv->game_exe[0]) {
        pv->ct.argv[0] = strdup(pv->game_exe);
        pv->ct.argv[1] = NULL;
    }

    /* the standard SteamOS surface: GPU + network on */
    ct->gpu_passthrough = true;
    ct->net_enabled = true;
    return pv;
}

/* PV2: set the game command after creation. The argv entries are
 * HEAP strings (wubu_ct_destroy-style teardown frees them), so the
 * pv's embedded buffer is copied, not pointed at. */
int wubu_pv_set_game(WubuPv *pv, const char *game_exe)
{
    if (!pv || !game_exe) return -1;
    strncpy(pv->game_exe, game_exe, sizeof(pv->game_exe) - 1);
    if (pv->ct.argv[0]) { free(pv->ct.argv[0]); pv->ct.argv[0] = NULL; }
    pv->ct.argv[0] = strdup(pv->game_exe);
    pv->ct.argv[1] = NULL;
    return 0;
}

/* PV3: launch the pressure-vessel container via the bwrap runtime. */
int wubu_pv_launch(WubuPv *pv)
{
    if (!pv) return -1;
    /* the env: LD_LIBRARY_PATH must reach the child */
    if (pv->ld_set) {
        setenv("LD_LIBRARY_PATH", pv->ld_library_path, 1);
    }
    return wubu_ct_start_bwrap(&pv->ct);
}

/* PV4: teardown. The WubuCt is EMBEDDED in the WubuPv (not heap-
 * allocated), so wubu_ct_destroy's trailing free(ct) would free the
 * middle of the pv block — free the embedded strings + the pv only. */
void wubu_pv_destroy(WubuPv *pv)
{
    if (!pv) return;
    WubuCt *ct = &pv->ct;
    if (ct->state == CT_RUNNING) {
        wubu_ct_kill(ct, SIGKILL);
        wubu_ct_wait(ct);
    }
    if (ct->styx_fd >= 0) {
        close(ct->styx_fd);
        unlink(ct->styx_path);
    }
    for (int i = 0; i < WUBU_CT_MAX_ARGS; i++)
        if (ct->argv[i]) { free(ct->argv[i]); ct->argv[i] = NULL; }
    for (int i = 0; i < WUBU_CT_MAX_ENV; i++)
        if (ct->envp[i]) { free(ct->envp[i]); ct->envp[i] = NULL; }
    free(pv);
}

/* PV5: the launch command preview (the operator can see what will
 * run: the curated library path + the game). */
void wubu_pv_describe(const WubuPv *pv, char *buf, size_t cap)
{
    if (!pv || !buf || cap == 0) return;
    snprintf(buf, cap,
             "pressure-vessel '%s': LD_LIBRARY_PATH=%s (runtime '%s', "
             "game libs '%s'), exe '%s'",
             pv->ct.name, pv->ld_library_path, pv->runtime_lib,
             pv->game_lib, pv->game_exe);
}

/* PV6: the test hook — the WubuCt for the assertions. */
WubuCt *wubu_pv_ct(WubuPv *pv)
{
    return pv ? &pv->ct : NULL;
}
