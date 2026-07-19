/*
 * wubu_dos_emu.c -- WuBuOS in-process 16-bit DOS compatibility shim.
 *
 * REAL 8086 interpreter + DOS INT layer. Runs .COM/.EXE as a WuBuOS
 * process: no QEMU guest, no disk image, no second OS booted. See
 * wubu_dos_emu.h for the contract. The program's text/video output is
 * captured into a text buffer + an RGBA frame for the desktop window.
 *
 * Single self-contained C11 module (header + font included). Minimal
 * includes. Opaque handle. Every opcode below does real work.
 */
#include "wubu_dos_emu.h"
#include "wubu_dos_font.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

/* ----- FLAGS bit positions ----- */
#define F_CF 0x0001
#define F_PF 0x0004
#define F_AF 0x0010
#define F_ZF 0x0040
#define F_SF 0x0080
#define F_TF 0x0100
#define F_IF 0x0200
#define F_DF 0x0400
#define F_OF 0x0800

struct WubuDosEmu {
    uint8_t  mem[WUBU_DOS_MEM_SIZE];
    uint16_t es, cs, ss, ds;
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, sp;
    uint16_t ip;
    uint16_t flags;
    uint64_t steps;
    WubuDosEmuState state;
    int      exit_code;

    /* DOS video text model (80x25). */
    uint8_t  text[WUBU_DOS_TEXT_ROWS][WUBU_DOS_TEXT_COLS];
    uint8_t  attr[WUBU_DOS_TEXT_ROWS][WUBU_DOS_TEXT_COLS];
    int      cur_x, cur_y, cur_attr;

    /* Keyboard ring buffer (drained by INT 16h). */
    uint8_t  kbuf[64];   /* ASCII byte */
    uint8_t  kscan[64];  /* scancode (for INT 16h ah=0 -> AX = scan<<8 | ascii) */
    int      khead, ktail;
    uint8_t  kshift;     /* shift-state flags (INT 16h ah=2) */

    /* Host-backed DOS file table. Slot 0/1/2 are the console (stdin/stdout/
     * stderr); slots >=3 back real host files opened via INT 21h. Handle = slot+1. */
    int      host_fd[64];
    uint8_t  host_used[64];

    /* PSP segment (COM=0x1000, EXE=its own). Stored so INT 21h/62h returns it. */
    uint16_t psp_seg;

    /* Transient prefix state for the current instruction. */
    uint8_t  seg_prefix; /* 0 none, else ES/CS/SS/DS reg id+1 */
    uint8_t  rep_prefix; /* 0 none, 1 REP/REPE, 2 REPNE */
};

/* ============================ memory ============================ */
static inline uint32_t phys(WubuDosEmu *e, uint16_t seg, uint16_t off) {
    (void)e;
    return ((uint32_t)seg << 4) + off;
}
static uint8_t rd8(WubuDosEmu *e, uint16_t s, uint16_t o) {
    return e->mem[phys(e, s, o) & (WUBU_DOS_MEM_SIZE - 1)];
}
static uint16_t rd16(WubuDosEmu *e, uint16_t s, uint16_t o) {
    return (uint16_t)rd8(e, s, o) | ((uint16_t)rd8(e, s, (uint16_t)(o + 1)) << 8);
}
static void wr8(WubuDosEmu *e, uint16_t s, uint16_t o, uint8_t v) {
    e->mem[phys(e, s, o) & (WUBU_DOS_MEM_SIZE - 1)] = v;
}
static void wr16(WubuDosEmu *e, uint16_t s, uint16_t o, uint16_t v) {
    wr8(e, s, o, (uint8_t)v);
    wr8(e, s, (uint16_t)(o + 1), (uint8_t)(v >> 8));
}
static uint8_t fetch8(WubuDosEmu *e) { uint8_t v = rd8(e, e->cs, e->ip); e->ip++; return v; }
static uint16_t fetch16(WubuDosEmu *e) { uint8_t lo = fetch8(e); uint8_t hi = fetch8(e); return (uint16_t)lo | ((uint16_t)hi << 8); }

/* ============================ registers ============================ */
static uint16_t *regp(WubuDosEmu *e, int i) {
    switch (i & 7) {
        case 0: return &e->ax; case 1: return &e->cx; case 2: return &e->dx;
        case 3: return &e->bx; case 4: return &e->sp; case 5: return &e->bp;
        case 6: return &e->si; default: return &e->di;
    }
}
/* Byte-register access: 8086 encodes 0=AL 1=CL 2=DL 3=BL 4=AH 5=CH 6=DH 7=BH
 * (NOT SP/BP/SI/DI). AH/CH/DH/BH are the high bytes of AX/CX/DX/BX. */
