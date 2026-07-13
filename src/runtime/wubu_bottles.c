/*
 * wubu_bottles.c  --  WuBuOS Bottles/Lutris Integration (FACADE)
 *
 * Cell 480: .wubu container format. This file is now a thin facade; the real
 * implementation is split into self-contained modules:
 *   wubu_bottle_lifecycle.c  (create/destroy/deps/mounts/env/save/install/run)
 *   wubu_bottle_io.c         (import/export: native .wubu + Lutris)
 *   wubu_bottle_flatpak.c    (Flatpak manifest + runtime detection)
 *   wubu_bottle_ops.c        (list + verify)
 * JSON helpers live in wubu_bottles_json.c; fs helpers in wubu_bottles_fs.c.
 * C11, opaque structs, minimal includes.
 */

#define _GNU_SOURCE

#include "wubu_bottles.h"
#include "wubu_bottles_internal.h"
