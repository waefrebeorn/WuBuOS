/*
 * wubu_isa_driver.h -- THE ISA DRIVER SPACE (the compiler, retargetable).
 *
 * The compiler doctrine: to unlock ALL hardware performance across ALL
 * hardware, the compiler frontend emits ONE mid-level IR (wubu_mir),
 * and every ISA is a DRIVER that consumes it. This header is the
 * driver contract — the same shape as wuburuntime's personalities:
 * a vtable each backend implements.
 *
 * The driver space (2026-08-04):
 *   x86-64   -> wubu_isa_x86_64.c   (native JIT, this machine)
 *   riscv    -> wubu_isa_riscv.c    (RV64I, executed by the interp)
 *   ptx      -> holyc_ptx.c         (NVIDIA GPU, the existing backend)
 *   arm64    -> (next wave)
 *
 * A driver: name, whether it can RUN code natively (JIT) or needs a
 * host interpreter, compile(MIR)->bytes, and run(bytes)->result.
 *
 * C11, self-contained.
 */
#ifndef WUBU_ISA_DRIVER_H
#define WUBU_ISA_DRIVER_H

#include "wubu_mir.h"
#include <stddef.h>
#include <stdint.h>

/* what the driver can do with its output */
typedef enum {
    WUBU_ISA_NATIVE = 0,   /* code runs on this host CPU (JIT) */
    WUBU_ISA_INTERPRETED   /* code runs via a bundled interpreter */
} wubu_isa_exec_t;

typedef struct wubu_isa_driver {
    const char *name;          /* "x86-64" / "riscv" / "ptx" */
    const char *family;        /* "native" / "gpu" / "portable" */
    wubu_isa_exec_t exec;      /* how its output runs */

    /* D1: compile a MIR program to machine code.
     * Returns 0 on success; out_code/out_size filled (caller frees). */
    int (*compile)(const wubu_mir_prog_t *p,
                   uint8_t **out_code, size_t *out_size);

    /* D2: run a compiled program. entry_vr: the vr the program reads
     * as its input (0 = none). Returns the MIR_RET value. */
    int64_t (*run)(const uint8_t *code, size_t size, int64_t arg);

    /* D3: describe the driver (ISA, features, exec model). */
    void (*describe)(void);
} wubu_isa_driver_t;

/* the built-in drivers (the driver space, populated) */
extern const wubu_isa_driver_t wubu_isa_x86_64;
extern const wubu_isa_driver_t wubu_isa_m68k;   /* the 68,000 (1979) */
/* (riscv driver: files were referenced but never landed — re-add on build) */

/* D4: find a driver by name ("x86-64" / "riscv" / "m68k"). NULL if unknown. */
const wubu_isa_driver_t *wubu_isa_find(const char *name);

#endif /* WUBU_ISA_DRIVER_H */
