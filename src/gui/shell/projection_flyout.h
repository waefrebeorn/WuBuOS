/*
 * projection_flyout.h -- Win+P: the display-projection selector.
 *
 * Windows shows four modes and GREYS the ones the hardware cannot do. This tree's rule is
 * that a menu ships only items that work, so the unavailable rows are present and DISABLED
 * rather than hidden (that is what Windows does too) -- and a disabled row cannot be picked,
 * so nothing here lies about what the machine can do.
 */

#ifndef WUBU_PROJECTION_FLYOUT_H
#define WUBU_PROJECTION_FLYOUT_H

#include <stdint.h>
#include <stdbool.h>

#define PROJECTION_MODE_PC_ONLY   0
#define PROJECTION_MODE_DUPLICATE 1
#define PROJECTION_MODE_EXTEND    2
#define PROJECTION_MODE_SECOND    3
#define PROJECTION_ROW_COUNT      4

void projection_flyout_toggle(void);
void projection_flyout_close(void);
bool projection_flyout_is_open(void);
void projection_flyout_render(uint32_t *fb, int fb_w, int fb_h);
bool projection_flyout_key(uint32_t key, uint32_t mods);   /* true when consumed */
bool projection_flyout_click(int x, int y);                /* true when consumed */

/* Test surface: the model behind the pixels. */
int  projection_row_count(void);
const char *projection_row_label(int i);
bool projection_row_enabled(int i);      /* false = no hardware for it, greyed */
bool projection_row_checked(int i);
int  projection_selected_row(void);
int  projection_mode(void);              /* the applied mode */
int  projection_rows_enabled_count(void);
/* Row i's rect: ONE owner for drawing, hit-testing and the test. */
bool projection_row_rect(int i, int fb_w, int fb_h, int *x, int *y, int *w, int *h);
/* The card's rect, same reason. */
bool projection_card_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);

#endif /* WUBU_PROJECTION_FLYOUT_H */
