/*
 * wubu_steaminput.c -- the STEAM INPUT layer (SteamOS EPIC E2 gap:
 * "Steam Input (controller config) not implemented").
 *
 * Steam Input does two things:
 *   1. maps a gamepad (buttons/axes/sticks) to VIRTUAL input — the
 *      controller becomes a keyboard+mouse so any game works
 *   2. keeps per-game configs (a game binds the sticks to its own
 *      actions; the default config is the controller-as-keyboard)
 *
 * This module implements the mapping engine + the config store:
 *   - wubu_si_init()          — zeroed default config (gamepad->keys)
 *   - wubu_si_map_button()    — bind a pad button to a key/scancode
 *   - wubu_si_map_axis()      — bind a stick axis to a key pair
 *   - wubu_si_feed()          — push a raw gamepad event; the engine
 *     emits the mapped KEY/MOUSE event into the kernel input queue
 *   - wubu_si_save()/load()   — per-game config persistence
 *
 * The default map (the classic Steam "controller as keyboard"):
 *   A=Space  B=Esc  X=Enter  Y=Tab  Start=Return  Select=Backspace
 *   LB=Shift  RB=Ctrl  D-pad=arrows  L-stick WASD  R-stick mouse
 *
 * C11, self-contained.
 */
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the gamepad buttons (the standard XInput layout) */
typedef enum {
    SI_A = 0, SI_B, SI_X, SI_Y,
    SI_LB, SI_RB, SI_LT, SI_RT,       /* bumpers + triggers */
    SI_BACK, SI_START,
    SI_L3, SI_R3,                     /* stick clicks */
    SI_DPAD_UP, SI_DPAD_DOWN, SI_DPAD_LEFT, SI_DPAD_RIGHT,
    SI_N_BUTTONS
} si_button_t;

/* the sticks + axes */
typedef enum {
    SI_LSTICK_X = 0, SI_LSTICK_Y,
    SI_RSTICK_X, SI_RSTICK_Y,
    SI_N_AXES
} si_axis_t;

/* the mouse button bit indices (match MouseEvent.buttons) */
enum { SI_M_LEFT = 0, SI_M_RIGHT, SI_M_MIDDLE };

#define SI_CFG_MAGIC  0x53494D50   /* 'SIMP' */
#define SI_CFG_VERSION 1

/* one button mapping: the emitted scancode (or -1 for mouse) */
typedef struct {
    int32_t scancode;   /* >= 0: a key; -1: left click; -2: right; -3: mid */
} si_button_map_t;

/* one axis mapping: the emitted keys at the two extremes */
typedef struct {
    int32_t neg_key;    /* pushed toward -1 */
    int32_t pos_key;    /* pushed toward +1 */
} si_axis_map_t;

typedef struct {
    si_button_map_t buttons[SI_N_BUTTONS];
    si_axis_map_t   axes[SI_N_AXES];
    int             mouse_speed;   /* the stick->mouse multiplier */
} si_config_t;

/* ---- the default config: the controller as keyboard+mouse ---- */
static void si_default_config(si_config_t *c)
{
    memset(c, 0, sizeof(*c));
    /* the action buttons */
    c->buttons[SI_A].scancode = 0x39;           /* Space */
    c->buttons[SI_B].scancode = 0x01;           /* Esc */
    c->buttons[SI_X].scancode = 0x1C;           /* Enter */
    c->buttons[SI_Y].scancode = 0x0F;           /* Tab */
    c->buttons[SI_START].scancode = 0x1C;       /* Enter */
    c->buttons[SI_BACK].scancode = 0x0E;        /* Backspace */
    c->buttons[SI_LB].scancode = 0x2A;          /* LShift */
    c->buttons[SI_RB].scancode = 0x1D;          /* LCtrl */
    c->buttons[SI_DPAD_UP].scancode = 0x48;     /* Up */
    c->buttons[SI_DPAD_DOWN].scancode = 0x50;   /* Down */
    c->buttons[SI_DPAD_LEFT].scancode = 0x4B;   /* Left */
    c->buttons[SI_DPAD_RIGHT].scancode = 0x4D;  /* Right */
    /* the left stick = WASD */
    c->axes[SI_LSTICK_X].neg_key = 0x11;        /* A */
    c->axes[SI_LSTICK_X].pos_key = 0x1F;        /* D */
    c->axes[SI_LSTICK_Y].neg_key = 0x1E;        /* W */
    c->axes[SI_LSTICK_Y].pos_key = 0x20;        /* S */
    /* the right stick = mouse */
    c->axes[SI_RSTICK_X].neg_key = -1;          /* mouse left */
    c->axes[SI_RSTICK_X].pos_key = -2;          /* mouse right */
    c->axes[SI_RSTICK_Y].neg_key = -3;          /* mouse up */
    c->axes[SI_RSTICK_Y].pos_key = -4;          /* mouse down */
    c->mouse_speed = 4;
}

