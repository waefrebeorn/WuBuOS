/*
 * repl.c  --  HolyC REPL Terminal - minimal stub
 */

#include "repl.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>
#include <string.h>

#define REPL_MAX_LINES 500
#define REPL_LINE_LEN 256

struct REPLState {
    char lines[REPL_MAX_LINES][REPL_LINE_LEN];
    int line_count;
    char input[REPL_LINE_LEN];
    int input_pos;
};

REPLState* repl_create(void) {
    return calloc(1, sizeof(REPLState));
}

void repl_destroy(REPLState *repl) {
    free(repl);
}

void repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, REPLState *repl) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)repl;
}

DosGuiWindow* repl_launch(void) {
    return dosgui_wm_create(80, 60, 400, 400, "HolyC REPL");
}

void repl_add_line(REPLState *repl, const char *line) {
    if (repl->line_count < REPL_MAX_LINES) {
        strncpy(repl->lines[repl->line_count], line, REPL_LINE_LEN - 1);
        repl->line_count++;
    }
}

void repl_input_char(REPLState *repl, char c) { (void)repl; (void)c; }
void repl_input_backspace(REPLState *repl) { (void)repl; }
void repl_submit_line(REPLState *repl) { (void)repl; }