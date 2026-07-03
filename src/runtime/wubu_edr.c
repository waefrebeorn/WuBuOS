/* wubu_edr.c  --  WuBuOS EDR Engine Shim
 *
 * All implementation moved to src/runtime/edr/ subdirectory.
 * This shim exists for backward compatibility with the test target.
 *
 * Modules:
 *   edr_core.c       — lifecycle, module system, alert buffer, worker thread
 *   edr_proc_pin.c   — NETLINK_CONNECTOR + cn_proc process pin
 *   edr_fanotify.c   — fanotify + inotify telemetry
 *   edr_poller.c     — periodic /proc scans
 */

#include "edr/edr_internal.h"

/* All real code is in the submodules — this file is intentionally empty
 * except to provide a compilation unit that pulls in the internal header.
 * The Makefile compiles each edr/ submodule separately. */