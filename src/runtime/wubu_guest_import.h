/*
 * wubu_guest_import.h -- L8: unified rootfs import -> launch ("load their system").
 *
 * ANY distro rootfs (directory, tar.gz, tar.zst, cpio.gz) becomes a managed,
 * launchable WuBuOS guest:
 *
 *   wubu_guest_import(path, name)   extract into ~/.wubu/roots/<name>,
 *                                   validate /bin/sh|/busybox, publish /n/guests
 *   wubu_guest_launch(name, cmd)    run through the SAME validated exec path
 *                                   L9 uses (wubu_ns_guests_launch ->
 *                                   wubu_ct_create/set_cmd/start)
 *   wubu_guest_remove(name)         delete the managed root + refresh /n/guests
 *
 * Owners (extend, never duplicate):
 *   - extraction/format sniffing: wubu_rd_load + detect_format (wubu_ramdisk)
 *   - exec + live container registry: wubu_host_exec (wubu_ct_*)
 *   - /n/guests rendering + launch ownership: wubu_ns_guests
 * This module owns ONLY the imported-root store (~/.wubu/roots) and the
 * import/validate/remove lifecycle. The directory IS the registry: a scan
 * of managed roots on every sync, no private mirror of container state.
 *
 * C11, hosted (the chroot/unshare machinery is Linux host delegation).
 */
#ifndef WUBU_GUEST_IMPORT_H
#define WUBU_GUEST_IMPORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Managed roots directory: $WUBU_GUEST_ROOTS_DIR, else $HOME/.wubu/roots.
 * (archd owns /var/wubu/roots for bare-metal Arch roots; this is the
 * unprivileged per-user import store, sibling of the ~/.wubu/n namespace.) */
void wubu_guest_roots_dir(char *out, size_t outsz);

/* Guest names are single path components: alnum + . _ - , never "."/"..",
 * never a slash, < WUBU_CT_NAME_MAX. Same contract as /n/guests. */
int wubu_guest_name_valid(const char *name);

/* Import a rootfs image (directory / tar.gz / tgz / tar.zst / cpio.gz / cgz)
 * as guest `name`. Extracts via wubu_rd_load into <roots>/<name>, refuses
 * unknown formats, archives that fail to extract, and roots with no
 * /bin/sh, /busybox or /bin/busybox (cleanup on every refusal). Publishes
 * through wubu_ns_guests_sync(). Returns 0 on success, -1 on refusal. */
int wubu_guest_import(const char *path, const char *name);

/* Launch an imported guest. cmdline is split quote-aware by the ONE exec
 * path (wubu_ns_guests_launch -> wubu_ct_*); NULL/empty means "/bin/sh".
 * Refuses names that are not imported or already running. 0 / -1. */
int wubu_guest_launch(const char *name, const char *cmdline);

/* Remove an imported guest: deletes <roots>/<name> (only a directory this
 * module owns -- marked by .wubu-guest) and refreshes /n/guests. Refuses
 * while the guest is running. 0 / -1. */
int wubu_guest_remove(const char *name);

/* -- Scan accessors (the /n/guests publisher + the GUI read these) ------- */

/* Number of managed imported roots (sorted by name). */
int wubu_guest_import_count(void);

/* Entry idx: name/root/state buffers. state is "running" when the live
 * registry holds the name, else "imported". Returns 0, or -1 if idx is
 * out of range. Any out buffer may be NULL/0. */
int wubu_guest_import_entry(int idx, char *name, size_t namesz,
                            char *root, size_t rootsz,
                            char *state, size_t statesz);

/* Root path of an imported guest, or -1 if the name is not imported.
 * This is what wubu_ns_guests_launch resolves the chroot root through. */
int wubu_guest_import_root(const char *name, char *out, size_t outsz);

/* Parse + execute an import argument string "<path> <name>" (path may be
 * double-quoted). The ONE parser for both the terminal 'guest import'
 * command and a 9P write to guests/import. 0 / -1. */
int wubu_guest_import_cmdline(const char *args);

/* Terminal dispatch: handle a whole "guest ..." command line. Writes a
 * human-readable result into out. Returns 1 when the line was a guest
 * command (handled), 0 when it is not (caller falls through to the pty). */
int wubu_guest_cli(const char *line, char *out, size_t outsz);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_GUEST_IMPORT_H */
