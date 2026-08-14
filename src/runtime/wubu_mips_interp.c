/*
 * wubu_mips_interp.c -- the MIPS interpreter.
 *
 * Executes the bytes emitted by wubu_isa_mips.c (the MIPS driver).
 * 32 32-bit GPRs ($0 hardwired 0), $ra, $sp, little-endian.
 * The subset: addu/subu/and/or/xor/slt/sll/srl/mult/mflo,
 * addiu/ori/lui/lw/sw/beq/bne/j/jr.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MIPS_MEM 65536  /* 64K guest memory */

typedef struct {
    int32_t r[32];     /* $0-$31 */
    uint32_t pc;
    uint32_t hi, lo;   /* multiply result */
    uint8_t mem[MIPS_MEM];
} mips_cpu_t;

static inline uint32_t fetch32(mips_cpu_t *cpu, const uint8_t *code, size_t size) {
    if (cpu->pc + 4 > size) return 0;
    /* little-endian (mipsel) */
    uint32_t w = (uint32_t)code[cpu->pc]
               | ((uint32_t)code[cpu->pc+1] << 8)
               | ((uint32_t)code[cpu->pc+2] << 16)
               | ((uint32_t)code[cpu->pc+3] << 24);
    cpu->pc += 4;
    return w;
}

static inline int32_t sext16(uint16_t v) { return (int32_t)(int16_t)v; }

static inline void write32(mips_cpu_t *cpu, uint32_t addr, uint32_t val) {
    if (addr + 4 <= MIPS_MEM) {
        cpu->mem[addr] = val & 0xFF;
        cpu->mem[addr+1] = (val >> 8) & 0xFF;
        cpu->mem[addr+2] = (val >> 16) & 0xFF;
        cpu->mem[addr+3] = (val >> 24) & 0xFF;
    }
}

static inline uint32_t read32(mips_cpu_t *cpu, uint32_t addr) {
    if (addr + 4 > MIPS_MEM) return 0;
    return (uint32_t)cpu->mem[addr]
         | ((uint32_t)cpu->mem[addr+1] << 8)
         | ((uint32_t)cpu->mem[addr+2] << 16)
         | ((uint32_t)cpu->mem[addr+3] << 24);
}

static inline void write16(mips_cpu_t *cpu, uint32_t addr, uint16_t val) {
    if (addr + 2 <= MIPS_MEM) {
        cpu->mem[addr] = val & 0xFF;
        cpu->mem[addr+1] = (val >> 8) & 0xFF;
    }
}

static inline uint32_t read16(mips_cpu_t *cpu, uint32_t addr) {
    if (addr + 2 > MIPS_MEM) return 0;
    return (uint32_t)cpu->mem[addr] | ((uint32_t)cpu->mem[addr+1] << 8);
}

