/*
 * editor.h  --  Simple Editor Wrapper
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_EDITOR_H
#define WUBU_EDITOR_H

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

typedef struct EditorState EditorState;

EditorState* editor_create(void);
void editor_destroy(EditorState *ed);

void editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, EditorState *ed);
DosGuiWindow* editor_launch(void);

#endif