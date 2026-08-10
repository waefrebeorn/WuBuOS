/*
 * notes.h  --  the Notes app (real, persisted).
 */
#include <stdint.h>

/* the window type (forward) */
struct DosGuiWindow;

#ifndef WUBUOS_NOTES_H
#define WUBUOS_NOTES_H

/* NT1: load the notes from ~/.wubu/notes/. */
void notes_load(void);

/* NT2: the note count. */
int notes_count(void);

/* NT3: the selection. */
int notes_selected(void);
void notes_select(int idx);

/* NT4: create a note (persisted). Returns 0 on success. */
int notes_create(const char *title);

/* NT5: delete the selected note (removes the real file). */
int notes_delete_selected(void);

/* NT6: save the selected note (writes the real file). */
int notes_save_selected(void);

/* NT7: the selected note's title + text. */
const char *notes_title(int idx);
const char *notes_text(void);

/* NT8: the test hooks. */
void notes_set_input(const char *title);
const char *notes_input(void);
void notes_test_reset(void);
int  notes_test_note_exists(const char *title);

/* the window bindings */
void notes_draw(struct DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void notes_key(struct DosGuiWindow *win, uint32_t key, uint32_t mods);
struct DosGuiWindow *notes_launch(void);

#endif
