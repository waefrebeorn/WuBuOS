/*
 * taskmgr.h  --  Task Manager (Windows 11 Style)
 * Processes, Performance, Details, Services, Startup, Users tabs
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_TASKMGR_H
#define WUBU_TASKMGR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Tabs */
typedef enum {
    TM_TAB_PROCESSES = 0,
    TM_TAB_PERFORMANCE,
    TM_TAB_DETAILS,
    TM_TAB_SERVICES,
    TM_TAB_STARTUP,
    TM_TAB_USERS,
    TM_TAB_COUNT
} TMTab;

/* Opaque state */
typedef struct TaskManagerState TaskManagerState;

/* API */
TaskManagerState* taskmgr_create(void);
void taskmgr_destroy(TaskManagerState *tm);

void taskmgr_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, TaskManagerState *tm);
DosGuiWindow* taskmgr_launch(void);

/* Process management */
void taskmgr_refresh_processes(TaskManagerState *tm);
void taskmgr_kill_process(TaskManagerState *tm, int pid);
void taskmgr_add_cpu_sample(TaskManagerState *tm, double cpu);

/* Tab switching */
void taskmgr_set_tab(TaskManagerState *tm, TMTab tab);

#endif