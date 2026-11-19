/*
 * wubu_riscv_interp.c -- the RISC-V RV64I interpreter.
 *
 * Executes the bytes emitted by wubu_isa_riscv.c (the RV64I driver).
 * The subset: ADDI, LUI, AUIPC, LD/SD, ADD/SUB, MUL, DIV, REM,
 * DIVU, REMU, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU,
 * BEQ/BNE/BLT/BGE (signed), JAL/JALR, ECALL/EBREAK.
 * 64-bit registers x0-x31, little-endian, flat memory.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RV_MEM 65536  /* 64K guest memory */

typedef struct {
    int64_t x[32];      /* x0-x31 */
    uint64_t pc;
    uint8_t n, z, v, c; /* condition codes */
    uint8_t mem[RV_MEM];
    uint32_t fret;      /* soft-float return bits (hostcall fn=11) */
    int fret_valid;
    uint64_t call_stack[32];  /* hostcall fn=27/28 call depth */
    int call_sp;
} rv_cpu_t;

/* ---- hostcall soft-float service (CUSTOM-0 opcode 0x0B) --------------
 * The driver emits {0x0B, fn, dst_off, sa_off, sb_off} with fp-relative
 * slot offsets (see wubu_isa_riscv.c emit_fhostcall). Prefer the compiler
 * repo's bit-exact softfloat when on the include path; host IEEE float
 * otherwise. Same fn table as the z80/6502 interpreters. */
#if defined(__has_include)
#  if __has_include("wubu_softfloat.h")
#    include "wubu_softfloat.h"
#    define HCRV_SF 1
#  endif
#endif
#ifndef HCRV_SF
static inline uint32_t hcrv_f2b(float f){ uint32_t b; memcpy(&b,&f,4); return b; }
static inline float hcrv_b2f(uint32_t b){ float f; memcpy(&f,&b,4); return f; }
static inline uint32_t wubu_sf_f32_add(uint32_t a, uint32_t b){ return hcrv_f2b(hcrv_b2f(a)+hcrv_b2f(b)); }
static inline uint32_t wubu_sf_f32_sub(uint32_t a, uint32_t b){ return hcrv_f2b(hcrv_b2f(a)-hcrv_b2f(b)); }
static inline uint32_t wubu_sf_f32_mul(uint32_t a, uint32_t b){ return hcrv_f2b(hcrv_b2f(a)*hcrv_b2f(b)); }
static inline uint32_t wubu_sf_f32_div(uint32_t a, uint32_t b){ return hcrv_f2b(hcrv_b2f(a)/hcrv_b2f(b)); }
static inline int wubu_sf_f32_cmp(uint32_t a, uint32_t b){ float x=hcrv_b2f(a),y=hcrv_b2f(b); return (x<y)?-1:(x>y)?1:0; }
static inline uint32_t wubu_sf_i64_to_f32(int64_t i){ return hcrv_f2b((float)i); }
static inline int64_t wubu_sf_f32_to_i64(uint32_t f){ return (int64_t)hcrv_b2f(f); }
#endif

/* fp-relative 4-byte LE slot read/write */
static inline uint32_t hcrv_rd32(rv_cpu_t *cpu, int64_t fp, uint32_t off)
{
    uint64_t a = (uint64_t)(fp + (int64_t)(int32_t)off);
    if (a + 4 > RV_MEM) return 0;
    return (uint32_t)cpu->mem[a] | ((uint32_t)cpu->mem[a+1] << 8)
         | ((uint32_t)cpu->mem[a+2] << 16) | ((uint32_t)cpu->mem[a+3] << 24);
}
static inline void hcrv_wr32(rv_cpu_t *cpu, int64_t fp, uint32_t off, uint32_t v)
{
    uint64_t a = (uint64_t)(fp + (int64_t)(int32_t)off);
    if (a + 4 > RV_MEM) return;
    cpu->mem[a]   = (uint8_t)(v & 0xFF);
    cpu->mem[a+1] = (uint8_t)((v >> 8) & 0xFF);
    cpu->mem[a+2] = (uint8_t)((v >> 16) & 0xFF);
    cpu->mem[a+3] = (uint8_t)((v >> 24) & 0xFF);
}
/* int results zero-extend into the full 8-byte slot so a later LD never
 * picks up stale upper bytes from a previous occupant */
