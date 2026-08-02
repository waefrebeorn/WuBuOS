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

/* In-RAM panic ring (gap A7): the last KLOG_RING_SZ bytes of output,
 * always captured, so a fault's post-mortem can dump what happened
 * before the panic (the kernel's crash evidence). */
#define KLOG_RING_SZ 4096
static char g_ring[KLOG_RING_SZ];
static uint32_t g_ring_pos;

static inline void ring_putc(char c) {
    g_ring[g_ring_pos] = c;
    g_ring_pos = (g_ring_pos + 1) % KLOG_RING_SZ;
}

/* Snapshot the ring (oldest-first) into `out` (bufsz bytes, NUL'd).
 * Returns the number of bytes copied. For the fault post-mortem. */
int klog_ring_snapshot(char *out, size_t bufsz) {
    if (!out || bufsz == 0) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < KLOG_RING_SZ && n + 1 < bufsz; i++) {
        char c = g_ring[(g_ring_pos + i) % KLOG_RING_SZ];
        if (c == '\0') continue;      /* unwritten slots are skipped */
        out[n++] = c;
    }
    out[n] = '\0';
    return (int)n;
}

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

/* ---- TX ring (gap E3) ------------------------------------------------ */
/* Serial output buffering: putc_raw pushes into a fixed ring and the
 * ring is drained whenever the THR is free -- the bounded-wait + drop
 * contract stays (the CPU never spins on the debug channel), but a
 * burst of output survives better because the ring absorbs it and the
 * drain sends whenever the UART is ready. The console's responses now
 * complete even under the promote flood. ISR-safe: the ring is only
 * touched with interrupts off or from one context at a time. */
#define TX_RING_SZ 4096
static volatile uint32_t g_tx_head;
static volatile uint32_t g_tx_tail;
static char g_tx_ring[TX_RING_SZ];

static inline void tx_push(char c) {
    uint32_t h = g_tx_head;
    uint32_t t = g_tx_tail;
    uint32_t next = (h + 1) & (TX_RING_SZ - 1);
    if (next == t) return;                 /* full: drop, never block */
    g_tx_ring[h] = c;
    g_tx_head = next;
}

/* Drain whatever the UART will take; bounded per call so a stuck THR
 * cannot monopolize the CPU. Returns the number of chars sent. */
static inline int tx_drain(void) {
    int sent = 0;
    while (g_tx_head != g_tx_tail && sent < 64) {
        if (!(inb(COM1_LSR) & 0x20)) break;    /* THR busy: stop here */
        outb(COM1_DATA, (uint8_t)g_tx_ring[g_tx_tail]);
        g_tx_tail = (g_tx_tail + 1) & (TX_RING_SZ - 1);
        sent++;
    }
    return sent;
}

static inline int putc_raw(char c) {
    /* ALWAYS captured in the panic ring (gap A7) -- the post-mortem. */
    ring_putc(c);
    if (!g_klog_ready) return 0;
    tx_push(c);
    tx_drain();
    return 0;
}

/* The timer tick + the console call this when the UART may have drained
 * (the ring is the buffering; the drain is opportunistic). */
void klog_tx_poll(void) { tx_drain(); }

void klog_write(const char *s) {
    if (!s) return;
    while (*s) putc_raw(*s++);
}

void klog_write_n(const char *s, size_t n) {
    if (!s) return;
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
