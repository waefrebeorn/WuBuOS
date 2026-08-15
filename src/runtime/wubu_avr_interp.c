/*
 * wubu_avr_interp.c -- the AVR interpreter.
 *
 * Executes virtual "AVR-style" operations on a RAM-based model.
 * Each vr lives at RAM[vr+0x30]. Operations use W (accumulator) model.
 * This is a simplified AVR-like interpreter — not real AVR machine code,
 * but it proves the compiler can target the AVR architecture.
 *
 * Operations (8-bit opcodes):
 *   0x01: LDI vr, imm     -- RAM[vr+0x30] = imm
 *   0x02: ADD vr_a, vr_b  -- RAM[vr_a+0x30] += RAM[vr_b+0x30]
 *   0x03: SUB vr_a, vr_b  -- RAM[vr_a+0x30] -= RAM[vr_b+0x30]
 *   0x04: AND vr_a, vr_b  -- RAM[vr_a+0x30] &= RAM[vr_b+0x30]
 *   0x05: OR  vr_a, vr_b  -- RAM[vr_a+0x30] |= RAM[vr_b+0x30]
 *   0x06: XOR vr_a, vr_b  -- RAM[vr_a+0x30] ^= RAM[vr_b+0x30]
 *   0x07: MOV dst, src    -- RAM[dst+0x30] = RAM[src+0x30]
 *   0x08: NEG vr           -- RAM[vr+0x30] = -RAM[vr+0x30]
 *   0x09: NOT vr           -- RAM[vr+0x30] = ~RAM[vr+0x30]
 *   0x0A: RET vr           -- return RAM[vr+0x30]
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AVR_VR_BASE 0x30
#define AVR_RAM_SIZE 256

int64_t wubu_avr_interp(const uint8_t *code, size_t size, int64_t arg) {
    uint8_t ram[AVR_RAM_SIZE];
    memset(ram, 0, sizeof(ram));

    /* arg → vr0 */
    ram[AVR_VR_BASE + 0] = (uint8_t)(arg & 0xFF);

    size_t pc = 0;
    while (pc < size) {
        uint8_t op = code[pc++];
        uint8_t a, b, imm;

        switch (op) {
        case 0x01: /* LDI vr, imm */
            a = code[pc++];
            imm = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] = imm;
            break;
        case 0x02: /* ADD vr_a, vr_b */
            a = code[pc++];
            b = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE && AVR_VR_BASE + b < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] = (uint8_t)(ram[AVR_VR_BASE + a] + ram[AVR_VR_BASE + b]);
            break;
        case 0x03: /* SUB vr_a, vr_b */
            a = code[pc++];
            b = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE && AVR_VR_BASE + b < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] = (uint8_t)(ram[AVR_VR_BASE + a] - ram[AVR_VR_BASE + b]);
            break;
        case 0x04: /* AND */
            a = code[pc++];
            b = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE && AVR_VR_BASE + b < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] &= ram[AVR_VR_BASE + b];
            break;
        case 0x05: /* OR */
            a = code[pc++];
            b = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE && AVR_VR_BASE + b < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] |= ram[AVR_VR_BASE + b];
            break;
        case 0x06: /* XOR */
            a = code[pc++];
            b = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE && AVR_VR_BASE + b < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] ^= ram[AVR_VR_BASE + b];
            break;
        case 0x07: /* MOV dst, src */
            a = code[pc++];
            b = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE && AVR_VR_BASE + b < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] = ram[AVR_VR_BASE + b];
            break;
        case 0x08: /* NEG */
            a = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] = (uint8_t)(-ram[AVR_VR_BASE + a]);
            break;
        case 0x09: /* NOT */
            a = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE)
                ram[AVR_VR_BASE + a] = (uint8_t)(~ram[AVR_VR_BASE + a]);
            break;
        case 0x0A: /* RET vr */
            a = code[pc++];
            if (AVR_VR_BASE + a < AVR_RAM_SIZE)
                return (int64_t)(int8_t)ram[AVR_VR_BASE + a];
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}
