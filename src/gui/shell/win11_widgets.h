/*
 * win11_widgets.h -- Win+W widgets board (Windows 11 parity).
 *
 * A left-anchored panel of live cards: clock/date, weather, and real
 * machine stats from the thermal driver.  Win+W toggles it; a press
 * outside closes it.
 */

#ifndef WUBU_WIN11_WIDGETS_H
#define WUBU_WIN11_WIDGETS_H

#include <stdint.h>
#include <stdbool.h>

void win11_widgets_toggle(void);
bool win11_widgets_is_open(void);
void win11_widgets_close(void);

void win11_widgets_render(uint32_t *fb, int fb_w, int fb_h);

/* Returns true when the press was consumed by the board. */
bool win11_widgets_click(int x, int y);

#endif /* WUBU_WIN11_WIDGETS_H */
