/*
 * notepad.c  --  Notepad++ Style Editor (minimal stub)
 */

#include "notepad.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>

#define NP_MAX_TABS 10
#define NP_MAX_LINES 5000
#define NP_MAX_LINE_LEN 1024

typedef struct {
    char lines[NP_MAX_LINES][NP_MAX_LINE_LEN];
    int line_count;
    int cursor_x, cursor_y;
    int scroll_y;
    int lang;
    char filename[256];
    bool modified;
} NotepadTab;

struct NotepadState {
    NotepadTab tabs[NP_MAX_TABS];
    int tab_count;
    int active_tab;
};

NotepadState* notepad_create(void) {
    NotepadState *np = calloc(1, sizeof(NotepadState));
    return np;
}

void notepad_destroy(NotepadState *np) {
    free(np);
}

void notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, NotepadState *np) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)np;
}

DosGuiWindow* notepad_launch(void) {
    return dosgui_wm_create(80, 60, 600, 400, "Notepad");
}

void notepad_new_tab(NotepadState *np) {
    if (np->tab_count >= NP_MAX_TABS) return;
    np->tabs[np->tab_count].line_count = 1;
    np->tab_count++;
    np->active_tab = np->tab_count - 1;
}

void notepad_close_tab(NotepadState *np, int idx) {
    if (np->tab_count <= 1) return;
    if (idx < 0 || idx >= np->tab_count) return;
    for (int i = idx; i < np->tab_count - 1; i++) np->tabs[i] = np->tabs[i + 1];
    np->tab_count--;
    if (np->active_tab >= np->tab_count) np->active_tab = np->tab_count - 1;
}

void notepad_switch_tab(NotepadState *np, int idx) {
    if (idx >= 0 && idx < np->tab_count) np->active_tab = idx;
}

void notepad_insert_char(NotepadState *np, char c) { (void)np; (void)c; }
void notepad_delete_char(NotepadState *np) { (void)np; }
void notepad_newline(NotepadState *np) { (void)np; }

NPLang notepad_detect_lang(const char *filename) {
    if (!filename) return 0;
    if (strcmp(filename, "Makefile") == 0) return 6;
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) return 1;
    if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".hpp") == 0) return 2;
    if (strcmp(ext, ".py") == 0) return 3;
    if (strcmp(ext, ".HC") == 0 || strcmp(ext, ".holyc") == 0) return 4;
    if (strcmp(ext, ".sh") == 0 || strcmp(ext, ".bash") == 0) return 5;
    if (strcmp(ext, ".json") == 0) return 7;
    if (strcmp(ext, ".xml") == 0 || strcmp(ext, ".html") == 0) return 8;
    return 0;
}