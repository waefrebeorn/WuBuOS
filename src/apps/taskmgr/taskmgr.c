/*
 * taskmgr.c  --  Task Manager (Windows 11 Style) - minimal stub
 */

#include "taskmgr.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

TaskManagerState* taskmgr_create(void) {
    TaskManagerState *tm = calloc(1, sizeof(TaskManagerState));
    if (tm) {
        tm->total_cpu = 24.4;
        tm->total_mem = 1024*1024*1130;
    }
    return tm;
}

void taskmgr_destroy(TaskManagerState *tm) {
    free(tm);
}

void taskmgr_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, TaskManagerState *tm) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)tm;
}

DosGuiWindow* taskmgr_launch(void) {
    return dosgui_wm_create(80, 60, 800, 600, "Task Manager");
}

void taskmgr_refresh_processes(TaskManagerState *tm) {
    tm->process_count = 0;
    TMProcess mock[] = {
        {1, "systemd", 0.5, 1024*1024*50, "Running", "root"},
        {100, "wubu_hosted", 2.3, 1024*1024*200, "Running", "wubu"},
        {200, "dosgui_wm", 1.1, 1024*1024*80, "Running", "wubu"},
        {300, "browser", 15.0, 1024*1024*500, "Running", "wubu"},
        {400, "code", 5.5, 1024*1024*300, "Running", "wubu"},
    };
    int n = sizeof(mock)/sizeof(mock[0]);
    for (int i = 0; i < n; i++) tm->processes[tm->process_count++] = mock[i];
}

void taskmgr_kill_process(TaskManagerState *tm, int pid) {
    for (int i = 0; i < tm->process_count; i++) {
        if (tm->processes[i].pid == pid) {
            for (int j = i; j < tm->process_count - 1; j++) tm->processes[j] = tm->processes[j + 1];
            tm->process_count--;
            break;
        }
    }
}

void taskmgr_add_cpu_sample(TaskManagerState *tm, double cpu) {
    tm->cpu_history[tm->hist_idx % 60] = cpu;
    tm->hist_idx++;
}

void taskmgr_set_tab(TaskManagerState *tm, TMTab tab) {
    if (tab >= 0 && tab < 6) tm->active_tab = tab;
}