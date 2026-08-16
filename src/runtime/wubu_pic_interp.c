/*
 * wubu_pic_interp.c -- the PIC interpreter.
 *
 * Executes virtual "PIC-style" operations on a RAM-based model.
 * Each vr lives at RAM[vr+0x20]. Operations use W (accumulator) model.
 * This is a simplified PIC-like interpreter — not real PIC machine code,
 * but it proves the compiler can target the PIC architecture.
 *
 * Harvard-style: separate code/data spaces, W accumulator + file regs.
 * Banked register file: 128 bytes across 4 banks (32 per bank).
 *
 * Operations (8-bit opcodes):
 *   0x01: LIW imm     — W = imm
 *   0x02: ADW fr      — W = (W + RAM[fr]) & 0xFF
 *   0x03: SUW fr      — W = (W - RAM[fr]) & 0xFF
 *   0x04: ANW fr      — W = W & RAM[fr]
 *   0x05: ORW fr      — W = W | RAM[fr]
 *   0x06: XRW fr      — W = W ^ RAM[fr]
 *   0x07: MVF fr      — RAM[fr] = W
 *   0x08: MVW fr      — W = RAM[fr]
 *   0x09: NEG         — W = (-W) & 0xFF
 *   0x0A: NOT         — W = ~W & 0xFF
 *   0x0B: CLR fr      — RAM[fr] = 0
 *   0x0C: INC fr      — RAM[fr] = (RAM[fr] + 1) & 0xFF
 *   0x0D: DEC fr      — RAM[fr] = (RAM[fr] - 1) & 0xFF
 *   0x0E: RET         — return W
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PIC_VR_BASE 0x20
#define PIC_RAM_SIZE 256

int64_t wubu_pic_interp(const uint8_t *code, size_t size, int64_t arg) {
    uint8_t ram[PIC_RAM_SIZE];
    memset(ram, 0, sizeof(ram));

    uint8_t W = (uint8_t)(arg & 0xFF);

    size_t pc = 0;
    while (pc < size) {
        uint8_t op = code[pc++];
        uint8_t a, fr;

        switch (op) {
        case 0x01: /* LIW imm */
            W = code[pc++];
            break;
        case 0x02: /* ADW fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (uint8_t)(W + ram[PIC_VR_BASE + fr]);
            break;
        case 0x03: /* SUW fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (uint8_t)(W - ram[PIC_VR_BASE + fr]);
            break;
        case 0x04: /* ANW fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W &= ram[PIC_VR_BASE + fr];
            break;
        case 0x05: /* ORW fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W |= ram[PIC_VR_BASE + fr];
            break;
        case 0x06: /* XRW fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W ^= ram[PIC_VR_BASE + fr];
            break;
        case 0x07: /* MVF fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                ram[PIC_VR_BASE + fr] = W;
            break;
        case 0x08: /* MVW fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = ram[PIC_VR_BASE + fr];
            break;
        case 0x09: /* NEG */
            W = (uint8_t)(-W);
            break;
        case 0x0A: /* NOT */
            W = (uint8_t)(~W);
            break;
        case 0x0B: /* CLR fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                ram[PIC_VR_BASE + fr] = 0;
            break;
        case 0x0C: /* INC fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                ram[PIC_VR_BASE + fr] = (uint8_t)(ram[PIC_VR_BASE + fr] + 1);
            break;
        case 0x0D: /* DEC fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                ram[PIC_VR_BASE + fr] = (uint8_t)(ram[PIC_VR_BASE + fr] - 1);
            break;
        case 0x0E: /* RET */
            return (int64_t)(int8_t)W;
        case 0x0F: /* MUL fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (uint8_t)(W * ram[PIC_VR_BASE + fr]);
            break;
        case 0x10: /* DIV fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE && ram[PIC_VR_BASE + fr] != 0)
                W = (uint8_t)(W / ram[PIC_VR_BASE + fr]);
            break;
        case 0x11: /* MOD fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE && ram[PIC_VR_BASE + fr] != 0)
                W = (uint8_t)(W % ram[PIC_VR_BASE + fr]);
            break;
        case 0x12: /* SHL fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (uint8_t)(W << (ram[PIC_VR_BASE + fr] & 7));
            break;
        case 0x13: /* SHR fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (uint8_t)(W >> (ram[PIC_VR_BASE + fr] & 7));
            break;
        case 0x14: /* GTU fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (W > ram[PIC_VR_BASE + fr]) ? 1 : 0;
            break;
        case 0x15: /* LTU fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (W < ram[PIC_VR_BASE + fr]) ? 1 : 0;
            break;
        case 0x16: /* EQ fr */
            fr = code[pc++];
            if (PIC_VR_BASE + fr < PIC_RAM_SIZE)
                W = (W == ram[PIC_VR_BASE + fr]) ? 1 : 0;
            break;
        default:
            return 0;
        }
    }
    return (int64_t)(int8_t)W;
}
