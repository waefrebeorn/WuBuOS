/*
 * wubu_crashcap.h -- when an app dies, leave evidence.
 *
 * The reference desktop records a crash report and shows it later (WER / systemd-coredump +
 * coredumpctl). Here a pointer bug killed a whole session and left nothing behind: the process
 * vanished and the only trace was the shell's own "Segmentation fault" line. This module installs
 * handlers for the fatal signals and writes a small, readable report before the process dies.
 *
 * It is deliberately NOT a debugger: the report carries the signal, the faulting address, the
 * register-independent backtrace (addresses) and the memory map at the fault, which is enough to
 * name the module and the offset.
 */
#ifndef WUBU_CRASHCAP_H
#define WUBU_CRASHCAP_H

/* Install the handlers. Returns 0, or -1 when nothing was installed (already installed is 0). */
int wubu_crashcap_install(void);

/* Reports are written here: $WUBU_CRASH_DIR, else $XDG_STATE_HOME/wubuos/crashes,
 * else ~/.local/state/wubuos/crashes. Never NULL. */
const char *wubu_crashcap_dir(void);

/* Number of reports currently on disk (0 when the directory does not exist). */
int wubu_crashcap_count(void);

/* Keep at most this many reports; the oldest are removed first. */
void wubu_crashcap_prune(int keep);

#endif /* WUBU_CRASHCAP_H */
