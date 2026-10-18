/*
 * input.c  --  My Seed Input Subsystem (hosted stub)
 *
 * Circular buffers for keyboard/mouse events with proper overflow handling.
 * Uses count-based full/empty detection to support full QUEUE_SIZE capacity.
 * Cell 202: Unified input dispatch queue.
 */
#include "input.h"
#include <string.h>

#define QUEUE_SIZE 256

static KeyEvent   g_key_queue[QUEUE_SIZE];
static int        g_key_head = 0;
static int        g_key_count = 0;

static MouseEvent g_mouse_queue[QUEUE_SIZE];
static int        g_mouse_head = 0;
static int        g_mouse_count = 0;

static int        g_mouse_x = 0, g_mouse_y = 0;

/* Current key state tracking for input_key_pressed */
static uint8_t g_key_state[256] = {0};

int input_init(void) {
    /* Reset all queue state */
    g_key_head = g_key_count = 0;
    g_mouse_head = g_mouse_count = 0;
    g_mouse_x = g_mouse_y = 0;
    memset(g_key_state, 0, sizeof(g_key_state));
    return 0;
}

void input_shutdown(void) {
    /* Nothing to clean up for stub */
}

void input_key_push(KeyEvent ev) {
    int tail = (g_key_head + g_key_count) % QUEUE_SIZE;

    if (g_key_count == QUEUE_SIZE) {
        /* Queue full - drop oldest by advancing head */
        g_key_head = (g_key_head + 1) % QUEUE_SIZE;
    } else {
        g_key_count++;
    }

    g_key_queue[tail] = ev;

    /* Update key state for input_key_pressed */
    if (ev.scancode < 256) {
        if (ev.kind == KEY_EVENT_DOWN)
            g_key_state[ev.scancode] = 1;
        else
            g_key_state[ev.scancode] = 0;
    }
}

int input_key_poll(KeyEvent *out) {
    if (g_key_count == 0) return 0;
    *out = g_key_queue[g_key_head];
    g_key_head = (g_key_head + 1) % QUEUE_SIZE;
    g_key_count--;
    return 1;
}

int input_key_wait(KeyEvent *out) {
    while (g_key_count == 0) ;
    return input_key_poll(out);
}

int input_key_pressed(uint32_t scancode) {
    if (scancode < 256)
        return g_key_state[scancode];
    return 0;
}

void input_mouse_push(MouseEvent ev) {
    /* If absolute position provided (dx=dy=0 indicates absolute), use it.
       Otherwise accumulate from dx/dy. */
    if (ev.dx == 0 && ev.dy == 0) {
        g_mouse_x = ev.x;
        g_mouse_y = ev.y;
    } else {
        g_mouse_x += ev.dx;
        g_mouse_y += ev.dy;
    }
    ev.x = g_mouse_x;
    ev.y = g_mouse_y;

    int tail = (g_mouse_head + g_mouse_count) % QUEUE_SIZE;

    if (g_mouse_count == QUEUE_SIZE) {
        /* Queue full - drop oldest */
        g_mouse_head = (g_mouse_head + 1) % QUEUE_SIZE;
    } else {
        g_mouse_count++;
    }

    g_mouse_queue[tail] = ev;
}

int input_mouse_poll(MouseEvent *out) {
    if (g_mouse_count == 0) return 0;
    *out = g_mouse_queue[g_mouse_head];
    g_mouse_head = (g_mouse_head + 1) % QUEUE_SIZE;
    g_mouse_count--;
    return 1;
}

void input_mouse_get_pos(int *x, int *y) {
    if (x) *x = g_mouse_x;
    if (y) *y = g_mouse_y;
}

/* Simple ASCII key push for PS/2 driver */
void input_key_push_simple(char c) {
    KeyEvent ev = {0};
    ev.scancode = (uint32_t)c;
    ev.keycode = (uint32_t)c;
    ev.kind = KEY_EVENT_DOWN;
    ev.modifiers = 0;
    input_key_push(ev);

    /* Also add key-up for auto-repeat simulation */
    ev.kind = KEY_EVENT_UP;
    input_key_push(ev);
}

