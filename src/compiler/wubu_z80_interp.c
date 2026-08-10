/*
 * wubu_z80_interp.c -- the Zilog Z80 interpreter.
 *
 * Executes the bytes emitted by wubu_isa_z80.c (the Z80 driver).
 * The compiler maps MIR to a small, verified Z80 subset:
 *   - virtual registers live in 16-bit little-endian MEMORY slots
 *     (frame at address 0, vr i at byte 2*i)
 *   - the accumulator A does the 8-bit ALU work
 *   - LD r, n / LD A, (nn) / LD (nn), A move values in/out of the slots
 *   - ALU on A: ADD/SUB/AND/OR/XOR/CP against every register + N
 *   - INC/DEC, JP/JR (with the cc conditions), CALL/RET, PUSH/POP,
 *     EX DE,HL, LD SP,HL
 *   - the MIR_RET convention: LD A, (va); HALT — the interpreter
 *     returns A (the result of the compiled program), matching the
 *     m68k (D0) / 6502 (A) family contract.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define Z80_MEM 65536   /* the full 64K Z80 address space (the driver's
                         * frames are tiny, but the block I/O + the
                         * (nn) addressing use the real 16-bit space) */

typedef struct {
    uint8_t a, f;
    uint8_t b, c, d, e, h, l;
    uint16_t sp, pc;
    uint16_t ix, iy;
    uint8_t halted;
    uint8_t mem[Z80_MEM];
    uint8_t iflag;   /* the interrupt flag (EI/DI) — minimal */
} z80_cpu_t;

/* the Z80 flags: S Z - H - P/V N C */
#define F_C 0x01
#define F_N 0x02
#define F_P 0x04
#define F_H 0x10
#define F_Z 0x40
#define F_S 0x80

static void set_flags_alu(z80_cpu_t *cpu, uint8_t res, int n_flag,
                          int pv_parity)
{
    cpu->f = 0;
    if (res & 0x80) cpu->f |= F_S;
    if (res == 0) cpu->f |= F_Z;
    if (n_flag) cpu->f |= F_N;
    /* parity/overflow: the driver's subset uses parity for logic, so
     * count the bits (the parity of the result) */
    if (pv_parity) {
        uint8_t v = res;
        int bits = 0;
        while (v) { bits += v & 1; v >>= 1; }
        if (!(bits & 1)) cpu->f |= F_P;
    }
    (void)cpu->f;   /* keep writing it (the flags are observable via
                       the driver's test harness later) */
}

/* SUB/CP flags: the Z80 sets C on borrow (A < v), which the DIV/MOD
 * loops test with JR C / JR NC. */
static void set_flags_sub(z80_cpu_t *cpu, uint8_t res, uint8_t a, uint8_t v)
{
    cpu->f = 0;
    if (res & 0x80) cpu->f |= F_S;
    if (res == 0) cpu->f |= F_Z;
    cpu->f |= F_N;
    if (a < v) cpu->f |= F_C;   /* borrow */
}

/* the 8-bit register file: A B C D E H L (the compiler's LD r,n +
 * ALU targets). Returns a pointer into the cpu. */
