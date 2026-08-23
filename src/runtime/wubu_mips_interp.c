/*
 * wibu_mips_interp.c — the MIPS interpreter.
 * Fixed: sllv/srlv use register shift amount. Added div/mod support.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include "wubu_softfloat.h"
#include <stdlib.h>
#include <string.h>

#define MIPS_MEM 65536

typedef struct {
    int32_t r[32];
    uint32_t pc;
    uint32_t hi, lo;
    uint8_t mem[MIPS_MEM];
    int fret_valid;
} mips_cpu_t;

static inline uint32_t fetch32(mips_cpu_t *cpu, const uint8_t *code, size_t size) {
    if (cpu->pc + 4 > size) return 0;
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

int64_t wubu_mips_run(const uint8_t *code, size_t size, int64_t arg) {
    size_t code_bytes = size < MIPS_MEM/2 ? size : MIPS_MEM/2;
    mips_cpu_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.r[4] = (int32_t)arg;
    /* Place stack after the code, aligned to 16 bytes */
    cpu.r[29] = (MIPS_MEM/2) & ~15;
    /* Copy code to beginning of mem */
    memcpy(cpu.mem, code, code_bytes);
    cpu.pc = 0;

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
                cpu.r[rd] = cpu.r[rs] + cpu.r[rt]; break;
            case 0x22: /* subu */
                cpu.r[rd] = cpu.r[rs] - cpu.r[rt]; break;
            case 0x24: /* and */
                cpu.r[rd] = cpu.r[rs] & cpu.r[rt]; break;
            case 0x25: /* or */
                cpu.r[rd] = cpu.r[rs] | cpu.r[rt]; break;
            case 0x26: /* xor */
                cpu.r[rd] = cpu.r[rs] ^ cpu.r[rt]; break;
            case 0x2A: /* slt — signed */
                cpu.r[rd] = ((int32_t)cpu.r[rs] < (int32_t)cpu.r[rt]) ? 1 : 0; break;
            case 0x2B: /* sltu — unsigned */
                cpu.r[rd] = ((uint32_t)cpu.r[rs] < (uint32_t)cpu.r[rt]) ? 1 : 0; break;
            case 0x00: /* sll — shift left logical (immediate) */
                cpu.r[rd] = (uint32_t)cpu.r[rt] << sa; break;
            case 0x02: /* srl — shift right logical (immediate) */
                cpu.r[rd] = (uint32_t)cpu.r[rt] >> sa; break;
            case 0x08: /* jr */
                cpu.pc = (uint32_t)cpu.r[rs]; continue;
            case 0x18: /* mult */
                { int64_t prod = (int64_t)(int32_t)cpu.r[rs] * (int64_t)(int32_t)cpu.r[rt];
                  cpu.lo = (uint32_t)(prod & 0xFFFFFFFF);
                  cpu.hi = (uint32_t)(prod >> 32); } break;
            case 0x12: /* mflo */
                cpu.r[rd] = cpu.lo; break;
            case 0x10: /* mfhi */
                cpu.r[rd] = cpu.hi; break;
            /* sllv — variable shift left (funct 0x04) */
            case 0x04:
                cpu.r[rd] = (uint32_t)cpu.r[rt] << (cpu.r[rs] & 0x1F); break;
            /* srlv — variable shift right (funct 0x06) */
            case 0x06:
                cpu.r[rd] = (uint32_t)cpu.r[rt] >> (cpu.r[rs] & 0x1F); break;
            /* div — quotient in lo, remainder in hi (funct 0x1A) */
            case 0x1A:
                if (cpu.r[rt] != 0) {
                    cpu.lo = (uint32_t)(int32_t)cpu.r[rs] / (uint32_t)(int32_t)cpu.r[rt];
                    cpu.hi = (uint32_t)(int32_t)cpu.r[rs] % (uint32_t)(int32_t)cpu.r[rt];
                }
                break;
            default: goto done;
            }
            cpu.r[0] = 0;
            break;
        case 0x09: /* addiu */
            cpu.r[rt] = cpu.r[rs] + (uint32_t)sext16(imm);
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
            continue;
        case 0x03: /* jal */
            cpu.r[31] = cpu.pc + 4;
            cpu.pc = (cpu.pc & 0xF0000000) | (target << 2);
            continue;
        case 0x23: /* lw */
            cpu.r[rt] = read32(&cpu, (uint32_t)cpu.r[rs] + sext16(imm));
            cpu.r[0] = 0;
            break;
        case 0x2B: /* sw */
            write32(&cpu, (uint32_t)cpu.r[rs] + sext16(imm), (uint32_t)cpu.r[rt]);
            break;
        case 0x3F: { /* WUBU_HOSTCALL: fn, dst, sa, sb as 4 inline words.
                     * Offsets are byte offsets from $gp-style base: we use
                     * the slot absolute addresses the driver stores (they
                     * are indices into cpu.mem via a reserved base reg). */
            uint32_t fn  = fetch32(&cpu, code, size);
            uint32_t dst = fetch32(&cpu, code, size);
            uint32_t sa  = fetch32(&cpu, code, size);
            uint32_t sb  = fetch32(&cpu, code, size);
            /* slots addressed off r[28] ($gp) which the prologue sets to
             * MIPS_MEM/2 — see driver; fall back to direct index if 0 */
            uint32_t basev = cpu.r[29];  /* $sp: slots are sp-relative */
            uint32_t fa = read32(&cpu, basev + sa);
            uint32_t fb = read32(&cpu, basev + sb);
            uint32_t r = 0;
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
            case 11: r = fa; cpu.fret_valid = 1; cpu.r[2] = (int32_t)r; break;
            default: break;
            }
            write32(&cpu, basev + dst, r);
            break;
        }
        default:
            goto done;
        }
    }
done:
    return (int64_t)cpu.r[2];
}
