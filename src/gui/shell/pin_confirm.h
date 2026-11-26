/*
 * pin_confirm.h -- "Pin <app> to the taskbar?" (PinningConfirmationDialog parity).
 *
 * The reference ships this because pinning used to be something that happened TO you. WuBuOS pinned
 * immediately and said so with a toast -- honest, but it is also how a taskbar fills with things nobody
 * chose. So: ask, ONCE, and let the user say "don't ask again" so the confirmation cannot become the
 * thing they hate more than the pin.
 *
 * Built on the shared ChoiceDialog: this dialog has no geometry, hit-test or key handling of its own.
 * C11.
 */
#ifndef PIN_CONFIRM_H
#define PIN_CONFIRM_H

#include <stdbool.h>
#include <stdint.h>

#include "choice_dialog.h"

#define PIN_CONFIRM_KEY_MAX   128
#define PIN_CONFIRM_TITLE_MAX 128
#define PIN_CONFIRM_EXEC_MAX  512

typedef struct {
    bool         open;
    char         key[PIN_CONFIRM_KEY_MAX];
    char         title[PIN_CONFIRM_TITLE_MAX];
    char         exec[PIN_CONFIRM_EXEC_MAX];
    ChoiceDialog card;
    int          answered;        /* 1 = pinned, 2 = cancelled, 0 = still asking */
    char         note[128];
} PinConfirmState;

/* The preference file: $WUBU_PIN_CONF, else ~/.config/wubu/pin.conf. */
const char *pin_confirm_conf_path(void);

/* False once the user ticked "Don't ask me again" (the caller then pins immediately). */
bool pin_confirm_should_ask(void);

/* Open the card. Returns false when asking is disabled or the surface cannot hold the card -- the caller
 * then proceeds as if the user had agreed, because a dialog that cannot be shown must not block a pin. */
bool pin_confirm_ask(const char *key, const char *title, const char *exec, int fb_w, int fb_h);

bool pin_confirm_is_open(void);
const PinConfirmState *pin_confirm_state(void);

/* Input, routed by wm_input while the card is up. click returns 1 when the pin was applied, 2 when
 * cancelled, 0 otherwise. */
int  pin_confirm_click(int x, int y, int fb_w, int fb_h);
bool pin_confirm_key(uint32_t key, int fb_w, int fb_h);
void pin_confirm_render(uint32_t *fb, int fb_w, int fb_h);

/* Test seam: forget the cached preference and the open state. */
void pin_confirm_reset(void);

#endif
