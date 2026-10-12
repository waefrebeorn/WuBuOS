/*
 * wubu_bonzi_context.h -- the companion's AMBIENT CONTEXT (Track B1).
 *
 * "Context is the prompt": the companion sees the focused window so the
 * user never describes what they're looking at. This produces a structured
 * text context (focused-window rect + title + WM window inventory) the
 * Brain reads as the prompt prefix, plus a recent-focus ring for
 * continuity. Dependency-light: the caller feeds it WM state; the pixel
 * capture (wubu_screenshot) is the caller's job for a vision model.
 */
#ifndef WUBU_BONZI_CONTEXT_H
#define WUBU_BONZI_CONTEXT_H

/* Record the focused window (from dosgui_wm_get_focused()). */
void wubu_bonzi_context_set_focus(const char *title, int x, int y, int w, int h);

/* Record the WM window count (breadth of what's open). */
void wubu_bonzi_context_set_window_count(int n);

/* Format the ambient context into buf as the prompt prefix. Returns bytes
 * written (snprintf semantics). */
int wubu_bonzi_context_format(char *buf, int cap);

/* Copy up to max recent focused-window titles into out[][128]; returns the
 * count copied. */
int wubu_bonzi_context_recent(char out[][128], int max);

#endif /* WUBU_BONZI_CONTEXT_H */
