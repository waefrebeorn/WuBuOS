/*
 * klog.c -- WuBuOS bare-metal kernel log sink (serial COM1)
 *
 * Self-contained freestanding output. Writes to the COM1 UART (0x3F8) so the
 * kernel can emit diagnostics under -nostdlib. Implements a minimal printf
 * subset (%s %d %u %x %X %p %c %%) sufficient for heap/debug reporting.
 *
 * No dependency on libc stdio. Uses only CPU I/O port insns and <stdint.h>.
 */

#include "klog.h"

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define COM1_PORT   0x3F8
#define COM1_DATA   (COM1_PORT + 0)
#define COM1_LSR    (COM1_PORT + 5)

static int g_klog_ready;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void klog_pause(void) {
    __asm__ __volatile__("pause" ::: "memory");
}

void klog_init(void) {
    /* Disable interrupts, set DLAB, baud divisor 1 (115200), 8N1. */
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x01);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 1, 0x00);
    g_klog_ready = 1;
}

static void putc_raw(char c) {
    if (!g_klog_ready) return;
    /* Wait for THR empty. */
    while ((inb(COM1_LSR) & 0x20) == 0) klog_pause();
    outb(COM1_DATA, (uint8_t)c);
}

void klog_write(const char *s) {
    if (!s) return;
    while (*s) putc_raw(*s++);
}

void klog_write_n(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) putc_raw(s[i]);
}

/* --- tiny printf subset --- */

static void putu(unsigned long v, int base, int upper) {
    if (v == 0) { putc_raw('0'); return; }
    char buf[20];
    int i = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    while (v) { buf[i++] = digits[v % base]; v /= base; }
    while (i--) putc_raw(buf[i]);
}

static void puts_dec(long v) {
    if (v < 0) { putc_raw('-'); v = -v; }
    putu((unsigned long)v, 10, 0);
}

int klog_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int written = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { putc_raw(*p); written++; continue; }
        p++;
        switch (*p) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) { putc_raw(*s); written++; s++; }
                break;
            }
            case 'd': case 'i':
                puts_dec(va_arg(ap, int)); written++;
                break;
            case 'u':
                putu(va_arg(ap, unsigned int), 10, 0); written++;
                break;
            case 'x':
                putu(va_arg(ap, unsigned long), 16, 0); written++;
                break;
            case 'X':
                putu(va_arg(ap, unsigned long), 16, 1); written++;
                break;
            case 'p': {
                putu((unsigned long)(uintptr_t)va_arg(ap, void *), 16, 0);
                written++;
                break;
            }
            case 'c':
                putc_raw((char)va_arg(ap, int)); written++;
                break;
            case '%':
                putc_raw('%'); written++;
                break;
            default:
                putc_raw('%'); putc_raw(*p); written += 2;
                break;
        }
    }
    va_end(ap);
    return written;
}
