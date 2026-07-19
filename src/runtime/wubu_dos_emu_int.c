/* wubu_dos_emu_int.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). */
#include "wubu_dos_emu_internal.h"

int dos_handle_to_fd(WubuDosEmu *e, uint16_t h) {
    if (h <= 2) return (int)h;                 /* stdin/out/err */
    int slot = (int)h - 1;
    if (slot < 0 || slot >= 64 || !e->host_used[slot]) return -1;
    return e->host_fd[slot];
}
/* Allocate a DOS handle for a freshly opened host fd (>=3). Returns handle or 0. */

void int16(WubuDosEmu *e) {
    uint8_t ah = (uint8_t)(e->ax >> 8);
    switch (ah) {
        case 0x00: { uint8_t v = kbd_pop(e); uint8_t sc = (e->khead == e->ktail) ? 0 : e->kscan[(e->khead) & 63]; e->ax = (uint16_t)((sc << 8) | v); break; }
        case 0x01: if (e->khead == e->ktail) setF(e, F_ZF, 1); else { setF(e, F_ZF, 0); e->ax = (uint16_t)((e->kscan[e->khead & 63] << 8) | e->kbuf[e->khead & 63]); } break;
        case 0x02: e->ax = (uint16_t)((e->ax & 0xFF00) | e->kshift); break; /* shift state in AL */
        default: break;
    }
}

void put_char(WubuDosEmu *e, char c) {
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

uint16_t dos_handle_alloc(WubuDosEmu *e, int host_fd) {
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

void int21(WubuDosEmu *e) {
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

void scroll_up(WubuDosEmu *e, int top, int left, int bot, int right, int lines, uint8_t attr) {
    if (lines <= 0) return;
    for (int y = top; y <= bot - lines; y++) {
        memcpy(e->text[y] + left, e->text[y + lines] + left, (size_t)(right - left + 1));
        memcpy(e->attr[y] + left, e->attr[y + lines] + left, (size_t)(right - left + 1));
    }
    for (int y = bot - lines + 1; y <= bot; y++)
        for (int x = left; x <= right; x++) { e->text[y][x] = ' '; e->attr[y][x] = attr; }
}

uint8_t kbd_pop(WubuDosEmu *e) {
    if (e->khead == e->ktail) return 0;
    uint8_t v = e->kbuf[e->khead];
    e->khead = (e->khead + 1) & 63;
    return v;
}

void int10(WubuDosEmu *e) {
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
