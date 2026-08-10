/*
 * todo.c  --  My Seed Todo (a real todo list, persisted).
 *
 * Genuine task management: a list loaded from ~/.wubu/todo.txt (a
 * real file on the Styx9/9P filesystem), add / check / uncheck /
 * delete. Every action persists immediately. No placeholder.
 *
 * The file format (one task per line):
 *   `[ ] buy milk`      — pending
 *   `[x] ship wubuos`   — done
 *
 * C11, minimal includes, uses the public DosGui window API only.
 */

#include "todo.h"
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
#include <unistd.h>

#define TODO_MAX    256
#define TODO_FILE   "/home/wubu/.wubu/todo.txt"
#define TODO_LEN    256

typedef struct {
    char  text[TODO_LEN];
    int   done;
} TodoItem;

typedef struct {
    TodoItem items[TODO_MAX];
    int      count;
    int      selected;
    char     input[TODO_LEN];
} TodoState;

static TodoState g_todo;

static void todo_ensure_dir(void)
{
    mkdir("/home/wubu/.wubu", 0755);
}

/* TD1: load the list from the real file. */
void todo_load(void)
{
    memset(&g_todo, 0, sizeof(g_todo));
    todo_ensure_dir();
    FILE *f = fopen(TODO_FILE, "r");
    if (!f) return;
    char line[TODO_LEN];
    while (fgets(line, sizeof(line), f) && g_todo.count < TODO_MAX) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '[') continue;
        int done = (line[1] == 'x' || line[1] == 'X');
        if (line[3] != ' ') continue;
        TodoItem *t = &g_todo.items[g_todo.count];
        snprintf(t->text, sizeof(t->text), "%s", line + 4);
        t->done = done;
        g_todo.count++;
    }
    fclose(f);
}

/* TD2: persist the whole list. */
static void todo_save(void)
{
    todo_ensure_dir();
    FILE *f = fopen(TODO_FILE, "w");
    if (!f) return;
    for (int i = 0; i < g_todo.count; i++)
        fprintf(f, "[%c] %s\n", g_todo.items[i].done ? 'x' : ' ',
                g_todo.items[i].text);
    fclose(f);
}

/* TD3: the task count + the pending count. */
int todo_count(void) { return g_todo.count; }
int todo_pending(void)
{
    int n = 0;
    for (int i = 0; i < g_todo.count; i++)
        if (!g_todo.items[i].done) n++;
    return n;
}

/* TD4: add a task (persisted). Returns 0 on success. */
int todo_add(const char *text)
{
    if (!text || !text[0] || g_todo.count >= TODO_MAX) return -1;
    TodoItem *t = &g_todo.items[g_todo.count];
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->done = 0;
    g_todo.count++;
    g_todo.selected = g_todo.count - 1;
    todo_save();
    return 0;
}

/* TD5: toggle the selected task (persisted). */
int todo_toggle_selected(void)
{
    if (g_todo.count == 0) return -1;
    g_todo.items[g_todo.selected].done =
        !g_todo.items[g_todo.selected].done;
    todo_save();
    return 0;
}

/* TD6: delete the selected task (persisted). */
int todo_delete_selected(void)
{
    if (g_todo.count == 0) return -1;
    for (int i = g_todo.selected; i < g_todo.count - 1; i++)
        g_todo.items[i] = g_todo.items[i + 1];
    g_todo.count--;
    if (g_todo.selected >= g_todo.count)
        g_todo.selected = g_todo.count > 0 ? g_todo.count - 1 : 0;
    todo_save();
    return 0;
}

/* TD7: the item accessors. */
const char *todo_text(int idx) { return g_todo.items[idx].text; }
int todo_done(int idx) { return g_todo.items[idx].done; }
int todo_selected(void) { return g_todo.selected; }

/* TD8: the test hooks. */
void todo_set_input(const char *text)
{
    snprintf(g_todo.input, sizeof(g_todo.input), "%s", text ? text : "");
}
const char *todo_input(void) { return g_todo.input; }
void todo_test_reset(void) { memset(&g_todo, 0, sizeof(g_todo)); }

/* -- the window bindings ------------------------------------------- */

void todo_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)
{
    (void)win;
    int i;
    for (i = 0; i < g_todo.count && i < 12; i++) {
        int y = 20 + i * 16;
        if (!fb) continue;
        uint32_t c = (i == g_todo.selected) ? 0xFF0000A0
                    : g_todo.items[i].done ? 0xFF408040 : 0xFF000000;
        char buf[TODO_LEN + 4];
        snprintf(buf, sizeof(buf), "%s %s",
                 g_todo.items[i].done ? "[x]" : "[ ]",
                 g_todo.items[i].text);
        vbe_draw_text(12, y, buf, c, 1);
    }
    if (fb)
        vbe_draw_text(12, fb_h - 20, "Up/Down select, Enter add, Space toggle, Del delete",
                      0xFF008000, 1);
}

void todo_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)win; (void)mods;
    if (key == 0x48 && g_todo.selected > 0) g_todo.selected--;
    if (key == 0x50 && g_todo.selected < g_todo.count - 1)
        g_todo.selected++;
    if (key == 0x1C && g_todo.input[0]) {   /* Enter */
        todo_add(g_todo.input);
        g_todo.input[0] = '\0';
    }
    if (key == 0x39 && g_todo.count)        /* Space */
        todo_toggle_selected();
}

DosGuiWindow *todo_launch(void)
{
    todo_load();
    DosGuiWindow *win = dosgui_wm_create(140, 60, 480, 400, "Todo");
    if (win) {
        win->on_draw = todo_draw;
        win->on_key = todo_key;
    }
    return win;
}