/* ---- the public API ---- */

typedef struct {
    si_config_t cfg;
    int  initialized;
    /* the held state (for edge detection: press on the transition) */
    uint8_t held[SI_N_BUTTONS];
    /* the mouse position accumulator (stick -> mouse deltas) */
    int mouse_x, mouse_y;
    /* the battery state (from the battery/status event) */
    int battery_mv;
    int battery_pct;
} wubu_si_t;

static wubu_si_t g_si;

/* SI1: init with the default config. */
void wubu_si_init(void)
{
    memset(&g_si, 0, sizeof(g_si));
    si_default_config(&g_si.cfg);
    g_si.initialized = 1;
}

/* SI2: bind a button. Returns 0 on success. */
int wubu_si_map_button(int button, int32_t scancode)
{
    if (!g_si.initialized || button < 0 || button >= SI_N_BUTTONS)
        return -1;
    g_si.cfg.buttons[button].scancode = scancode;
    return 0;
}

/* SI3: bind an axis to a key pair. */
int wubu_si_map_axis(int axis, int32_t neg_key, int32_t pos_key)
{
    if (!g_si.initialized || axis < 0 || axis >= SI_N_AXES)
        return -1;
    g_si.cfg.axes[axis].neg_key = neg_key;
    g_si.cfg.axes[axis].pos_key = pos_key;
    return 0;
}

/* SI4: feed a raw gamepad button event (button, down).
 * Emits the mapped key into the kernel input queue. */
void wubu_si_feed_button(int button, int down)
{
    if (!g_si.initialized || button < 0 || button >= SI_N_BUTTONS)
        return;
    if (down == g_si.held[button]) return;   /* no edge */
    g_si.held[button] = (uint8_t)down;

    int32_t sc = g_si.cfg.buttons[button].scancode;
    KeyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.scancode = (uint32_t)sc;
    ev.keycode = (uint32_t)sc;
    ev.kind = down ? KEY_EVENT_DOWN : KEY_EVENT_UP;
    if (sc == -1) {          /* left click */
        MouseEvent m; memset(&m, 0, sizeof(m));
        m.buttons = down ? (1 << SI_M_LEFT) : 0;
        input_mouse_push(m);
        return;
    }
    if (sc == -2) {          /* right click */
        MouseEvent m; memset(&m, 0, sizeof(m));
        m.buttons = down ? (1 << SI_M_RIGHT) : 0;
        input_mouse_push(m);
        return;
    }
    if (sc == -3) {          /* middle click */
        MouseEvent m; memset(&m, 0, sizeof(m));
        m.buttons = down ? (1 << SI_M_MIDDLE) : 0;
        input_mouse_push(m);
        return;
    }
    input_key_push(ev);
}

/* SI5: feed a raw stick axis (axis, value in [-1,1]).
 * Past the dead zone it emits the mapped key (axes are buttons at the
 * extremes for simplicity; the mouse axes accumulate deltas). */
void wubu_si_feed_axis(int axis, float value)
{
    if (!g_si.initialized || axis < 0 || axis >= SI_N_AXES)
        return;
    if (value > -0.25f && value < 0.25f) return;   /* dead zone */
    int32_t key = value < 0 ? g_si.cfg.axes[axis].neg_key
                            : g_si.cfg.axes[axis].pos_key;
    if (key == -3 || key == -4) {
        /* the mouse axes: accumulate deltas, emit a mouse move */
        int dx = (key == -3) ? (int)(value * g_si.cfg.mouse_speed) : 0;
        int dy = (key == -4) ? (int)(value * g_si.cfg.mouse_speed) : 0;
        g_si.mouse_x += dx;
        g_si.mouse_y += dy;
        MouseEvent m; memset(&m, 0, sizeof(m));
        input_mouse_get_pos(&m.x, &m.y);
        m.x += dx; m.y += dy;
        m.dx = dx; m.dy = dy;
        input_mouse_push(m);
        return;
    }
    /* the key axes: a press while pushed, release otherwise */
    KeyEvent ev; memset(&ev, 0, sizeof(ev));
    ev.scancode = (uint32_t)key;
    ev.keycode = (uint32_t)key;
    ev.kind = (value < 0) ? KEY_EVENT_DOWN : KEY_EVENT_DOWN; /* edge
               * handled by held-state below */
    /* simple: push a down event while deflected (the kernel treats
     * it as held; no edge tracking for the axes) */
    input_key_push(ev);
}

