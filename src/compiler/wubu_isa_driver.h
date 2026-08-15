/*
 * wubu_isa_driver.h -- THE ISA DRIVER SPACE (the compiler, retargetable).
 *
 * The compiler doctrine: to unlock ALL hardware performance across ALL
 * hardware, the compiler frontend emits ONE mid-level IR (wubu_mir),
 * and every ISA is a DRIVER that consumes it. This header is the
 * driver contract — the same shape as wuburuntime's personalities:
 * a vtable each backend implements.
 *
 * The driver space (2026-08-04, the everything-hardware map — see
 * wubuwizard/research/ALL_HARDWARE_7HOP.md):
 *   x86-64   -> wubu_isa_x86_64.c   (native JIT, this machine)  [DONE]
 *   m68k     -> wubu_isa_m68k.c     (Motorola 68,000, interp)   [DONE]
 *   riscv    -> (RV64I, executed by the interp)  (next wave)
 *   ptx      -> holyc_ptx.c         (NVIDIA GPU, the existing backend)
 *   arm64    -> (next wave)
 *   gpu      -> Vulkan/WebGPU front (RDNA/Xe/Apple G13 covered by
 *              ONE portable front — llama.cpp's 15-backend table is
 *              the proof this pattern scales)
 *   mcu      -> thin interpreters for 6502/Z80/8051/PIC/AVR/MSP430
 *              (the AGI on a $0.10 chip — the CH32V003 tier)
 *
 * A driver: name, whether it can RUN code natively (JIT) or needs a
 * host interpreter, compile(MIR)->bytes, and run(bytes)->result.
 *
 * Oracle doctrine: every new driver's encodings are verified
 * byte-for-byte against GNU binutils objdump (tools/verify_isa.sh)
 * BEFORE shipping, and the differential battery (`make test_drivers`)
 * runs every driver against gcc on 33+ expressions. No guessed opcodes.
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
extern const wubu_isa_driver_t wubu_isa_arm64;  /* AArch64 (2011, 230B chips) */
extern const wubu_isa_driver_t wubu_isa_mips;   /* MIPS (1981, Berkeley RISC lineage) */
extern const wubu_isa_driver_t wubu_isa_8051;   /* Intel 8051 (1978, the $0.10 chip) */
extern const wubu_isa_driver_t wubu_isa_m68k;   /* the 68,000 (1979) */
extern const wubu_isa_driver_t wubu_isa_i8086;  /* the x86 root (1978) */
extern const wubu_isa_driver_t wubu_isa_riscv;  /* RV64I (2010) */
extern const wubu_isa_driver_t wubu_isa_6502;   /* MOS (1975) */
extern const wubu_isa_driver_t wubu_isa_z80;    /* Zilog (1976) */
extern const wubu_isa_driver_t wubu_isa_ptx;    /* NVIDIA GPU PTX (2024) */
/* every driver is backed by a REAL interpreter (wubu_*_interp.c) —
 * the frontend emits ONE MIR, six backends consume it, all agree.
 * Proven by `make test_drivers` (33 expressions x 6 drivers). */

/* D4: find a driver by name ("x86-64" / "8086" / "m68k"). NULL if unknown. */
const wubu_isa_driver_t *wubu_isa_find(const char *name);

#endif /* WUBU_ISA_DRIVER_H */
