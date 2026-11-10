/*
 * wubu_shell_greeter.h -- GAP F: the visible first-boot greeter (UX Stream E).
 *
 * The onboarding DECISION layer (src/runtime/wubu_onboarding.c) says whether
 * this boot should show a configurator or go straight to the desktop, but the
 * configurator itself did not exist -- "greeter" was launched and nothing was
 * drawn.  This module is that screen: a real modal dialog on the dosgui_*
 * stack (theme engine + VBE + WM input) that lets the user pick a theme and
 * then persists the onboarding sentinel, so the NEXT boot goes to the desktop.
 *
 * C11, minimal includes.  Renders through the public WM/VBE/theme APIs only.
 */
#ifndef WUBU_SHELL_GREETER_H
#define WUBU_SHELL_GREETER_H

#include <stdbool.h>
#include <stdint.h>

/* Dialog geometry (also used by the capture/test harness for hit-testing).
 * Distinct suffixes: the include guard is _H, so width/height use _WIDTH/_HEIGHT. */
#define WUBU_SHELL_GREETER_WIDTH  520
#define WUBU_SHELL_GREETER_HEIGHT 400
#define WUBU_SHELL_GREETER_ROW_H  30

/* Run the first-boot greeter end to end: open the dialog, drive it until the
 * user starts (or the dialog is closed), persist the onboarding sentinel.
 * Returns 1 if onboarding was completed, 0 if it was skipped/closed without
 * completing, -1 if no WM/theme was available. */
int  wubu_shell_greeter_run(void);

/* -- Harness/test surface (also used by the dialog's own callbacks) ------- */

/* Open the greeter dialog (no-op if already open). */
void wubu_shell_greeter_open(void);
/* True while the greeter dialog is on screen. */
bool wubu_shell_greeter_is_open(void);

/* Feed a key (WM key encoding: 30=up 31=down 28=left 29=right, '\r'=enter,
 * 27=escape, printable ASCII as itself). */
void wubu_shell_greeter_key(uint32_t key);
/* Feed a click at WINDOW-RELATIVE (x,y); kind 1=press 2=release. */
void wubu_shell_greeter_click(int x, int y, int kind);

/* Currently selected theme id (WubuThemeId). */
int  wubu_shell_greeter_selected_theme(void);
/* Window-relative rect of theme row i (0..THEME_COUNT-1). */
void wubu_shell_greeter_theme_row(int i, int *x, int *y, int *w, int *h);
/* Window-relative rect of the Start button. */
void wubu_shell_greeter_button_rect(int *x, int *y, int *w, int *h);

/* Window-relative rect of first-run offer i (0=Getting Started, 1=GUI
 * Designer, 2=Terminal).  These are the "where do I start" answers a new user
 * needs on the FIRST boot, when the desktop is otherwise silent. */
void wubu_shell_greeter_offer_rect(int i, int *x, int *y, int *w, int *h);
/* Act on first-run offer i. */
void wubu_shell_greeter_offer(int i);

/* The signature the greeter will persist on Start ("" until one is chosen). */
const char *wubu_shell_greeter_signature(void);

/* Draw the greeter dialog content into the WM (called by on_draw). */
void wubu_shell_greeter_draw(uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_SHELL_GREETER_H */