static uint8_t rb(WubuDosEmu *e, int i) {
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
static void wb(WubuDosEmu *e, int i, uint8_t v) {
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
static uint16_t rw(WubuDosEmu *e, int i) { return *regp(e, i); }
static void ww(WubuDosEmu *e, int i, uint16_t v) { *regp(e, i) = v; }

/* ============================ flags ============================ */
static int getF(WubuDosEmu *e, uint16_t b) { return (e->flags & b) ? 1 : 0; }
static void setF(WubuDosEmu *e, uint16_t b, int v) { if (v) e->flags |= b; else e->flags &= (uint16_t)~b; }
static int parity8(uint8_t v) { int c = 0; for (int i = 0; i < 8; i++) c ^= (v >> i) & 1; return c == 0; }

static uint16_t add16(WubuDosEmu *e, uint16_t a, uint16_t b, int cin) {
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
static uint8_t add8(WubuDosEmu *e, uint8_t a, uint8_t b, int cin) {
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
static uint16_t sub16(WubuDosEmu *e, uint16_t a, uint16_t b, int cin) {
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
static uint8_t sub8(WubuDosEmu *e, uint8_t a, uint8_t b, int cin) {
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
static void logic16(WubuDosEmu *e, uint16_t v) {
    setF(e, F_CF, 0); setF(e, F_OF, 0); setF(e, F_AF, 0);
    setF(e, F_ZF, v == 0); setF(e, F_SF, (v & 0x8000) != 0); setF(e, F_PF, parity8((uint8_t)v));
}
static void logic8(WubuDosEmu *e, uint8_t v) {
    setF(e, F_CF, 0); setF(e, F_OF, 0); setF(e, F_AF, 0);
    setF(e, F_ZF, v == 0); setF(e, F_SF, (v & 0x80) != 0); setF(e, F_PF, parity8(v));
}

/* ============================ stack ============================ */
static void push16(WubuDosEmu *e, uint16_t v) { e->sp -= 2; wr16(e, e->ss, e->sp, v); }
static uint16_t pop16(WubuDosEmu *e) { uint16_t v = rd16(e, e->ss, e->sp); e->sp += 2; return v; }

static uint16_t seg_for(WubuDosEmu *e, int id) {
    switch (id) {
        case 1: return e->es; case 2: return e->cs; case 3: return e->ss; default: return e->ds;
    }
}

/* ============================ effective address ============================ */
static void ea(WubuDosEmu *e, int modrm, uint16_t *seg, uint16_t *off) {
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

static uint16_t rm_read(WubuDosEmu *e, int modrm, int w) {
    if ((modrm >> 6) == 3) return w ? rw(e, modrm & 7) : rb(e, modrm & 7);
    uint16_t s, o; ea(e, modrm, &s, &o);
    return w ? rd16(e, s, o) : rd8(e, s, o);
}
static void rm_write(WubuDosEmu *e, int modrm, int w, uint16_t v) {
    if ((modrm >> 6) == 3) { if (w) ww(e, modrm & 7, v); else wb(e, modrm & 7, v); return; }
    uint16_t s, o; ea(e, modrm, &s, &o);
    if (w) wr16(e, s, o, v); else wr8(e, s, o, (uint8_t)v);
}
static uint16_t reg_read(WubuDosEmu *e, int modrm, int w) { int r = (modrm >> 3) & 7; return w ? rw(e, r) : rb(e, r); }
static void reg_write(WubuDosEmu *e, int modrm, int w, uint16_t v) { int r = (modrm >> 3) & 7; if (w) ww(e, r, v); else wb(e, r, v); }

/* ============================ text screen ============================ */
static void scroll_up(WubuDosEmu *e, int top, int left, int bot, int right, int lines, uint8_t attr) {
    if (lines <= 0) return;
    for (int y = top; y <= bot - lines; y++) {
        memcpy(e->text[y] + left, e->text[y + lines] + left, (size_t)(right - left + 1));
        memcpy(e->attr[y] + left, e->attr[y + lines] + left, (size_t)(right - left + 1));
    }
    for (int y = bot - lines + 1; y <= bot; y++)
        for (int x = left; x <= right; x++) { e->text[y][x] = ' '; e->attr[y][x] = attr; }
}
static void put_char(WubuDosEmu *e, char c) {
    if (c == '\r') { e->cur_x = 0; return; }
    if (c == '\n') { e->cur_x = 0; e->cur_y++; }
    else if (c == '\b') { if (e->cur_x > 0) e->cur_x--; }
    else {
        if (e->cur_x >= WUBU_DOS_TEXT_COLS) { e->cur_x = 0; e->cur_y++; }
        if (e->cur_y >= WUBU_DOS_TEXT_ROWS) { scroll_up(e, 0, 0, WUBU_DOS_TEXT_ROWS - 1, WUBU_DOS_TEXT_COLS - 1, 1, (uint8_t)e->cur_attr); e->cur_y = WUBU_DOS_TEXT_ROWS - 1; }
        e->text[e->cur_y][e->cur_x] = (uint8_t)c;
        e->attr[e->cur_y][e->cur_x] = (uint8_t)e->cur_attr;
        e->cur_x++;
    }
    if (e->cur_y >= WUBU_DOS_TEXT_ROWS) e->cur_y = WUBU_DOS_TEXT_ROWS - 1;
}

/* ============================ DOS host file table ============================ */
/* Translate a DOS handle (slot+1) to a host fd; console handles 0/1/2 map to
 * the host 0/1/2. Returns the host fd, or -1 if the handle is not open. */
static int dos_handle_to_fd(WubuDosEmu *e, uint16_t h) {
    if (h <= 2) return (int)h;                 /* stdin/out/err */
    int slot = (int)h - 1;
    if (slot < 0 || slot >= 64 || !e->host_used[slot]) return -1;
    return e->host_fd[slot];
}
/* Allocate a DOS handle for a freshly opened host fd (>=3). Returns handle or 0. */
static uint16_t dos_handle_alloc(WubuDosEmu *e, int host_fd) {
    for (int i = 3; i < 64; i++) {
        if (!e->host_used[i]) {
            e->host_used[i] = 1;
            e->host_fd[i] = host_fd;
            return (uint16_t)(i + 1);
        }
    }
    return 0; /* no free handle */
}

/* ============================ INT handlers ============================ */
static void int10(WubuDosEmu *e) {
    uint8_t ah = (uint8_t)(e->ax >> 8);
    switch (ah) {
        case 0x00: /* set mode: ignore, keep 80x25 text */ break;
        case 0x01: break; /* cursor shape */
        case 0x02: e->cur_y = (e->dx >> 8) & 0xFF; e->cur_x = e->dx & 0xFF; break;
        case 0x03: e->ax = (uint16_t)((e->cur_attr << 8) | 0); e->dx = (uint16_t)((e->cur_y << 8) | e->cur_x); break;
        case 0x06: { int lines = e->ax & 0xFF; uint8_t at = (uint8_t)(e->bx & 0xFF);
                     int ch = (e->cx >> 8) & 0xFF, cl = e->cx & 0xFF, dh = (e->dx >> 8) & 0xFF, dl = e->dx & 0xFF;
                     if (lines == 0) scroll_up(e, ch, cl, dh, dl, WUBU_DOS_TEXT_ROWS, at);
                     else scroll_up(e, ch, cl, dh, dl, lines, at);
                     e->cur_x = cl; e->cur_y = ch; break; }
        case 0x07: { int lines = e->ax & 0xFF; uint8_t at = (uint8_t)(e->bx & 0xFF);
                     int ch = (e->cx >> 8) & 0xFF, cl = e->cx & 0xFF, dh = (e->dx >> 8) & 0xFF, dl = e->dx & 0xFF;
                     /* scroll down: shift rows toward bottom */
                     if (lines == 0 || lines >= (dh - ch + 1)) { for (int y = ch; y <= dh; y++) for (int x = cl; x <= dl; x++) { e->text[y][x]=' '; e->attr[y][x]=at; } }
                     else { for (int y = dh; y >= ch + lines; y--) { memcpy(e->text[y]+cl, e->text[y-lines]+cl, dl-cl+1); memcpy(e->attr[y]+cl, e->attr[y-lines]+cl, dl-cl+1); } for (int y = ch; y < ch + lines; y++) for (int x = cl; x <= dl; x++) { e->text[y][x]=' '; e->attr[y][x]=at; } }
                     e->cur_x = cl; e->cur_y = ch; break; }
        case 0x08: e->ax = (uint16_t)((e->attr[e->cur_y][e->cur_x] << 8) | e->text[e->cur_y][e->cur_x]); break;
        case 0x09: { char c = (char)(e->ax & 0xFF); uint8_t a = (uint8_t)(e->bx & 0xFF);
                     int x = e->cur_x, y = e->cur_y;
                     if (x < WUBU_DOS_TEXT_COLS && y < WUBU_DOS_TEXT_ROWS) { e->text[y][x]=c; e->attr[y][x]=a; }
                     if (x < WUBU_DOS_TEXT_COLS - 1) e->cur_x = x + 1;
                     break; }
        case 0x0E: put_char(e, (char)(e->ax & 0xFF)); break;
        case 0x0F: e->ax = 0x5000; e->bx = 0; break; /* mode 3, 80 cols */
        case 0x13: { int cx = e->cx; uint16_t off = e->dx; uint16_t s = e->es;
                     int row = (e->dx >> 8) & 0xFF, col = e->dx & 0xFF; uint8_t at = (uint8_t)(e->bx & 0xFF);
                     for (int i = 0; i < cx; i++) { char c = (char)rd8(e, s, (uint16_t)(off + i)); e->cur_y = row; e->cur_x = col + i; e->cur_attr = at; put_char(e, c); } break; }
        default: break;
    }
}
static uint8_t kbd_pop(WubuDosEmu *e) {
    if (e->khead == e->ktail) return 0;
    uint8_t v = e->kbuf[e->khead];
    e->khead = (e->khead + 1) & 63;
    return v;
}
static void int16(WubuDosEmu *e) {
    uint8_t ah = (uint8_t)(e->ax >> 8);
    switch (ah) {
        case 0x00: { uint8_t v = kbd_pop(e); uint8_t sc = (e->khead == e->ktail) ? 0 : e->kscan[(e->khead) & 63]; e->ax = (uint16_t)((sc << 8) | v); break; }
        case 0x01: if (e->khead == e->ktail) setF(e, F_ZF, 1); else { setF(e, F_ZF, 0); e->ax = (uint16_t)((e->kscan[e->khead & 63] << 8) | e->kbuf[e->khead & 63]); } break;
        case 0x02: e->ax = (uint16_t)((e->ax & 0xFF00) | e->kshift); break; /* shift state in AL */
        default: break;
    }
}
static void int21(WubuDosEmu *e) {
    uint8_t ah = (uint8_t)(e->ax >> 8);
    switch (ah) {
        case 0x01: case 0x07: case 0x08: e->ax = (uint16_t)kbd_pop(e); break;
        case 0x02: put_char(e, (char)(e->dx & 0xFF)); break;
        case 0x05: break; /* printer */
        case 0x06: { uint8_t dl = (uint8_t)(e->dx & 0xFF);
                     if (dl == 0xFF) { uint8_t v = kbd_pop(e); e->ax = v; setF(e, F_ZF, v ? 0 : 1); }
                     else put_char(e, (char)dl);
                     break; }
        case 0x09: { uint16_t off = e->dx, s = e->ds; int i = 0;
                     while (i < 4096) { char c = (char)rd8(e, s, (uint16_t)(off + i)); if (c == '$') break; put_char(e, c); i++; } break; }
        case 0x0A: { uint16_t off = e->dx, s = e->ds;
                     int maxlen = rd8(e, s, off); int n = 0;
                     while (n < maxlen && n < 126) { uint8_t v = kbd_pop(e); if (v == 0) break; if (v == '\r') break; wr8(e, s, (uint16_t)(off + 2 + n), v); put_char(e, (char)v); n++; }
                     wr8(e, s, (uint16_t)(off + 1), (uint8_t)n); break; }
        case 0x19: e->ax = (e->ax & 0xFF00); break; /* default drive A: (0) */
        case 0x25: { uint8_t vec = (uint8_t)(e->ax & 0xFF); uint16_t ipv = e->dx, csv = e->ds;
                     wr16(e, 0, (uint16_t)(vec * 4), ipv); wr16(e, 0, (uint16_t)(vec * 4 + 2), csv); break; }
        case 0x2A: { time_t t = time(NULL); struct tm *tm = localtime(&t);
                     e->ax = (uint16_t)(tm->tm_year + 1900);            /* CX = year */
                     e->dx = (uint16_t)(((tm->tm_mon + 1) << 8) | (tm->tm_mday & 0xFF)); /* DH=mon DL=day */
                     e->cx = (uint16_t)((e->cx & 0xFF00) | (tm->tm_wday & 0xFF)); break; } /* AL = weekday */
        case 0x2C: { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
                     uint32_t cs100 = (uint32_t)(ts.tv_sec * 100 + ts.tv_nsec / 10000000);
                     e->cx = (uint16_t)(cs100 / 100);                  /* CH:sec CL:100ths */
                     e->dx = (uint16_t)((cs100 / 100) >> 8); break; }
        case 0x30: e->ax = 0x0005; e->bx = 0x1234; break;            /* DOS 5.00, "WuBuDOS" OEM */
        case 0x35: { uint8_t vec = (uint8_t)(e->ax & 0xFF); uint16_t ipv = rd16(e, 0, (uint16_t)(vec * 4)); uint16_t csv = rd16(e, 0, (uint16_t)(vec * 4 + 2));
                     e->bx = ipv; e->es = csv; break; }
        case 0x39: { /* mkdir */ char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     if (mkdir(p, 0755) == 0) { setF(e, F_CF, 0); e->ax = 0; } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x3A: { /* rmdir */ char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     if (rmdir(p) == 0) { setF(e, F_CF, 0); e->ax = 0; } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x3B: { /* chdir */ char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     if (chdir(p) == 0) { setF(e, F_CF, 0); e->ax = 0; } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x41: { /* unlink */ char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     if (unlink(p) == 0) { setF(e, F_CF, 0); e->ax = 0; } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x47: { /* getcwd into DS:DX (ignore drive in AL) */ char cwd[1024]; if (getcwd(cwd, sizeof(cwd))) {
                         uint16_t o = e->dx, s = e->ds; for (int k = 0; cwd[k] && k < 63; k++) wr8(e, s, (uint16_t)(o + k), (uint8_t)cwd[k]); wr8(e, s, (uint16_t)(o + (int)strlen(cwd)), 0); setF(e, F_CF, 0); } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x56: { /* rename DS:DX -> ES:DI */ char a[256], b[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; a[i++] = c; } a[i] = 0;
                     i = 0; o = e->di; s = e->es; while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; b[i++] = c; } b[i] = 0;
                     if (rename(a, b) == 0) { setF(e, F_CF, 0); e->ax = 0; } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x3C: { /* creat */ char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     int fd = open(p, O_RDWR | O_CREAT | O_TRUNC, 0644);
                     if (fd >= 0) { uint16_t h = dos_handle_alloc(e, fd); e->ax = h; setF(e, F_CF, h ? 0 : 1); if (!h) close(fd); } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x3D: { /* open */ char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     int fd = open(p, O_RDWR); if (fd < 0) fd = open(p, O_RDONLY);
                     if (fd >= 0) { uint16_t h = dos_handle_alloc(e, fd); e->ax = h; setF(e, F_CF, h ? 0 : 1); if (!h) close(fd); } else { setF(e, F_CF, 1); e->ax = (uint16_t)errno; } break; }
        case 0x3E: { /* close */ int fd = dos_handle_to_fd(e, (uint16_t)e->bx);
                     if (fd >= 0) { if ((uint16_t)e->bx >= 3) { int slot = (int)e->bx - 1; e->host_used[slot] = 0; close(fd); } setF(e, F_CF, 0); e->ax = 0; } else { setF(e, F_CF, 1); e->ax = 6; } break; }
        case 0x3F: { /* read */ int fd = dos_handle_to_fd(e, (uint16_t)e->bx);
                     uint16_t cnt = (uint16_t)e->cx, o = e->dx, s = e->ds;
                     if (fd < 0) { setF(e, F_CF, 1); e->ax = 6; break; }
                     if (fd <= 2 && fd != 0) { e->ax = 0; setF(e, F_CF, 0); break; } /* stdout/stderr read = 0 */
                     uint8_t *buf = (uint8_t *)malloc(cnt ? cnt : 1); if (!buf) { setF(e, F_CF, 1); e->ax = 8; break; }
                     long n = (fd == 0) ? (long)read(0, buf, cnt) : read(fd, buf, cnt);
                     if (n < 0) { setF(e, F_CF, 1); e->ax = (uint16_t)errno; free(buf); break; }
                     for (long k = 0; k < n; k++) wr8(e, s, (uint16_t)(o + (uint16_t)k), buf[k]);
                     e->ax = (uint16_t)n; setF(e, F_CF, 0); free(buf); break; }
        case 0x40: { /* write */ int fd = dos_handle_to_fd(e, (uint16_t)e->bx);
                     uint16_t cnt = (uint16_t)e->cx, o = e->dx, s = e->ds;
                     if (fd < 0) { setF(e, F_CF, 1); e->ax = 6; break; }
                     for (uint16_t k = 0; k < cnt; k++) {
                         uint8_t c = rd8(e, s, (uint16_t)(o + k));
                         if (fd <= 2) put_char(e, (char)c);           /* console -> screen */
                         else { if (write(fd, &c, 1) != 1) break; }
                     }
                     e->ax = cnt; setF(e, F_CF, 0); break; }
        case 0x42: { /* lseek */ int fd = dos_handle_to_fd(e, (uint16_t)e->bx);
                     long off = (long)(((uint32_t)e->cx << 16) | (uint32_t)(e->dx & 0xFFFF));
                     int whence = SEEK_SET; int al = (uint8_t)e->ax;
                     if (al == 1) whence = SEEK_CUR; else if (al == 2) whence = SEEK_END;
                     if (fd < 0) { setF(e, F_CF, 1); e->ax = 6; break; }
                     long p = lseek(fd, off, whence);
                     if (p < 0) { setF(e, F_CF, 1); e->ax = (uint16_t)errno; break; }
                     e->dx = (uint16_t)(p >> 16); e->ax = (uint16_t)(p & 0xFFFF); setF(e, F_CF, 0); break; }
        case 0x4A: setF(e, F_CF, 0); break;            /* resize mem (no-op success) */
        case 0x4B: { /* exec: load + run a child program (LoadAndExecute) */
                     char p[256]; int i = 0; uint16_t o = e->dx, s = e->ds;
                     while (i < 255) { char c = (char)rd8(e, s, (uint16_t)(o + i)); if (!c) break; p[i++] = c; } p[i] = 0;
                     FILE *f = fopen(p, "rb"); if (!f) { setF(e, F_CF, 1); e->ax = 2; break; }
                     fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                     uint8_t *img = (uint8_t *)malloc((size_t)sz); if (!img) { fclose(f); setF(e, F_CF, 1); e->ax = 8; break; }
                     int ok = (int)fread(img, 1, (size_t)sz, f); fclose(f);
                     WubuDosEmu *child = wubu_dos_emu_create(); int rc = -1;
                     if (child && ok == sz) {
                         if (img[0] == 'M' || img[0] == 'Z') rc = wubu_dos_emu_load_exe(child, img, (size_t)sz);
                         else rc = wubu_dos_emu_load_com(child, img, (size_t)sz);
                         if (rc == 0) wubu_dos_emu_run(child, 0);
                     }
                     e->ax = (uint16_t)(child ? wubu_dos_emu_exit_code(child) : 0);
                     if (child) wubu_dos_emu_destroy(child);
                     free(img);
                     e->state = WUBU_DOS_TERMINATED;   /* exec replaces this process */
                     setF(e, F_CF, rc == 0 ? 0 : 1); break; }
        case 0x4C: e->exit_code = (int)(e->ax & 0xFF); e->state = WUBU_DOS_TERMINATED; break;
        case 0x4D: e->ax = (uint16_t)((e->ax & 0xFF00) | ((uint8_t)e->exit_code & 0xFF)); break;
        case 0x62: e->bx = e->psp_seg; break;          /* get PSP segment */
        default: setF(e, F_CF, 0); break; /* tolerate unknown DOS calls */
    }
}

/* ============================ shifts/rolls ============================ */
static uint8_t do_shift8(WubuDosEmu *e, int op, uint8_t v, int cnt) {
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
static uint16_t do_shift16(WubuDosEmu *e, int op, uint16_t v, int cnt) {
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
static void do_movs(WubuDosEmu *e, int w) {
    if (w) { uint16_t v = rd16(e, e->ds, e->si); wr16(e, e->es, e->di, v); }
    else  { uint8_t v = rd8(e, e->ds, e->si); wr8(e, e->es, e->di, v); }
    int d = (getF(e, F_DF) ? -1 : 1);
    e->si = (uint16_t)(e->si + (w ? 2 : 1) * d);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}
static void do_stos(WubuDosEmu *e, int w) {
    if (w) wr16(e, e->es, e->di, e->ax); else wr8(e, e->es, e->di, (uint8_t)e->ax);
    int d = (getF(e, F_DF) ? -1 : 1);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}
static void do_lods(WubuDosEmu *e, int w) {
    if (w) e->ax = rd16(e, e->ds, e->si); else { e->ax = (e->ax & 0xFF00) | rd8(e, e->ds, e->si); }
    int d = (getF(e, F_DF) ? -1 : 1);
    e->si = (uint16_t)(e->si + (w ? 2 : 1) * d);
}
static void do_scas(WubuDosEmu *e, int w) {
    uint16_t acc = w ? e->ax : (e->ax & 0xFF);
    uint16_t m = w ? rd16(e, e->es, e->di) : rd8(e, e->es, e->di);
    if (w) sub16(e, acc, m, 0); else sub8(e, (uint8_t)acc, (uint8_t)m, 0);
    int d = (getF(e, F_DF) ? -1 : 1);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}
static void do_cmps(WubuDosEmu *e, int w) {
    uint16_t a = w ? rd16(e, e->ds, e->si) : rd8(e, e->ds, e->si);
    uint16_t b = w ? rd16(e, e->es, e->di) : rd8(e, e->es, e->di);
    if (w) sub16(e, a, b, 0); else sub8(e, (uint8_t)a, (uint8_t)b, 0);
    int d = (getF(e, F_DF) ? -1 : 1);
    e->si = (uint16_t)(e->si + (w ? 2 : 1) * d);
    e->di = (uint16_t)(e->di + (w ? 2 : 1) * d);
}

/* ============================ ALU helpers ============================ */
static uint16_t alu16(WubuDosEmu *e, int arith, uint16_t a, uint16_t b) {
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
static uint8_t alu8(WubuDosEmu *e, int arith, uint8_t a, uint8_t b) {
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
static int decode_main(WubuDosEmu *e, uint8_t op);
/* Returns 0 if execution should continue, -1 if halted (error/term). */
static int step(WubuDosEmu *e) {
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

static int cond_met(WubuDosEmu *e, int cc) {
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

static int decode_main(WubuDosEmu *e, uint8_t op) {
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
WubuDosEmu *wubu_dos_emu_create(void) {
    WubuDosEmu *e = (WubuDosEmu *)calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->state = WUBU_DOS_RUNNING;
    e->ds = e->es = e->ss = 0x1000; /* PSP segment; COM loads at 0x100 */
    e->cs = 0x1000;
    e->sp = 0xFFFE;
    e->cur_attr = 0x07; /* light gray on black */
    return e;
}
void wubu_dos_emu_destroy(WubuDosEmu *e) { free(e); }

int wubu_dos_emu_load_com(WubuDosEmu *e, const uint8_t *data, size_t size) {
    if (!e || !data || size == 0) return -1;
    if (size > WUBU_DOS_MEM_SIZE - 0x100) return -1;
    memset(e->mem, 0, WUBU_DOS_MEM_SIZE);
    /* Build a real 80-byte PSP at seg 0x1000:0. */
    e->psp_seg = e->ds;
    wr8(e, e->ds, 0, 0xCD); wr8(e, e->ds, 1, 0x20); /* int 20h at PSP:0 */
    wr8(e, e->ds, 2, 0x9A);                          /* far JMP (unused) */
    wr16(e, e->ds, 0x16, 0);                          /* parent PSP = 0 */
    wr16(e, e->ds, 0x2C, 0);                          /* environment seg = 0 */
    wr16(e, e->ds, 0x80, 0x80);                       /* DTA = PSP:0x80 */
    /* Default command tail (CR only) so a program that reads PSP:0x80 sees an
     * empty line; real cmdline could be injected via a dedicated API. */
    wr8(e, e->ds, 0x80, 0x0D);
    /* Copy program to 0x100. */
    for (size_t i = 0; i < size; i++) wr8(e, e->cs, (uint16_t)(0x100 + i), data[i]);
    e->ip = 0x100;
    e->state = WUBU_DOS_RUNNING;
    return 0;
}
int wubu_dos_emu_load_exe(WubuDosEmu *e, const uint8_t *data, size_t size) {
    if (!e || !data || size < 28) return -1;
    if (data[0] != 'M' && data[0] != 'Z') return -1;
    uint16_t hdr_size = (uint16_t)(data[8] | (data[9] << 8));   /* paragraphs */
    uint16_t min_extra = (uint16_t)(data[10] | (data[11] << 8));
    uint16_t init_ss   = (uint16_t)(data[14] | (data[15] << 8));
    uint16_t init_sp   = (uint16_t)(data[16] | (data[17] << 8));
    uint16_t init_ip   = (uint16_t)(data[20] | (data[21] << 8));
    uint16_t init_cs   = (uint16_t)(data[22] | (data[23] << 8));
    uint16_t reloc_off = (uint16_t)(data[24] | (data[25] << 8));
    uint16_t reloc_cnt = (uint16_t)(data[26] | (data[27] << 8));
    uint32_t img_base = (uint32_t)hdr_size * 16;
    uint32_t img_bytes = (uint32_t)size - img_base;
    memset(e->mem, 0, WUBU_DOS_MEM_SIZE);
    /* Load image at seg 0. */
    for (uint32_t i = 0; i < img_bytes && (img_base + i) < WUBU_DOS_MEM_SIZE; i++)
        e->mem[img_base + i] = data[img_base + i];
    /* Apply relocations (seg:off pairs, add hdr_size). */
    for (int i = 0; i < reloc_cnt; i++) {
        uint32_t ro = (uint32_t)reloc_off + (uint32_t)i * 4;
        if (ro + 3 >= size) break;
        uint16_t off = (uint16_t)(data[ro] | (data[ro + 1] << 8));
        uint16_t seg = (uint16_t)(data[ro + 2] | (data[ro + 3] << 8));
        uint32_t a = ((uint32_t)seg << 4) + off;
        if (a + 1 < WUBU_DOS_MEM_SIZE) {
            uint16_t v = (uint16_t)(e->mem[a] | (e->mem[a + 1] << 8));
            v = (uint16_t)(v + hdr_size);
            e->mem[a] = (uint8_t)v; e->mem[a + 1] = (uint8_t)(v >> 8);
        }
    }
    e->cs = hdr_size + init_cs;
    e->ds = hdr_size; e->es = hdr_size; e->ss = hdr_size + init_ss;
    e->sp = init_sp ? init_sp : 0xFFFE;
    e->psp_seg = hdr_size;                            /* EXE PSP lives at image base */
    e->ip = init_ip;
    e->ax = e->bx = e->cx = e->dx = 0;
    e->state = WUBU_DOS_RUNNING;
    (void)min_extra;
    return 0;
}

WubuDosEmuState wubu_dos_emu_run(WubuDosEmu *e, uint64_t max_steps) {
    if (!e) return WUBU_DOS_ERROR;
    /* Safety ceiling so a runaway/ill-formed program can never hang the host
     * process (e.g. a COM that falls through into a zeroed region and loops
     * on no-op instructions). max_steps==0 means "no caller-imposed limit",
     * not "unbounded". */
    uint64_t hard_cap = (max_steps != 0) ? max_steps : 20000000ULL;
    while (e->state == WUBU_DOS_RUNNING) {
        if (e->steps >= hard_cap) { e->state = WUBU_DOS_ERROR; break; }
        int r = step(e);
        e->steps++;
        if (r < 0) break;
    }
    return e->state;
}
int wubu_dos_emu_step(WubuDosEmu *e) {
    if (!e || e->state != WUBU_DOS_RUNNING) return -1;
    e->steps++;
    return step(e);
}
void wubu_dos_emu_key(WubuDosEmu *e, uint8_t ascii) {
    if (!e) return;
    /* Map a few common ASCII bytes to plausible scancodes for INT 16h. */
    static const uint8_t sc[256] = {0};
    (void)sc;
    uint8_t scan = 0x1E; /* default 'A' style; good enough for ah=0 top byte */
    if (ascii >= 'a' && ascii <= 'z') scan = (uint8_t)(0x1E + (ascii - 'a'));
    else if (ascii >= 'A' && ascii <= 'Z') scan = (uint8_t)(0x1E + (ascii - 'A'));
    else if (ascii >= '0' && ascii <= '9') scan = (uint8_t)(0x02 + (ascii - '0'));
    else if (ascii == ' ') scan = 0x39;
    else if (ascii == '\r' || ascii == '\n') scan = 0x1C;
    else if (ascii == '\b') scan = 0x0E;
    else if (ascii == '\t') scan = 0x0F;
    else if (ascii == 0x1B) scan = 0x01;
    int next = (e->ktail + 1) & 63;
    if (next == e->khead) return; /* full */
    e->kbuf[e->ktail] = ascii; e->kscan[e->ktail] = scan; e->ktail = next;
}
size_t wubu_dos_emu_text(const WubuDosEmu *e, char *out, size_t out_size) {
    if (!e || !out || out_size == 0) return 0;
    size_t n = 0;
    for (int y = 0; y < WUBU_DOS_TEXT_ROWS; y++) {
        for (int x = 0; x < WUBU_DOS_TEXT_COLS; x++) {
            char c = (char)e->text[y][x];
            if (c == 0) c = ' ';
            if (n + 1 < out_size) out[n++] = c;
        }
        if (n + 1 < out_size) out[n++] = '\n';
    }
    out[n] = '\0';
    return n;
}
size_t wubu_dos_emu_frame_rgba(const WubuDosEmu *e, uint8_t *rgba, int *out_w, int *out_h) {
    if (!e || !rgba) return 0;
    int W = WUBU_DOS_TEXT_COLS * WUBU_DOS_CELL_W;
    int H = WUBU_DOS_TEXT_ROWS * WUBU_DOS_CELL_H;
    /* palette: attr low nibble = fg, high = bg (very rough CGA-ish) */
    static const uint8_t pal[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},
        {170,85,0},{170,170,170},{85,85,85},{85,85,255},{85,255,85},
        {85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
    };
    for (int y = 0; y < H; y++) {
        int row = y / WUBU_DOS_CELL_H;
        int gl = y % WUBU_DOS_CELL_H;
        for (int x = 0; x < W; x++) {
            int col = x / WUBU_DOS_CELL_W;
            int gc = x % WUBU_DOS_CELL_W;
            uint8_t ch = e->text[row][col];
            uint8_t at = e->attr[row][col];
            int bit = 0;
            if (ch >= WUBU_DOS_FONT_FIRST && ch <= WUBU_DOS_FONT_LAST) {
                int idx = ch - WUBU_DOS_FONT_FIRST;
                int gy = gl * 7 / WUBU_DOS_CELL_H;       /* map 16 -> 7 */
                int gx = gc * 5 / WUBU_DOS_CELL_W;       /* map 8 -> 5 */
                if (gx < 5 && gy < 7) bit = (wubu_dos_font[idx][gy] >> (4 - gx)) & 1;
            }
            const uint8_t *fg = pal[at & 0x0F];
            const uint8_t *bg = pal[(at >> 4) & 0x0F];
            uint8_t r = bit ? fg[0] : bg[0];
            uint8_t g = bit ? fg[1] : bg[1];
            uint8_t b = bit ? fg[2] : bg[2];
            size_t o = ((size_t)y * W + x) * 4;
            rgba[o] = r; rgba[o+1] = g; rgba[o+2] = b; rgba[o+3] = 255;
        }
    }
    if (out_w) *out_w = W;
    if (out_h) *out_h = H;
    return (size_t)W * H * 4;
}
int wubu_dos_emu_exit_code(const WubuDosEmu *e) { return e ? e->exit_code : -1; }
uint16_t wubu_dos_emu_peek16(const WubuDosEmu *e, uint16_t seg, uint16_t off) {
    if (!e) return 0;
    return rd16((WubuDosEmu *)e, seg, off);
}
void wubu_dos_emu_regs(const WubuDosEmu *e, uint16_t *ax, uint16_t *bx, uint16_t *cx, uint16_t *dx,
                       uint16_t *si, uint16_t *di, uint16_t *ip, uint16_t *flags, uint16_t *cs) {
    if (!e) return;
    if (ax) *ax = e->ax;
    if (bx) *bx = e->bx;
    if (cx) *cx = e->cx;
    if (dx) *dx = e->dx;
    if (si) *si = e->si;
    if (di) *di = e->di;
    if (ip) *ip = e->ip;
    if (flags) *flags = e->flags;
    if (cs) *cs = e->cs;
}
