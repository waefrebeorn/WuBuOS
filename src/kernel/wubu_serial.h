#ifndef WUBU_SERIAL_H
#define WUBU_SERIAL_H

/*
 * wubu_serial.h — COM1 UART helpers (bounded, never-blocking).
 *
 * The serial is a DEBUG channel: the kernel must never block on it.
 * Every TX wait is bounded; on timeout the character is dropped.
 * Shared between wubu_console.c, wubu_console_recovery.c, and any
 * other ring-0 debug output.
 */

#include <stdint.h>

#define COM1_PORT 0x3F8
#define COM1_LSR  (COM1_PORT + 5)

static inline uint8_t serial_rx_ready(void) {
    return (uint8_t)(inb(COM1_LSR) & 0x01);
}
static inline uint8_t serial_rx(void) {
    return inb(COM1_PORT);
}
/* BOUNDED TX wait (the tick-12/33/153 freeze root cause): when a
 * slow/no reader backs up the serial socket, the UART's THR-empty
 * stops and the OLD unbounded wait spun the CPU FOREVER (the kernel
 * appeared frozen with the serial-register state). The serial is a
 * DEBUG channel -- the kernel must never block on it: wait a bounded
 * number of polls, then DROP the character. */
static inline int serial_tx(uint8_t c) {
    for (int i = 0; i < 65536; i++) {
        if (inb(COM1_LSR) & 0x20) { outb(COM1_PORT, c); return 0; }
    }
    /* timeout: the char is dropped; the kernel continues */
    return 0;
}

#endif /* WUBU_SERIAL_H */
