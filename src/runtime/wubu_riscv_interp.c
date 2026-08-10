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
} rv_cpu_t;

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

        /* unrecognized: halt */
        break;
    }
    return cpu.x[10];  /* a0 = return value */
}