int64_t wubu_mips_run(const uint8_t *code, size_t size, int64_t arg) {
    mips_cpu_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.r[4] = (int32_t)arg;  /* $a0 = arg */
    cpu.r[29] = MIPS_MEM - 16; /* $sp = top of memory */
    cpu.pc = 0;

    /* Copy code to guest memory at offset 0 */
    size_t code_bytes = size < MIPS_MEM/2 ? size : MIPS_MEM/2;
    memcpy(cpu.mem, code, code_bytes);

    for (int iter = 0; iter < 1000000; iter++) {
        uint32_t inst = fetch32(&cpu, code, size);
        uint32_t op = (inst >> 26) & 0x3F;
        uint32_t rs = (inst >> 21) & 0x1F;
        uint32_t rt = (inst >> 16) & 0x1F;
        uint32_t rd = (inst >> 11) & 0x1F;
        uint32_t sa = (inst >> 6) & 0x1F;
        uint32_t funct = inst & 0x3F;
        uint16_t imm = inst & 0xFFFF;
        uint32_t target = inst & 0x03FFFFFF;

        switch (op) {
        case 0x00: /* SPECIAL */
            switch (funct) {
            case 0x20: /* addu */
                cpu.r[rd] = (uint32_t)cpu.r[rs] + (uint32_t)cpu.r[rt]; break;
            case 0x22: /* subu */
                cpu.r[rd] = (uint32_t)cpu.r[rs] - (uint32_t)cpu.r[rt]; break;
            case 0x24: /* and */
                cpu.r[rd] = cpu.r[rs] & cpu.r[rt]; break;
            case 0x25: /* or */
                cpu.r[rd] = cpu.r[rs] | cpu.r[rt]; break;
            case 0x26: /* xor */
                cpu.r[rd] = cpu.r[rs] ^ cpu.r[rt]; break;
            case 0x2A: /* slt */
                cpu.r[rd] = ((int32_t)cpu.r[rs] < (int32_t)cpu.r[rt]) ? 1 : 0; break;
            case 0x21: /* addu (alternate: rs+rt->rd, 3-reg) */
                cpu.r[rd] = (uint32_t)cpu.r[rs] + (uint32_t)cpu.r[rt]; break;
            case 0x00: /* sll */
                cpu.r[rd] = (uint32_t)cpu.r[rt] << sa; break;
            case 0x02: /* srl */
                cpu.r[rd] = (uint32_t)cpu.r[rt] >> sa; break;
            case 0x08: /* jr */
                cpu.pc = (uint32_t)cpu.r[rs]; break;
            case 0x18: /* mult */
                { int64_t prod = (int64_t)(int32_t)cpu.r[rs] * (int64_t)(int32_t)cpu.r[rt];
                  cpu.lo = (uint32_t)(prod & 0xFFFFFFFF);
                  cpu.hi = (uint32_t)(prod >> 32); } break;
            case 0x12: /* mflo */
                cpu.r[rd] = cpu.lo; break;
            case 0x10: /* mfhi */
                cpu.r[rd] = cpu.hi; break;
            default: goto done;
            }
            cpu.r[0] = 0;  /* $0 always 0 */
            break;
        case 0x09: /* addiu */
            cpu.r[rt] = (uint32_t)cpu.r[rs] + (uint32_t)sext16(imm);
            cpu.r[0] = 0;
            break;
        case 0x0C: /* andi */
            cpu.r[rt] = cpu.r[rs] & (uint32_t)imm;
            cpu.r[0] = 0;
            break;
        case 0x0D: /* ori */
            cpu.r[rt] = cpu.r[rs] | (uint32_t)imm;
            cpu.r[0] = 0;
            break;
        case 0x0E: /* xori */
            cpu.r[rt] = cpu.r[rs] ^ (uint32_t)imm;
            cpu.r[0] = 0;
            break;
        case 0x0F: /* lui */
            cpu.r[rt] = (uint32_t)imm << 16;
            cpu.r[0] = 0;
            break;
        case 0x04: /* beq */
            if (cpu.r[rs] == cpu.r[rt])
                cpu.pc = cpu.pc + (sext16(imm) << 2);
            break;
        case 0x05: /* bne */
            if (cpu.r[rs] != cpu.r[rt])
                cpu.pc = cpu.pc + (sext16(imm) << 2);
            break;
        case 0x02: /* j */
            cpu.pc = (cpu.pc & 0xF0000000) | (target << 2);
            break;
        case 0x03: /* jal */
            cpu.r[31] = cpu.pc + 4;
            cpu.pc = (cpu.pc & 0xF0000000) | (target << 2);
            break;
        case 0x23: /* lw */
            cpu.r[rt] = read32(&cpu, (uint32_t)cpu.r[rs] + sext16(imm));
            cpu.r[0] = 0;
            break;
        case 0x2B: /* sw */
            write32(&cpu, (uint32_t)cpu.r[rs] + sext16(imm), (uint32_t)cpu.r[rt]);
            break;
        case 0x20: /* lb */ case 0x24: /* lbu */
        case 0x21: /* lh */ case 0x25: /* lhu */
            /* skip for now */ break;
        default:
            goto done;
        }
    }
done:
    return (int64_t)cpu.r[2];  /* $v0 = return value */
}
