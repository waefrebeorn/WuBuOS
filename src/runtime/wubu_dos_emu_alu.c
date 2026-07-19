/* wubu_dos_emu_alu.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). */
#include "wubu_dos_emu_internal.h"

uint16_t reg_read(WubuDosEmu *e, int modrm, int w) { int r = (modrm >> 3) & 7; return w ? rw(e, r) : rb(e, r); }

void logic16(WubuDosEmu *e, uint16_t v) {
    setF(e, F_CF, 0); setF(e, F_OF, 0); setF(e, F_AF, 0);
    setF(e, F_ZF, v == 0); setF(e, F_SF, (v & 0x8000) != 0); setF(e, F_PF, parity8((uint8_t)v));
}

uint16_t seg_for(WubuDosEmu *e, int id) {
    switch (id) {
        case 1: return e->es; case 2: return e->cs; case 3: return e->ss; default: return e->ds;
    }
}

/* ============================ effective address ============================ */

void do_cmps(WubuDosEmu *e, int w) {
    uint16_t a = w ? rd16(e, e->ds, e->si) : rd8(e, e->ds, e->si);
    uint16_t b = w ? rd16(e, e->es, e->di) : rd8(e, e->es, e->di);
    if (w) sub16(e, a, b, 0); else sub8(e, (uint8_t)a, (uint8_t)b, 0);
    int d = (getF(e, F_DF) ? -1 : 1);
    e->si = (uint16_t)(e->si + (w ? 2 : 1) * d);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}

/* ============================ ALU helpers ============================ */

uint16_t add16(WubuDosEmu *e, uint16_t a, uint16_t b, int cin) {
    uint32_t r = (uint32_t)a + b + (cin ? 1u : 0u);
    uint16_t res = (uint16_t)r;
    setF(e, F_CF, (r >> 16) != 0);
    setF(e, F_ZF, res == 0);
    setF(e, F_SF, (res & 0x8000) != 0);
    setF(e, F_AF, (((a & 0xF) + (b & 0xF) + (cin ? 1 : 0)) & 0x10) != 0);
    int sa = (int16_t)a < 0, sb = (int16_t)b < 0, sr = (int16_t)res < 0;
    setF(e, F_OF, (sa == sb) && (sr != sa));
    setF(e, F_PF, parity8((uint8_t)res));
    return res;
}

void logic8(WubuDosEmu *e, uint8_t v) {
    setF(e, F_CF, 0); setF(e, F_OF, 0); setF(e, F_AF, 0);
    setF(e, F_ZF, v == 0); setF(e, F_SF, (v & 0x80) != 0); setF(e, F_PF, parity8(v));
}

/* ============================ stack ============================ */

void reg_write(WubuDosEmu *e, int modrm, int w, uint16_t v) { int r = (modrm >> 3) & 7; if (w) ww(e, r, v); else wb(e, r, v); }

/* ============================ text screen ============================ */

uint16_t pop16(WubuDosEmu *e) { uint16_t v = rd16(e, e->ss, e->sp); e->sp += 2; return v; }


uint16_t do_shift16(WubuDosEmu *e, int op, uint16_t v, int cnt) {
    cnt &= 31; if (cnt == 0) return v;
    uint16_t res = v;
    for (int i = 0; i < cnt; i++) {
        int oldcf = getF(e, F_CF);
        switch (op) {
            case 0: case 1: { int msb = (res >> 15) & 1; res = (uint16_t)(res << 1); setF(e, F_CF, msb); } break;
            case 2: case 3: { int bit15 = (res >> 15) & 1, bit0 = res & 1; res = (uint16_t)((res << 1) | oldcf); setF(e, F_CF, bit0); setF(e, F_OF, bit15 != bit0); } break;
            case 4: { int bit0 = res & 1; res = (uint16_t)(res >> 1); setF(e, F_CF, bit0); } break;
            case 5: { int bit0 = res & 1, msb = (res >> 15) & 1; res = (uint16_t)((res >> 1) | (msb << 15)); setF(e, F_CF, bit0); } break;
            default: { int bit0 = res & 1; res = (uint16_t)((res >> 1) | (res & 0x8000)); setF(e, F_CF, bit0); } break;
        }
    }
    logic16(e, res);
    return res;
}

/* ============================ string ops ============================ */

void do_scas(WubuDosEmu *e, int w) {
    uint16_t acc = w ? e->ax : (e->ax & 0xFF);
    uint16_t m = w ? rd16(e, e->es, e->di) : rd8(e, e->es, e->di);
    if (w) sub16(e, acc, m, 0); else sub8(e, (uint8_t)acc, (uint8_t)m, 0);
    int d = (getF(e, F_DF) ? -1 : 1);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}

