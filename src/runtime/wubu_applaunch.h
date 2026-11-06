/*
 * wubu_applaunch.h  --  run a session app by loading its PE from /n.
 */
#ifndef WUBU_APPLAUNCH_H
#define WUBU_APPLAUNCH_H

#include "wubu_session_boot.h"   /* wubu_sboot_launch_fn */
#include "wubu_zx.h"

typedef struct {
    const wubu_zx_ops *ops;
    wubu_pe_resolve_fn resolve;   /* NULL => wubu_modsym_resolve */
    uintptr_t base;
} wubu_zx_applaunch_cfg;

/* A wubu_sboot_launch_fn: loads argv[0] (as /n/<name>) through wubu_zxreg_load
 * and reports success when the module ran. `ud` must be a wubu_zx_applaunch_cfg*.
 * The registry keeps the reference (unload later to release). */
int wubu_applaunch_as_session_launcher(wubu_session_boot_t *boot,
                                       const wubu_xdga_entry_t *entry,
                                       char **argv, void *ud);

#endif /* WUBU_APPLAUNCH_H */
