/*
 * todo.h  --  the Todo app (real, persisted).
 */
#ifndef WUBUOS_TODO_H
#define WUBUOS_TODO_H

#include <stdint.h>

/* the window type (forward) */
struct DosGuiWindow;

/* TD1: load the list from ~/.wubu/todo.txt. */
void todo_load(void);

/* TD3: the counts. */
int todo_count(void);
int todo_pending(void);

/* TD4: add a task (persisted). */
int todo_add(const char *text);

/* TD5: toggle the selected task (persisted). */
int todo_toggle_selected(void);

/* TD6: delete the selected task (persisted). */
int todo_delete_selected(void);

/* TD7: the item accessors. */
const char *todo_text(int idx);
int todo_done(int idx);
int todo_selected(void);

/* TD8: the test hooks. */
void todo_set_input(const char *text);
const char *todo_input(void);
void todo_test_reset(void);

/* the window bindings */
void todo_draw(struct DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void todo_key(struct DosGuiWindow *win, uint32_t key, uint32_t mods);
struct DosGuiWindow *todo_launch(void);

#endif
