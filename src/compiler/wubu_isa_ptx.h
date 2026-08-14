/*
 * wubu_isa_ptx.h -- the NVIDIA PTX ISA driver (the GPU leg).
 *
 * The eighth driver in the ISA driver space (wubu_isa_driver.h):
 *   x86-64, arm64, m68k, 8086, riscv, 6502, z80, **ptx**.
 *
 * PTX is NVIDIA's parallel thread execution ISA — the "x86 assembly
 * of GPUs". The driver:
 *   1. Consumes the SAME wubu_mir_t every other driver consumes.
 *   2. Emits PTX assembly text (.version 8.0, .target sm_89).
 *   3. Compiles PTX -> cubin with ptxas.
 *   4. Loads cubin, launches the kernel on the GPU, returns the result.
 *
 * The PTX kernel takes two parameters:
 *   .param .b64 result  — where the 64-bit output is written
 *   .param .b64 arg     — the program's input value
 *
 * Every MIR virtual register maps to a PTX `.reg .b64` register, so
 * the driver is a straight 1:1 translation: no register allocation,
 * no spilling — the GPU has hundreds of registers.
 *
 * C11, self-contained.
 */
#ifndef WUBU_ISA_PTX_H
#define WUBU_ISA_PTX_H

#include "wubu_isa_driver.h"

/* the PTX driver (registered in wubu_isa_driver.c) */
extern const wubu_isa_driver_t wubu_isa_ptx;

#endif /* WUBU_ISA_PTX_H */