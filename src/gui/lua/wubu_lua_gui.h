/*
 * wubu_lua_gui.h -- Lua-driven GUI for WuBuOS.
 *
 * The GUI layer is Lua; the substrate is C11.  A script owns one slerm-Lua
 * state and draws through a narrow binding table (`gui.*`) onto the WuBuOS
 * VBE framebuffer -- the same surface the Win98 shell composites.  Scripts
 * never see a kernel structure, a driver handle or a raw framebuffer
 * pointer; they call gui.rect() and the C11 host does the work.
 *
 * The boundary is deliberately small:
 *
 *   Lua owns: layout, labels, reactive behaviour, iteration.
 *   C11 owns: the framebuffer, input routing, the filesystem, memory.
 *
 * Scripts come from the normal filesystem, so a Lua GUI is a file like any
 * other -- editable, hot-reloadable, and reachable over 9P.
 */

#ifndef WUBU_LUA_GUI_H
#define WUBU_LUA_GUI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct WubuLuaGui WubuLuaGui;

/* Create a GUI state for a window of the given framebuffer size.
 * Returns NULL on allocation failure or when the Lua state cannot start. */
WubuLuaGui *wubu_lua_gui_new(int fb_w, int fb_h);

void wubu_lua_gui_free(WubuLuaGui *g);

/* Load and run a script.  Returns 0 on success; on failure the error text
 * is written to `err` (up to err_cap bytes). */
int  wubu_lua_gui_load(WubuLuaGui *g, const char *path, char *err, size_t err_cap);

/* True when the script defined a draw() function. */
bool wubu_lua_gui_has_draw(const WubuLuaGui *g);

/* Ask the script to draw a frame.  The script draws through gui.* onto the
 * WuBuOS VBE surface, exactly like every other GUI module, so the WM
 * composites it with everything else.  `fb_w`/`fb_h` are the window's
 * content size, exposed to the script as gui.width()/gui.height().
 * Returns 0 on success. */
int  wubu_lua_gui_draw(WubuLuaGui *g, int fb_w, int fb_h);

/* Deliver a key to the script's on_key(key) handler, if present.
 * `key` is a printable character or one of the WB_LUA_KEY_* names below. */
int  wubu_lua_gui_key(WubuLuaGui *g, const char *key, uint32_t mods);

/* Name of the last error, or NULL.  Valid until the next call. */
const char *wubu_lua_gui_error(const WubuLuaGui *g);

/* Key names delivered to on_key(). */
#define WB_LUA_KEY_ENTER   "enter"
#define WB_LUA_KEY_ESC     "esc"
#define WB_LUA_KEY_BACKSPACE "backspace"
#define WB_LUA_KEY_TAB     "tab"
#define WB_LUA_KEY_UP      "up"
#define WB_LUA_KEY_DOWN    "down"
#define WB_LUA_KEY_LEFT    "left"
#define WB_LUA_KEY_RIGHT   "right"

#endif /* WUBU_LUA_GUI_H */
