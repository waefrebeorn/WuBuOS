/*
 * ps2.c  --  PS/2 Keyboard and Mouse Driver (Bare Metal)
 *
 * Ported from Mythos Fable (filipvabrousek/osdev) for WuBuDos bare-metal mode.
 * Uses inline I/O port functions for portability.
 */

#include "ps2.h"
#include "interrupt.h"
#include "input.h"
#include <stdint.h>

/* ================================================================
 * I/O Port Inline Functions (GCC x86 built-ins)
 * ================================================================ */

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2))
            return;
}

static inline void ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1)
            return;
}

static void mouse_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(0x64, 0xD4);           /* next byte goes to the aux device */
    ps2_wait_write();
    outb(0x60, cmd);
    ps2_wait_read();
    (void)inb(0x60);            /* eat the ACK (0xFA) */
}

/* ================================================================
 * Global State
 * ================================================================ */

static int g_screen_w = 640;
static int g_screen_h = 480;
static uint8_t g_mouse_cycle = 0;
static uint8_t g_mouse_pkt[3] = {0};

volatile int ps2_mouse_x = 0;
volatile int ps2_mouse_y = 0;
volatile uint8_t ps2_mouse_buttons = 0;
volatile bool ps2_key_pressed[256] = {0};

/* ================================================================
 * Initialization
 * ================================================================ */

void ps2_init(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;

    /* Drain stale data */
    while (inb(0x64) & 1)
        (void)inb(0x60);

    /* Enable aux port (mouse) */
    ps2_wait_write();
    outb(0x64, 0xA8);

    /* Read command byte */
    ps2_wait_write();
    outb(0x64, 0x20);
    ps2_wait_read();
    uint8_t cb = inb(0x60);

    /* Enable IRQ1 + IRQ12, enable both clocks */
    cb |= 0x03;
    cb &= ~0x30;

    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, cb);

    /* Mouse defaults + enable data reporting */
    mouse_cmd(0xF6);
    mouse_cmd(0xF4);

    /* Drain again before interrupts go live */
    while (inb(0x64) & 1)
        (void)inb(0x60);

    ps2_mouse_x = g_screen_w / 2;
    ps2_mouse_y = g_screen_h / 2;
}

void ps2_mouse_center(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    ps2_mouse_x = g_screen_w / 2;
    ps2_mouse_y = g_screen_h / 2;
}

/* ================================================================
 * Keyboard Handler (IRQ1)
 * ================================================================ */

/* Scancode Set 1 -> ASCII, unshifted */
static const char keymap[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',  8,
    9, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',  0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',  0, '*',  0, ' ',
};

void ps2_keyboard_handler(void) {
    uint8_t sc = inb(0x60);

    if (!(sc & 0x80)) {  /* make code only */
        if (sc < 128) {
            ps2_key_pressed[sc] = true;
            char c = keymap[sc];
            if (c) {
                input_key_push_simple(c);  /* Push ASCII char to input queue */
            }
        }
    } else {
        /* Key release */
        uint8_t make_sc = sc & 0x7F;
        if (make_sc < 128) {
            ps2_key_pressed[make_sc] = false;
        }
    }

    /* EOI for IRQ1 */
    outb(0x20, 0x20);
}

/* Get last scancode and clear */
uint8_t ps2_get_scancode(void) {
    for (int i = 1; i < 128; i++) {
        if (ps2_key_pressed[i]) {
            ps2_key_pressed[i] = false;
            return i;
        }
    }
    return 0;
}

/* ================================================================
 * Mouse Handler (IRQ12)
 * ================================================================ */

void ps2_mouse_handler(void) {
    uint8_t data = inb(0x60);

    switch (g_mouse_cycle) {
    case 0:
        if (data & 0x08) {      /* bit 3 always set in byte 0: stay synced */
            g_mouse_pkt[0] = data;
            g_mouse_cycle = 1;
        }
        break;
    case 1:
        g_mouse_pkt[1] = data;
        g_mouse_cycle = 2;
        break;
    case 2: {
        g_mouse_cycle = 0;
        uint8_t b0 = g_mouse_pkt[0];
        if (b0 & 0xC0) return;  /* overflow: drop packet */

        int dx = g_mouse_pkt[1] - ((b0 & 0x10) << 4);  /* 9-bit signed */
        int dy = data       - ((b0 & 0x20) << 3);
        int nx = ps2_mouse_x + dx;
        int ny = ps2_mouse_y - dy;        /* PS/2 y+ is up */

        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx > g_screen_w - 1) nx = g_screen_w - 1;
        if (ny > g_screen_h - 1) ny = g_screen_h - 1;

        ps2_mouse_x = nx;
        ps2_mouse_y = ny;
        ps2_mouse_buttons = b0 & 0x07;
        break;
    }
    }

    /* EOI for IRQ12 (slave PIC) + IRQ1 (master PIC) */
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* Poll mouse state for non-interrupt contexts */
bool ps2_mouse_poll(int *dx, int *dy, uint8_t *buttons) {
    /* In interrupt-driven mode, state is updated in handler.
     * This just returns current deltas since last poll. */
    static int last_x = 0, last_y = 0;
    static uint8_t last_btn = 0;

    *dx = ps2_mouse_x - last_x;
    *dy = ps2_mouse_y - last_y;
    *buttons = ps2_mouse_buttons;

    bool changed = (*dx != 0 || *dy != 0 || *buttons != last_btn);
    last_x = ps2_mouse_x;
    last_y = ps2_mouse_y;
    last_btn = *buttons;

    return changed;
}
