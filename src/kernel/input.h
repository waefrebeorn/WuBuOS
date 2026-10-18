/*
 * input.h  --  My Seed Keyboard/Mouse Input Subsystem
 */
#ifndef MYSEED_INPUT_H
#define MYSEED_INPUT_H
#include <stdint.h>

typedef enum {
    KEY_EVENT_DOWN = 0,
    KEY_EVENT_UP   = 1,
} KeyEventKind;

typedef struct {
    uint32_t      scancode;
    uint32_t      keycode;   /* Translated key */
    KeyEventKind  kind;
    uint32_t      modifiers; /* Shift, Ctrl, Alt flags */
} KeyEvent;

typedef struct {
    int x, y;          /* Position */
    int dx, dy;        /* Delta since last event */
    int buttons;       /* Bit 0=left, 1=right, 2=middle */
    int scroll;        /* Scroll delta */
} MouseEvent;

/* Keyboard API */
void    input_key_push(KeyEvent ev);
int     input_key_poll(KeyEvent *out);
int     input_key_wait(KeyEvent *out); /* Block until key */
int     input_key_pressed(uint32_t scancode); /* Check if key is held */

/* Mouse API */
void    input_mouse_push(MouseEvent ev);
int     input_mouse_poll(MouseEvent *out);
void    input_mouse_get_pos(int *x, int *y);

/* Modifier flags */
#define MOD_SHIFT  0x01
#define MOD_CTRL   0x02
#define MOD_ALT    0x04
#define MOD_WIN    0x08

/* Simple ASCII key push (for PS/2 driver) */
void input_key_push_simple(char c);

/* PS/2 set-2 decoder: feed raw bytes, get KeyEvents. Handles make/break
 * (0xF0), the E0 extended prefix, and the E1 1D 45 E1 9D C5 Pause sequence.
 * Unsupported key codes are still emitted (keycode 0) so nothing is dropped. */
void input_ps2_reset(void);
void input_ps2_feed(uint8_t byte);

/* Init */
int  input_init(void);
void input_shutdown(void);

#endif