void do_lods(WubuDosEmu *e, int w) {
    if (w) e->ax = rd16(e, e->ds, e->si); else { e->ax = (e->ax & 0xFF00) | rd8(e, e->ds, e->si); }
    int d = (getF(e, F_DF) ? -1 : 1);
    e->si = (uint16_t)(e->si + (w ? 2 : 1) * d);
}

void rm_write(WubuDosEmu *e, int modrm, int w, uint16_t v) {
    if ((modrm >> 6) == 3) { if (w) ww(e, modrm & 7, v); else wb(e, modrm & 7, v); return; }
    uint16_t s, o; ea(e, modrm, &s, &o);
    if (w) wr16(e, s, o, v); else wr8(e, s, o, (uint8_t)v);
}

uint16_t sub16(WubuDosEmu *e, uint16_t a, uint16_t b, int cin) {
    uint32_t r = (uint32_t)a - b - (cin ? 1u : 0u);
    uint16_t res = (uint16_t)r;
    setF(e, F_CF, (r >> 16) != 0);
    setF(e, F_ZF, res == 0);
    setF(e, F_SF, (res & 0x8000) != 0);
    setF(e, F_AF, ((int)(a & 0xF) - (b & 0xF) - (cin ? 1 : 0)) < 0);
    int sa = (int16_t)a < 0, sb = (int16_t)b < 0, sr = (int16_t)res < 0;
    setF(e, F_OF, (sa != sb) && (sr != sa));
    setF(e, F_PF, parity8((uint8_t)res));
    return res;
}

void do_movs(WubuDosEmu *e, int w) {
    if (w) { uint16_t v = rd16(e, e->ds, e->si); wr16(e, e->es, e->di, v); }
    else  { uint8_t v = rd8(e, e->ds, e->si); wr8(e, e->es, e->di, v); }
    int d = (getF(e, F_DF) ? -1 : 1);
    e->si = (uint16_t)(e->si + (w ? 2 : 1) * d);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}

uint8_t add8(WubuDosEmu *e, uint8_t a, uint8_t b, int cin) {
    uint16_t r = (uint16_t)a + b + (cin ? 1u : 0u);
    uint8_t res = (uint8_t)r;
    setF(e, F_CF, (r >> 8) != 0);
    setF(e, F_ZF, res == 0);
    setF(e, F_SF, (res & 0x80) != 0);
    setF(e, F_AF, (((a & 0xF) + (b & 0xF) + (cin ? 1 : 0)) & 0x10) != 0);
    int sa = (int8_t)a < 0, sb = (int8_t)b < 0, sr = (int8_t)res < 0;
    setF(e, F_OF, (sa == sb) && (sr != sa));
    setF(e, F_PF, parity8(res));
    return res;
}

uint16_t rm_read(WubuDosEmu *e, int modrm, int w) {
    if ((modrm >> 6) == 3) return w ? rw(e, modrm & 7) : rb(e, modrm & 7);
    uint16_t s, o; ea(e, modrm, &s, &o);
    return w ? rd16(e, s, o) : rd8(e, s, o);
}

uint8_t alu8(WubuDosEmu *e, int arith, uint8_t a, uint8_t b) {
    uint8_t r;
    switch (arith) {
        case 0: r = add8(e, a, b, 0); break;
        case 1: r = a | b; logic8(e, r); break;
        case 2: r = add8(e, a, b, getF(e, F_CF)); break;
        case 3: r = sub8(e, a, b, getF(e, F_CF)); break;
        case 4: r = a & b; logic8(e, r); break;
        case 5: r = sub8(e, a, b, 0); break;
        case 6: r = a ^ b; logic8(e, r); break;
        default: r = sub8(e, a, b, 0); break;
    }
    return r;
}

/* ============================ one instruction ============================ */
/* Returns 0 if execution should continue, -1 if halted (error/term). */

