/*
 * ps2.h  --  PS/2 Keyboard and Mouse Driver (Bare Metal)
 *
 * Ported from Mythos Fable (filipvabrousek/osdev) for WuBuDos bare-metal mode.
 * Provides keyboard scancode handling and PS/2 mouse packet processing.
 * Uses abstracted I/O port functions for ZealOS kernel compatibility.
 */

#ifndef MYSEED_PS2_H
#define MYSEED_PS2_H

#include <stdint.h>
#include <stdbool.h>

/* PS/2 Mouse State */
extern volatile int ps2_mouse_x;
extern volatile int ps2_mouse_y;
extern volatile uint8_t ps2_mouse_buttons;

/* Keyboard */
extern volatile bool ps2_key_pressed[256];

/* Initialize PS/2 controller, keyboard, and mouse. The optional probe
 * receives the validated self-test / device-ID / ACK results (gap A11):
 * NULL skips the reporting, the init still validates. */
typedef struct {
    int      self_test;   /* 0 = 0xAA->0x55 OK; -1 timeout; -2 wrong */
    int      kbd_id;      /* >=0: keyboard device ID (0x83/0xAB/...) */
    int      mouse_id;    /* >=0: mouse device ID (0x00/0x03/0x04/...) */
    int      mouse_ack;   /* 0 = F6+F4 ACKed; -1 timeout; -2 wrong   */
    uint32_t flags;       /* PS2_PROBE_* bits */
} ps2_probe_t;

#define PS2_PROBE_SELFTEST_OK   0x01
#define PS2_PROBE_KBD_OK        0x02
#define PS2_PROBE_MOUSE_ACK_OK  0x04
#define PS2_PROBE_MOUSE_OK      0x08

void ps2_init(int screen_w, int screen_h, ps2_probe_t *probe);

/* Keyboard handler - call from IRQ1 (interrupt 0x21) */
void ps2_keyboard_handler(void);

/* Mouse handler - call from IRQ12 (interrupt 0x2C) */
void ps2_mouse_handler(void);

/* Get last keyboard scancode (make code only) */
uint8_t ps2_get_scancode(void);

/* Check if mouse buttons changed since last poll */
bool ps2_mouse_poll(int *dx, int *dy, uint8_t *buttons);

/* Reset mouse position to center */
void ps2_mouse_center(int screen_w, int screen_h);

#endif /* MYSEED_PS2_H */