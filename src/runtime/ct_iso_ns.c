/*
 * ct_iso_ns.c  --  WuBuOS container namespace unshare helper (Cell 420 split).
 */
/* unshare() is a Linux/glibc extension hidden under -std=c11; _GNU_SOURCE
 * exposes it. WUBU_HOSTED does NOT — it only toggles WuBu's own hosted
 * shims, so the previous `#define WUBU_HOSTED` here was both wrong and
 * caused a redefine warning against the -DWUBU_HOSTED command-line flag. */
#define _GNU_SOURCE 1
#include "wubu_gnu_compat.h"
#include "ct_iso_ns.h"
#include <sched.h>

int wubu_ns_unshare(int flags) {
    if (flags == 0) return 0;
    return unshare(flags);
}