static uint8_t *reg8(z80_cpu_t *cpu, unsigned r)
{
    switch (r) {
    case 0: return &cpu->b;
    case 1: return &cpu->c;
    case 2: return &cpu->d;
    case 3: return &cpu->e;
    case 4: return &cpu->h;
    case 5: return &cpu->l;
    case 7: return &cpu->a;
    default: return &cpu->a;   /* (hl) handled separately by the caller */
    }
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* the condition codes for JP/JR cc (the driver emits the 3-bit cc:
 * 0=NZ 1=Z 2=NC 3=C 4=PO 5=PE 6=P 7=M) */
static int cond_cc(const z80_cpu_t *cpu, unsigned cc)
{
    switch (cc) {
    case 0: return !(cpu->f & F_Z);
    case 1: return (cpu->f & F_Z) != 0;
    case 2: return !(cpu->f & F_C);
    case 3: return (cpu->f & F_C) != 0;
    case 4: return (cpu->f & F_P) == 0;
    case 5: return (cpu->f & F_P) != 0;
    case 6: return (cpu->f & F_S) == 0;
    case 7: return (cpu->f & F_S) != 0;
    }
    return 0;
}

int64_t wubu_z80_run(const uint8_t *code, size_t size, int64_t arg)
{
    (void)arg;
    z80_cpu_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.sp = 0xFFFF;   /* the Z80 SP starts at the top of the 64K */
    cpu.pc = 0;

    /* load the program into the guest memory at 0 (the driver's
     * absolute-addressing emitter expects the code at the base) */
    size_t cl = size < Z80_MEM ? size : (size_t)Z80_MEM;
    memcpy(cpu.mem, code, cl);

    while (!cpu.halted && cpu.pc < cl) {
        uint8_t op = cpu.mem[cpu.pc++];
        unsigned r;

        switch (op) {
        /* LD r, n : 00rrr110 n */
        case 0x06: case 0x0E: case 0x16: case 0x1E:
        case 0x26: case 0x2E: case 0x3E: {
            uint8_t n = cpu.mem[cpu.pc++];
            unsigned rr = (op >> 3) & 7;
            if (rr == 6) cpu.a = n;   /* LD A, n (0x3E) */
            else *reg8(&cpu, rr) = n;
            break;
        }
        /* LD r, r' : 01rrrsss (r=111 -> A; the compiler emits LD B,A
         * = 0x47, LD C,A, LD D,A, LD E,A, LD H,A, LD L,A to move the
         * accumulator into a working register before an ALU op) */
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x67:
        case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6F:
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7F: {
            unsigned dst = (op >> 3) & 7;
            unsigned src = op & 7;
            uint16_t hl = (uint16_t)((cpu.h << 8) | cpu.l);
            uint8_t v = (src == 6) ? cpu.mem[hl] : *reg8(&cpu, src);
            if (dst == 6) {
                /* LD (HL), r — a store through the HL pointer */
                if (src != 6) cpu.mem[hl] = v;
            } else {
                *reg8(&cpu, dst) = v;
            }
            break;
        }
        /* LD A, (nn) : 00111010 */
        case 0x3A: {
            uint16_t nn = rd16(&cpu.mem[cpu.pc]);
            cpu.pc += 2;
            cpu.a = cpu.mem[nn];
            break;
        }
        /* LD (nn), A : 00110010 */
        case 0x32: {
            uint16_t nn = rd16(&cpu.mem[cpu.pc]);
            cpu.pc += 2;
            cpu.mem[nn] = cpu.a;
            break;
        }
        /* LD HL, (nn) : 00101010 */
        case 0x2A: {
            uint16_t nn = rd16(&cpu.mem[cpu.pc]);
            cpu.pc += 2;
            cpu.l = cpu.mem[nn];
            cpu.h = cpu.mem[nn + 1];
            break;
        }
        /* LD (nn), HL : 00100010 */
        case 0x22: {
            uint16_t nn = rd16(&cpu.mem[cpu.pc]);
            cpu.pc += 2;
            cpu.mem[nn] = cpu.l;
            cpu.mem[nn + 1] = cpu.h;
            break;
        }

        /* the ALU group: ADD A,r = 10000rrr, SUB r = 10010rrr,
         * AND r = 10100rrr, OR r = 10110rrr, XOR r = 10101rrr,
         * CP r = 10111rrr */
        case 0x80: case 0x81: case 0x82: case 0x83:
        case 0x84: case 0x85: case 0x87: {
            r = op & 7;
            uint8_t v = (r == 6) ? cpu.mem[(uint16_t)((cpu.h << 8) | cpu.l)] : *reg8(&cpu, r);
            uint8_t res = (uint8_t)(cpu.a + v);
            set_flags_alu(&cpu, res, 0, 0);
            cpu.a = res;
            break;
        }
        case 0x90: case 0x91: case 0x92: case 0x93:
        case 0x94: case 0x95: case 0x97: {
            r = op & 7;
            uint8_t v = (r == 6) ? cpu.mem[(uint16_t)((cpu.h << 8) | cpu.l)] : *reg8(&cpu, r);
            uint8_t res = (uint8_t)(cpu.a - v);
            set_flags_sub(&cpu, res, cpu.a, v);
            cpu.a = res;
            break;
        }
        case 0xA0: case 0xA1: case 0xA2: case 0xA3:
        case 0xA4: case 0xA5: case 0xA7: {
            r = op & 7;
            uint8_t v = (r == 6) ? cpu.mem[(uint16_t)((cpu.h << 8) | cpu.l)] : *reg8(&cpu, r);
            cpu.a &= v;
            set_flags_alu(&cpu, cpu.a, 0, 1);   /* parity of the result */
            break;
        }
        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
        case 0xAC: case 0xAD: case 0xAF: {
            r = op & 7;
            uint8_t v = (r == 6) ? cpu.mem[(uint16_t)((cpu.h << 8) | cpu.l)] : *reg8(&cpu, r);
            cpu.a ^= v;
            set_flags_alu(&cpu, cpu.a, 0, 1);
            break;
        }
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB7: {
            r = op & 7;
            uint8_t v = (r == 6) ? cpu.mem[(uint16_t)((cpu.h << 8) | cpu.l)] : *reg8(&cpu, r);
            cpu.a |= v;
            set_flags_alu(&cpu, cpu.a, 0, 1);
            break;
        }
        /* XOR A, n : 11101110 n — the NOT helper (MIR_NOT emits
         * XOR A, 0xFF) and immediate xors */
        case 0xEE: {
            uint8_t n = cpu.mem[cpu.pc++];
            cpu.a ^= n;
            set_flags_alu(&cpu, cpu.a, 0, 1);
            break;
        }
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBF: {
            r = op & 7;
            uint8_t v = (r == 6) ? cpu.mem[(uint16_t)((cpu.h << 8) | cpu.l)] : *reg8(&cpu, r);
            uint8_t res = (uint8_t)(cpu.a - v);   /* CP sets flags, no store */
            set_flags_sub(&cpu, res, cpu.a, v);
            break;
        }

        /* INC/DEC r : 00rrr100 / 00rrr101 — MUST set the flags (the
         * MUL/DIV/SHL/SHR loops test DEC B with JR NZ) */
        case 0x04: case 0x0C: case 0x14: case 0x1C:
        case 0x24: case 0x2C: case 0x3C: {
            r = (op >> 3) & 7;
            uint8_t *dst = (r == 6) ? &cpu.a : reg8(&cpu, r);
            *dst = (uint8_t)(*dst + 1);
            cpu.f &= (uint8_t)(F_C | F_P);   /* INC keeps C/PV */
            if (*dst & 0x80) cpu.f |= F_S;
            if (*dst == 0) cpu.f |= F_Z;
            break;
        }
        case 0x05: case 0x0D: case 0x15: case 0x1D:
        case 0x25: case 0x2D: case 0x3D: {
            r = (op >> 3) & 7;
            uint8_t *dst = (r == 6) ? &cpu.a : reg8(&cpu, r);
            *dst = (uint8_t)(*dst - 1);
            cpu.f &= (uint8_t)(F_C | F_P);   /* DEC keeps C/PV */
            cpu.f |= F_N;
            if (*dst & 0x80) cpu.f |= F_S;
            if (*dst == 0) cpu.f |= F_Z;
            break;
        }

        /* JP nn : 11000011 */
        case 0xC3: {
            cpu.pc = rd16(&cpu.mem[cpu.pc]);
            break;
        }
        /* JP cc, nn : 11cc010 nn */
        case 0xC2: case 0xCA: case 0xD2: case 0xDA:
        case 0xE2: case 0xEA: case 0xF2: case 0xFA: {
            unsigned cc = (op >> 3) & 7;
            uint16_t nn = rd16(&cpu.mem[cpu.pc]);
            cpu.pc += 2;
            if (cond_cc(&cpu, cc)) cpu.pc = nn;
            break;
        }
        /* JR d : 00011000 */
        case 0x18: {
            int8_t d = (int8_t)cpu.mem[cpu.pc++];
            cpu.pc = (uint16_t)(cpu.pc + d);
            break;
        }
        /* JR cc, d : 001cc000 */
        case 0x20: case 0x28: case 0x30: case 0x38: {
            unsigned cc = (op >> 3) & 3;   /* NZ Z NC C */
            int8_t d = (int8_t)cpu.mem[cpu.pc++];
            unsigned ccmap[4] = { 0, 1, 2, 3 };
            if (cond_cc(&cpu, ccmap[cc])) cpu.pc = (uint16_t)(cpu.pc + d);
            break;
        }
        /* CALL nn : 11001101 */
        case 0xCD: {
            uint16_t nn = rd16(&cpu.mem[cpu.pc]);
            cpu.pc += 2;
            cpu.sp -= 2;
            wr16(&cpu.mem[cpu.sp], cpu.pc);
            cpu.pc = nn;
            break;
        }
        /* RET : 11001001 */
        case 0xC9: {
            cpu.pc = rd16(&cpu.mem[cpu.sp]);
            cpu.sp += 2;
            break;
        }
        /* PUSH rr : 11x10101 / POP rr : 11x10001 (BC DE HL AF) */
        case 0xC5: case 0xD5: case 0xE5: case 0xF5: {
            unsigned pair = (op >> 4) & 3;
            uint16_t v;
            switch (pair) {
            case 0: v = (uint16_t)((cpu.b << 8) | cpu.c); break;
            case 1: v = (uint16_t)((cpu.d << 8) | cpu.e); break;
            case 2: v = (uint16_t)((cpu.h << 8) | cpu.l); break;
            default: v = (uint16_t)((cpu.a << 8) | cpu.f); break;
            }
            cpu.sp -= 2;
            wr16(&cpu.mem[cpu.sp], v);
            break;
        }
        case 0xC1: case 0xD1: case 0xE1: case 0xF1: {
            unsigned pair = (op >> 4) & 3;
            uint16_t v = rd16(&cpu.mem[cpu.sp]);
            cpu.sp += 2;
            switch (pair) {
            case 0: cpu.b = (uint8_t)(v >> 8); cpu.c = (uint8_t)v; break;
            case 1: cpu.d = (uint8_t)(v >> 8); cpu.e = (uint8_t)v; break;
            case 2: cpu.h = (uint8_t)(v >> 8); cpu.l = (uint8_t)v; break;
            default: cpu.a = (uint8_t)(v >> 8); cpu.f = (uint8_t)v; break;
            }
            break;
        }
        /* EX DE, HL : 11101011 */
        case 0xEB: {
            uint8_t t = cpu.d; cpu.d = cpu.h; cpu.h = t;
            t = cpu.e; cpu.e = cpu.l; cpu.l = t;
            break;
        }
        /* LD SP, HL : 11111001 */
        case 0xF9: {
            cpu.sp = (uint16_t)((cpu.h << 8) | cpu.l);
            break;
        }
        /* NOP : 00000000 */
        case 0x00:
            break;
        /* the 0xCB prefix: the shift/rotate family on A
         * (the compiler emits SRL A for MIR_SHR) */
        case 0xCB: {
            uint8_t cb = cpu.mem[cpu.pc++];
            switch (cb) {
            case 0x3F: cpu.a >>= 1; break;                      /* SRL A */
            case 0x2F: cpu.a = (uint8_t)((int8_t)cpu.a >> 1); break; /* SRA A */
            case 0x37: cpu.a = (uint8_t)(cpu.a << 1); break;    /* SLL A */
            case 0x07: cpu.a = (uint8_t)((cpu.a << 1) | (cpu.a >> 7)); break; /* RLC A */
            default: cpu.halted = 1; break;
            }
            break;
        }
        /* HALT : 01110110 — the MIR_RET convention: read A */
        case 0x76:
            cpu.halted = 1;
            break;

        default:
            /* decode miss: halt defensively (a miss must not loop) */
            cpu.halted = 1;
            break;
        }
    }

    /* the driver's RET convention: the compiled program ends with
     * LD A,(va); HALT — the accumulator carries the result. The 8-bit
     * result is SIGN-EXTENDED to the int64 contract (negative 8-bit
     * values like ~0 = 0xFF return -1, matching every other driver). */
    return (int64_t)(int8_t)cpu.a;
}
