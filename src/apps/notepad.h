/*
 * notepad.h  --  WuBuOS Notepad++ Style Editor
 *
 * Opaque struct + C11 only. No god headers.
 */

#ifndef WUBU_NOTEPAD_H
#define WUBU_NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct DosGuiWindow DosGuiWindow;
typedef struct NotepadState NotepadState;
typedef struct NotepadTab NotepadTab;

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

/* Public API */
NotepadState* notepad_state_get(void);
DosGuiWindow* notepad_launch(void);
DosGuiWindow* notepad_open(const char *filename);
void notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void notepad_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Inspection accessors for the live buffer (the state is module-static).
 * Lets a test drive notepad_key/mouse and assert what they did. */
void        notepad_reset(void);
int         notepad_line_count(void);
const char *notepad_line_text(int index);
int         notepad_cursor_line(void);
int         notepad_cursor_col(void);
bool        notepad_modified(void);

/* Tab management */
void np_new_tab(void);
void np_close_tab(int idx);
void np_switch_tab(int idx);
int np_get_active_tab(void);
int np_get_tab_count(void);

/* Text operations */
void np_insert_char(NotepadTab *tab, char c);
void np_delete_char(NotepadTab *tab);
void np_newline(NotepadTab *tab);

/* Language detection */
int np_detect_lang(const char *filename);
const char* np_lang_name(NPLang lang);

/* State accessors for testing */
int np_tab_count(NotepadState *state);
int np_active_tab(NotepadState *state);
NotepadTab* np_get_tab(NotepadState *state, int idx);

#endif /* WUBU_NOTEPAD_H */