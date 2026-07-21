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

/* Opaque state */
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

/* Read-only inspection accessors (opaque-struct safe; for tests/debug). */
int np_tab_count(const NotepadState *np);
int np_active_tab(const NotepadState *np);

/* Text operations */
void notepad_insert_char(NotepadState *np, char c);
void notepad_delete_char(NotepadState *np);
void notepad_newline(NotepadState *np);

/* Language detection */
NPLang notepad_detect_lang(const char *filename);

#endif