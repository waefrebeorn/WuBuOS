/*
 * wubu_8051_interp.c -- the Intel 8051 interpreter.
 *
 * Executes the bytes emitted by wubu_isa_8051.c (the 8051 driver).
 * 8-bit microcontroller: ACC (A), B register, R0-R7 (4 banks),
 * SP, DPTR (16-bit), PSW flags, 16-bit PC.
 *
 * Internal RAM: 128 bytes (0x00-0x7F). Virtual registers live at
 * address (vr + 0x30). SP starts at 0x30 to avoid collision with vrs.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define I8051_RAM_SIZE 128
#define I8051_VR_BASE  0x30   /* virtual register mapping start */

typedef struct {
    uint8_t  a;                 /* accumulator */
    uint8_t  b;                 /* B register (MUL/DIV) */
    uint8_t  r[8];              /* R0-R7 (current bank) */
    uint8_t  sp;                /* stack pointer (grows upward) */
    uint8_t  dpl, dph;          /* DPTR low/high */
    uint8_t  psw;               /* program status word */
    uint16_t pc;                /* program counter */
    uint8_t  ram[I8051_RAM_SIZE]; /* internal RAM */
    int      halted;
} i8051_t;

/* PSW bits */
#define PSW_C   0x80   /* carry */
#define PSW_AC  0x40   /* auxiliary carry */
#define PSW_OV  0x04   /* overflow */
#define PSW_P   0x01   /* parity */
#define PSW_RS0 0x08   /* register bank select bit 0 */
#define PSW_RS1 0x10   /* register bank select bit 1 */

static uint8_t i8051_read_ram(i8051_t *cpu, uint8_t addr)
{
    if (addr < I8051_RAM_SIZE) return cpu->ram[addr];
    return 0;
}

static void i8051_write_ram(i8051_t *cpu, uint8_t addr, uint8_t v)
{
    if (addr < I8051_RAM_SIZE) cpu->ram[addr] = v;
}

/* compute parity of a byte (even parity: 1 if odd number of 1-bits) */
static uint8_t parity8(uint8_t v)
{
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return (~v) & 1;
}

/* update P flag */
static void update_parity(i8051_t *cpu)
{
    cpu->psw = (cpu->psw & ~PSW_P) | (parity8(cpu->a) ? PSW_P : 0);
}

/* get Rn address from current bank */
static uint8_t rn_addr(i8051_t *cpu, uint8_t n)
{
    /* bank select from PSW RS1:RS0 */
    uint8_t bank = (cpu->psw & (PSW_RS1 | PSW_RS0)) >> 3;
    return (uint8_t)(bank * 8 + n);
}

/* read Rn */
static uint8_t read_rn(i8051_t *cpu, uint8_t n)
{
    return i8051_read_ram(cpu, rn_addr(cpu, n & 0x07));
}

/* write Rn */
static void write_rn(i8051_t *cpu, uint8_t n, uint8_t v)
{
    i8051_write_ram(cpu, rn_addr(cpu, n & 0x07), v);
}

/* push byte to stack */
static void push(i8051_t *cpu, uint8_t v)
{
    cpu->sp++;
    i8051_write_ram(cpu, cpu->sp, v);
}

/* pop byte from stack */
static uint8_t pop(i8051_t *cpu)
{
    uint8_t v = i8051_read_ram(cpu, cpu->sp);
    cpu->sp--;
    return v;
}

