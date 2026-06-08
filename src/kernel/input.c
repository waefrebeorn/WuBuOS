/*
 * input.c — My Seed Input Subsystem (hosted stub)
 */
#include "input.h"
#include <string.h>

#define QUEUE_SIZE 256

static KeyEvent   g_key_queue[QUEUE_SIZE];
static int        g_key_head = 0, g_key_tail = 0;
static MouseEvent g_mouse_queue[QUEUE_SIZE];
static int        g_mouse_head = 0, g_mouse_tail = 0;
static int        g_mouse_x = 0, g_mouse_y = 0;

int input_init(void) { return 0; }
void input_shutdown(void) { }

void input_key_push(KeyEvent ev) {
    g_key_queue[g_key_tail] = ev;
    g_key_tail = (g_key_tail + 1) % QUEUE_SIZE;
}

int input_key_poll(KeyEvent *out) {
    if (g_key_head == g_key_tail) return 0;
    *out = g_key_queue[g_key_head];
    g_key_head = (g_key_head + 1) % QUEUE_SIZE;
    return 1;
}

int input_key_wait(KeyEvent *out) {
    while (g_key_head == g_key_tail) ;
    return input_key_poll(out);
}

int input_key_pressed(uint32_t scancode) {
    /* Check the last 16 events in the queue for a matching key-down */
    int count = 16;
    int idx = (g_key_tail - 1 + QUEUE_SIZE) % QUEUE_SIZE;
    while (count-- > 0 && idx != g_key_head) {
        if (g_key_queue[idx].scancode == scancode && g_key_queue[idx].kind == KEY_EVENT_DOWN) {
            return 1;
        }
        idx = (idx - 1 + QUEUE_SIZE) % QUEUE_SIZE;
    }
    return 0;
}

void input_mouse_push(MouseEvent ev) {
    g_mouse_x += ev.dx; g_mouse_y += ev.dy;
    ev.x = g_mouse_x; ev.y = g_mouse_y;
    g_mouse_queue[g_mouse_tail] = ev;
    g_mouse_tail = (g_mouse_tail + 1) % QUEUE_SIZE;
}

int input_mouse_poll(MouseEvent *out) {
    if (g_mouse_head == g_mouse_tail) return 0;
    *out = g_mouse_queue[g_mouse_head];
    g_mouse_head = (g_mouse_head + 1) % QUEUE_SIZE;
    return 1;
}

void input_mouse_get_pos(int *x, int *y) {
    if (x) *x = g_mouse_x;
    if (y) *y = g_mouse_y;
}
