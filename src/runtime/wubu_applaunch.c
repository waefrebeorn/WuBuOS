/*
 * wubu_applaunch.c  --  run a session app by loading its PE from /n.
 *
 * The P3<->module bridge: a wubu_sboot_launch_fn that starts an autostart app
 * by reading its executable bytes from the /n namespace (e.g. argv[0] =
 * "prog.bin" -> /n/prog.bin) and running them through the module-runner
 * registry (wubu_zxreg_load) instead of exec()-ing a host binary. This is the
 * seam that lets the session/boot treat a compiled holyd module (or any PE the
 * kernel loader understands) as an app.
 *
 *   as_session_launcher: castable to wubu_sboot_launch_fn. `ud` must point at a
 *   `wubu_zx_applaunch_cfg { ops; resolve; base; }`. Returns 0 when the app
 *   loaded+ran (registry got a reference); nonzero otherwise.
 *
 * The registry is left holding the reference so a later unload releases it; a
 * session that wants ephemeral apps can unload immediately after start_all.
 */
#include "wubu_applaunch.h"
#include "wubu_zxreg.h"
#include "wubu_zx.h"

int wubu_applaunch_as_session_launcher(wubu_session_boot_t *boot,
                                       const wubu_xdga_entry_t *entry,
                                       char **argv, void *ud)
{
    (void)boot; (void)entry;
    wubu_zx_applaunch_cfg *cfg = (wubu_zx_applaunch_cfg *)ud;
    if (!cfg || !cfg->ops) return -1;
    if (!argv || !argv[0] || !argv[0][0]) return -1;

    /* the app binary is the exec path (e.g. "apps/prog.bin" or "/n/apps/prog.bin");
     * wubu_zxload's build_path already resolves /n/... AND keeps subdirs, so pass
     * it through verbatim — stripping to the final component would break any
     * subdirectory path (apps/prog.bin -> /n/prog.bin, a miss). */
    const char *name = argv[0];

    wubu_zxreg_entry_t *slot = NULL;
    int rc = wubu_zxreg_load(cfg->ops, name, cfg->base, cfg->resolve, &slot);
    /* load success == app ran; return 0 (started) regardless of its result */
    return (slot && slot->valid) ? 0 : rc;
}
