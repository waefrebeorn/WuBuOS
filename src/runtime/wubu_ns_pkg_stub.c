/*
 * wubu_ns_pkg_stub.c -- Minimal definitions of the namespace + package
 * runtime symbols that GUI test binaries pull in via wubu_exec.c /
 * wubu_host_exec.c / styxfs_server.c but do NOT exercise.
 *
 * The DOS-window and DOS-proc GUI tests link the exec/ns/pkg runtime
 * transitively (for the DOS emulator path), but they only test the GUI
 * window + DOS window chrome, never the actual namespace routing or
 * package install. These stubs return the safe "not available" value so
 * the GUI test links and runs. The REAL implementations live in
 * wubu_ns_kernel.c / wubu_ns_pkg.c / wubu_pkg.c and are linked by the
 * kernel/runtime builds (test_ns_*, test_pkgmgr, the real shell).
 *
 * Matches the wubu_hc_eval_stub.c pattern ("runtime without the full
 * subsystem": return the contract value, no-op).
 */
#include <stdint.h>

/* -- namespace route (styxfs_server.c calls these) ------------------ */
int g_ns_root = 0;
int wubu_kvfs_route_path(const char *p, uint32_t *v) { (void)p; (void)v; return -1; }
int wubu_kvfs_route_read(const char *p, void *o, uint32_t n) { (void)p; (void)o; (void)n; return -1; }
int wubu_kvfs_route_write(const char *p, const void *i, uint32_t n) { (void)p; (void)i; (void)n; return -1; }

/* -- package runtime (wubu_ns_pkg.c calls these) --------------------- */
int ns_write(int fd, const void *buf, uint32_t len) { (void)fd; (void)buf; (void)len; return -1; }
int pkg_install(const char *p) { (void)p; return -1; }
int pkg_remove(const char *p) { (void)p; return -1; }
int pkg_add_repo(const char *p) { (void)p; return -1; }

/* -- wine env / security monitor (wubu_host_exec.c calls these) ------ */
int wubu_wine_setup_prefix(const char *p) { (void)p; return -1; }
int wubu_wine_configure_env(const void *e) { (void)e; return -1; }
void *wubu_secmon_create(void) { return 0; }
int wubu_secmon_attach(void *m, const void *p) { (void)m; (void)p; return 0; }
