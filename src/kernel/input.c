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
