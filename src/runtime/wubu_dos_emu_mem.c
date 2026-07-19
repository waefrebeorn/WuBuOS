/* wubu_dos_emu_mem.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). */
#include "wubu_dos_emu_internal.h"

void wr8(WubuDosEmu *e, uint16_t s, uint16_t o, uint8_t v) {
    e->mem[phys(e, s, o) & (WUBU_DOS_MEM_SIZE - 1)] = v;
}

void wr16(WubuDosEmu *e, uint16_t s, uint16_t o, uint16_t v) {
    wr8(e, s, o, (uint8_t)v);
    wr8(e, s, (uint16_t)(o + 1), (uint8_t)(v >> 8));
}

uint16_t fetch16(WubuDosEmu *e) { uint8_t lo = fetch8(e); uint8_t hi = fetch8(e); return (uint16_t)lo | ((uint16_t)hi << 8); }

/* ============================ registers ============================ */

uint8_t rd8(WubuDosEmu *e, uint16_t s, uint16_t o) {
    return e->mem[phys(e, s, o) & (WUBU_DOS_MEM_SIZE - 1)];
}

uint8_t fetch8(WubuDosEmu *e) { uint8_t v = rd8(e, e->cs, e->ip); e->ip++; return v; }

uint16_t rd16(WubuDosEmu *e, uint16_t s, uint16_t o) {
    return (uint16_t)rd8(e, s, o) | ((uint16_t)rd8(e, s, (uint16_t)(o + 1)) << 8);
}
