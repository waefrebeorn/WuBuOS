/*
 * choice_dialog.h -- the one modal card in the shell: a titled list of rows with optional checks.
 *
 * Why this exists: the Open-With resolver was the first modal card, and the next three dialogs
 * (add suggested folders, rename confirm, delete confirm) would each have hand-rolled the same box,
 * the same row pitch, the same hit-test and the same key handling. Explorer's address bar once hit-
 * tested 28 px above where it drew; a second card is a second chance to make that mistake. So the card
 * lives here, ONCE, and a dialog is a model plus a call to this.
 *
 * Every rect is a pure function of the surface size: renderer, hit-test and gates all call
 * cd_rect()/cd_row_rect(). Nothing queries the framebuffer behind the caller's back -- a card that was
 * opened on one surface and clicked on another must not silently work.
 *
 * C11.
 */
#ifndef CHOICE_DIALOG_H
#define CHOICE_DIALOG_H

#include <stdbool.h>
#include <stdint.h>

#define CD_MAX_ROWS      12
#define CD_LABEL_MAX     128
#define CD_DETAIL_MAX    128
#define CD_TITLE_MAX     96
#define CD_SUB_MAX       160
#define CD_BUTTON_MAX    24
#define CD_FOOTER_MAX    96

typedef struct {
    char label[CD_LABEL_MAX];
    char detail[CD_DETAIL_MAX];   /* second line; "" for none */
    bool checked;                 /* multi-select state, or the ticked row of a single-select list */
    bool disabled;                /* shown, not selectable: a candidate that cannot be used */
} ChoiceRow;

typedef struct {
    bool  open;
    bool  multi;                  /* checkboxes (true) or a single highlighted row (false) */
    int   selected;               /* single-select highlight / keyboard cursor in both modes */
    int   count;
    ChoiceRow rows[CD_MAX_ROWS];
    char  title[CD_TITLE_MAX];
    char  subtitle[CD_SUB_MAX];
    char  ok_label[CD_BUTTON_MAX];
    char  cancel_label[CD_BUTTON_MAX];
    char  footer[CD_FOOTER_MAX];  /* optional "always do this" tick */
    bool  footer_checked;
    int   surface_w, surface_h;   /* the surface it was opened on */
} ChoiceDialog;

/* Hit codes. */
#define CD_HIT_NONE    (-1)
#define CD_HIT_CANCEL  (-2)
#define CD_HIT_OK      (-3)
#define CD_HIT_FOOTER  (-4)

void cd_open(ChoiceDialog *d, const char *title, const char *subtitle, bool multi,
             int surface_w, int surface_h);
int  cd_add(ChoiceDialog *d, const char *label, const char *detail, bool checked);
void cd_close(ChoiceDialog *d);

/* The card and the row, as pure functions of the surface the dialog was opened on. */
bool cd_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h);
bool cd_row_rect(const ChoiceDialog *d, int i, int *x, int *y, int *w, int *h);
bool cd_ok_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h);
bool cd_cancel_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h);
bool cd_footer_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h);

/* What a click lands on. A click outside the card returns CD_HIT_NONE and does NOT cancel:
 * dismissing a modal by a stray press is how a user loses work. */
int  cd_hit(ChoiceDialog *d, int x, int y);

/* Keyboard: Up/Down move, Space ticks, Enter accepts, Esc cancels. Returns true when it consumed the
 * key (so the shell does not act on it behind the card). */
bool cd_key(ChoiceDialog *d, uint32_t key);

void cd_render(const ChoiceDialog *d, uint32_t *fb, int fb_w, int fb_h);

#endif
