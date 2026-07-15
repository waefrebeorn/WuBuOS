/*
 * klog.h -- WuBuOS bare-metal kernel log sink
 *
 * Minimal freestanding output for the kernel: writes formatted text to the
 * COM1 (0x3F8) serial port. Used by subsystems (memory, etc.) that need to
 * report diagnostics under -nostdlib where libc stdio does not exist.
 *
 * Self-contained: depends only on <stdint.h>. No printf/fprintf.
 */

#ifndef WUBU_KLOG_H
#define WUBU_KLOG_H

#include <stdint.h>
#include <stddef.h>

/* Initialize the COM1 UART (8N1, 115200). Safe to call once at boot. */
void klog_init(void);

/* Write a raw NUL-terminated string to the log sink. */
void klog_write(const char *s);

/* Write exactly `n` bytes. */
void klog_write_n(const char *s, size_t n);

/* Formatted log: a tiny printf subset supporting %s %d %u %x %p %c %%.
 * Returns the number of characters written. Intended for kernel diagnostics
 * only -- not a full printf. */
int klog_printf(const char *fmt, ...);

#endif /* WUBU_KLOG_H */
