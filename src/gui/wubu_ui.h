/*
 * wubu_ui.h -- AGI UI automation layer for WuBuOS.
 *
 * The core idea (from the "AGI operating system" vision): a person should be
 * able to WATCH the system operate itself -- the cursor moves, windows open,
 * text is typed -- exactly as if a user were driving it. So this layer is a
 * thin facade over the SAME input path a real human uses
 * (dosgui_wm_handle_mouse / dosgui_wm_handle_key). There is no second,
 * privileged code path: synthetic input is indistinguishable from human input
 * to the rest of the system. Driving wubu_ui_* makes g_dwm.mouse_x/y move, so
 * the rendered cursor follows and every observer sees the AGI act.
 *
 * It also records a ring buffer of UI actions so a session can be replayed
 * ("watch it again") or inspected -- the substrate for demonstrable AGI control.
 *
 * Depends only on the public WM input API. No rendering, no window internals.
 */
#ifndef WUBU_UI_H
#define WUBU_UI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -- Mouse ---------------------------------------------------------- */

/* Move the cursor to (x,y). The WM's rendered cursor follows. */
void wubu_ui_mouse_move(int x, int y);

/* Press / release a mouse button (btn: 1=left 2=right 3=middle). */
void wubu_ui_mouse_down(int btn);
void wubu_ui_mouse_up(int btn);

/* Click at (x,y): move, press, release. */
void wubu_ui_click(int x, int y, int btn);

/* Drag from (x0,y0) to (x1,y1): move, press, interpolate, release. */
void wubu_ui_drag(int x0, int y0, int x1, int y1, int btn);

/* -- Keyboard ------------------------------------------------------- */

/* Inject a key event (same key/mods encoding the WM expects). */
void wubu_ui_key(uint32_t key, uint32_t mods);

/* Type a string: one wubu_ui_key() per character (ANSI printable). */
void wubu_ui_type(const char *text);

/* -- Record / replay ----------------------------------------------- */

typedef enum {
    WUBU_UI_MOVE,
    WUBU_UI_DOWN,
    WUBU_UI_UP,
    WUBU_UI_KEY
} WubuUiEventType;

typedef struct {
    WubuUiEventType type;
    int             x, y;     /* move/down/up */
    int             btn;      /* down/up */
    uint32_t        key;      /* key */
    uint32_t        mods;     /* key */
} WubuUiEvent;

/* Enable/disable recording of UI actions into the ring buffer. */
void wubu_ui_record(bool on);

/* Number of recorded events currently buffered. */
int  wubu_ui_recorded_count(void);

/* Replay the recorded buffer (drive the same actions again). */
void wubu_ui_replay(void);

/* Clear the recorded buffer. */
void wubu_ui_record_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_UI_H */