static inline void hcrv_wr64z(rv_cpu_t *cpu, int64_t fp, uint32_t off, uint32_t v)
{
    hcrv_wr32(cpu, fp, off, v);
    hcrv_wr32(cpu, fp, off + 4, 0);
}
/* mem64 cells: flat guest memory, cell*8 (z80 convention); the frame
 * lives at the top of memory so low cells never collide with slots. */
static inline uint8_t hcrv_cell_rd(rv_cpu_t *cpu, uint32_t cell)
{
    uint64_t a = (uint64_t)(cell & 0xFF) * 8u;
    return (a < RV_MEM) ? cpu->mem[a] : 0;
}
static inline void hcrv_cell_wr(rv_cpu_t *cpu, uint32_t cell, uint8_t v)
{
    uint64_t a = (uint64_t)(cell & 0xFF) * 8u;
    if (a < RV_MEM) cpu->mem[a] = v;
}

static uint32_t fetch32(rv_cpu_t *cpu, const uint8_t *code, size_t size)
{
    if (cpu->pc + 4 > size) return 0;
    uint32_t w = (uint32_t)((code[cpu->pc] << 0) |
                            (code[cpu->pc+1] << 8) |
                            (code[cpu->pc+2] << 16) |
                            (code[cpu->pc+3] << 24));
    cpu->pc += 4;
    return w;
}

static int64_t sext32(int32_t v) { return (int64_t)v; }
static int64_t sext12(uint32_t imm12) {
    int64_t v = (int64_t)(imm12 & 0xFFF);
    if (imm12 & 0x800) v |= ~0xFFFLL;
    return v;
}
static int64_t sext20(uint32_t imm20) {
    int64_t v = (int64_t)(imm20 & 0xFFFFF);
    if (imm20 & 0x80000) v |= ~0xFFFFFLL;
    return v;
}
static int64_t sext21(uint32_t imm21) {
    int64_t v = (int64_t)(imm21 & 0x1FFFFF);
    if (imm21 & 0x100000) v |= ~0x1FFFFFLL;
    return v;
}

static void set_nz(rv_cpu_t *cpu, int64_t v)
{
    cpu->n = (v < 0);
    cpu->z = (v == 0);
}

