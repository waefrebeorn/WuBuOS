/*
 * wubu_m68k_interp.c -- the Motorola 68000 interpreter.
 *
 * Executes the bytes emitted by wubu_isa_m68k.c (the 68000 driver).
 * The subset: LINK/UNLK/RTS, MOVE.L (reg, imm, (d16,A6)), the ALU
 * .L ops (ADD/SUB/AND/OR/EOR/CMP), MULS/DIVS .W, NEG/NOT, MOVEQ,
 * SUBQ, LSL, TST, and the Bcc.s family (BRA/BEQ/BNE/BGT/BGE/BLT/BLE).
 * 32-bit registers D0-D7, A0-A7 (A7 = stack pointer), big-endian
 * memory, condition codes N Z V C X. The PC-relative branch
 * displacement is relative to the address of the word AFTER the
 * branch instruction (the 68000 semantics).
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define M68K_MEM 65536        /* 64K: the 68000 addresses 16MB, but the
                               * driver's frames are tiny; 64K is the
                               * interpreter's guest memory budget */

typedef struct {
    int32_t d[8];             /* D0-D7 */
    int32_t a[8];             /* A0-A7 (A7 = stack pointer) */
    uint32_t pc;
    uint8_t n, z, v, c, x;    /* condition codes */
    uint8_t mem[M68K_MEM];
} m68k_cpu_t;

static uint16_t fetch16(m68k_cpu_t *cpu, const uint8_t *code, size_t size)
{
    if (cpu->pc + 2 > size) return 0;
    uint16_t w = (uint16_t)((code[cpu->pc] << 8) | code[cpu->pc + 1]);
    cpu->pc += 2;
    return w;
}
static uint32_t fetch32(m68k_cpu_t *cpu, const uint8_t *code, size_t size)
{
    return ((uint32_t)fetch16(cpu, code, size) << 16) | fetch16(cpu, code, size);
}

static void set_nz(m68k_cpu_t *cpu, int32_t v)
{
    cpu->n = (v < 0);
    cpu->z = (v == 0);
}
static void set_flags_add(m68k_cpu_t *cpu, uint32_t res, uint32_t a, uint32_t b)
{
    cpu->n = (res >> 31) & 1;
    cpu->z = (res == 0);
    cpu->v = ((a ^ b) & ~(res ^ b) & 0x80000000u) != 0;
    cpu->c = (res < a) ? 1 : 0;   /* unsigned carry: 32-bit wrap */
}
/* SUB/CMP: dst - src. V = (dst^src) & (dst^res) sign bit (signed overflow:
 * operands of opposite sign, result sign opposite the minuend) */
static void set_flags_sub(m68k_cpu_t *cpu, uint32_t res, uint32_t dst, uint32_t src)
{
    cpu->n = (res >> 31) & 1;
    cpu->z = (res == 0);
    cpu->v = ((dst ^ src) & (dst ^ res) & 0x80000000u) != 0;
    cpu->c = (dst < src) ? 1 : 0;  /* borrow */
}

/* condition-code evaluation for Bcc (signed) */
static int cond_true(const m68k_cpu_t *cpu, uint16_t cc)
{
    switch (cc) {
    case 0x0: return 1;                      /* BRA */
    case 0x7: return cpu->z;                 /* BEQ */
    case 0x6: return !cpu->z;                /* BNE */
    case 0xE: return !(cpu->z || (cpu->n != cpu->v)); /* BGT */
    case 0xC: return cpu->n == cpu->v;       /* BGE */
    case 0xD: return cpu->n != cpu->v;       /* BLT */
    case 0xF: return cpu->z || (cpu->n != cpu->v);   /* BLE */
    default:  return 0;
    }
}

