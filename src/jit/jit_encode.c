/* jit_encode.c -- x86-64 opcode encoding helpers (self-contained).
 *
 * enc_* : emit raw x86-64 opcodes into a buffer. Pure byte writers. Shared by
 * the JIT expression compiler and backends. Declared in jit_internal.h.
 * Minimal includes.
 */

#include "jit_internal.h"
#include <string.h>

int enc_mov_eax_imm32(unsigned char *buf, int32_t imm) {
    buf[0] = 0xB8;
    memcpy(buf + 1, &imm, 4);
    return 5;
}

int enc_mov_rdi_imm64(unsigned char *buf, int64_t imm) {
    buf[0] = 0x48; buf[1] = 0xBF;
    memcpy(buf + 2, &imm, 8);
    return 10;
}

int enc_add_eax_edi(unsigned char *buf) {
    buf[0] = 0x01; buf[1] = 0xF8;
    return 2;
}

int enc_imul_eax_edi(unsigned char *buf) {
    buf[0] = 0x0F; buf[1] = 0xAF; buf[2] = 0xC7;
    return 3;
}

int enc_sub_eax_esi(unsigned char *buf) {
    buf[0] = 0x29; buf[1] = 0xF0;
    return 2;
}

int enc_xor_eax_eax(unsigned char *buf) {
    buf[0] = 0x31; buf[1] = 0xC0;
    return 2;
}

int enc_ret(unsigned char *buf) {
    buf[0] = 0xC3;
    return 1;
}

int enc_mov_eax_edi(unsigned char *buf) {
    buf[0] = 0x89; buf[1] = 0xF8;
    return 2;
}

int enc_add_eax_esi(unsigned char *buf) {
    buf[0] = 0x01; buf[1] = 0xF0;
    return 2;
}

int enc_mov_eax_esi(unsigned char *buf) {
    buf[0] = 0x89; buf[1] = 0xF0;
    return 2;
}

int enc_neg_eax(unsigned char *buf) {
    buf[0] = 0x48; buf[1] = 0xF7; buf[2] = 0xD8;
    return 3;
}
