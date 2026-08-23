/*
 * wubu_6502_interp.c -- the MOS 6502 interpreter.
 *
 * Executes the bytes emitted by wubu_isa_6502.c (the 6502 driver).
 * Reads instructions from the `code` buffer, reads/writes data from
 * a 64K flat memory array.
 *
 * Implemented: LDA/LDX/LDY, STA/STX/STY,
 *   ADC/SBC/AND/ORA/EOR/CMP (immediate, zero-page, absolute),
 *   ASL/LSR/ROL/ROR (accumulator + zero-page),
 *   INC/DEC/INX/INY/DEX/DEY zero-page,
 *   TAX/TAY/TSX/TXA/TXS/TYA,
 *   PHA/PLA/PHP/PLP,
 *   BEQ/BNE/BCC/BCS/BMI/BPL/BVC/BVS (relative branches),
 *   JMP abs, JSR, RTS, RTI, BRK, NOP,
 *   CLC/SEC/CLD/SED/CLI/SEI/CLV.
 * 8-bit accumulator/addressing, 16-bit PC.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_softfloat.h"

#define CPU6502_MEM 65536  /* 64K address space */

/* P register bits */
#define P_C 0x01
#define P_Z 0x02
#define P_I 0x04
#define P_D 0x08
#define P_B 0x10
#define P_U 0x20
#define P_V 0x40
#define P_N 0x80

typedef struct {
    uint8_t a, x, y, s, p;     /* registers */
    uint16_t pc;
    uint8_t mem[CPU6502_MEM];  /* data memory */
    int halted;
    /* soft-float return scratch: set by hostcall fn=11 (FRET) */
    uint32_t fret;
    int fret_valid;
} cpu6502_t;

#define FLAG_N (cpu.p & P_N)
#define FLAG_Z (cpu.p & P_Z)
#define FLAG_C (cpu.p & P_C)
#define FLAG_V (cpu.p & P_V)

static uint8_t read8(cpu6502_t *cpu, uint16_t addr)
{
    if (addr < CPU6502_MEM) return cpu->mem[addr];
    return 0;
}
static void write8(cpu6502_t *cpu, uint16_t addr, uint8_t v)
{
    if (addr < CPU6502_MEM) cpu->mem[addr] = v;
}

/* set N and Z flags based on an 8-bit result */
static void set_nz(cpu6502_t *cpu, uint8_t v)
{
    cpu->p = (cpu->p & ~(P_N | P_Z)) |
             (v & P_N) |
             (v == 0 ? P_Z : 0);
}

/* set N, Z, V, C for a binary ALU operation */
static void set_nzv_c(cpu6502_t *cpu, uint16_t res, uint8_t a, uint8_t b,
                    int is_sub)
{
    uint8_t result8 = (uint8_t)(res & 0xFF);
    set_nz(cpu, result8);
    if (is_sub) {
        /* V for subtraction: (a^b) & (~a^res) & 0x80 */
        cpu->p = (cpu->p & ~P_V) |
                 (((a ^ b) & (~a ^ result8) & 0x80) ? P_V : 0);
        /* C = no borrow: a >= b */
        cpu->p = (cpu->p & ~P_C) | (a >= b ? P_C : 0);
    } else {
        /* V for addition: ~(a^b) & (a^res) & 0x80 */
        cpu->p = (cpu->p & ~P_V) |
                 ((~(a ^ b) & (a ^ result8) & 0x80) ? P_V : 0);
        /* C = carry out */
        cpu->p = (cpu->p & ~P_C) | (res > 0xFF ? P_C : 0);
    }
}

