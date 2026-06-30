/*
 * editor.c  --  Simple Editor Wrapper - minimal stub
 */

#include "editor.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

struct EditorState { int dummy; };

EditorState* editor_create(void) { return calloc(1, sizeof(EditorState)); }
void editor_destroy(EditorState *ed) { free(ed); }

void editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, EditorState *ed) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)ed;
}

DosGuiWindow* editor_launch(void) {
    return dosgui_wm_create(80, 60, 600, 500, "Editor");
}