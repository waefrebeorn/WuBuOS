/* wubu_dos_emu_regs.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). */
#include "wubu_dos_emu_internal.h"

uint16_t *regp(WubuDosEmu *e, int i) {
    switch (i & 7) {
        case 0: return &e->ax; case 1: return &e->cx; case 2: return &e->dx;
        case 3: return &e->bx; case 4: return &e->sp; case 5: return &e->bp;
        case 6: return &e->si; default: return &e->di;
    }
}
/* Byte-register access: 8086 encodes 0=AL 1=CL 2=DL 3=BL 4=AH 5=CH 6=DH 7=BH
 * (NOT SP/BP/SI/DI). AH/CH/DH/BH are the high bytes of AX/CX/DX/BX. */

void ww(WubuDosEmu *e, int i, uint16_t v) { *regp(e, i) = v; }

/* ============================ flags ============================ */

uint16_t rw(WubuDosEmu *e, int i) { return *regp(e, i); }

void wb(WubuDosEmu *e, int i, uint8_t v) {
    switch (i & 7) {
        case 0: e->ax = (e->ax & 0xFF00) | v; break;
        case 1: e->cx = (e->cx & 0xFF00) | v; break;
        case 2: e->dx = (e->dx & 0xFF00) | v; break;
        case 3: e->bx = (e->bx & 0xFF00) | v; break;
        case 4: e->ax = (e->ax & 0x00FF) | ((uint16_t)v << 8); break;
        case 5: e->cx = (e->cx & 0x00FF) | ((uint16_t)v << 8); break;
        case 6: e->dx = (e->dx & 0x00FF) | ((uint16_t)v << 8); break;
        default: e->bx = (e->bx & 0x00FF) | ((uint16_t)v << 8); break;
    }
}

int getF(WubuDosEmu *e, uint16_t b) { return (e->flags & b) ? 1 : 0; }

int parity8(uint8_t v) { int c = 0; for (int i = 0; i < 8; i++) c ^= (v >> i) & 1; return c == 0; }


uint8_t rb(WubuDosEmu *e, int i) {
    switch (i & 7) {
        case 0: return (uint8_t)e->ax;
        case 1: return (uint8_t)e->cx;
        case 2: return (uint8_t)e->dx;
        case 3: return (uint8_t)e->bx;
        case 4: return (uint8_t)(e->ax >> 8);
        case 5: return (uint8_t)(e->cx >> 8);
        case 6: return (uint8_t)(e->dx >> 8);
        default: return (uint8_t)(e->bx >> 8);
    }
}

void setF(WubuDosEmu *e, uint16_t b, int v) { if (v) e->flags |= b; else e->flags &= (uint16_t)~b; }
