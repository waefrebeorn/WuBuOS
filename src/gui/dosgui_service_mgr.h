/*
 * dosgui_service_mgr.h -- Desktop-side service/autostart manager.
 *
 * E3 integration: wubu_archd (16/16) is wired here as the Desktop's
 * autostart/service manager. The Desktop owns a live WubuArchd instance:
 * it initializes archd at desktop boot, starts every registered autostart
 * service (via the real wubu_archd_svc_start -> arch-chroot systemctl),
 * and stops them at desktop shutdown.
 *
 * This is real wiring, not a panel that merely connects to archd's socket:
 * dosgui_desktop_init() calls dosgui_service_mgr_init() + _boot(), and
 * dosgui_desktop_shutdown() calls dosgui_service_mgr_shutdown().
 *
 * C11, no nested functions.
 */
#ifndef DOSGUI_SERVICE_MGR_H
#define DOSGUI_SERVICE_MGR_H

#include "../runtime/wubu_archd.h"

#define DOSGUI_SVC_MGR_MAX_AUTOSTART 64

/* One registered autostart entry: a service in a named Arch root that the
 * Desktop starts automatically at boot. */
typedef struct {
    char root[WUBU_ARCHD_MAX_ROOT_NAME];
    char svc[WUBU_ARCHD_MAX_PACKAGE_NAME];
    bool booted;        /* last boot() attempted to start it */
    int  last_result;   /* result of last wubu_archd_svc_start (0 ok, <0 fail) */
} DosguiAutostartEntry;

/* Result of a boot attempt, one per autostart entry (index-aligned). */
typedef struct {
    int attempted;      /* number of entries boot() tried to start */
    int started;        /* number that returned success */
    int failed;         /* number that returned failure */
} DosguiBootResult;

/* Initialize the service manager: create+init the archd instance and load any
 * autostart entries from the config file. Returns 0 on success. */
int  dosgui_service_mgr_init(void);

/* Register an autostart entry (root + service). Dedups by (root,svc).
 * Returns index >= 0 on success, -1 on error (null/empty/invalid/full). */
int  dosgui_service_mgr_register_autostart(const char *root, const char *svc);

/* Start every registered autostart service via the real archd svc_start.
 * Records per-entry results. Returns a DosguiBootResult summary. */
DosguiBootResult dosgui_service_mgr_boot(void);

/* Stop every service this manager booted. */
void dosgui_service_mgr_stop_all(void);

/* Tear down: stop booted services and shut down the archd instance. */
void dosgui_service_mgr_shutdown(void);

/* Accessors (for tests / panel). */
int  dosgui_service_mgr_count(void);
const DosguiAutostartEntry *dosgui_service_mgr_entry(int i);
WubuArchd *dosgui_service_mgr_handle(void);
bool dosgui_service_mgr_active(void);

#endif /* DOSGUI_SERVICE_MGR_H */
