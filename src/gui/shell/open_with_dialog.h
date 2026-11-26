/*
 * open_with_dialog.h -- "How do you want to open this file?" (AppResolverUX parity).
 *
 * Windows has a resolver for the case the OS cannot decide: two apps claim a type, or the user wants a
 * different one, or the type is unknown. WuBuOS had the mechanics (wubu_mime: types, handlers,
 * associations, set_default) and no way for a person to use them -- the association existed and nothing
 * asked.
 *
 * This module is the asking. The candidate list is built from the mime system (the type's default
 * handler plus every desktop entry that declares the type), the choice is applied through
 * wubu_mime_associate/set_default when the user ticks "Always use this app", and the chosen handler id
 * comes back so the caller can launch it.
 *
 * C11.
 */
#ifndef WUBU_OPEN_WITH_DIALOG_H
#define WUBU_OPEN_WITH_DIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OPEN_WITH_MAX_APPS 8
#define OPEN_WITH_PATH_MAX 512

typedef struct {
    char id[128];
    char name[128];
} OpenWithApp;

typedef struct {
    bool        open;
    char        path[OPEN_WITH_PATH_MAX];
    char        ext[32];
    char        mime[128];
    OpenWithApp apps[OPEN_WITH_MAX_APPS];
    int         count;
    int         selected;
    bool        remember;          /* "Always use this app" */
} OpenWithState;

/* Build candidates for a path. Returns the count (0 when the mime system knows nothing usable). */
int open_with_candidates(const char *path, OpenWithApp *out, int cap);

/* Open the dialog for a path. Returns false when there is nothing to choose from. */
bool open_with_open(const char *path);
void open_with_close(void);
bool open_with_is_open(void);   /* for input routing: no struct knowledge needed */
const OpenWithState *open_with_state(void);

/* Move the selection by delta (wraps). Returns the newly selected id, or NULL. */
const char *open_with_move(int delta);
void open_with_toggle_remember(void);

/* Accept the selection: applies the association when `remember` is set, closes the dialog, and returns
 * the handler id the caller should launch (NULL when nothing was chosen). */
const char *open_with_accept(void);

/* Launch the handler accepted last (after open_with_click returned 1, or open_with_accept). Kept
 * separate so the module owns PATH+ID together and the caller never reassembles them. */
int open_with_launch_last(void);

/* Geometry, hit-test and render. The dialog is a centred card; hit-test and render share the rects. */
bool open_with_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);
/* The rect of candidate row i. Published so a gate can click exactly what the renderer drew -- a test
 * that recomputes the geometry itself passes while drawing and hit-testing drift apart. */
bool open_with_row_rect(int fb_w, int fb_h, int i, int *x, int *y, int *w, int *h);
/* -1 = outside, -2 = the Remember checkbox, -3 = Cancel, -4 = Open, >= 0 = an app row. */
int  open_with_hit(int mx, int my, int fb_w, int fb_h);
/* Click: performs the action for that point and returns what happened
 * (0 nothing, 1 chosen-and-closed, 2 cancelled). */
int  open_with_click(int mx, int my, int fb_w, int fb_h);
void open_with_render(uint32_t *fb, int fb_w, int fb_h);

#endif