int64_t wubu_8051_interp_exec(const uint8_t *code, size_t size, int64_t arg) {
    i8051_t cpu;
    memset(&cpu, 0, sizeof(cpu));

    /* SP starts at 0x30 to avoid collision with virtual regs (0x30+) */
    cpu.sp = 0x30;

    /* Store arg into virtual register 0 (RAM address 0x30) */
    i8051_write_ram(&cpu, I8051_VR_BASE, (uint8_t)(arg & 0xFF));

    while (!cpu.halted && cpu.pc < size) {
        uint8_t op = code[cpu.pc++];
        uint8_t data, tmp, rn;
        uint16_t result;
        int8_t rel;

        switch (op) {
        /* ---- MOV A,#data ---- */
        case 0x74:
            cpu.a = code[cpu.pc++];
            break;

        /* ---- MOV A,direct (0xE5) ---- */
        case 0xE5: {
            uint8_t addr = code[cpu.pc++];
            cpu.a = (addr < I8051_RAM_SIZE) ? cpu.ram[addr] : 0;
            break;
        }

        /* ---- MOV direct,A (0xF5) ---- */
        case 0xF5: {
            uint8_t addr = code[cpu.pc++];
            if (addr < I8051_RAM_SIZE) cpu.ram[addr] = cpu.a;
            break;
        }

        /* ---- MOV A,Rn (0xE8-0xEF) ---- */
        case 0xE8: case 0xE9: case 0xEA: case 0xEB:
        case 0xEC: case 0xED: case 0xEE: case 0xEF:
            rn = op & 0x07;
            cpu.a = read_rn(&cpu, rn);
            break;

        /* ---- MOV Rn,A (0xA8-0xAF) ---- */
        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
        case 0xAC: case 0xAD: case 0xAE: case 0xAF:
            rn = op & 0x07;
            write_rn(&cpu, rn, cpu.a);
            break;

        /* ---- ADD A,#data ---- */
        case 0x24:
            data = code[cpu.pc++];
            result = (uint16_t)(cpu.a + data);
            /* auxiliary carry: carry from bit 3 */
            if ((cpu.a & 0x0F) + (data & 0x0F) > 0x0F)
                cpu.psw |= PSW_AC;
            else
                cpu.psw &= ~PSW_AC;
            /* overflow: sign of operands same, sign of result different */
            if (((cpu.a ^ result) & (data ^ result) & 0x80) != 0)
                cpu.psw |= PSW_OV;
            else
                cpu.psw &= ~PSW_OV;
            cpu.a = (uint8_t)(result & 0xFF);
            if (result > 0xFF) cpu.psw |= PSW_C; else cpu.psw &= ~PSW_C;
            update_parity(&cpu);
            break;

        /* ---- ADD A,Rn (0x28-0x2F) ---- */
        case 0x28: case 0x29: case 0x2A: case 0x2B:
        case 0x2C: case 0x2D: case 0x2E: case 0x2F:
            rn = op & 0x07;
            data = read_rn(&cpu, rn);
            result = (uint16_t)(cpu.a + data);
            if ((cpu.a & 0x0F) + (data & 0x0F) > 0x0F)
                cpu.psw |= PSW_AC;
            else
                cpu.psw &= ~PSW_AC;
            if (((cpu.a ^ result) & (data ^ result) & 0x80) != 0)
                cpu.psw |= PSW_OV;
            else
                cpu.psw &= ~PSW_OV;
            cpu.a = (uint8_t)(result & 0xFF);
            if (result > 0xFF) cpu.psw |= PSW_C; else cpu.psw &= ~PSW_C;
            update_parity(&cpu);
            break;

        /* ---- ADD A,direct (0x25) ---- */
        case 0x25: {
            uint8_t addr = code[cpu.pc++];
            data = (addr < I8051_RAM_SIZE) ? cpu.ram[addr] : 0;
            result = (uint16_t)(cpu.a + data);
            if ((cpu.a & 0x0F) + (data & 0x0F) > 0x0F) cpu.psw |= PSW_AC; else cpu.psw &= ~PSW_AC;
            if (((cpu.a ^ result) & (data ^ result) & 0x80) != 0) cpu.psw |= PSW_OV; else cpu.psw &= ~PSW_OV;
            cpu.a = (uint8_t)(result & 0xFF);
            if (result > 0xFF) cpu.psw |= PSW_C; else cpu.psw &= ~PSW_C;
            update_parity(&cpu);
            break;
        }

        /* ---- SUBB A,#data ---- */
        case 0x94:
            data = code[cpu.pc++];
            result = (uint16_t)(cpu.a - data - ((cpu.psw & PSW_C) ? 1 : 0));
            /* auxiliary borrow */
            if ((cpu.a & 0x0F) < (data & 0x0F) + ((cpu.psw & PSW_C) ? 1 : 0))
                cpu.psw |= PSW_AC;
            else
                cpu.psw &= ~PSW_AC;
            /* overflow */
            if (((cpu.a ^ data) & (cpu.a ^ (uint8_t)result) & 0x80) != 0)
                cpu.psw |= PSW_OV;
            else
                cpu.psw &= ~PSW_OV;
            cpu.a = (uint8_t)(result & 0xFF);
            if (result > 0xFF) cpu.psw &= ~PSW_C; else cpu.psw |= PSW_C;
            update_parity(&cpu);
            break;

        /* ---- SUBB A,Rn (0x98-0x9F) ---- */
        case 0x98: case 0x99: case 0x9A: case 0x9B:
        case 0x9C: case 0x9D: case 0x9E: case 0x9F:
            rn = op & 0x07;
            data = read_rn(&cpu, rn);
            goto subb_common;

        /* ---- SUBB A,direct (0x95) ---- */
        case 0x95: {
            uint8_t addr = code[cpu.pc++];
            data = (addr < I8051_RAM_SIZE) ? cpu.ram[addr] : 0;
            goto subb_common;
        }

        subb_common:
            result = (uint16_t)(cpu.a - data - ((cpu.psw & PSW_C) ? 1 : 0));
            if ((cpu.a & 0x0F) < (data & 0x0F) + ((cpu.psw & PSW_C) ? 1 : 0))
                cpu.psw |= PSW_AC;
            else
                cpu.psw &= ~PSW_AC;
            if (((cpu.a ^ data) & (cpu.a ^ (uint8_t)result) & 0x80) != 0)
                cpu.psw |= PSW_OV;
            else
                cpu.psw &= ~PSW_OV;
            cpu.a = (uint8_t)(result & 0xFF);
            if (result > 0xFF) cpu.psw &= ~PSW_C; else cpu.psw |= PSW_C;
            update_parity(&cpu);
            break;

        /* ---- MUL AB ---- */
        case 0xA4:
            result = (uint16_t)(cpu.a * cpu.b);
            cpu.a = (uint8_t)(result & 0xFF);
            cpu.b = (uint8_t)((result >> 8) & 0xFF);
            cpu.psw &= ~PSW_OV;  /* OV = 1 if result > 255 */
            if (result > 0xFF) cpu.psw |= PSW_OV;
            cpu.psw &= ~PSW_C;   /* C always cleared */
            update_parity(&cpu);
            break;

        /* ---- DIV AB ---- */
        case 0x84:
            if (cpu.b == 0) {
                cpu.psw |= PSW_OV;  /* divide by zero: OV = 1 */
            } else {
                tmp = cpu.a;
                cpu.a = (uint8_t)(tmp / cpu.b);
                cpu.b = (uint8_t)(tmp % cpu.b);
                cpu.psw &= ~PSW_OV;
            }
            cpu.psw &= ~PSW_C;
            update_parity(&cpu);
            break;

        /* ---- ANL A,#data ---- */
        case 0x54:
            cpu.a &= code[cpu.pc++];
            update_parity(&cpu);
            break;

        /* ---- ANL A,direct (0x55) ---- */
        case 0x55: {
            uint8_t addr = code[cpu.pc++];
            cpu.a &= (addr < I8051_RAM_SIZE) ? cpu.ram[addr] : 0;
            update_parity(&cpu);
            break;
        }

        /* ---- ORL A,#data ---- */
        case 0x44:
            cpu.a |= code[cpu.pc++];
            update_parity(&cpu);
            break;

        /* ---- ORL A,direct (0x45) ---- */
        case 0x45: {
            uint8_t addr = code[cpu.pc++];
            cpu.a |= (addr < I8051_RAM_SIZE) ? cpu.ram[addr] : 0;
            update_parity(&cpu);
            break;
        }

        /* ---- XRL A,#data ---- */
        case 0x64:
            cpu.a ^= code[cpu.pc++];
            update_parity(&cpu);
            break;

        /* ---- XRL A,direct (0x65) ---- */
        case 0x65: {
            uint8_t addr = code[cpu.pc++];
            cpu.a ^= (addr < I8051_RAM_SIZE) ? cpu.ram[addr] : 0;
            update_parity(&cpu);
            break;
        }

        /* ---- INC A ---- */
        case 0x04:
            cpu.a++;
            update_parity(&cpu);
            break;

        /* ---- DEC A ---- */
        case 0x14:
            cpu.a--;
            update_parity(&cpu);
            break;

        /* ---- CLR A ---- */
        case 0xE4:
            cpu.a = 0;
            update_parity(&cpu);
            break;

        /* ---- CPL A ---- */
        case 0xF4:
            cpu.a = ~cpu.a;
            update_parity(&cpu);
            break;

        /* ---- DJNZ Rn,rel (0xD8-0xDF) ---- */
        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            rn = op & 0x07;
            data = read_rn(&cpu, rn);
            data--;
            write_rn(&cpu, rn, data);
            rel = (int8_t)code[cpu.pc++];
            if (data != 0) cpu.pc += rel;
            break;

        /* ---- SJMP rel ---- */
        case 0x80:
            rel = (int8_t)code[cpu.pc++];
            cpu.pc += rel;
            break;

        /* ---- CJNE A,#data,rel ---- */
        case 0xB4:
            data = code[cpu.pc++];
            rel = (int8_t)code[cpu.pc++];
            /* set carry if A < data */
            if (cpu.a < data)
                cpu.psw |= PSW_C;
            else
                cpu.psw &= ~PSW_C;
            if (cpu.a != data)
                cpu.pc += rel;
            break;

        /* ---- MOV DPTR,#data16 ---- */
        case 0x90:
            cpu.dph = code[cpu.pc++];
            cpu.dpl = code[cpu.pc++];
            break;

        /* ---- NOP ---- */
        case 0x00:
            break;

        /* ---- RET ---- */
        case 0x22: {
            uint8_t hi = pop(&cpu);
            uint8_t lo = pop(&cpu);
            cpu.pc = (uint16_t)((hi << 8) | lo);
            break;
        }

        default:
            cpu.halted = 1;
            break;
        }
    }

    /* return ACC sign-extended to int64 */
    return (int64_t)(int8_t)cpu.a;
}