/* SI6: save the config. Returns 0 on success. */
int wubu_si_save(const char *path)
{
    if (!path) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t magic = SI_CFG_MAGIC, ver = SI_CFG_VERSION;
    fwrite(&magic, 4, 1, f);
    fwrite(&ver, 4, 1, f);
    fwrite(&g_si.cfg, sizeof(si_config_t), 1, f);
    fclose(f);
    return 0;
}

/* SI7: load a config. Returns 0 on success. */
int wubu_si_load(const char *path)
{
    if (!path) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint32_t magic, ver;
    if (fread(&magic, 4, 1, f) != 1 || fread(&ver, 4, 1, f) != 1 ||
        magic != SI_CFG_MAGIC || ver != SI_CFG_VERSION) {
        fclose(f);
        return -1;
    }
    si_config_t cfg;
    if (fread(&cfg, sizeof(si_config_t), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    g_si.cfg = cfg;
    return 0;
}

/* SI8: the test hooks */
typedef struct {
    int  initialized;
    int  button_a_scancode;
    int  axis_lx_neg;
    int  axis_lx_pos;
    int  mouse_speed;
} wubu_si_view_t;

int wubu_si_get(wubu_si_view_t *out)
{
    if (!out) return -1;
    out->initialized = g_si.initialized;
    out->button_a_scancode = g_si.cfg.buttons[SI_A].scancode;
    out->axis_lx_neg = g_si.cfg.axes[SI_LSTICK_X].neg_key;
    out->axis_lx_pos = g_si.cfg.axes[SI_LSTICK_X].pos_key;
    out->mouse_speed = g_si.cfg.mouse_speed;
    return 0;
}

/* ====================================================================
 * THE STEAM DECK REPORT PARSER — stolen from Valve's hid-steam.c
 * (drivers/hid/hid-steam.c, steam_do_deck_input_event). The Deck
 * sends a 64-byte controller state report (report ID 9) every ~4ms:
 *
 *   byte 8  : TR2 TL2 TR TL Y B X A        (bit 0 = TR2 ... bit 7 = A)
 *   byte 9  : dpad-up right left down | select mode start gripL2
 *   byte 10 : gripR2 lpad-click rpad-click lpad-touch rpad-touch
 *             unknown thumbL
 *   byte 11 : (bit 2) = thumbR click
 *   byte 13 : (bit 1) = gripL, (bit 2) = gripR
 *   byte 14 : (bit 2) = base (the Steam button)
 *   bytes 16/18 : left trackpad X/Y (when lpad-touched)
 *   bytes 20/22 : right trackpad X/Y (when rpad-touched)
 *   bytes 44/46 : left/right trigger (the ABS_HAT2Y/2X)
 *   bytes 48/50 : left stick X/Y (Y is negated)
 *   bytes 52/54 : right stick X/Y (Y is negated)
 *
 * The parser routes each decoded control through the SAME mapping
 * table the feed API uses (buttons -> the mapped scancode, sticks ->
 * the mapped key pairs), so a real Deck over USB/Bluetooth drives the
 * same virtual input as the synthetic feed.
 * ================================================================== */

/* the per-button decode mapping: [byte-index][bit] -> SI button id */
static const struct {
    int byte;   /* the report byte */
    int bit;    /* the bit within it */
    int btn;    /* the SI button id */
} si_deck_btns[] = {
    { 8,  0, SI_RT  },  { 8,  1, SI_LT  },  { 8,  2, SI_RB  },
    { 8,  3, SI_LB  },  { 8,  4, SI_Y   },  { 8,  5, SI_B   },
    { 8,  6, SI_X   },  { 8,  7, SI_A   },
    { 9,  0, SI_DPAD_UP },  { 9,  1, SI_DPAD_RIGHT },
    { 9,  2, SI_DPAD_LEFT }, { 9,  3, SI_DPAD_DOWN },
    { 9,  4, SI_BACK },  { 9,  5, SI_START },   /* mode = steam */
    { 10, 6, SI_L3   },  { 11, 2, SI_R3   },
    { 14, 2, SI_START },  /* the base/steam button acts as Start */
};

/* SI9: parse one 64-byte Deck controller report. Every decoded button
 * and axis flows through the mapping table -> the kernel input queue.
 * Returns the number of input events emitted. */
int wubu_si_parse_deck_report(const uint8_t *data, size_t size)
{
    if (!g_si.initialized || !data || size < 64)
        return -1;
    int emitted = 0;

    /* the buttons: compare against the held state, emit on edges */
    for (size_t i = 0; i < sizeof(si_deck_btns) / sizeof(si_deck_btns[0]); i++) {
        int byte = si_deck_btns[i].byte;
        int bit  = si_deck_btns[i].bit;
        int btn  = si_deck_btns[i].btn;
        int down = (data[byte] >> bit) & 1;
        if (down != g_si.held[btn]) {
            wubu_si_feed_button(btn, down);
            emitted++;
        }
    }

    /* the sticks (bytes 48/50 = L, 52/54 = R; Y is negated) */
    float lx = (int16_t)((data[48] | (data[49] << 8))) / 32767.0f;
    float ly = -(int16_t)((data[50] | (data[51] << 8))) / 32767.0f;
    float rx = (int16_t)((data[52] | (data[53] << 8))) / 32767.0f;
    float ry = -(int16_t)((data[54] | (data[55] << 8))) / 32767.0f;
    wubu_si_feed_axis(SI_LSTICK_X, lx);
    wubu_si_feed_axis(SI_LSTICK_Y, ly);
    wubu_si_feed_axis(SI_RSTICK_X, rx);
    wubu_si_feed_axis(SI_RSTICK_Y, ry);
    emitted += 4;

    /* the triggers (bytes 44/46) — the LI/Rt to SI_LT/SI_RT */
    int lt = data[44] | (data[45] << 8);
    int rt = data[46] | (data[47] << 8);
    if (lt > 8000)  { if (!g_si.held[SI_LT]) { wubu_si_feed_button(SI_LT, 1); emitted++; } }
    else            { if (g_si.held[SI_LT])  { wubu_si_feed_button(SI_LT, 0); emitted++; } }
    if (rt > 8000)  { if (!g_si.held[SI_RT]) { wubu_si_feed_button(SI_RT, 1); emitted++; } }
    else            { if (g_si.held[SI_RT])  { wubu_si_feed_button(SI_RT, 0); emitted++; } }

    return emitted;
}

/* ====================================================================
 * THE DECK SENSORS (IMU) + BATTERY — also stolen from hid-steam.c.
 *
 *   sensors (in the SAME 64-byte deck report):
 *     bytes 24/26/28 : accelerometer X/Z/Y
 *     bytes 30/32/34 : gyro X/Z/Y
 *     (the timestamp increments by 4ms per report — the deck sends
 *      every 4ms with no HID timestamp)
 *
 *   battery (the separate battery/status event, steam_do_battery_event):
 *     offset 12-13 : u16 voltage (mV)
 *     offset 14    : u8 battery percent
 * ================================================================== */

/* SI10: parse the IMU portion of a Deck report. The gyro Y axis is
 * turned into mouse deltas (the gyro-to-mouse feature SteamOS uses
 * for aim). Returns the number of mouse events emitted. */
int wubu_si_parse_deck_sensors(const uint8_t *data, size_t size)
{
    if (!g_si.initialized || !data || size < 64)
        return -1;
    /* the raw readings (the same layout as hid-steam.c) */
    int16_t accel_x = (int16_t)(data[24] | (data[25] << 8));
    int16_t accel_z = -(int16_t)(data[26] | (data[27] << 8));
    int16_t accel_y = (int16_t)(data[28] | (data[29] << 8));
    int16_t gyro_x  = (int16_t)(data[30] | (data[31] << 8));
    int16_t gyro_z  = -(int16_t)(data[32] | (data[33] << 8));
    int16_t gyro_y  = (int16_t)(data[34] | (data[35] << 8));

    /* the gyro-to-mouse: the yaw (gyro Z) drives the mouse X */
    int dx = (int)(gyro_z * g_si.cfg.mouse_speed / 512);
    int dy = (int)(gyro_x * g_si.cfg.mouse_speed / 512);
    if (dx == 0 && dy == 0) return 0;
    g_si.mouse_x += dx;
    g_si.mouse_y += dy;
    MouseEvent m;
    memset(&m, 0, sizeof(m));
    input_mouse_get_pos(&m.x, &m.y);
    m.x += dx; m.y += dy;
    m.dx = dx; m.dy = dy;
    input_mouse_push(m);
    return 1;
}

/* SI11: parse the battery event (the status report with the voltage
 * + percent). Returns the percent, or -1 on error. */
int wubu_si_parse_battery(const uint8_t *data, size_t size)
{
    if (!data || size < 15) return -1;
    int16_t volts = (int16_t)(data[12] | (data[13] << 8));
    int percent = data[14];
    if (percent > 100) percent = 100;
    /* stash the voltage in the module state for the tests */
    g_si.battery_mv = volts;
    g_si.battery_pct = percent;
    return percent;
}

/* the battery state (for the /n control plane) */
int wubu_si_battery_mv(void) { return g_si.battery_mv; }
int wubu_si_battery_pct(void) { return g_si.battery_pct; }
