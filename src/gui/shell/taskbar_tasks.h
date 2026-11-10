/*
 * taskbar_tasks.h -- the taskbar model: app identity, pinning, running
 * state, persistence.
 *
 * The taskbar used to render one button per WINDOW, which meant an app with
 * three windows was three unrelated buttons, nothing could be pinned, and a
 * closed app left no trace.  This module is the missing model underneath it:
 *
 *   A TASK is an APP, not a window.  It has an identity key, a title, an
 *   optional launch command, a pinned flag, and zero or more running windows.
 *
 * Pins persist to ~/.config/wubuos/taskbar so a user's taskbar survives a
 * reboot -- the thing every mainstream OS has and WuBuOS did not.
 */
#ifndef DOSGUI_TASKBAR_TASKS_H
#define DOSGUI_TASKBAR_TASKS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WUBU_TASKBAR_MAX      32
#define WUBU_TASKBAR_KEY_LEN  64
#define WUBU_TASKBAR_TITLE_LEN 64
#define WUBU_TASKBAR_EXEC_LEN 160

typedef struct {
    char  key[WUBU_TASKBAR_KEY_LEN];     /* app identity ("editor")      */
    char  title[WUBU_TASKBAR_TITLE_LEN]; /* display title                */
    char  exec[WUBU_TASKBAR_EXEC_LEN];   /* launch command, "" if none   */
    bool  pinned;                        /* survives the app closing     */
    int   win_id;                        /* a running window, -1 if none */
} WubuTask;

/* Lifecycle.  init() loads the persisted pins. */
void wubu_taskbar_init(void);
void wubu_taskbar_shutdown(void);

/* Pin management.  Pinning is idempotent. */
int  wubu_taskbar_pin(const char *key, const char *title, const char *exec);
int  wubu_taskbar_unpin(const char *key);
bool wubu_taskbar_is_pinned(const char *key);

/* Running state.  A task can be pinned, running, both, or neither. */
void wubu_taskbar_set_running(const char *key, int win_id, const char *title);
void wubu_taskbar_clear_running(int win_id);

/* Queries. */
int                wubu_taskbar_count(void);
const WubuTask    *wubu_taskbar_get(int i);
const WubuTask    *wubu_taskbar_find(const char *key);
int                wubu_taskbar_running_count(void);
bool               wubu_taskbar_is_running(int win_id);

/* Persistence.  save() writes the pinned set; load() reads it. */
void wubu_taskbar_load(void);
void wubu_taskbar_save(void);

/* Where the pins live, for callers that show the path. */
const char *wubu_taskbar_config_path(void);

#ifdef __cplusplus
}
#endif

#endif /* DOSGUI_TASKBAR_TASKS_H */
