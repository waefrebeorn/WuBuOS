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

#define TM_MAX_PROCESSES 256

typedef struct {
    int pid;
    char name[64];
    double cpu_percent;
    uint64_t mem_bytes;
    char status[32];
    char user[32];
} TMProcess;

/* Task Manager state (fields exposed for tests / inspection) */
struct TaskManagerState {
    TMProcess processes[TM_MAX_PROCESSES];
    int process_count;
    int active_tab;
    int selected_pid;
    int scroll_offset;
    double cpu_history[60];
    double mem_history[60];
    int hist_idx;
    double total_cpu;
    uint64_t total_mem;
    uint64_t total_disk_read, total_disk_write, total_net_recv, total_net_send;
};

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