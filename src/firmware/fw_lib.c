/*
 * fw_lib.c  --  WuBuFW freestanding string/mem + console (serial + VGA text).
 *
 * The firmware links -nostdlib, so every byte of libc used here is ours.
 * Console output goes to both COM1 (16550) and the VGA text buffer at
 * 0xB8000 so QEMU works with -serial and with a graphical window.
 */

#include "fw.h"
#include <stdarg.h>

/* -- mem/string --------------------------------------------------- */

void *fw_memset(void *d, int c, size_t n) {
    unsigned char *p = (unsigned char *)d;
    unsigned char v = (unsigned char)c;
    while (n >= 8 && ((uintptr_t)p & 7) == 0) {
        uint64_t w = 0x0101010101010101ULL * v;
        *(uint64_t *)p = w; p += 8; n -= 8;
    }
    while (n--) *p++ = v;
    return d;
}

void *fw_memcpy(void *d, const void *s, size_t n) {
    unsigned char *dp = (unsigned char *)d;
    const unsigned char *sp = (const unsigned char *)s;
    if (dp == sp || n == 0) return d;
    if (dp < sp) {
        while (n >= 8 && ((uintptr_t)dp & 7) == 0 && ((uintptr_t)sp & 7) == 0) {
            *(uint64_t *)dp = *(const uint64_t *)sp; dp += 8; sp += 8; n -= 8;
        }
        while (n--) *dp++ = *sp++;
    } else {
        dp += n; sp += n;
        while (n--) *--dp = *--sp;
    }
    return d;
}

int fw_memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    return 0;
}

size_t fw_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

int fw_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

size_t fw_strlen16(const CHAR16 *s) { size_t n = 0; while (s[n]) n++; return n; }

static CHAR16 up16(CHAR16 c) { return (c >= 'a' && c <= 'z') ? (CHAR16)(c - 32) : c; }

int fw_stricmp16(const CHAR16 *a, const CHAR16 *b) {
    while (*a && up16(*a) == up16(*b)) { a++; b++; }
    return (int)up16(*a) - (int)up16(*b);
}

/* GCC may emit calls to these regardless of -ffreestanding. */
void *memset(void *d, int c, size_t n) { return fw_memset(d, c, n); }
void *memcpy(void *d, const void *s, size_t n) { return fw_memcpy(d, s, n); }
void *memmove(void *d, const void *s, size_t n) { return fw_memcpy(d, s, n); }
int   memcmp(const void *a, const void *b, size_t n) { return fw_memcmp(a, b, n); }

/* -- serial (COM1) ------------------------------------------------- */

#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* disable interrupts        */
    outb(COM1 + 3, 0x80);   /* DLAB                      */
    outb(COM1 + 0, 0x01);   /* 115200 baud divisor lo    */
    outb(COM1 + 1, 0x00);   /* divisor hi                */
    outb(COM1 + 3, 0x03);   /* 8N1                       */
    outb(COM1 + 2, 0xC7);   /* FIFO enable, clear, 14B   */
    outb(COM1 + 4, 0x0B);   /* DTR | RTS | OUT2          */
}

static void serial_putc(char c) {
    unsigned spin = 0;
    while (!(inb(COM1 + 5) & 0x20)) { if (++spin > 100000u) break; }
    outb(COM1, (uint8_t)c);
}

/* -- VGA text mode ------------------------------------------------- */

#define VGA_MEM  ((volatile uint16_t *)0x000B8000ULL)
#define VGA_COLS 80
#define VGA_ROWS 25

static int vga_row, vga_col;
static uint8_t vga_attr = 0x0F;

static void vga_scroll(void) {
    for (int r = 1; r < VGA_ROWS; r++)
        for (int c = 0; c < VGA_COLS; c++)
            VGA_MEM[(r - 1) * VGA_COLS + c] = VGA_MEM[r * VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; c++)
        VGA_MEM[(VGA_ROWS - 1) * VGA_COLS + c] = ((uint16_t)vga_attr << 8) | ' ';
    vga_row = VGA_ROWS - 1;
}

static void vga_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_COLS + vga_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

