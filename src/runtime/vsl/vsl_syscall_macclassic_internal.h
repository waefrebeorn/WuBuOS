/*
 * vsl_syscall_macclassic_internal.h  --  VSL Classic Mac OS (68K) Trap Personality
 *
 * Classic Mac OS (1984-2001) used the Motorola 68000 A-line (line-A, $Axxx)
 * trap instruction as its entire system-call ABI. The OS Trap Manager held a
 * 256-entry OS dispatch table and a 1024-entry Toolbox table; applications
 * called them via A-line trap words. This is the PRE-XNU Mac — VSL already has
 * the XNU/macOS (BSD+Mach) personality, so adding classic Mac covers BOTH Mac
 * eras and is a clean "toast/abandoned" VSL personality.
 *
 * Trap word format: $Axxx. The low byte (or low 12 bits) is the trap number.
 * We encode a classic-Mac call as:  num = 0xB0000000 | (trap & 0xFFF)
 * where `trap` is the documented Inside-Macintosh trap number (e.g. NewPtr=$A11E
 * -> 0x11E, Open=$A000 -> 0x000). Handlers are backed by host syscalls via the
 * Linux/VSL layer (malloc/free/open/read/write/close/clock) -- real effects.
 *
 * C11, self-contained; reuses vsl_syscall_internal.h host helpers.
 */

#ifndef VSL_SYSCALL_MACCLASSIC_INTERNAL_H
#define VSL_SYSCALL_MACCLASSIC_INTERNAL_H

#include "vsl/vsl_syscall_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <malloc.h>  /* malloc_usable_size (glibc) */

/* -- Classic Mac trap numbers (Inside Macintosh, stable public constants) -- */
/* Memory Manager */
#define MACC_NEWPTR       0x11E   /* A11E */
#define MACC_DISPOSEPTR   0x01F   /* A01F */
#define MACC_GETPTRSIZE   0x11F   /* A11F */
#define MACC_NEWHANDLE    0x122   /* A122 */
#define MACC_DISPOSEHANDLE 0x021  /* A021 */
/* File Manager */
#define MACC_OPEN         0x000   /* A000 */
#define MACC_CLOSE        0x001   /* A001 */
#define MACC_READ         0x002   /* A002 */
#define MACC_WRITE        0x003   /* A003 */
#define MACC_CREATE       0x0A1   /* A0A1 */
#define MACC_DELETE       0x008   /* A008 */
#define MACC_GETFPOS      0x0A4   /* A0A4 */
#define MACC_SETFPos      0x0A3   /* A0A3 (SetFPos) */
/* Time Manager */
#define MACC_TICKCOUNT    0x975   /* A975 */
/* Event / UI (we map to stdout for the headless VSL host) */
#define MACC_DRAWCHAR     0x8F4   /* A8F4 */
#define MACC_WRITECHAR    0x88B   /* A88B */
#define MACC_WRITE0       0x88C   /* A88C (WriteLn) */

/* Highest implemented trap number (table size). */
#define MACC_TRAP_MAX     0x976

/* -- Handler type (matches vsl_syscall_fn signature) -------------- */
typedef int64_t (*macclassic_syscall_fn)(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f);

/* -- Trap dispatch table (defined in vsl_syscall_macclassic.c) ---- */
extern const macclassic_syscall_fn macclassic_trap_table[MACC_TRAP_MAX + 1];

/* -- Entry point (called by the VSL facade when class 0xB0 is set) */
int64_t vsl_macclassic_syscall_dispatch(uint64_t trap, uint64_t a, uint64_t b,
                                         uint64_t c, uint64_t d, uint64_t e, uint64_t f);

/* -- Error convention: classic Mac returns 0 for success, OSErr (negative-ish
 * small int) on failure. We return OSErr directly; unimplemented -> -1 (paramErr). */
static inline int64_t macclassic_oserr(int err) { return (int64_t)err; }

#endif /* VSL_SYSCALL_MACCLASSIC_INTERNAL_H */
