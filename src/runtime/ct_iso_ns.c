/*
 * ct_iso_ns.c  --  WuBuOS container namespace unshare helper (Cell 420 split).
 */
#define _GNU_SOURCE  /* unshare() is Linux-specific, hidden under -std=c11 without this */
#include "ct_iso_ns.h"
#include <sched.h>

int wubu_ns_unshare(int flags) {
    if (flags == 0) return 0;
    return unshare(flags);
}