int64_t wubu_6502_run(const uint8_t *code, size_t size, int64_t arg)
{
    (void)arg;
    cpu6502_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.s = 0xFF;
    cpu.p = P_U | P_I;
    cpu.pc = 0;

    while (!cpu.halted && cpu.pc < size) {
        uint8_t op = code[cpu.pc++];
        uint8_t lo, hi;
        uint16_t addr, ea;
        int8_t rel;
        uint8_t m;

        switch (op) {
        /* ---- LDA ---- */
        case 0xA9: /* LDA #imm */      cpu.a = code[cpu.pc++]; set_nz(&cpu, cpu.a); break;
        case 0xA5: /* LDA zp */        addr = code[cpu.pc++];   cpu.a = read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0xB5: /* LDA zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; cpu.a = read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0xAD: /* LDA abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.a = read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0xBD: /* LDA abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; cpu.a = read8(&cpu, ea); set_nz(&cpu, cpu.a); break;
        case 0xB9: /* LDA abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; cpu.a = read8(&cpu, ea); set_nz(&cpu, cpu.a); break;
        case 0xA1: /* LDA (ind,X) */   lo = code[cpu.pc++]; addr = code[(lo + cpu.x) & 0xFF] | (code[(lo + cpu.x + 1) & 0xFF]<<8); cpu.a = read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0xB1: /* LDA (ind),Y */   lo = code[cpu.pc++]; addr = code[lo] | (code[lo+1]<<8); ea = (uint16_t)(addr + cpu.y); cpu.a = read8(&cpu, ea); set_nz(&cpu, cpu.a); break;

        /* ---- LDX ---- */
        case 0xA2: /* LDX #imm */      cpu.x = code[cpu.pc++]; set_nz(&cpu, cpu.x); break;
        case 0xA6: /* LDX zp */        addr = code[cpu.pc++];   cpu.x = read8(&cpu, addr); set_nz(&cpu, cpu.x); break;
        case 0xB6: /* LDX zp,Y */      addr = (code[cpu.pc++] + cpu.y) & 0xFF; cpu.x = read8(&cpu, addr); set_nz(&cpu, cpu.x); break;
        case 0xAE: /* LDX abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.x = read8(&cpu, addr); set_nz(&cpu, cpu.x); break;
        case 0xBE: /* LDX abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; cpu.x = read8(&cpu, ea); set_nz(&cpu, cpu.x); break;

        /* ---- LDY ---- */
        case 0xA0: /* LDY #imm */      cpu.y = code[cpu.pc++]; set_nz(&cpu, cpu.y); break;
        case 0xA4: /* LDY zp */        addr = code[cpu.pc++];   cpu.y = read8(&cpu, addr); set_nz(&cpu, cpu.y); break;
        case 0xB4: /* LDY zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; cpu.y = read8(&cpu, addr); set_nz(&cpu, cpu.y); break;
        case 0xAC: /* LDY abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.y = read8(&cpu, addr); set_nz(&cpu, cpu.y); break;
        case 0xBC: /* LDY abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; cpu.y = read8(&cpu, ea); set_nz(&cpu, cpu.y); break;

        /* ---- STA / STX / STY ---- */
        case 0x85: /* STA zp */        addr = code[cpu.pc++];   write8(&cpu, addr, cpu.a); break;
        case 0x95: /* STA zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; write8(&cpu, addr, cpu.a); break;
        case 0x8D: /* STA abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; write8(&cpu, addr, cpu.a); break;
        case 0x9D: /* STA abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; write8(&cpu, ea, cpu.a); break;
        case 0x99: /* STA abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; write8(&cpu, ea, cpu.a); break;
        case 0x81: /* STA (ind,X) */   lo = code[cpu.pc++]; addr = code[(lo + cpu.x) & 0xFF] | (code[(lo + cpu.x + 1) & 0xFF]<<8); write8(&cpu, addr, cpu.a); break;
        case 0x91: /* STA (ind),Y */   lo = code[cpu.pc++]; addr = code[lo] | (code[lo+1]<<8); ea = (uint16_t)(addr + cpu.y); write8(&cpu, ea, cpu.a); break;
        case 0x86: /* STX zp */        addr = code[cpu.pc++];   write8(&cpu, addr, cpu.x); break;
        case 0x96: /* STX zp,Y */      addr = (code[cpu.pc++] + cpu.y) & 0xFF; write8(&cpu, addr, cpu.x); break;
        case 0x8E: /* STX abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; write8(&cpu, addr, cpu.x); break;
        case 0x84: /* STY zp */        addr = code[cpu.pc++];   write8(&cpu, addr, cpu.y); break;
        case 0x94: /* STY zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; write8(&cpu, addr, cpu.y); break;
        case 0x8C: /* STY abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; write8(&cpu, addr, cpu.y); break;

        /* ---- ADC ---- */
        case 0x69: /* ADC #imm */      m = code[cpu.pc++]; goto adc_byte;
        case 0x65: /* ADC zp */        addr = code[cpu.pc++];   m = read8(&cpu, addr); goto adc_byte;
        case 0x75: /* ADC zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; m = read8(&cpu, addr); goto adc_byte;
        case 0x6D: /* ADC abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; m = read8(&cpu, addr); goto adc_byte;
        case 0x7D: /* ADC abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; m = read8(&cpu, ea); goto adc_byte;
        case 0x79: /* ADC abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; m = read8(&cpu, ea); goto adc_byte;
        adc_byte: {
            uint8_t a = cpu.a;
            uint16_t res = (uint16_t)(cpu.a + m + (FLAG_C ? 1 : 0));
            set_nzv_c(&cpu, res, a, m, 0);
            cpu.a = (uint8_t)(res & 0xFF);
            break;
        }

        /* ---- SBC ---- */
        case 0xE9: /* SBC #imm */      m = code[cpu.pc++]; goto sbc_byte;
        case 0xE5: /* SBC zp */        addr = code[cpu.pc++];   m = read8(&cpu, addr); goto sbc_byte;
        case 0xF5: /* SBC zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; m = read8(&cpu, addr); goto sbc_byte;
        case 0xED: /* SBC abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; m = read8(&cpu, addr); goto sbc_byte;
        case 0xFD: /* SBC abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; m = read8(&cpu, ea); goto sbc_byte;
        case 0xF9: /* SBC abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; m = read8(&cpu, ea); goto sbc_byte;
        sbc_byte: {
            uint8_t a = cpu.a;
            uint16_t res = (uint16_t)(cpu.a - m - (1 - (FLAG_C ? 1 : 0)));
            set_nzv_c(&cpu, res, a, m, 1);
            cpu.a = (uint8_t)(res & 0xFF);
            break;
        }

        /* ---- AND ---- */
        case 0x29: /* AND #imm */      cpu.a &= code[cpu.pc++]; set_nz(&cpu, cpu.a); break;
        case 0x25: /* AND zp */        addr = code[cpu.pc++];   cpu.a &= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x35: /* AND zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; cpu.a &= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x2D: /* AND abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.a &= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x3D: /* AND abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; cpu.a &= read8(&cpu, ea); set_nz(&cpu, cpu.a); break;
        case 0x39: /* AND abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; cpu.a &= read8(&cpu, ea); set_nz(&cpu, cpu.a); break;

        /* ---- ORA ---- */
        case 0x09: /* ORA #imm */      cpu.a |= code[cpu.pc++]; set_nz(&cpu, cpu.a); break;
        case 0x05: /* ORA zp */        addr = code[cpu.pc++];   cpu.a |= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x15: /* ORA zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; cpu.a |= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x0D: /* ORA abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.a |= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x1D: /* ORA abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; cpu.a |= read8(&cpu, ea); set_nz(&cpu, cpu.a); break;
        case 0x19: /* ORA abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; cpu.a |= read8(&cpu, ea); set_nz(&cpu, cpu.a); break;

        /* ---- EOR ---- */
        case 0x49: /* EOR #imm */      cpu.a ^= code[cpu.pc++]; set_nz(&cpu, cpu.a); break;
        case 0x45: /* EOR zp */        addr = code[cpu.pc++];   cpu.a ^= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x55: /* EOR zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; cpu.a ^= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x4D: /* EOR abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.a ^= read8(&cpu, addr); set_nz(&cpu, cpu.a); break;
        case 0x5D: /* EOR abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; cpu.a ^= read8(&cpu, ea); set_nz(&cpu, cpu.a); break;
        case 0x59: /* EOR abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; cpu.a ^= read8(&cpu, ea); set_nz(&cpu, cpu.a); break;

        /* ---- CMP ---- */
        case 0xC9: /* CMP #imm */      m = code[cpu.pc++]; goto cmp_byte;
        case 0xC5: /* CMP zp */        addr = code[cpu.pc++];   m = read8(&cpu, addr); goto cmp_byte;
        case 0xD5: /* CMP zp,X */      addr = (code[cpu.pc++] + cpu.x) & 0xFF; m = read8(&cpu, addr); goto cmp_byte;
        case 0xCD: /* CMP abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; m = read8(&cpu, addr); goto cmp_byte;
        case 0xDD: /* CMP abs,X */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.x; m = read8(&cpu, ea); goto cmp_byte;
        case 0xD9: /* CMP abs,Y */     addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; ea = addr + cpu.y; m = read8(&cpu, ea); goto cmp_byte;
        cmp_byte: {
            uint8_t a = cpu.a;
            uint16_t res = (uint16_t)(cpu.a - m);
            set_nzv_c(&cpu, res, a, m, 1);
            break;
        }

        /* ---- CPX ---- */
        case 0xE0: /* CPX #imm */      m = code[cpu.pc++]; { uint16_t r=(uint16_t)(cpu.x-m); set_nzv_c(&cpu,r,cpu.x,m,1); } break;
        case 0xE4: /* CPX zp */        addr = code[cpu.pc++]; m = read8(&cpu, addr); { uint16_t r=(uint16_t)(cpu.x-m); set_nzv_c(&cpu,r,cpu.x,m,1); } break;
        case 0xEC: /* CPX abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; m = read8(&cpu, addr); { uint16_t r=(uint16_t)(cpu.x-m); set_nzv_c(&cpu,r,cpu.x,m,1); } break;

        /* ---- CPY ---- */
        case 0xC0: /* CPY #imm */      m = code[cpu.pc++]; { uint16_t r=(uint16_t)(cpu.y-m); set_nzv_c(&cpu,r,cpu.y,m,1); } break;
        case 0xC4: /* CPY zp */        addr = code[cpu.pc++]; m = read8(&cpu, addr); { uint16_t r=(uint16_t)(cpu.y-m); set_nzv_c(&cpu,r,cpu.y,m,1); } break;
        case 0xCC: /* CPY abs */       addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; m = read8(&cpu, addr); { uint16_t r=(uint16_t)(cpu.y-m); set_nzv_c(&cpu,r,cpu.y,m,1); } break;

        /* ---- Shifts (accumulator) ---- */
        case 0x0A: /* ASL A */         { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (cpu.a & 0x80) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); cpu.a = (uint8_t)(cpu.a << 1); set_nz(&cpu, cpu.a); } break;
        case 0x4A: /* LSR A */         { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (cpu.a & 0x01) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); cpu.a = (uint8_t)(cpu.a >> 1); set_nz(&cpu, cpu.a); } break;
        case 0x2A: /* ROL A */         { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (cpu.a & 0x80) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); cpu.a = (uint8_t)((cpu.a << 1) | old_c); set_nz(&cpu, cpu.a); } break;
        case 0x6A: /* ROR A */         { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (cpu.a & 0x01) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); cpu.a = (uint8_t)((cpu.a >> 1) | (old_c << 7)); set_nz(&cpu, cpu.a); } break;

        /* ---- Shifts (zero-page) ---- */
        case 0x06: /* ASL zp */        addr = code[cpu.pc++]; m = read8(&cpu, addr); { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (m & 0x80) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); m = (uint8_t)(m << 1); write8(&cpu, addr, m); set_nz(&cpu, m); } break;
        case 0x46: /* LSR zp */        addr = code[cpu.pc++]; m = read8(&cpu, addr); { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (m & 0x01) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); m = (uint8_t)(m >> 1); write8(&cpu, addr, m); set_nz(&cpu, m); } break;
        case 0x26: /* ROL zp */        addr = code[cpu.pc++]; m = read8(&cpu, addr); { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (m & 0x80) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); m = (uint8_t)((m << 1) | old_c); write8(&cpu, addr, m); set_nz(&cpu, m); } break;
        case 0x66: /* ROR zp */        addr = code[cpu.pc++]; m = read8(&cpu, addr); { uint8_t old_c = (cpu.p & P_C); uint8_t nc = (m & 0x01) ? 1 : 0; cpu.p = (cpu.p & ~P_C) | (nc ? P_C : 0); m = (uint8_t)((m >> 1) | (old_c << 7)); write8(&cpu, addr, m); set_nz(&cpu, m); } break;

        /* ---- INC/DEC zero-page ---- */
        case 0xE6: /* INC zp */        addr = code[cpu.pc++]; m = (uint8_t)(read8(&cpu, addr) + 1); write8(&cpu, addr, m); set_nz(&cpu, m); break;
        case 0xC6: /* DEC zp */        addr = code[cpu.pc++]; m = (uint8_t)(read8(&cpu, addr) - 1); write8(&cpu, addr, m); set_nz(&cpu, m); break;

        /* ---- INX/INY/DEX/DEY ---- */
        case 0xE8: /* INX */            cpu.x = (uint8_t)(cpu.x + 1); set_nz(&cpu, cpu.x); break;
        case 0xC8: /* INY */            cpu.y = (uint8_t)(cpu.y + 1); set_nz(&cpu, cpu.y); break;
        case 0xCA: /* DEX */            cpu.x = (uint8_t)(cpu.x - 1); set_nz(&cpu, cpu.x); break;
        case 0x88: /* DEY */            cpu.y = (uint8_t)(cpu.y - 1); set_nz(&cpu, cpu.y); break;

        /* ---- Transfers ---- */
        case 0xAA: /* TAX */            cpu.x = cpu.a; set_nz(&cpu, cpu.x); break;
        case 0xA8: /* TAY */            cpu.y = cpu.a; set_nz(&cpu, cpu.y); break;
        case 0x98: /* TYA */            cpu.a = cpu.y; set_nz(&cpu, cpu.a); break;
        case 0x8A: /* TXA */            cpu.a = cpu.x; set_nz(&cpu, cpu.a); break;
        case 0xBA: /* TSX */            cpu.x = cpu.s; set_nz(&cpu, cpu.x); break;
        case 0x9A: /* TXS */            cpu.s = cpu.x; break;

        /* ---- Stack ---- */
        case 0x48: /* PHA */            write8(&cpu, 0x0100 + cpu.s, cpu.a); cpu.s = (uint8_t)(cpu.s - 1); break;
        case 0x68: /* PLA */            cpu.s = (uint8_t)(cpu.s + 1); cpu.a = read8(&cpu, 0x0100 + cpu.s); set_nz(&cpu, cpu.a); break;
        case 0x08: /* PHP */            write8(&cpu, 0x0100 + cpu.s, cpu.p | P_B | P_U); cpu.s = (uint8_t)(cpu.s - 1); break;
        case 0x28: /* PLP */            cpu.s = (uint8_t)(cpu.s + 1); cpu.p = (read8(&cpu, 0x0100 + cpu.s) & ~P_B) | P_U; break;

        /* ---- Branches (relative, 8-bit signed) ---- */
        case 0x10: /* BPL */            rel = (int8_t)code[cpu.pc++]; if (!FLAG_N) cpu.pc += rel; break;
        case 0x30: /* BMI */            rel = (int8_t)code[cpu.pc++]; if (FLAG_N) cpu.pc += rel; break;
        case 0x50: /* BVC */            rel = (int8_t)code[cpu.pc++]; if (!FLAG_V) cpu.pc += rel; break;
        case 0x70: /* BVS */            rel = (int8_t)code[cpu.pc++]; if (FLAG_V) cpu.pc += rel; break;
        case 0x90: /* BCC */            rel = (int8_t)code[cpu.pc++]; if (!FLAG_C) cpu.pc += rel; break;
        case 0xB0: /* BCS */            rel = (int8_t)code[cpu.pc++]; if (FLAG_C) cpu.pc += rel; break;
        case 0xD0: /* BNE */            rel = (int8_t)code[cpu.pc++]; if (!FLAG_Z) cpu.pc += rel; break;
        case 0xF0: /* BEQ */            rel = (int8_t)code[cpu.pc++]; if (FLAG_Z) cpu.pc += rel; break;

        /* ---- Jumps ---- */
        case 0x4C: /* JMP abs */        addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2; cpu.pc = addr; break;
        case 0x20: /* JSR abs */        addr = code[cpu.pc] | (code[cpu.pc+1]<<8); cpu.pc += 2;
                                      { uint16_t ret = cpu.pc;
                                        cpu.s = (uint8_t)(cpu.s - 1); write8(&cpu, 0x0100 + cpu.s, (uint8_t)((ret >> 8) & 0xFF));
                                        cpu.s = (uint8_t)(cpu.s - 1); write8(&cpu, 0x0100 + cpu.s, (uint8_t)(ret & 0xFF));
                                        cpu.pc = addr; } break;
        case 0x60: /* RTS */            cpu.s = (uint8_t)(cpu.s + 1); lo = read8(&cpu, 0x0100 + cpu.s);
                                      cpu.s = (uint8_t)(cpu.s + 1); hi = read8(&cpu, 0x0100 + cpu.s);
                                      cpu.pc = (uint16_t)((hi << 8) | lo) + 1; break;
        case 0x02: /* WUBU_HOSTCALL - soft-float escape hatch */
            {
                uint8_t fn = code[cpu.pc++];
                uint8_t dst = code[cpu.pc++];
                uint8_t sa  = code[cpu.pc++];
                uint8_t sb  = code[cpu.pc++];
                uint32_t fa, fb;
                /* read two 4-byte little-endian f32 operands from ZP
                 * memory (NOT the const code[] buffer). */
                fa  = (uint32_t)cpu.mem[sa+0];
                fa |= ((uint32_t)cpu.mem[sa+1]) << 8;
                fa |= ((uint32_t)cpu.mem[sa+2]) << 16;
                fa |= ((uint32_t)cpu.mem[sa+3]) << 24;
                fb  = (uint32_t)cpu.mem[sb+0];
                fb |= ((uint32_t)cpu.mem[sb+1]) << 8;
                fb |= ((uint32_t)cpu.mem[sb+2]) << 16;
                fb |= ((uint32_t)cpu.mem[sb+3]) << 24;
                uint32_t r;
                switch (fn) {
                    case 0:  r = wubu_sf_f32_add(fa, fb); break;
                    case 1:  r = wubu_sf_f32_sub(fa, fb); break;
                    case 2:  r = wubu_sf_f32_mul(fa, fb); break;
                    case 3:  r = wubu_sf_f32_div(fa, fb); break;
                    case 10: r = fa ^ 0x80000000u; break;   /* FNEG */
                    case 6:  r = (wubu_sf_f32_cmp(fa, fb) == 0) ? 0xFFFFFFFFu : 0; break;
                    case 8:  r = (wubu_sf_f32_cmp(fa, fb)  < 0) ? 0xFFFFFFFFu : 0; break;
                    case 9:  r = (wubu_sf_f32_cmp(fa, fb) <= 0) ? 0xFFFFFFFFu : 0; break;
                    case 11: r = fa; cpu.fret = fa; cpu.fret_valid = 1; break; /* FRET */
                default: r = 0; break;
                }
                /* write 4-byte result to dst ZP slot (little-endian)
                 * into the writable 64K memory image (code is const). */
                cpu.mem[dst+0] = (uint8_t)(r & 0xFF);
                cpu.mem[dst+1] = (uint8_t)((r >> 8) & 0xFF);
                cpu.mem[dst+2] = (uint8_t)((r >> 16) & 0xFF);
                cpu.mem[dst+3] = (uint8_t)((r >> 24) & 0xFF);
                break;
            }
        case 0x40: /* RTI */            cpu.s = (uint8_t)(cpu.s + 1); cpu.p = (read8(&cpu, 0x0100 + cpu.s) & ~P_B) | P_U;
                                      cpu.s = (uint8_t)(cpu.s + 1); lo = read8(&cpu, 0x0100 + cpu.s);
                                      cpu.s = (uint8_t)(cpu.s + 1); hi = read8(&cpu, 0x0100 + cpu.s);
                                      cpu.pc = (uint16_t)((hi << 8) | lo); break;

        /* ---- Flag instructions ---- */
        case 0x18: /* CLC */            cpu.p &= ~P_C; break;
        case 0x38: /* SEC */            cpu.p |= P_C; break;
        case 0xD8: /* CLD */            cpu.p &= ~P_D; break;
        case 0xF8: /* SED */            cpu.p |= P_D; break;
        case 0x58: /* CLI */            cpu.p &= ~P_I; break;
        case 0x78: /* SEI */            cpu.p |= P_I; break;
        case 0xB8: /* CLV */            cpu.p &= ~P_V; break;

        case 0xEA: /* NOP */            break;

        default:
            cpu.halted = 1;
            break;
        }
    }
    /* If a float-return hostcall (fn=11) ran, return its 32-bit f32 bits.
     * Otherwise sign-extend the 8-bit accumulator so the battery's negative
     * expectations (-1 for ~0, etc.) agree with the other drivers. */
    if (cpu.fret_valid) return (int64_t)(int32_t)cpu.fret;
    return (int64_t)(int8_t)cpu.a;
}
