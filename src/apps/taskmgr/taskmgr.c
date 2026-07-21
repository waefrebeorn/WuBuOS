/*
 * taskmgr.c  --  Task Manager (Windows 11 Style) - minimal stub
 */

#include "taskmgr.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

#define TM_MAX_PROCESSES 256

typedef struct {
    int pid;
    char name[64];
    double cpu_percent;
    uint64_t mem_bytes;
    char status[32];
    char user[32];
} TMProcess;

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

static void taskmgr_draw_content(DosGuiWindow *win, const ChromeContentRect *content) {
    (void)win;
    if (!content) return;
    TaskManagerState *tm = (TaskManagerState*)win->user_data;
    if (!tm) return;

    int cx = content->x;
    int cy = content->y;
    int cw = content->w;
    int ch = content->h;

    /* Fill background */
    vbe_fill_rect(cx, cy, cw, ch, tc()->win_face);
    vbe_rect(cx, cy, cw, ch, tc()->border_dark);

    /* Tab bar area */
    int tab_h = 24;
    for (int t = 0; t < 6; t++) {
        int tx = cx + t * (cw / 6);
        int tw = cw / 6;
        if (t == tm->active_tab) {
            vbe_fill_rect(tx, cy, tw, tab_h, tc()->select_bg);
            vbe_draw_text(tx + 4, cy + 8, 
                t == 0 ? "Processes" : t == 1 ? "Performance" : t == 2 ? "App History" : 
                t == 3 ? "Startup" : t == 4 ? "Users" : "Details",
                tc()->select_text, 1);
        } else {
            vbe_fill_rect(tx, cy, tw, tab_h, tc()->btn_face);
            vbe_3d_raised(tx, cy, tw, tab_h, tc()->border_light, tc()->border_face,
                          tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(tx + 4, cy + 8,
                t == 0 ? "Processes" : t == 1 ? "Performance" : t == 2 ? "App History" : 
                t == 3 ? "Startup" : t == 4 ? "Users" : "Details",
                tc()->btn_text, 1);
        }
    }

    /* Content area below tabs */
    int content_y = cy + tab_h + 2;
    int content_h = ch - tab_h - 4;
    
    if (tm->active_tab == 0) {
        /* Processes tab - show process list */
        vbe_draw_text(cx + 4, content_y + 4, "PID  Name                 CPU%    Memory", 0x00000000, 1);
        for (int i = 0; i < tm->process_count && i < 15; i++) {
            char line[128];
            snprintf(line, sizeof(line), "%-4d %-20s %5.1f  %lu MB",
                     tm->processes[i].pid, tm->processes[i].name,
                     tm->processes[i].cpu_percent,
                     tm->processes[i].mem_bytes / (1024*1024));
            vbe_draw_text(cx + 4, content_y + 20 + i * 16, line, 
                         i == tm->selected_pid ? tc()->select_text : 0x00000000, 1);
        }
    } else if (tm->active_tab == 1) {
        /* Performance tab - CPU/Memory graphs */
        int graph_w = cw - 8;
        int graph_h = content_h - 12;
        int gx = cx + 4;
        int gy = content_y + 4;
        vbe_rect(gx, gy, graph_w, graph_h, tc()->border_dark);
        
        /* CPU history graph */
        int mid_y = gy + graph_h / 2;
        vbe_hline(gx, gx + graph_w, mid_y, tc()->border_dark);
        int mid_x = gx + graph_w / 2;
        vbe_vline(mid_x, gy, gy + graph_h, tc()->border_dark);
        
        uint32_t cpu_color = tc()->win_title_active;
        for (int px = 0; px < graph_w; px++) {
            int idx = (tm->hist_idx - graph_w + px + 60) % 60;
            double v = tm->cpu_history[idx];
            int py = mid_y - (int)(v * (graph_h/2) / 100.0);
            if (py >= gy && py < gy + graph_h)
                vbe_set_pixel(gx + px, py, cpu_color);
        }
        
        vbe_draw_text(gx + 4, gy + 4, "CPU Usage History (%)", 0x00000000, 1);
    } else {
        /* Other tabs placeholder */
        static const char *tab_names[] = {"Processes", "Performance", "App History", "Startup", "Users", "Details"};
        vbe_draw_text(cx + 4, content_y + 4, tab_names[tm->active_tab], 0x00666666, 1);
        vbe_draw_text(cx + 4, content_y + 24, "(tab content not implemented)", 0x00999999, 1);
    }
}

void taskmgr_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, TaskManagerState *tm) {
    (void)fb_w; (void)fb_h;
    if (!win || !fb) return;
    TaskManagerState *t = tm ? tm : (TaskManagerState*)win->user_data;
    if (!t) return;

    /* Draw chrome (frame + title bar + buttons) and get content rect. */
    ChromeContentRect content = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);

    /* Draw task manager content within chrome-provided rect. */
    taskmgr_draw_content(win, &content);
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

int taskmgr_get_active_tab(const TaskManagerState *tm) {
    return tm ? tm->active_tab : 0;
}
int taskmgr_process_count(const TaskManagerState *tm) {
    return tm ? tm->process_count : 0;
}
int taskmgr_process_pid(const TaskManagerState *tm, int i) {
    return (tm && i >= 0 && i < tm->process_count) ? tm->processes[i].pid : 0;
}
const char *taskmgr_process_name(const TaskManagerState *tm, int i) {
    return (tm && i >= 0 && i < tm->process_count) ? tm->processes[i].name : "";
}
double taskmgr_process_cpu(const TaskManagerState *tm, int i) {
    return (tm && i >= 0 && i < tm->process_count) ? tm->processes[i].cpu_percent : 0.0;
}
double taskmgr_total_cpu(const TaskManagerState *tm) {
    return tm ? tm->total_cpu : 0.0;
}
int taskmgr_hist_idx(const TaskManagerState *tm) {
    return tm ? tm->hist_idx : 0;
}