/*
 * repl.h  --  HolyC REPL Terminal
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_REPL_H
#define WUBU_REPL_H

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

typedef struct REPLState REPLState;

REPLState* repl_create(void);
void repl_destroy(REPLState *repl);

void repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, REPLState *repl);
DosGuiWindow* repl_launch(void);

void repl_add_line(REPLState *repl, const char *line);
void repl_input_char(REPLState *repl, char c);
void repl_input_backspace(REPLState *repl);
void repl_submit_line(REPLState *repl);

#endif