int64_t wubu_riscv_run(const uint8_t *code, size_t size, int64_t arg)
{
    (void)arg;
    rv_cpu_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.x[2] = RV_MEM;  /* sp at top of memory */

    while (cpu.pc + 4 <= size) {
        uint32_t inst = fetch32(&cpu, code, size);
        cpu.x[0] = 0;   /* RISC-V invariant: x0 is hardwired zero. JAL/JALR
                         * with rd=x0 and any ALU dst=x0 must not corrupt it
                         * (a polluted x0 silently offsets every `addi rd,x0,imm`). */
        uint8_t opcode = inst & 0x7F;
        uint8_t rd = (inst >> 7) & 0x1F;
        uint8_t funct3 = (inst >> 12) & 0x7;
        uint8_t rs1 = (inst >> 15) & 0x1F;
        uint8_t rs2 = (inst >> 20) & 0x1F;
        uint8_t funct7 = (inst >> 25) & 0x7F;

        if (opcode == 0x33) { /* OP */
            int64_t a = cpu.x[rs1];
            int64_t b = cpu.x[rs2];
            int64_t res = 0;
            switch (funct3) {
            case 0x0: /* ADD/SUB/MUL */
                if (funct7 == 0x00) res = a + b;         /* ADD */
                else if (funct7 == 0x20) res = a - b;    /* SUB */
                else if (funct7 == 0x01) res = a * b;    /* MUL */
                break;
            case 0x1: res = (int64_t)((uint64_t)a << (b & 0x3F)); break; /* SLL */
            case 0x2: res = (a < b) ? 1 : 0; break; /* SLT */
            case 0x3: res = ((uint64_t)a < (uint64_t)b) ? 1 : 0; break; /* SLTU */
            case 0x4: /* XOR / DIV / DIVU */
                if (funct7 == 0x00) res = a ^ b;            /* XOR */
                else if (funct7 == 0x01) {
                    if (b == 0) { res = -1; cpu.x[11] = a; } /* DIV by 0 */
                    else {
                        res = a / b;
                        cpu.x[11] = a % b;   /* the emitter's MIR_MOD
                                              * reads the remainder from
                                              * a1 after a DIV */
                    }
                } else if (funct7 == 0x05) {                 /* DIVU */
                    if ((uint64_t)b == 0) { res = -1; cpu.x[11] = a; }
                    else {
                        res = (int64_t)((uint64_t)a / (uint64_t)b);
                        cpu.x[11] = (int64_t)((uint64_t)a % (uint64_t)b);
                    }
                } else res = a ^ b;
                break;
            case 0x5: /* SRL/SRA */
                if (funct7 == 0x00) res = (int64_t)((uint64_t)a >> (b & 0x3F)); /* SRL */
                else res = a >> (b & 0x3F); /* SRA */
                break;
            case 0x6: /* OR / REM / REMU (REM leaves the remainder in
                      * x11 like the emitter's MIR_MOD expects) */
                if (funct7 == 0x00) {
                    res = a | b;                        /* OR */
                } else if (funct7 == 0x01) {
                    if (b == 0) res = a;                /* REM by 0 */
                    else {
                        res = a / b;
                        cpu.x[11] = a % b;              /* the remainder */
                    }
                } else if (funct7 == 0x05) {            /* REMU */
                    if ((uint64_t)b == 0) res = a;
                    else {
                        res = (int64_t)((uint64_t)a / (uint64_t)b);
                        cpu.x[11] = (int64_t)((uint64_t)a % (uint64_t)b);
                    }
                } else res = a | b;
                break;
            case 0x7: res = a & b; break; /* AND */
            default: break;
            }
            cpu.x[rd] = res;
            set_nz(&cpu, res);
            continue;
        }

        if (opcode == 0x13) { /* OP-IMM */
            int64_t a = cpu.x[rs1];
            int64_t imm = sext12(inst >> 20);
            int64_t res = 0;
            switch (funct3) {
            case 0x0: res = a + imm; break; /* ADDI */
            case 0x1: res = (int64_t)((uint64_t)a << (imm & 0x3F)); break; /* SLLI */
            case 0x2: res = (a < imm) ? 1 : 0; break; /* SLTI */
            case 0x3: res = ((uint64_t)a < (uint64_t)imm) ? 1 : 0; break; /* SLTIU */
            case 0x4: res = a ^ imm; break; /* XORI */
            case 0x5: /* SRLI/SRAI */
                if (funct7 == 0x00) res = (int64_t)((uint64_t)a >> (imm & 0x3F)); /* SRLI */
                else res = a >> (imm & 0x3F); /* SRAI */
                break;
            case 0x6: res = a | imm; break; /* ORI */
            case 0x7: res = a & imm; break; /* ANDI */
            default: break;
            }
            cpu.x[rd] = res;
            set_nz(&cpu, res);
            continue;
        }

        if (opcode == 0x03) { /* LOAD */
            int64_t offset = sext12(inst >> 20);
            uint64_t addr = (uint64_t)(cpu.x[rs1] + offset);
            int64_t val = 0;
            switch (funct3) {
            case 0x0: val = (int8_t)cpu.mem[addr]; break; /* LB */
            case 0x1: val = (int16_t)((cpu.mem[addr] << 8) | cpu.mem[addr+1]); break; /* LH */
            case 0x2: val = sext32(*(int32_t*)&cpu.mem[addr]); break; /* LW */
            case 0x3: val = (int64_t)((uint64_t)cpu.mem[addr] |
                        ((uint64_t)cpu.mem[addr+1] << 8) |
                        ((uint64_t)cpu.mem[addr+2] << 16) |
                        ((uint64_t)cpu.mem[addr+3] << 24) |
                        ((uint64_t)cpu.mem[addr+4] << 32) |
                        ((uint64_t)cpu.mem[addr+5] << 40) |
                        ((uint64_t)cpu.mem[addr+6] << 48) |
                        ((uint64_t)cpu.mem[addr+7] << 56)); break; /* LD */
            default: break;
            }
            cpu.x[rd] = val;
            set_nz(&cpu, val);
            continue;
        }

        if (opcode == 0x23) { /* STORE */
            int64_t offset = sext12(((inst >> 25) & 0x7F) << 5) | ((inst >> 7) & 0x1F);
            uint64_t addr = (uint64_t)(cpu.x[rs1] + offset);
            int64_t val = cpu.x[rs2];
            switch (funct3) {
            case 0x0: cpu.mem[addr] = (uint8_t)(val & 0xFF); break; /* SB */
            case 0x1: cpu.mem[addr] = (uint8_t)(val & 0xFF); cpu.mem[addr+1] = (uint8_t)((val >> 8) & 0xFF); break; /* SH */
            case 0x2: *(int32_t*)&cpu.mem[addr] = (int32_t)(val & 0xFFFFFFFF); break; /* SW */
            case 0x3: /* SD */
                for (int i = 0; i < 8; i++) cpu.mem[addr+i] = (uint8_t)((val >> (i*8)) & 0xFF);
                break;
            default: break;
            }
            continue;
        }

        if (opcode == 0x63) { /* BRANCH */
            int64_t offset = sext12(((inst >> 31) & 1) << 12) |
                             (((inst >> 25) & 0x3F) << 5) |
                             (((inst >> 8) & 0xF) << 1) |
                             (((inst >> 7) & 1) << 11);
            int taken = 0;
            switch (funct3) {
            case 0x0: taken = (cpu.x[rs1] == cpu.x[rs2]); break; /* BEQ */
            case 0x1: taken = (cpu.x[rs1] != cpu.x[rs2]); break; /* BNE */
            case 0x4: taken = (cpu.x[rs1] < cpu.x[rs2]); break; /* BLT */
            case 0x5: taken = ((uint64_t)cpu.x[rs1] < (uint64_t)cpu.x[rs2]); break; /* BLTU */
            case 0x6: taken = (cpu.x[rs1] >= cpu.x[rs2]); break; /* BGE */
            case 0x7: taken = ((uint64_t)cpu.x[rs1] >= (uint64_t)cpu.x[rs2]); break; /* BGEU */
            default: break;
            }
            if (taken) cpu.pc = (uint64_t)((int64_t)cpu.pc + offset - 4);
            continue;
        }

        if (opcode == 0x6F) { /* JAL */
            /* J-type: imm[20]=inst[31], imm[19:12]=inst[19:12],
             * imm[11]=inst[20], imm[10:1]=inst[30:21] */
            int64_t offset = (((inst >> 31) & 1) << 20) |
                             (((inst >> 12) & 0xFF) << 12) |
                             (((inst >> 20) & 1) << 11) |
                             (((inst >> 21) & 0x3FF) << 1);
            if (offset & 0x100000) offset -= 0x200000;   /* sign */
            cpu.x[rd] = cpu.pc;  /* return address */
            cpu.pc = (uint64_t)((int64_t)cpu.pc + offset - 4);
            continue;
        }

        if (opcode == 0x67) { /* JALR */
            int64_t offset = sext12(inst >> 20);
            uint64_t t = cpu.x[rs1] + offset;
            if (rd == 0 && rs1 == 1 && cpu.x[1] == 0 && offset == 0) {
                /* ret (jalr x0, x1, 0) with an UNINITIALIZED ra: the
                 * compiled program is STANDALONE (the emitter's ret
                 * closes it) — this is the program end, not a jump
                 * to address 0 (which would loop forever). */
                break;
            }
            cpu.x[rd] = cpu.pc;
            cpu.pc = t & ~1ULL;
            continue;
        }

        if (opcode == 0x37) { /* LUI */
            cpu.x[rd] = (int64_t)(inst & 0xFFFFF000);
            set_nz(&cpu, cpu.x[rd]);
            continue;
        }

        if (opcode == 0x17) { /* AUIPC */
            cpu.x[rd] = (int64_t)((inst & 0xFFFFF000) + cpu.pc - 4);
            set_nz(&cpu, cpu.x[rd]);
            continue;
        }

        if (opcode == 0x73) { /* SYSTEM */
            if (funct3 == 0x0 && rs2 == 0 && rd == 0) break; /* ECALL */
            if (funct3 == 0x0 && rs2 == 0 && rd == 1) break; /* EBREAK */
            continue;
        }

        if (opcode == 0x0B) { /* CUSTOM-0: WUBU_HOSTCALL */
            uint32_t fn      = fetch32(&cpu, code, size);
            uint32_t dst_off = fetch32(&cpu, code, size);
            uint32_t sa_off  = fetch32(&cpu, code, size);
            uint32_t sb_off  = fetch32(&cpu, code, size);
            int64_t fp = cpu.x[8];
            uint32_t fa = hcrv_rd32(&cpu, fp, sa_off);
            uint32_t fb = hcrv_rd32(&cpu, fp, sb_off);
            uint32_t r = 0;
            int wide = 1;
            switch (fn) {
            case 0:  r = wubu_sf_f32_add(fa, fb); break;
            case 1:  r = wubu_sf_f32_sub(fa, fb); break;
            case 2:  r = wubu_sf_f32_mul(fa, fb); break;
            case 3:  r = wubu_sf_f32_div(fa, fb); break;
            case 10: r = fa ^ 0x80000000u; break;
            case 6:  r = (wubu_sf_f32_cmp(fa,fb)==0)?0xFFFFFFFFu:0; break;
            case 7:  r = (wubu_sf_f32_cmp(fa,fb)!=0)?0xFFFFFFFFu:0; break;
            case 8:  r = (wubu_sf_f32_cmp(fa,fb) <0)?0xFFFFFFFFu:0; break;
            case 9:  r = (wubu_sf_f32_cmp(fa,fb)<=0)?0xFFFFFFFFu:0; break;
            case 4:  r = wubu_sf_i64_to_f32((int64_t)(int32_t)fa); break;
            case 5:  r = (uint32_t)wubu_sf_f32_to_i64(fa); wide = 0; break; /* int dst: 8-byte slot */
            case 11: r = fa; cpu.fret = fa; cpu.fret_valid = 1; break;
            /* MEM_LOAD: cell index = 4-byte LE addr slot; low byte out */
            case 25: r = hcrv_cell_rd(&cpu, fa); wide = 0; break;
            /* MEM_STORE: sa = value slot, sb = cell-index slot */
            case 26: hcrv_cell_wr(&cpu, fb, (uint8_t)(fa & 0xFF)); r = 0; wide = 0; break;
            /* CALL: abs32 target word follows the 5-word prologue */
            case 27: {
                uint32_t target = fetch32(&cpu, code, size);
                if (cpu.call_sp < 32) {
                    cpu.call_stack[cpu.call_sp++] = cpu.pc;
                    cpu.pc = target;
                }
                r = 0; wide = 0;
                break;
            }
            /* FUNC_RET: return byte -> vr0 slot (fp+8); pop or halt */
            case 28: {
                uint8_t v = (uint8_t)(fa & 0xFF);
                hcrv_wr64z(&cpu, fp, 8, v);
                if (cpu.call_sp > 0) cpu.pc = cpu.call_stack[--cpu.call_sp];
                else { cpu.x[10] = (int64_t)(int8_t)v; return cpu.x[10]; }
                r = 0; wide = 0;
                break;
            }
            default: break;
            }
            if (dst_off != 0) {
                if (wide) hcrv_wr32(&cpu, fp, dst_off, r);
                else hcrv_wr64z(&cpu, fp, dst_off, r);
            }
            continue;
        }

        /* unrecognized: halt */
        break;
    }
    if (cpu.fret_valid) return (int64_t)(int32_t)cpu.fret;
    return cpu.x[10];  /* a0 = return value */
}