/* ================================================================
 * PS/2 set-2 decoder (DRIVERS #956)
 *
 * The wire format is stateful: 0xF0 marks the next code as a release, 0xE0
 * marks an extended key, and Pause arrives as the six-byte E1 1D 45 E1 9D C5
 * sequence rather than as a normal make/break pair.
 * ================================================================ */
#include <stdint.h>

static struct {
    int e0;
    int brk;
    int pause_state;   /* 1..5 while matching the Pause sequence */
} g_ps2;

void input_ps2_reset(void) {
    g_ps2.e0 = 0;
    g_ps2.brk = 0;
    g_ps2.pause_state = 0;
}

static int ps2_keycode(uint8_t code, int extended) {
    if (extended) {
        switch (code) {
            case 0x48: return 0x26;  /* Up */
            case 0x50: return 0x28;  /* Down */
            case 0x4B: return 0x25;  /* Left */
            case 0x4D: return 0x27;  /* Right */
            default:   return 0;
        }
    }
    switch (code) {
        case 0x1C: return 'a'; case 0x32: return 'b'; case 0x21: return 'c';
        case 0x23: return 'd'; case 0x24: return 'e'; case 0x2B: return 'f';
        case 0x34: return 'g'; case 0x33: return 'h'; case 0x43: return 'i';
        case 0x3B: return 'j'; case 0x42: return 'k'; case 0x4B: return 'l';
        case 0x3A: return 'm'; case 0x31: return 'n'; case 0x44: return 'o';
        case 0x4D: return 'p'; case 0x15: return 'q'; case 0x2D: return 'r';
        case 0x1B: return 's'; case 0x2C: return 't'; case 0x3C: return 'u';
        case 0x2A: return 'v'; case 0x1D: return 'w'; case 0x22: return 'x';
        case 0x35: return 'y'; case 0x1A: return 'z';
        case 0x16: return '1'; case 0x1E: return '2'; case 0x26: return '3';
        case 0x25: return '4'; case 0x2E: return '5'; case 0x36: return '6';
        case 0x3D: return '7'; case 0x3E: return '8'; case 0x46: return '9';
        case 0x45: return '0';
        case 0x29: return ' ';  case 0x5A: return '\n'; case 0x76: return 0x1B;
        case 0x66: return '\b'; case 0x0D: return '\t';
        case 0x12: return 0x10;  /* L shift */
        case 0x59: return 0x10;  /* R shift */
        case 0x14: return 0x11;  /* L ctrl  */
        case 0x58: return 0x11;  /* R ctrl  */
        default:   return 0;
    }
}

void input_ps2_feed(uint8_t byte) {
    static const uint8_t pause_seq[6] = {0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5};

    /* Pause is a sequence, not a make/break pair: consume it as one event. */
    if (g_ps2.pause_state > 0) {
        if (byte == pause_seq[g_ps2.pause_state]) {
            g_ps2.pause_state++;
            if (g_ps2.pause_state == 6) {
                KeyEvent ev = {0};
                ev.scancode = 0xE11D45u;
                ev.keycode  = 0x13;          /* VK_PAUSE */
                ev.kind     = KEY_EVENT_DOWN;
                input_key_push(ev);
                g_ps2.pause_state = 0;
            }
            return;
        }
        g_ps2.pause_state = (byte == 0xE1) ? 1 : 0;
        return;
    }

    if (byte == 0xE1) { g_ps2.pause_state = 1; return; }
    if (byte == 0xE0) { g_ps2.e0 = 1; return; }
    if (byte == 0xF0) { g_ps2.brk = 1; return; }

    KeyEvent ev = {0};
    ev.scancode = g_ps2.e0 ? (0xE000u | byte) : (uint32_t)byte;
    ev.keycode  = (uint32_t)ps2_keycode(byte, g_ps2.e0);
    ev.kind     = g_ps2.brk ? KEY_EVENT_UP : KEY_EVENT_DOWN;
    input_key_push(ev);

    g_ps2.e0 = 0;
    g_ps2.brk = 0;
}
