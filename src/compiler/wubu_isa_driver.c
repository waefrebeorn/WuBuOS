/*
 * wubu_isa_driver.c -- the ISA driver registry (the driver space).
 *
 * The driver space (wubu_isa_driver.h): every ISA is a driver. The
 * frontend emits ONE mid-level IR (wubu_mir) and N backends consume
 * it. This file is the registry that maps names to drivers.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <string.h>

/* the built-in drivers, one per ISA (all extern in the header) */
extern const wubu_isa_driver_t wubu_isa_x86_64;
extern const wubu_isa_driver_t wubu_isa_m68k;
extern const wubu_isa_driver_t wubu_isa_i8086;

const wubu_isa_driver_t *wubu_isa_find(const char *name)
{
    if (!name) return NULL;
    if (!strcmp(name, "x86-64") || !strcmp(name, "x86_64") ||
        !strcmp(name, "x86")) return &wubu_isa_x86_64;
    if (!strcmp(name, "m68k") || !strcmp(name, "68000") ||
        !strcmp(name, "motorola-68000") || !strcmp(name, "68k"))
        return &wubu_isa_m68k;
    if (!strcmp(name, "8086") || !strcmp(name, "i8086") ||
        !strcmp(name, "x86-16") || !strcmp(name, "8088"))
        return &wubu_isa_i8086;
    return NULL;
}
