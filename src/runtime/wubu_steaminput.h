/*
 * wubu_steaminput.h -- the Steam Input layer (SteamOS E2).
 */
#ifndef WUBU_STEAMINPUT_H
#define WUBU_STEAMINPUT_H

#include <stdint.h>
#include <stddef.h>

/* the gamepad buttons (XInput layout) */
enum {
    SI_A = 0, SI_B, SI_X, SI_Y,
    SI_LB, SI_RB, SI_LT, SI_RT,
    SI_BACK, SI_START, SI_L3, SI_R3,
    SI_DPAD_UP, SI_DPAD_DOWN, SI_DPAD_LEFT, SI_DPAD_RIGHT,
    SI_N_BUTTONS
};

/* the sticks + axes */
enum {
    SI_LSTICK_X = 0, SI_LSTICK_Y, SI_RSTICK_X, SI_RSTICK_Y,
    SI_N_AXES
};

/* SI1: init with the default controller-as-keyboard config. */
void wubu_si_init(void);

/* SI2: bind a button to a scancode (or -1/-2/-3 = mouse clicks). */
int wubu_si_map_button(int button, int32_t scancode);

/* SI3: bind an axis to a key pair. */
int wubu_si_map_axis(int axis, int32_t neg_key, int32_t pos_key);

/* SI4/5: feed raw gamepad events (the engine emits mapped input). */
void wubu_si_feed_button(int button, int down);
void wubu_si_feed_axis(int axis, float value);

/* SI9: parse one 64-byte Steam Deck controller report (report ID 9,
 * the protocol stolen from Valve's hid-steam.c) and route every
 * decoded control through the mapping table. Returns the events. */
int wubu_si_parse_deck_report(const uint8_t *data, size_t size);

/* SI6/7: per-game config persistence. */
int wubu_si_save(const char *path);
int wubu_si_load(const char *path);

/* SI8: the test hooks. */
typedef struct {
    int initialized;
    int button_a_scancode;
    int axis_lx_neg;
    int axis_lx_pos;
    int mouse_speed;
} wubu_si_view_t;
int wubu_si_get(wubu_si_view_t *out);

#endif
