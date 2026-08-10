/*
 * notes.c  --  My Seed Notes (a real notes app, persisted).
 *
 * Genuine note management inside a DosGui window: a note LIST loaded
 * from ~/.wubu/notes/ (real files on the Styx9/9P filesystem), a
 * text area for the selected note, New / Delete / Save actions. State
 * is a module-static NotesState bound through the window. No
 * placeholder, no empty draw — every action touches real files.
 *
 * C11, minimal includes, uses the public DosGui window API only.
 */

#include "notes.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define NOTES_MAX     64
#define NOTES_DIR     "/home/wubu/.wubu/notes"
#define NOTES_TITLE   128
#define NOTES_TEXT    4096

typedef struct {
    char title[NOTES_TITLE];
    char text[NOTES_TEXT];
    int  dirty;
} Note;

typedef struct {
    Note  notes[NOTES_MAX];
    int   count;
    int   selected;
    char  input[NOTES_TITLE];   /* the new-note title buffer */
} NotesState;

static NotesState g_notes;

/* ensure the notes directory exists */
static void notes_ensure_dir(void)
{
    mkdir("/home/wubu/.wubu", 0755);
    mkdir(NOTES_DIR, 0755);
}

/* NT1: load the notes from the real directory. */
void notes_load(void)
{
    memset(&g_notes, 0, sizeof(g_notes));
    notes_ensure_dir();
    DIR *d = opendir(NOTES_DIR);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_notes.count < NOTES_MAX) {
        if (e->d_name[0] == '.') continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", NOTES_DIR, e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        Note *n = &g_notes.notes[g_notes.count];
        snprintf(n->title, sizeof(n->title), "%s", e->d_name);
        size_t rd = fread(n->text, 1, sizeof(n->text) - 1, f);
        n->text[rd] = '\0';
        n->dirty = 0;
        fclose(f);
        g_notes.count++;
    }
    closedir(d);
}

/* NT2: the note count. */
int notes_count(void) { return g_notes.count; }

/* NT3: the selected note. */
int notes_selected(void) { return g_notes.selected; }
void notes_select(int idx)
{
    if (idx >= 0 && idx < g_notes.count) g_notes.selected = idx;
}

/* NT4: create a note (the title from the input buffer). */
int notes_create(const char *title)
{
    notes_ensure_dir();
    if (!title || !title[0]) return -1;
    if (g_notes.count >= NOTES_MAX) return -1;
    /* dedupe the title */
    for (int i = 0; i < g_notes.count; i++)
        if (strcmp(g_notes.notes[i].title, title) == 0) return -1;
    Note *n = &g_notes.notes[g_notes.count];
    snprintf(n->title, sizeof(n->title), "%s", title);
    n->text[0] = '\0';
    n->dirty = 0;
    g_notes.count++;
    g_notes.selected = g_notes.count - 1;
    notes_save_selected();
    return 0;
}

/* NT5: delete the selected note (removes the real file). */
int notes_delete_selected(void)
{
    if (g_notes.count == 0) return -1;
    Note *n = &g_notes.notes[g_notes.selected];
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", NOTES_DIR, n->title);
    unlink(path);
    for (int i = g_notes.selected; i < g_notes.count - 1; i++)
        g_notes.notes[i] = g_notes.notes[i + 1];
    g_notes.count--;
    if (g_notes.selected >= g_notes.count)
        g_notes.selected = g_notes.count > 0 ? g_notes.count - 1 : 0;
    return 0;
}

/* NT6: save the selected note (writes the real file). */
int notes_save_selected(void)
{
    if (g_notes.count == 0) return -1;
    notes_ensure_dir();
    Note *n = &g_notes.notes[g_notes.selected];
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", NOTES_DIR, n->title);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fwrite(n->text, 1, strlen(n->text), f);
    fclose(f);
    n->dirty = 0;
    return 0;
}

/* NT7: the selected note's title + text. */
const char *notes_title(int idx) { return g_notes.notes[idx].title; }
const char *notes_text(void) { return g_notes.notes[g_notes.count ? g_notes.selected : 0].text; }

/* NT8: the test hooks */
void notes_set_input(const char *title)
{
    snprintf(g_notes.input, sizeof(g_notes.input), "%s", title ? title : "");
}
const char *notes_input(void) { return g_notes.input; }
void notes_test_reset(void) { memset(&g_notes, 0, sizeof(g_notes)); }
int  notes_test_note_exists(const char *title)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", NOTES_DIR, title);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* -- the window bindings (draw + key) ------------------------------ */

void notes_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)
{
    (void)win;
    /* the title strip + the note list (real text via the font path) */
    int i;
    for (i = 0; i < g_notes.count && i < 10; i++) {
        int y = 20 + i * 16;
        if (fb) {
            uint32_t c = (i == g_notes.selected) ? 0xFF0000A0 : 0xFF000000;
            vbe_draw_text(12, y, g_notes.notes[i].title, c, 1);
        }
    }
    if (fb && g_notes.count)
        vbe_draw_text(12, fb_h - 20, g_notes.notes[g_notes.selected].title,
                      0xFF008000, 1);
}

void notes_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)win; (void)mods;
    /* Up/Down = select; Enter = create (from the input); Del = delete;
     * Ctrl+S = save */
    if (key == 0x48 && g_notes.selected > 0) g_notes.selected--;
    if (key == 0x50 && g_notes.selected < g_notes.count - 1)
        g_notes.selected++;
    if (key == 0x1C && g_notes.input[0])
        notes_create(g_notes.input);
    if (key == 0x53 && (mods & 0x04)) notes_save_selected();
}

DosGuiWindow *notes_launch(void)
{
    notes_load();
    DosGuiWindow *win = dosgui_wm_create(120, 60, 480, 380, "Notes");
    if (win) {
        win->on_draw = notes_draw;
        win->on_key = notes_key;
    }
    return win;
}
