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

/* Initialize PS/2 controller, keyboard, and mouse */
void ps2_init(int screen_w, int screen_h);

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