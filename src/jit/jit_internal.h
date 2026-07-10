/* jit_internal.h -- Internal helpers shared by jit sub-modules.
 * Public API + types in jit.h. The x86-64 opcode encoding helpers live in
 * jit_encode.c and are declared here so all submodules link the SAME
 * implementation (no double-coding).
 */

#ifndef JIT_INTERNAL_H
#define JIT_INTERNAL_H

#include "jit.h"
#include <stdint.h>
#include <string.h>

/* -- x86-64 opcode encoders (jit_encode.c) ----------------------- */
int enc_mov_eax_imm32(unsigned char *buf, int32_t imm);
int enc_mov_rdi_imm64(unsigned char *buf, int64_t imm);
int enc_add_eax_edi(unsigned char *buf);
int enc_imul_eax_edi(unsigned char *buf);
int enc_sub_eax_esi(unsigned char *buf);
int enc_xor_eax_eax(unsigned char *buf);
int enc_ret(unsigned char *buf);
int enc_mov_eax_edi(unsigned char *buf);
int enc_add_eax_esi(unsigned char *buf);
int enc_mov_eax_esi(unsigned char *buf);
int enc_neg_eax(unsigned char *buf);

#endif /* JIT_INTERNAL_H */
