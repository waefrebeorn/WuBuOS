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

/* Read one byte from the data port with a bounded wait (gap A11: the
 * old path assumed every command's response arrived). Returns -1 on
 * timeout so the caller can distinguish a real ACK from silence. */
static int ps2_read_byte(uint8_t *out) {
    ps2_wait_read();
    if (!(inb(0x64) & 1)) return -1;   /* timeout: nothing arrived */
    if (out) *out = inb(0x60);
    return 0;
}

/* Issue a controller command + validate the ACK (0xFA) / the response
 * against an expected byte. Returns 0 on match, -1 on timeout, -2 on
 * a wrong byte. */
static int ps2_cmd_ack(uint8_t cmd, uint8_t expect, bool strict) {
    ps2_wait_write();
    outb(0x64, cmd);
    uint8_t r;
    if (ps2_read_byte(&r) != 0) return -1;
    if (strict && r != expect) return -2;
    return 0;
}

/* Controller self-test (0xAA must answer 0x55). */
static int ps2_self_test(void) {
    return ps2_cmd_ack(0xAA, 0x55, true);
}

/* Device ID handshake (0xF2 to a port's device). The device answers
 * with an ACK (0xFA) first, then the ID: the keyboard sends 0xAB 0x83
 * (or a single 0xAB), the mouse a single ID byte (0x00/0x03/0x04/...).
 * Returns the ID (>=0) or a negative error. */
static int ps2_dev_id(void) {
    ps2_wait_write();
    outb(0x60, 0xF2);
    uint8_t r;
    if (ps2_read_byte(&r) != 0) return -1;      /* nothing at all */
    if (r == 0xFA) {                             /* ACK: read the ID */
        if (ps2_read_byte(&r) != 0) return -1;
    }
    if (r == 0xAB) {           /* keyboard: 0xAB then the type byte */
        uint8_t r2;
        if (ps2_read_byte(&r2) == 0) return (int)r2;
        return 0xAB;           /* single-byte keyboard ID */
    }
    return (int)r;             /* mouse / other device ID byte */
}

/* Device-ID handshake for the AUX (mouse) port: the 0xD4 prefix routes
 * the 0xF2 to the second device. */
static int ps2_aux_dev_id(void) {
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, 0xF2);
    uint8_t r;
    if (ps2_read_byte(&r) != 0) return -1;
    if (r == 0xFA) {                             /* ACK: read the ID */
        if (ps2_read_byte(&r) != 0) return -1;
    }
    return (int)r;             /* the mouse's ID byte */
}

static int mouse_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(0x64, 0xD4);           /* next byte goes to the aux device */
    ps2_wait_write();
    outb(0x60, cmd);
    uint8_t ack;
    if (ps2_read_byte(&ack) != 0) return -1;
    return (ack == 0xFA) ? 0 : -2;
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

void ps2_init(int screen_w, int screen_h, ps2_probe_t *probe) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    if (probe) {
        probe->self_test = 0;
        probe->kbd_id = 0;
        probe->mouse_id = 0;
        probe->mouse_ack = 0;
        probe->flags = 0;
    }

    /* Drain stale data */
    while (inb(0x64) & 1)
        (void)inb(0x60);

    /* Controller self-test (0xAA -> 0x55) + report the result. */
    int st = ps2_self_test();
    if (probe) {
        probe->self_test = st;
        if (st == 0) probe->flags |= PS2_PROBE_SELFTEST_OK;
    }

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

    /* Device-ID handshake for the keyboard port (gap A11). */
    int kbd_id = ps2_dev_id();
    if (probe) {
        probe->kbd_id = kbd_id;
        if (kbd_id >= 0) probe->flags |= PS2_PROBE_KBD_OK;
    }

    /* Mouse defaults + enable data reporting; validate the ACKs now
     * instead of eating them blindly. */
    int m1 = mouse_cmd(0xF6);
    int m2 = mouse_cmd(0xF4);
    if (probe) {
        probe->mouse_ack = (m1 == 0 && m2 == 0) ? 0 : -1;
        if (m1 == 0 && m2 == 0) probe->flags |= PS2_PROBE_MOUSE_ACK_OK;
    }
    int mid = ps2_aux_dev_id();
    if (probe) {
        probe->mouse_id = mid;
        if (mid >= 0) probe->flags |= PS2_PROBE_MOUSE_OK;
    }

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
                /* Feed the unified input layer too (GameInput-style):
                 * one event model for every device, common time base. */
                extern void wubu_hid_feed_key(uint32_t, bool, uint32_t);
                wubu_hid_feed_key((uint32_t)(uint8_t)c, true, 0);
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
