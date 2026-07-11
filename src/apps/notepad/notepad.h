/*
 * notepad.h  --  Notepad++ Style Editor (tabs, syntax highlighting, find/replace)
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_NOTEPAD_H
#define WUBU_NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Language types */
typedef enum {
    NP_LANG_NONE = 0,
    NP_LANG_C,
    NP_LANG_CPP,
    NP_LANG_PYTHON,
    NP_LANG_HOLYC,
    NP_LANG_SHELL,
    NP_LANG_MAKEFILE,
    NP_LANG_JSON,
    NP_LANG_XML,
    NP_LANG_COUNT
} NPLang;

/* Notepad sizing constants */
#define NP_MAX_TABS 10
#define NP_MAX_LINES 5000
#define NP_MAX_LINE_LEN 1024

/* A single editor tab */
typedef struct {
    char lines[NP_MAX_LINES][NP_MAX_LINE_LEN];
    int line_count;
    int cursor_x, cursor_y;
    int scroll_y;
    int lang;
    char filename[256];
    bool modified;
} NotepadTab;

/* Notepad state (fields exposed for tests / inspection) */
struct NotepadState {
    NotepadTab tabs[NP_MAX_TABS];
    int tab_count;
    int active_tab;
};

typedef struct NotepadState NotepadState;

/* API */
NotepadState* notepad_create(void);
void notepad_destroy(NotepadState *np);

void notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, NotepadState *np);
DosGuiWindow* notepad_launch(void);

/* Tab management */
void notepad_new_tab(NotepadState *np);
void notepad_close_tab(NotepadState *np, int idx);
void notepad_switch_tab(NotepadState *np, int idx);

/* Text operations */
void notepad_insert_char(NotepadState *np, char c);
void notepad_delete_char(NotepadState *np);
void notepad_newline(NotepadState *np);

/* Language detection */
NPLang notepad_detect_lang(const char *filename);

#endif