int64_t wubu_m68k_run(const uint8_t *code, size_t size, int64_t arg)
{
    (void)arg;
    m68k_cpu_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.a[7] = M68K_MEM;       /* A7 = SP at top of guest memory */

    /* guest memory: the frame goes at the top; the interpreter maps the
     * 68000's flat address space into mem[] with an offset so A7-based
     * addresses stay in-bounds. We keep it simple: addresses are direct
     * indices into mem[], and the LINK allocates downward from A7. */

    while (cpu.pc + 2 <= size) {
        uint16_t w = fetch16(&cpu, code, size);
        uint8_t op = (uint8_t)(w >> 8);
        uint8_t lo = (uint8_t)(w & 0xFF);

        /* LINK A6,#-d : 0x4E56 + s16 */
        if (w == 0x4E56) {
            int16_t d = (int16_t)fetch16(&cpu, code, size);
            cpu.a[7] -= 4;
            /* push old A6 onto the guest stack */
            uint32_t addr = (uint32_t)cpu.a[7];
            cpu.mem[addr]     = (uint8_t)(cpu.a[6] >> 24);
            cpu.mem[addr + 1] = (uint8_t)(cpu.a[6] >> 16);
            cpu.mem[addr + 2] = (uint8_t)(cpu.a[6] >> 8);
            cpu.mem[addr + 3] = (uint8_t)(cpu.a[6]);
            cpu.a[6] = cpu.a[7];
            cpu.a[7] += d;
            continue;
        }
        if (w == 0x4E5E) { /* UNLK A6 */
            cpu.a[7] = cpu.a[6];
            uint32_t addr = (uint32_t)cpu.a[7];
            cpu.a[6] = (int32_t)(((uint32_t)cpu.mem[addr] << 24) |
                                 ((uint32_t)cpu.mem[addr+1] << 16) |
                                 ((uint32_t)cpu.mem[addr+2] << 8) |
                                 (uint32_t)cpu.mem[addr+3]);
            cpu.a[7] += 4;
            continue;
        }
        if (w == 0x4E75) { /* RTS */
            return (int64_t)cpu.d[0];        /* result in D0 */
        }

        /* MOVE.L Dn,Dm : 0x2000 | (m<<9) | n  (dest mode 000, src mode 000) */
        if ((w & 0xF1F8) == 0x2000) {
            int m = (w >> 9) & 7;
            int n = w & 7;
            cpu.d[m] = cpu.d[n];
            set_nz(&cpu, cpu.d[m]);
            continue;
        }
        /* MOVE.L #imm32,Dm : 0x203C | (m<<9) + imm32 (src mode 111, reg 100) */
        if ((w & 0xF1FF) == 0x203C) {
            int m = (w >> 9) & 7;
            cpu.d[m] = (int32_t)fetch32(&cpu, code, size);
            set_nz(&cpu, cpu.d[m]);
            continue;
        }
        /* MOVE.L Dn,(d16,A6) : 0x2D40 | n  (dest mode 101=d16,An reg 110=A6) */
        if ((w & 0xFFF8) == 0x2D40) {
            int n = w & 7;
            int16_t d16 = (int16_t)fetch16(&cpu, code, size);
            uint32_t addr = (uint32_t)(cpu.a[6] + d16);
            int32_t v = cpu.d[n];
            cpu.mem[addr]     = (uint8_t)((uint32_t)v >> 24);
            cpu.mem[addr + 1] = (uint8_t)((uint32_t)v >> 16);
            cpu.mem[addr + 2] = (uint8_t)((uint32_t)v >> 8);
            cpu.mem[addr + 3] = (uint8_t)((uint32_t)v);
            continue;
        }
        /* MOVE.L (d16,A6),Dm : 0x202E | (m<<9)  (src mode 101, reg 110=A6) */
        if ((w & 0xF1FF) == 0x202E) {
            int m = (w >> 9) & 7;
            int16_t d16 = (int16_t)fetch16(&cpu, code, size);
            uint32_t addr = (uint32_t)(cpu.a[6] + d16);
            cpu.d[m] = (int32_t)(((uint32_t)cpu.mem[addr] << 24) |
                                 ((uint32_t)cpu.mem[addr+1] << 16) |
                                 ((uint32_t)cpu.mem[addr+2] << 8) |
                                 (uint32_t)cpu.mem[addr+3]);
            set_nz(&cpu, cpu.d[m]);
            continue;
        }

        /* ALU <ea=Dn>,Dn .L: base | (m<<9) | n
         *   base: ADD 0xD080 SUB 0x9080 AND 0xC080 OR 0x8080 CMP 0xB080
         *   mask: (w & 0xF1F8) == base  (size bits 7-6 = 10, src mode 000) */
        if ((w & 0xF1F8) == 0xD080 || (w & 0xF1F8) == 0x9080 ||
            (w & 0xF1F8) == 0xC080 || (w & 0xF1F8) == 0x8080 ||
            (w & 0xF1F8) == 0xB080) {
            int m = (w >> 9) & 7;            /* dest Dm */
            int n = w & 7;                   /* src Dn */
            int32_t a_ = cpu.d[n];
            int32_t b_ = cpu.d[m];
            int32_t r;
            switch (w & 0xF000) {
            case 0xD000: /* ADD.L */
                r = b_ + a_;
                set_flags_add(&cpu, (uint32_t)r, (uint32_t)b_, (uint32_t)a_);
                break;
            case 0x9000: /* SUB.L (dst - src) */
                r = b_ - a_;
                set_flags_sub(&cpu, (uint32_t)r, (uint32_t)b_, (uint32_t)a_);
                break;
            case 0xC000: /* AND.L */
                r = b_ & a_;
                set_nz(&cpu, r);
                break;
            case 0x8000: /* OR.L */
                r = b_ | a_;
                set_nz(&cpu, r);
                break;
            default:     /* 0xB000 CMP.L: sets flags only */
                r = b_ - a_;
                set_flags_sub(&cpu, (uint32_t)r, (uint32_t)b_, (uint32_t)a_);
                continue;
            }
            cpu.d[m] = r;
            continue;
        }
        /* EOR.L Dn,Dm : 0xB180 | (n<<9) | m  (src Dn at 11-9, dest at 2-0) */
        if ((w & 0xF1F8) == 0xB180) {
            int n = (w >> 9) & 7;
            int m = w & 7;
            cpu.d[m] ^= cpu.d[n];
            set_nz(&cpu, cpu.d[m]);
            continue;
        }
        /* MULS.W Dn,Dm : 0xC1C0 | (m<<9) | n  (16-bit signed product → Dm) */
        if ((w & 0xF1F8) == 0xC1C0) {
            int m = (w >> 9) & 7;
            int n = w & 7;
            cpu.d[m] = (int32_t)((int16_t)cpu.d[n] * (int16_t)cpu.d[m]);
            set_nz(&cpu, cpu.d[m]);
            continue;
        }
        /* DIVS.W Dn,Dm : 0x81C0 | (m<<9) | n  (32-bit Dm / 16-bit Dn;
         * quo→Dm, rem→D1 so the driver can MOVE.L D1,D0 for MIR_MOD) */
        if ((w & 0xF1F8) == 0x81C0) {
            int m = (w >> 9) & 7;
            int n = w & 7;
            int16_t q = (int16_t)cpu.d[n];
            int32_t rem = cpu.d[m] % q;
            int32_t quot = cpu.d[m] / q;
            cpu.d[m] = quot;
            cpu.d[1] = rem;      /* the driver reads D1 for MOD */
            set_nz(&cpu, quot);
            continue;
        }
        /* NEG.L Dn : 0x4480 | n */
        if ((w & 0xFFF8) == 0x4480) {
            int n = w & 7;
            cpu.d[n] = -cpu.d[n];
            set_nz(&cpu, cpu.d[n]);
            continue;
        }
        /* NOT.L Dn : 0x4680 | n */
        if ((w & 0xFFF8) == 0x4680) {
            int n = w & 7;
            cpu.d[n] = ~cpu.d[n];
            set_nz(&cpu, cpu.d[n]);
            continue;
        }
        /* TST.L Dn : 0x4A80 | n */
        if ((w & 0xFFF8) == 0x4A80) {
            int n = w & 7;
            set_nz(&cpu, cpu.d[n]);
            continue;
        }
        /* MOVEQ #imm8,Dn : 0x7000 | (n<<9) | imm */
        if ((w & 0xF100) == 0x7000) {
            int n = (w >> 9) & 7;
            int8_t imm = (int8_t)lo;
            cpu.d[n] = imm;
            set_nz(&cpu, cpu.d[n]);
            continue;
        }
        /* SUBQ.L #1,Dn : 0x5380 | n */
        if ((w & 0xFFF8) == 0x5380) {
            int n = w & 7;
            cpu.d[n] -= 1;
            set_nz(&cpu, cpu.d[n]);
            continue;
        }
        /* LSL.L #1,D0 : 0xE388   LSR.L #1,D0 : 0xE288 */
        if (w == 0xE388 || w == 0xE288) {
            uint32_t v = (uint32_t)cpu.d[0];
            cpu.d[0] = (w == 0xE388) ? (int32_t)(v << 1) : (int32_t)(v >> 1);
            set_nz(&cpu, cpu.d[0]);
            continue;
        }
        /* Bcc.s : 0x6xxx  (BRA/BEQ/BNE/BGT/BGE/BLT/BLE) */
        if ((w & 0xF000) == 0x6000) {
            uint16_t cc = (w >> 8) & 0xF;
            int8_t disp = (int8_t)lo;
            if (cond_true(&cpu, cc))
                cpu.pc = (uint32_t)((int32_t)cpu.pc + disp);
            continue;
        }

        /* unrecognized: halt (defensive — a decode miss must not loop) */
        break;
    }
    return (int64_t)cpu.d[0];
}
