/* wubu_dos_emu_decode.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). */
#include "wubu_dos_emu_internal.h"

int step(WubuDosEmu *e) {
    e->seg_prefix = 0; e->rep_prefix = 0;
    /* Prefixes. */
    for (;;) {
        uint8_t op = fetch8(e);
        if (op == 0x26) { e->seg_prefix = 1; continue; }
        if (op == 0x2E) { e->seg_prefix = 2; continue; }
        if (op == 0x36) { e->seg_prefix = 3; continue; }
        if (op == 0x3E) { e->seg_prefix = 4; continue; }
        if (op == 0xF0) continue;       /* LOCK */
        if (op == 0xF2) { e->rep_prefix = 2; continue; }
        if (op == 0xF3) { e->rep_prefix = 1; continue; }
        return decode_main(e, op);
    }
}

/* decode_main defined below (forward decl kept in same TU). */


int decode_main(WubuDosEmu *e, uint8_t op) {
    int w = (op & 1);
    switch (op) {
        /* ---- ADD/OR/ADC/SBB/AND/SUB/XOR/CMP family ---- */
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x08: case 0x09: case 0x0A: case 0x0B:
        case 0x10: case 0x11: case 0x12: case 0x13:
        case 0x18: case 0x19: case 0x1A: case 0x1B:
        case 0x20: case 0x21: case 0x22: case 0x23:
        case 0x28: case 0x29: case 0x2A: case 0x2B:
        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x38: case 0x39: case 0x3A: case 0x3B: {
            int modrm = fetch8(e);
            int is_cmp = (op >= 0x38);
            int arith = (op >> 3) & 7; /* 0 ADD,1 OR,2 ADC,3 SBB,4 AND,5 SUB,6 XOR,7 CMP */
            if (op & 2) { /* reg op r/m ; dest = reg */
                uint16_t src = rm_read(e, modrm, w);
                uint16_t dst = reg_read(e, modrm, w);
                uint16_t res = w ? alu16(e, arith, dst, src) : alu8(e, arith, (uint8_t)dst, (uint8_t)src);
                if (!is_cmp) reg_write(e, modrm, w, res);
            } else if (w) { /* r/m16 op r16 ; dest = r/m16 */
                uint16_t dst = rm_read(e, modrm, 1);
                uint16_t src = reg_read(e, modrm, 1);
                uint16_t res = alu16(e, arith, dst, src);
                if (!is_cmp) rm_write(e, modrm, 1, res);
            } else { /* r/m8 op r8 ; dest = r/m8 */
                uint8_t dst = (uint8_t)rm_read(e, modrm, 0);
                uint8_t src = (uint8_t)reg_read(e, modrm, 0);
                uint8_t res = alu8(e, arith, dst, src);
                if (!is_cmp) rm_write(e, modrm, 0, res);
            }
            return 0;
        }
        case 0x04: case 0x05: { /* AL/AX += imm */
            if (w) { e->ax = add16(e, e->ax, fetch16(e), 0); }
            else { uint8_t r = add8(e, (uint8_t)e->ax, fetch8(e), 0); e->ax = (e->ax & 0xFF00) | r; }
            return 0;
        }
        case 0x0C: case 0x0D: { if (w) { e->ax |= fetch16(e); logic16(e, e->ax); } else { uint8_t r = (uint8_t)e->ax | fetch8(e); e->ax = (e->ax & 0xFF00) | r; logic8(e, r); } return 0; }
        case 0x14: case 0x15: { if (w) { e->ax = add16(e, e->ax, fetch16(e), getF(e, F_CF)); } else { uint8_t r = add8(e, (uint8_t)e->ax, fetch8(e), getF(e, F_CF)); e->ax = (e->ax & 0xFF00) | r; } return 0; }
        case 0x1C: case 0x1D: { if (w) { e->ax = sub16(e, e->ax, fetch16(e), getF(e, F_CF)); } else { uint8_t r = sub8(e, (uint8_t)e->ax, fetch8(e), getF(e, F_CF)); e->ax = (e->ax & 0xFF00) | r; } return 0; }
        case 0x24: case 0x25: { if (w) { e->ax &= fetch16(e); logic16(e, e->ax); } else { uint8_t r = (uint8_t)e->ax & fetch8(e); e->ax = (e->ax & 0xFF00) | r; logic8(e, r); } return 0; }
        case 0x2C: case 0x2D: { if (w) { e->ax = sub16(e, e->ax, fetch16(e), 0); } else { uint8_t r = sub8(e, (uint8_t)e->ax, fetch8(e), 0); e->ax = (e->ax & 0xFF00) | r; } return 0; }
        case 0x34: case 0x35: { if (w) { e->ax ^= fetch16(e); logic16(e, e->ax); } else { uint8_t r = (uint8_t)e->ax ^ fetch8(e); e->ax = (e->ax & 0xFF00) | r; logic8(e, r); } return 0; }
        case 0x3C: case 0x3D: { if (w) { sub16(e, e->ax, fetch16(e), 0); } else { uint8_t r = sub8(e, (uint8_t)e->ax, fetch8(e), 0); (void)r; } return 0; }

        /* ---- PUSH/POP segment ---- */
        case 0x06: push16(e, e->es); return 0;
        case 0x07: e->es = pop16(e); return 0;
        case 0x0E: push16(e, e->cs); return 0;
        case 0x0F: e->cs = pop16(e); return 0;
        case 0x16: push16(e, e->ss); return 0;
        case 0x17: e->ss = pop16(e); return 0;
        case 0x1E: push16(e, e->ds); return 0;
        case 0x1F: e->ds = pop16(e); return 0;

        /* ---- decimals (mostly no-ops; kept for completeness) ---- */
        case 0x27: case 0x2F: case 0x37: case 0x3F: return 0;

        /* ---- INC/DEC reg ---- */
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            ww(e, op & 7, add16(e, rw(e, op & 7), 1, 0)); return 0;
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            ww(e, op & 7, sub16(e, rw(e, op & 7), 1, 0)); return 0;

        /* ---- PUSH/POP reg ---- */
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            push16(e, rw(e, op & 7)); return 0;
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            ww(e, op & 7, pop16(e)); return 0;

        /* ---- conditional jumps rel8 ---- */
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            int8_t r = (int8_t)fetch8(e);
            if (cond_met(e, op - 0x70)) e->ip = (uint16_t)(e->ip + r);
            return 0;
        }

        /* ---- GRP1 ---- */
        case 0x80: case 0x81: case 0x82: case 0x83: {
            int modrm = fetch8(e); int arith = (modrm >> 3) & 7;
            uint16_t imm = (op == 0x81) ? fetch16(e) : (uint16_t)(int16_t)(int8_t)fetch8(e);
            uint16_t dst = rm_read(e, modrm, w); uint16_t res;
            switch (arith) { case 0: res = add16(e, dst, imm, 0); break; case 1: res = dst | imm; logic16(e, res); break;
                case 2: res = add16(e, dst, imm, getF(e, F_CF)); break; case 3: res = sub16(e, dst, imm, getF(e, F_CF)); break;
                case 4: res = dst & imm; logic16(e, res); break; case 5: res = sub16(e, dst, imm, 0); break;
                case 6: res = dst ^ imm; logic16(e, res); break; default: res = sub16(e, dst, imm, 0); break; }
            if (arith != 7) rm_write(e, modrm, w, res);
            return 0;
        }

        /* ---- TEST / XCHG / MOV r/m,r ---- */
        case 0x84: case 0x85: { int m = fetch8(e); uint16_t a = rm_read(e, m, w), b = reg_read(e, m, w); logic16(e, w ? (a & b) : (a & b)); return 0; }
        case 0x86: case 0x87: { int m = fetch8(e); uint16_t a = rm_read(e, m, w), b = reg_read(e, m, w); rm_write(e, m, w, b); reg_write(e, m, w, a); return 0; }
        case 0x88: { int m = fetch8(e); rm_write(e, m, 0, reg_read(e, m, 0)); return 0; }
        case 0x89: { int m = fetch8(e); rm_write(e, m, 1, reg_read(e, m, 1)); return 0; }
        case 0x8A: { int m = fetch8(e); reg_write(e, m, 0, rm_read(e, m, 0)); return 0; }
        case 0x8B: { int m = fetch8(e); reg_write(e, m, 1, rm_read(e, m, 1)); return 0; }
        case 0x8C: { int m = fetch8(e); int sr = (m >> 3) & 7; uint16_t sv = (sr==0)?e->es:(sr==1)?e->cs:(sr==2)?e->ss:e->ds; rm_write(e, m, 1, sv); return 0; }
        case 0x8D: { int m = fetch8(e); uint16_t s, o; ea(e, m, &s, &o); reg_write(e, m, 1, o); return 0; }
        case 0x8E: { int m = fetch8(e); int sr = (m >> 3) & 7; uint16_t sv = rm_read(e, m, 1);
                     if (sr==0) e->es = sv; else if (sr==1) e->cs = sv; else if (sr==2) e->ss = sv; else e->ds = sv; return 0; }
        case 0x8F: { int m = fetch8(e); rm_write(e, m, 1, pop16(e)); return 0; }

        /* ---- XCHG AX,r / NOP ---- */
        case 0x90: return 0;
        case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
            uint16_t t = e->ax; e->ax = rw(e, op & 7); ww(e, op & 7, t); return 0;
        }

        /* ---- flags / misc ---- */
        case 0x98: e->ax = (int8_t)(e->ax & 0xFF); return 0; /* CBW */
        case 0x99: { if ((int8_t)(e->ax & 0xFF) < 0) e->dx = 0xFFFF; else e->dx = 0; return 0; } /* CWD */
        case 0x9C: push16(e, e->flags); return 0; /* PUSHF */
        case 0x9D: e->flags = pop16(e); return 0;  /* POPF */
        case 0x9E: { uint8_t ah = 0; ah |= getF(e,F_CF); ah |= getF(e,F_PF)<<2; ah |= getF(e,F_AF)<<4; ah |= getF(e,F_ZF)<<6; ah |= getF(e,F_SF)<<7; e->ax = (e->ax & 0x00FF) | (ah << 8); return 0; } /* SAHF */
        case 0x9F: { uint8_t ah = (uint8_t)(e->ax >> 8); setF(e,F_CF,ah&1); setF(e,F_PF,ah&4); setF(e,F_AF,ah&16); setF(e,F_ZF,ah&64); setF(e,F_SF,ah&128); return 0; } /* LAHF */

        /* ---- MOV moffs ---- */
        case 0xA0: e->ax = (e->ax & 0xFF00) | rd8(e, e->ds, fetch16(e)); return 0;
        case 0xA1: e->ax = rd16(e, e->ds, fetch16(e)); return 0;
        case 0xA2: wr8(e, e->ds, fetch16(e), (uint8_t)e->ax); return 0;
        case 0xA3: wr16(e, e->ds, fetch16(e), e->ax); return 0;

        /* ---- string ops ---- */
        case 0xA4: case 0xA5: { int ww_ = (op & 1); if (e->rep_prefix) { while (e->cx && e->state==WUBU_DOS_RUNNING) { do_movs(e, ww_); e->cx--; } } else do_movs(e, ww_); return 0; }
        case 0xA6: case 0xA7: { int ww_ = (op & 1); if (e->rep_prefix) { int c = (e->rep_prefix==2)?0:1; while (e->cx) { do_cmps(e, ww_); e->cx--; if (getF(e,F_ZF)!=c) break; } } else do_cmps(e, ww_); return 0; }
        case 0xAA: case 0xAB: { int ww_ = (op & 1); if (e->rep_prefix) { while (e->cx) { do_stos(e, ww_); e->cx--; } } else do_stos(e, ww_); return 0; }
        case 0xAC: case 0xAD: { int ww_ = (op & 1); if (e->rep_prefix) { while (e->cx) { do_lods(e, ww_); e->cx--; } } else do_lods(e, ww_); return 0; }
        case 0xAE: case 0xAF: { int ww_ = (op & 1); if (e->rep_prefix) { int c = (e->rep_prefix==2)?0:1; while (e->cx) { do_scas(e, ww_); e->cx--; if (getF(e,F_ZF)!=c) break; } } else do_scas(e, ww_); return 0; }

        /* ---- MOV imm ---- */
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
            wb(e, op & 7, fetch8(e)); return 0;
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            ww(e, op & 7, fetch16(e)); return 0;

        /* ---- GRP2 shifts ---- */
        case 0xC0: case 0xC1: { int m = fetch8(e); int op2 = (m >> 3) & 7; int cnt = fetch8(e);
            if ((m >> 6) == 3) { if (w) rm_write(e, m, 1, do_shift16(e, op2, rm_read(e, m, 1), cnt)); else rm_write(e, m, 0, do_shift8(e, op2, rm_read(e, m, 0), cnt)); }
            else { uint16_t s, o; ea(e, m, &s, &o); if (w) { uint16_t v = rd16(e, s, o); wr16(e, s, o, do_shift16(e, op2, v, cnt)); } else { uint8_t v = rd8(e, s, o); wr8(e, s, o, do_shift8(e, op2, v, cnt)); } }
            return 0; }
        case 0xD0: case 0xD1: { int m = fetch8(e); int op2 = (m >> 3) & 7;
            if (w) rm_write(e, m, 1, do_shift16(e, op2, rm_read(e, m, 1), 1)); else rm_write(e, m, 0, do_shift8(e, op2, rm_read(e, m, 0), 1)); return 0; }
        case 0xD2: case 0xD3: { int m = fetch8(e); int op2 = (m >> 3) & 7; int cnt = e->cx & 0x1F;
            if (w) rm_write(e, m, 1, do_shift16(e, op2, rm_read(e, m, 1), cnt)); else rm_write(e, m, 0, do_shift8(e, op2, rm_read(e, m, 0), cnt)); return 0; }

        /* ---- RET / far stuff ---- */
        case 0xC2: { uint16_t imm = fetch16(e); e->ip = pop16(e); e->sp = (uint16_t)(e->sp + imm); return 0; }
        case 0xC3: e->ip = pop16(e); return 0;
        case 0xC4: { int m = fetch8(e); uint16_t s, o; ea(e, m, &s, &o); uint16_t off = rd16(e, s, o); uint16_t seg = rd16(e, s, (uint16_t)(o+2)); reg_write(e, m, 1, off); e->es = seg; return 0; } /* LES */
        case 0xC5: { int m = fetch8(e); uint16_t s, o; ea(e, m, &s, &o); uint16_t off = rd16(e, s, o); uint16_t seg = rd16(e, s, (uint16_t)(o+2)); reg_write(e, m, 1, off); e->ds = seg; return 0; } /* LDS */
        case 0xC6: { int m = fetch8(e); if ((m >> 6) == 3) { rm_write(e, m, 0, fetch8(e)); } else { uint16_t s, o; ea(e, m, &s, &o); wr8(e, s, o, fetch8(e)); } return 0; }
        case 0xC7: { int m = fetch8(e); uint16_t s, o; uint16_t v;
            if ((m >> 6) == 3) { v = fetch16(e); rm_write(e, m, 1, v); }
            else { ea(e, m, &s, &o); v = fetch16(e); wr16(e, s, o, v); }
            return 0; }
        case 0xC9: e->sp = e->bp; e->bp = pop16(e); return 0; /* LEAVE */
        case 0xCA: { uint16_t imm = fetch16(e); e->ip = pop16(e); e->cs = pop16(e); e->sp = (uint16_t)(e->sp + imm); return 0; }
        case 0xCB: e->ip = pop16(e); e->cs = pop16(e); return 0; /* RETF */
        case 0xCC: e->state = WUBU_DOS_ERROR; return -1; /* INT3 */
        case 0xCD: { uint8_t vec = fetch8(e);
            if (vec == 0x10) { int10(e); return 0; }
            if (vec == 0x16) { int16(e); return 0; }
            if (vec == 0x20) { /* DOS terminate (classic COM exit) */ e->exit_code = 0; e->state = WUBU_DOS_TERMINATED; return -1; }
            if (vec == 0x21) { int21(e); if (e->state != WUBU_DOS_RUNNING) return -1; return 0; }
            /* Generic INT: dispatch via IVT if a real handler installed, else no-op. */
            { uint16_t ipv = rd16(e, 0, (uint16_t)(vec * 4)); uint16_t csv = rd16(e, 0, (uint16_t)(vec * 4 + 2));
              if (ipv || csv) { push16(e, e->flags); push16(e, e->cs); push16(e, e->ip); e->cs = csv; e->ip = ipv; } }
            return 0; }
        case 0xCE: if (getF(e, F_OF)) { /* INTO */ e->state = WUBU_DOS_ERROR; return -1; } return 0;
        case 0xCF: e->ip = pop16(e); e->cs = pop16(e); e->flags = pop16(e); return 0; /* IRET */

        /* ---- AAM/AAD/XLAT ---- */
        case 0xD4: { uint8_t base = fetch8(e); uint8_t al = (uint8_t)e->ax; e->ax = (al / base) * 256 + (al % base); logic8(e, (uint8_t)e->ax); return 0; }
        case 0xD5: { uint8_t base = fetch8(e); uint8_t ah = (uint8_t)(e->ax >> 8), al = (uint8_t)e->ax; e->ax = (uint16_t)(al + ah * base); logic8(e, (uint8_t)e->ax); return 0; }
        case 0xD7: e->ax = (e->ax & 0xFF00) | rd8(e, e->ds, (uint16_t)(e->bx + (e->ax & 0xFF))); return 0; /* XLAT */

        /* ---- LOOP / JCXZ ---- */
        case 0xE0: case 0xE1: case 0xE2: { int8_t r = (int8_t)fetch8(e); e->cx--; int go = 0;
            if (op == 0xE2) go = (e->cx != 0);
            else if (op == 0xE0) go = (e->cx != 0) && !getF(e, F_ZF);
            else go = (e->cx != 0) && getF(e, F_ZF);
            if (go) e->ip = (uint16_t)(e->ip + r);
            return 0; }
        case 0xE3: { int8_t r = (int8_t)fetch8(e); if (e->cx == 0) e->ip = (uint16_t)(e->ip + r); return 0; }

        /* ---- IN/OUT (ignored, no host port) ---- */
        case 0xE4: case 0xE5: case 0xE6: case 0xE7: case 0xEC: case 0xED: case 0xEE: case 0xEF: fetch8(e); return 0;

        /* ---- CALL/JMP ---- */
        case 0xE8: { int16_t r = (int16_t)fetch16(e); push16(e, e->ip); e->ip = (uint16_t)(e->ip + r); return 0; }
        case 0xE9: { int16_t r = (int16_t)fetch16(e); e->ip = (uint16_t)(e->ip + r); return 0; }
        case 0xEA: { uint16_t ipv = fetch16(e), csv = fetch16(e); e->cs = csv; e->ip = ipv; return 0; }
        case 0xEB: { int8_t r = (int8_t)fetch8(e); e->ip = (uint16_t)(e->ip + r); return 0; }

        /* ---- flag set/clr ---- */
        case 0xF4: e->state = WUBU_DOS_TERMINATED; return 0; /* HLT -> program idle/halt -> done */
        case 0xF5: setF(e, F_CF, !getF(e, F_CF)); return 0; /* CMC */
        case 0xF8: setF(e, F_CF, 0); return 0;
        case 0xF9: setF(e, F_CF, 1); return 0;
        case 0xFA: setF(e, F_IF, 0); return 0;
        case 0xFB: setF(e, F_IF, 1); return 0;
        case 0xFC: setF(e, F_DF, 0); return 0;
        case 0xFD: setF(e, F_DF, 1); return 0;

        /* ---- GRP3 (TEST/NOT/NEG/MUL/IMUL/DIV/IDIV) ---- */
        case 0xF6: case 0xF7: {
            int m = fetch8(e); int op2 = (m >> 3) & 7;
            if (op2 == 0 || op2 == 1) { /* TEST: disp16 (if mem) must be consumed before the imm */
                uint16_t s, o; uint16_t v;
                if ((m >> 6) == 3) { v = rm_read(e, m, w); }
                else { ea(e, m, &s, &o); v = rd16(e, s, o); }
                uint16_t imm = (op == 0xF6) ? fetch8(e) : fetch16(e);
                logic16(e, w ? (v & imm) : (v & (imm & 0xFF)));
                return 0;
            }
            if (op2 == 2) { if (w) rm_write(e, m, 1, (uint16_t)(~rm_read(e, m, 1))); else { uint8_t r = (uint8_t)(~rm_read(e, m, 0)); logic8(e, r); rm_write(e, m, 0, r); } return 0; }
            if (op2 == 3) { if (w) { uint16_t v = rm_read(e, m, 1); rm_write(e, m, 1, sub16(e, 0, v, 0)); } else { uint8_t v = rm_read(e, m, 0); rm_write(e, m, 0, sub8(e, 0, v, 0)); } return 0; }
            if (op2 == 4) { /* MUL */
                if (w) { uint32_t r = (uint32_t)e->ax * rm_read(e, m, 1); e->ax = (uint16_t)r; e->dx = (uint16_t)(r >> 16); setF(e, F_CF, e->dx != 0); setF(e, F_OF, e->dx != 0); }
                else { uint16_t r = (uint16_t)((uint8_t)e->ax * rm_read(e, m, 0)); e->ax = r; setF(e, F_CF, (r >> 8) != 0); setF(e, F_OF, (r >> 8) != 0); }
                return 0;
            }
            if (op2 == 5) { /* IMUL */
                if (w) { int32_t r = (int32_t)(int16_t)e->ax * (int16_t)rm_read(e, m, 1); e->ax = (uint16_t)r; e->dx = (uint16_t)(r >> 16); setF(e, F_CF, e->dx != (uint16_t)((int16_t)e->ax < 0 ? 0xFFFF : 0)); setF(e, F_OF, getF(e, F_CF)); }
                else { int16_t r = (int16_t)(int8_t)e->ax * (int8_t)rm_read(e, m, 0); e->ax = (uint16_t)r; setF(e, F_CF, (r >> 8) != (r & 0x80 ? 0xFF : 0)); setF(e, F_OF, getF(e, F_CF)); }
                return 0;
            }
            if (op2 == 6) { /* DIV */
                if (w) { uint16_t d = rm_read(e, m, 1); if (!d) { e->state = WUBU_DOS_ERROR; return -1; } uint32_t num = ((uint32_t)e->dx << 16) | e->ax; e->ax = (uint16_t)(num / d); e->dx = (uint16_t)(num % d); }
                else { uint8_t d = rm_read(e, m, 0); if (!d) { e->state = WUBU_DOS_ERROR; return -1; } uint16_t num = e->ax; e->ax = (uint8_t)(num / d) | ((uint8_t)(num % d) << 8); }
                return 0;
            }
            if (op2 == 7) { /* IDIV */
                if (w) { int16_t d = (int16_t)rm_read(e, m, 1); if (!d) { e->state = WUBU_DOS_ERROR; return -1; } int32_t num = ((int32_t)(int16_t)e->dx << 16) | (int32_t)(uint16_t)e->ax; e->ax = (uint16_t)(int16_t)(num / d); e->dx = (uint16_t)(int16_t)(num % d); }
                else { int8_t d = (int8_t)rm_read(e, m, 0); if (!d) { e->state = WUBU_DOS_ERROR; return -1; } int16_t num = (int16_t)e->ax; e->ax = (uint16_t)(int16_t)((int8_t)(num / d) | ((int8_t)(num % d) << 8)); }
                return 0;
            }
            return 0;
        }

        /* ---- GRP4/GRP5 ---- */
        case 0xFE: { int m = fetch8(e); int op2 = (m >> 3) & 7;
            if (op2 == 0) { uint8_t v = rm_read(e, m, 0); rm_write(e, m, 0, add8(e, v, 1, 0)); }
            else if (op2 == 1) { uint8_t v = rm_read(e, m, 0); rm_write(e, m, 0, sub8(e, v, 1, 0)); }
            return 0; }
        case 0xFF: { int m = fetch8(e); int op2 = (m >> 3) & 7;
            switch (op2) {
                case 0: rm_write(e, m, 1, add16(e, rm_read(e, m, 1), 1, 0)); break;
                case 1: rm_write(e, m, 1, sub16(e, rm_read(e, m, 1), 1, 0)); break;
                case 2: push16(e, e->ip); e->ip = rm_read(e, m, 1); break;       /* CALL near */
                case 3: { uint16_t s, o; ea(e, m, &s, &o); uint16_t ipv = rd16(e, s, o); uint16_t csv = rd16(e, s, (uint16_t)(o + 2)); fprintf(stderr, "[callfar] o=%04X ipv=%04X csv=%04X\n", o, ipv, csv); push16(e, e->cs); push16(e, e->ip); e->cs = csv; e->ip = ipv; } break; /* CALL far */
                case 4: e->ip = rm_read(e, m, 1); break;                        /* JMP near */
                case 5: { uint16_t s, o; ea(e, m, &s, &o); e->cs = rd16(e, s, (uint16_t)(o+2)); e->ip = rd16(e, s, o); } break; /* JMP far */
                case 6: push16(e, rm_read(e, m, 1)); break;                     /* PUSH r/m */
                default: break;
            }
            return 0; }

        default:
            e->state = WUBU_DOS_ERROR;
            return -1;
    }
}

/* ============================ public API ============================ */

int cond_met(WubuDosEmu *e, int cc) {
    switch (cc) {
        case 0: return getF(e, F_OF);
        case 1: return !getF(e, F_OF);
        case 2: return getF(e, F_CF);
        case 3: return !getF(e, F_CF);
        case 4: return getF(e, F_ZF);
        case 5: return !getF(e, F_ZF);
        case 6: return getF(e, F_CF) || getF(e, F_ZF);
        case 7: return !(getF(e, F_CF) || getF(e, F_ZF));
        case 8: return getF(e, F_SF);
        case 9: return !getF(e, F_SF);
        case 10: return getF(e, F_PF);
        case 11: return !getF(e, F_PF);
        case 12: return getF(e, F_SF) != getF(e, F_OF);
        case 13: return getF(e, F_SF) == getF(e, F_OF);
        case 14: return getF(e, F_ZF) || (getF(e, F_SF) != getF(e, F_OF));
        default: return !getF(e, F_ZF) && (getF(e, F_SF) == getF(e, F_OF));
    }
}

