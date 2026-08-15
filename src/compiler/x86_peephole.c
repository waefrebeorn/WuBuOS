/*
 * x86_peephole.c — Peephole optimizer for x86-64 JIT code.
 *
 * Eliminates redundant patterns in the emitted code.
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Check if byte is a REX prefix */
static inline int is_rex(uint8_t b) { return (b >= 0x40 && b <= 0x4F); }

/* Decode a mov r64, r64 instruction.
 * Returns 1 if decoded, 0 if not a mov.
 * Outputs: dst_reg (0-15), src_reg (0-15), length (3) */
static int decode_mov(uint8_t *code, size_t n, size_t i, int *dst, int *src, size_t *len) {
    if (i + 2 >= n) return 0;
    if (!is_rex(code[i])) return 0;
    if (code[i+1] != 0x89 && code[i+1] != 0x8B) return 0;
    uint8_t modrm = code[i+2];
    if ((modrm >> 6) != 3) return 0; /* register-register only */

    int rex_r = (code[i] >> 2) & 1; /* REX.R extends reg field */
    int rex_b = (code[i] >> 0) & 1; /* REX.B extends rm field */

    int reg = ((modrm >> 3) & 7) | (rex_r << 3);
    int rm = (modrm & 7) | (rex_b << 3);

    if (code[i+1] == 0x89) {
        /* MOV r/m64, r64: dst=rm, src=reg */
        *dst = rm;
        *src = reg;
    } else {
        /* MOV r64, r/m64: dst=reg, src=rm */
        *dst = reg;
        *src = rm;
    }
    *len = 3;
    return 1;
}

/* Eliminate redundant mov reg, reg (same register) */
static size_t eliminate_self_mov(uint8_t *code, size_t n) {
    size_t removed = 0;
    size_t i = 0;
    while (i + 2 < n) {
        int dst, src;
        size_t len;
        if (decode_mov(code, n, i, &dst, &src, &len) && dst == src) {
            memmove(&code[i], &code[i+len], n - (i+len));
            n -= len;
            removed++;
            continue;
        }
        i++;
    }
    return n;
}

/* Eliminate store-reload pairs: mov dst, rax; mov rax, dst
 * Pattern: any mov that writes to reg X from rax(0), followed by
 *          any mov that writes to rax(0) from reg X.
 * This eliminates the redundant store+reload when a computed result
 * is immediately needed in rax. */
static size_t eliminate_store_reload(uint8_t *code, size_t n) {
    size_t removed = 0;
    size_t i = 0;
    while (i + 5 < n) {
        int dst1, src1, dst2, src2;
        size_t len1, len2;
        if (decode_mov(code, n, i, &dst1, &src1, &len1)) {
            size_t j = i + len1;
            if (decode_mov(code, n, j, &dst2, &src2, &len2)) {
                /* Check: first writes dst1=src1 where src1 is accumulator-ish,
                 * second writes rax(0)=dst1 (reload from same reg) */
                if (dst2 == 0 && src2 == dst1 && src1 == 0) {
                    /* Pattern: mov dst, rax; mov rax, dst → eliminate both */
                    memmove(&code[i], &code[j+len2], n - (j+len2));
                    n -= (len1 + len2);
                    removed += 2;
                    continue;
                }
            }
        }
        i++;
    }
    return n;
}

/* Shrink movabs reg, imm64 → mov reg, imm32 when imm fits in 32 bits.
 * Uses a separate output buffer to avoid corrupting overlapping patterns. */
static size_t shrink_movabs(uint8_t *code, size_t n) {
    uint8_t *out = malloc(n + 16); /* extra space for safety */
    if (!out) return n;
    size_t oi = 0;
    size_t i = 0;
    while (i < n) {
        int can_read10 = (i + 9 < n);
        int is_movabs = 0;
        int need_rex_b = 0;

        if (can_read10) {
            if (code[i] == 0x48 && (code[i+1] & 0xF8) == 0xB8) { is_movabs = 1; }
            else if (code[i] == 0x49 && (code[i+1] & 0xF8) == 0xB8) { is_movabs = 1; need_rex_b = 1; }
            else if (code[i] == 0x4C && (code[i+1] & 0xF8) == 0xB8) { is_movabs = 1; }
            else if (code[i] == 0x4D && (code[i+1] & 0xF8) == 0xB8) { is_movabs = 1; need_rex_b = 1; }
        }

        if (is_movabs) {
            int64_t imm = 0;
            for (int j = 0; j < 8; j++) {
                imm |= ((int64_t)(uint8_t)code[i+2+j]) << (j*8);
            }
            if (imm >= -2147483648LL && imm <= 2147483647LL) {
                uint8_t rd = code[i+1] & 7;
                if (!need_rex_b) {
                    out[oi++] = (uint8_t)(0xB8 | rd);
                } else {
                    out[oi++] = 0x41;
                    out[oi++] = (uint8_t)(0xB8 | rd);
                }
                out[oi++] = (uint8_t)(imm & 0xFF);
                out[oi++] = (uint8_t)((imm >> 8) & 0xFF);
                out[oi++] = (uint8_t)((imm >> 16) & 0xFF);
                out[oi++] = (uint8_t)((imm >> 24) & 0xFF);
                i += 10;
                continue;
            }
            /* Imm doesn't fit in 32 bits: copy all 10 bytes */
            for (int k = 0; k < 10 && i < n; k++) {
                out[oi++] = code[i++];
            }
            continue;
        }
        /* Not a movabs pattern: copy byte */
        out[oi++] = code[i++];
    }
    memcpy(code, out, oi);
    free(out);
    return oi;
}

/* Eliminate sub rsp, 0 */
static size_t eliminate_zero_sub(uint8_t *code, size_t n) {
    size_t i = 0;
    while (i + 6 < n) {
        if (code[i] == 0x48 && code[i+1] == 0x81 && code[i+2] == 0xEC) {
            int32_t imm = (int32_t)((uint32_t)code[i+3] | ((uint32_t)code[i+4] << 8) |
                          ((uint32_t)code[i+5] << 16) | ((uint32_t)code[i+6] << 24));
            if (imm == 0) {
                memmove(&code[i], &code[i+7], n - (i+7));
                n -= 7;
                continue;
            }
        }
        i++;
    }
    return n;
}

/* Main peephole entry point.
 * Returns new code size. Modifies code in-place. */
size_t x86_peephole_optimize(uint8_t *code, size_t n) {
    if (n == 0) return 0;

    size_t prev_n;
    int max_passes = 10;
    do {
        prev_n = n;
        /* eliminate_self_mov disabled */
        n = shrink_movabs(code, n);
        /* eliminate_zero_sub removed: driver already skips sub rsp when frame=0 */
        if (n == 0) break;
    } while (n < prev_n && --max_passes > 0);

    return n;
}
