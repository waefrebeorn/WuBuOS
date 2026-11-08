/*
 * editor.h  --  GUI adapter over the real WubuEditor engine
 *
 * The engine (../wubu_editor.h, 468 LOC + find/selection/undo/macro/bookmark
 * modules) is a pure text-editing core: it owns tabs, lines, the cursor,
 * undo, find/replace, folding and macros, but knows nothing about a
 * framebuffer.  This header is the window-facing face of that engine --
 * it binds the engine to a DosGuiWindow (draw / key / mouse) so the
 * desktop registry can launch a real editor instead of a stub.
 *
 * NOTE: the guard is WUBU_EDITOR_GUI_H, not WUBU_EDITOR_H -- the engine
 * header already owns that one, and a collision would silently skip one of
 * the two files in any TU that includes both.
 */

#ifndef WUBU_EDITOR_GUI_H
#define WUBU_EDITOR_GUI_H

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Opaque handle to the underlying WubuEditor. */
typedef struct EditorState EditorState;

EditorState *editor_create(void);
void         editor_destroy(EditorState *ed);

/* Draw the editor into a window's content area. */
void editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h,
                 EditorState *ed);

/* Input entry points (bound to the window's on_key / on_mouse slots). */
void editor_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void editor_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Create a window bound to a fresh editor instance. */
DosGuiWindow *editor_launch(void);

/* Return the engine handle bound to a window created by editor_launch()
 * (NULL otherwise).  Lets a caller inspect the live buffer -- a test, or a
 * debugger -- without needing the DosGuiWindow definition. */
void *editor_engine_of_window(DosGuiWindow *win);

#endif /* WUBU_EDITOR_GUI_H */
