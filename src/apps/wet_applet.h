/*
 * wet_applet.h -- launch a .wet applet as a WuBuOS window.
 *
 * A .wet applet is a Lua script; this module is the bridge that puts one in a
 * DosGuiWindow.  The framework owns the tree and the layout, the window owns
 * the chrome, and this file only routes: draw into the content rect, forward
 * keys and pointer events, reflow when the window resizes.
 */
#ifndef WET_APPLET_H
#define WET_APPLET_H

#include "../gui/wm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open a .wet applet.  `path` may be NULL, which opens the built-in demo
 * applet -- the shell menu item needs to work without a file argument. */
DosGuiWindow *launch_wet_applet(const char *path);

/* Load a .wet script into an existing applet window (used by the test). */
DosGuiWindow *wet_applet_open(const char *path, const char *title,
                              int x, int y, int w, int h);

/* The routing callbacks, exposed so a test can drive the window. */
void wet_applet_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void wet_applet_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void wet_applet_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Inspection, for tests: is an applet loaded, and which one. */
int     wet_applet_ok(void);
struct WetApp;
struct WetApp *wet_applet_app(void);

#ifdef __cplusplus
}
#endif

#endif /* WET_APPLET_H */