void ea(WubuDosEmu *e, int modrm, uint16_t *seg, uint16_t *off) {
    int mod = modrm >> 6, rm = modrm & 7;
    uint16_t s = (e->seg_prefix ? seg_for(e, e->seg_prefix) : e->ds);
    uint16_t o = 0;
    if (mod == 0) {
        switch (rm) {
            case 0: o = (uint16_t)(e->bx + e->si); break;
            case 1: o = (uint16_t)(e->bx + e->di); break;
            case 2: o = (uint16_t)(e->bp + e->si); s = e->ss; break;
            case 3: o = (uint16_t)(e->bp + e->di); s = e->ss; break;
            case 4: o = e->si; break;
            case 5: o = e->di; break;
            case 6: o = fetch16(e); break;
            default: o = e->bx; break;
        }
    } else if (mod == 1) {
        int8_t d = (int8_t)fetch8(e);
        switch (rm) {
            case 0: o = (uint16_t)(e->bx + e->si + d); break;
            case 1: o = (uint16_t)(e->bx + e->di + d); break;
            case 2: o = (uint16_t)(e->bp + e->si + d); s = e->ss; break;
            case 3: o = (uint16_t)(e->bp + e->di + d); s = e->ss; break;
            case 4: o = (uint16_t)(e->si + d); break;
            case 5: o = (uint16_t)(e->di + d); break;
            case 6: o = (uint16_t)(e->bp + d); s = e->ss; break;
            default: o = (uint16_t)(e->bx + d); break;
        }
    } else {
        int16_t d = (int16_t)fetch16(e);
        switch (rm) {
            case 0: o = (uint16_t)(e->bx + e->si + d); break;
            case 1: o = (uint16_t)(e->bx + e->di + d); break;
            case 2: o = (uint16_t)(e->bp + e->si + d); s = e->ss; break;
            case 3: o = (uint16_t)(e->bp + e->di + d); s = e->ss; break;
            case 4: o = (uint16_t)(e->si + d); break;
            case 5: o = (uint16_t)(e->di + d); break;
            case 6: o = (uint16_t)(e->bp + d); s = e->ss; break;
            default: o = (uint16_t)(e->bx + d); break;
        }
    }
    *seg = s; *off = o;
}


uint8_t sub8(WubuDosEmu *e, uint8_t a, uint8_t b, int cin) {
    uint16_t r = (uint16_t)a - b - (cin ? 1u : 0u);
    uint8_t res = (uint8_t)r;
    setF(e, F_CF, (r >> 8) != 0);
    setF(e, F_ZF, res == 0);
    setF(e, F_SF, (res & 0x80) != 0);
    setF(e, F_AF, ((int)(a & 0xF) - (b & 0xF) - (cin ? 1 : 0)) < 0);
    int sa = (int8_t)a < 0, sb = (int8_t)b < 0, sr = (int8_t)res < 0;
    setF(e, F_OF, (sa != sb) && (sr != sa));
    setF(e, F_PF, parity8(res));
    return res;
}

uint16_t alu16(WubuDosEmu *e, int arith, uint16_t a, uint16_t b) {
    uint16_t r;
    switch (arith) {
        case 0: r = add16(e, a, b, 0); break;
        case 1: r = a | b; logic16(e, r); break;
        case 2: r = add16(e, a, b, getF(e, F_CF)); break;
        case 3: r = sub16(e, a, b, getF(e, F_CF)); break;
        case 4: r = a & b; logic16(e, r); break;
        case 5: r = sub16(e, a, b, 0); break;
        case 6: r = a ^ b; logic16(e, r); break;
        default: r = sub16(e, a, b, 0); break;
    }
    return r;
}

void push16(WubuDosEmu *e, uint16_t v) { e->sp -= 2; wr16(e, e->ss, e->sp, v); }

uint8_t do_shift8(WubuDosEmu *e, int op, uint8_t v, int cnt) {
    cnt &= 31; if (cnt == 0) return v;
    uint8_t res = v;
    for (int i = 0; i < cnt; i++) {
        int oldcf = getF(e, F_CF);
        switch (op) {
            case 0: case 1: { int msb = (res >> 7) & 1; res = (uint8_t)(res << 1); setF(e, F_CF, msb); } break;
            case 2: case 3: { int bit7 = (res >> 7) & 1, bit0 = res & 1; res = (uint8_t)((res << 1) | oldcf); setF(e, F_CF, bit0); setF(e, F_OF, bit7 != bit0); } break;
            case 4: { int bit0 = res & 1; res = (uint8_t)(res >> 1); setF(e, F_CF, bit0); } break;
            case 5: { int bit0 = res & 1, msb = (res >> 7) & 1; res = (uint8_t)((res >> 1) | (msb << 7)); setF(e, F_CF, bit0); } break;
            default: { int bit0 = res & 1; res = (uint8_t)((res >> 1) | (res & 0x80)); setF(e, F_CF, bit0); } break; /* SAR */
        }
    }
    logic8(e, res);
    return res;
}

void do_stos(WubuDosEmu *e, int w) {
    if (w) wr16(e, e->es, e->di, e->ax); else wr8(e, e->es, e->di, (uint8_t)e->ax);
    int d = (getF(e, F_DF) ? -1 : 1);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}