static void vga_putc(char c) {
    if (c == '\n') { vga_col = 0; vga_row++; }
    else if (c == '\r') { vga_col = 0; }
    else if (c == '\b') { if (vga_col) vga_col--; }
    else if (c == '\t') { vga_col = (vga_col + 8) & ~7; }
    else {
        VGA_MEM[vga_row * VGA_COLS + vga_col] = ((uint16_t)vga_attr << 8) | (uint8_t)c;
        vga_col++;
    }
    if (vga_col >= VGA_COLS) { vga_col = 0; vga_row++; }
    while (vga_row >= VGA_ROWS) vga_scroll();
    vga_cursor();
}

void fw_vga_clear(void) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_MEM[i] = ((uint16_t)vga_attr << 8) | ' ';
    vga_row = vga_col = 0;
    vga_cursor();
}

void fw_vga_set_attr(uint8_t a) { vga_attr = a; }
void fw_vga_set_pos(int row, int col) {
    if (row >= 0 && row < VGA_ROWS) vga_row = row;
    if (col >= 0 && col < VGA_COLS) vga_col = col;
    vga_cursor();
}
void fw_vga_get_pos(int *row, int *col) { if (row) *row = vga_row; if (col) *col = vga_col; }

/* -- public console ------------------------------------------------ */

void fw_con_init(void) {
    serial_init();
    fw_vga_clear();
}

void fw_putc(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    vga_putc(c);
}

void fw_puts(const char *s) { while (*s) fw_putc(*s++); }

void fw_puthex(uint64_t v) {
    static const char d[] = "0123456789ABCDEF";
    char buf[17]; int i = 16;
    buf[16] = 0;
    if (!v) { fw_puts("0"); return; }
    while (v) { buf[--i] = d[v & 0xF]; v >>= 4; }
    fw_puts(&buf[i]);
}

void fw_putdec(uint64_t v) {
    char buf[21]; int i = 20;
    buf[20] = 0;
    if (!v) { fw_puts("0"); return; }
    while (v) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
    fw_puts(&buf[i]);
}

void fw_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { fw_putc(*p); continue; }
        p++;
        int lng = 0;
        int width = 0, zero = 0;
        while (*p == '0') { zero = 1; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        while (*p == 'l') { lng++; p++; }
        switch (*p) {
        case 's': { const char *s = va_arg(ap, const char *); fw_puts(s ? s : "(null)"); break; }
        case 'c': fw_putc((char)va_arg(ap, int)); break;
        case 'd': {
            int64_t v = lng ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int);
            if (v < 0) { fw_putc('-'); v = -v; }
            fw_putdec((uint64_t)v); break;
        }
        case 'u': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned);
            char buf[24]; int i = (int)sizeof(buf);
            buf[--i] = 0; if (!v) buf[--i] = '0';
            while (v) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
            int n = (int)sizeof(buf) - 1 - i;
            while (n++ < width) fw_putc(zero ? '0' : ' ');
            fw_puts(&buf[i]); break;
        }
        case 'x':
        case 'X': {
            static const char hexd[] = "0123456789ABCDEF";
            uint64_t v = lng ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned);
            char hb[20]; int hi = (int)sizeof(hb);
            if (!v) hb[--hi] = '0';
            while (v) { hb[--hi] = hexd[v & 0xF]; v >>= 4; }
            int n = (int)sizeof(hb) - hi;
            while (n++ < width) fw_putc(zero ? '0' : ' ');
            fw_puts(&hb[hi]); break;
        }
        case 'p': fw_puts("0x"); fw_puthex((uint64_t)(uintptr_t)va_arg(ap, void *)); break;
        case 'w': { /* CHAR16* */
            const CHAR16 *w = va_arg(ap, const CHAR16 *);
            while (w && *w) { fw_putc(*w < 128 ? (char)*w : '?'); w++; }
            break;
        }
        case '%': fw_putc('%'); break;
        default: fw_putc('%'); fw_putc(*p); break;
        }
    }
    va_end(ap);
}

/* -- keyboard (PS/2 set 1, polled) + serial input ------------------- */

static const char kbd_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

int fw_getc_nb(void) {
    if (inb(COM1 + 5) & 0x01) return (int)(unsigned char)inb(COM1);
    if (inb(0x64) & 0x01) {
        uint8_t sc = inb(0x60);
        if (sc & 0x80) return -1;           /* key release */
        if (sc < 128 && kbd_map[sc]) return (int)(unsigned char)kbd_map[sc];
        /* extended/nav keys reported as scancode | 0x100 */
        return 0x100 | sc;
    }
    return -1;
}
