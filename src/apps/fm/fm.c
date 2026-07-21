/*
 * fm.c  --  File Manager (9P/Styx Operations) - minimal stub
 */

#include "fm.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>
#include <string.h>

#define FM_MAX_ENTRIES 512

struct FileManagerState {
    FMEntry entries[FM_MAX_ENTRIES];
    int entry_count;
    char current_path[1024];
    int selected_idx;
    int scroll_offset;
    bool show_hidden;
    int fid;
    char styx_path[1024];
};

FileManagerState* fm_create(void) {
    return calloc(1, sizeof(FileManagerState));
}

void fm_destroy(FileManagerState *fm) {
    free(fm);
}

void fm_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, FileManagerState *fm) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)fm;
}

DosGuiWindow* fm_launch(void) {
    return dosgui_wm_create(80, 60, 700, 500, "File Manager");
}

void fm_scan_dir(FileManagerState *fm, const char *path) {
    fm->entry_count = 0;
    strncpy(fm->current_path, path, sizeof(fm->current_path) - 1);
    FMEntry mock[] = {
        {"..", true, 0, "2024-01-01", "drwxr-xr-x"},
        {"src", true, 0, "2024-06-28", "drwxr-xr-x"},
        {"apps", true, 0, "2024-06-28", "drwxr-xr-x"},
        {"kernel", true, 0, "2024-06-28", "drwxr-xr-x"},
        {"main.c", false, 1024, "2024-06-28", "-rw-r--r--"},
        {"Makefile", false, 4096, "2024-06-28", "-rw-r--r--"},
        {"README.md", false, 2048, "2024-06-28", "-rw-r--r--"},
    };
    int n = sizeof(mock)/sizeof(mock[0]);
    for (int i = 0; i < n; i++) fm->entries[fm->entry_count++] = mock[i];
}

void fm_navigate(FileManagerState *fm, const char *path) { fm_scan_dir(fm, path); }
void fm_refresh(FileManagerState *fm) { fm_scan_dir(fm, fm->current_path); }

int fm_open_fid(FileManagerState *fm, const char *path) { fm->fid = 1; strncpy(fm->styx_path, path, sizeof(fm->styx_path) - 1); return 1; }
int fm_read_fid(FileManagerState *fm, int fid, void *buf, uint32_t offset, uint32_t count) { (void)fm; (void)fid; (void)buf; (void)offset; (void)count; return 0; }
int fm_write_fid(FileManagerState *fm, int fid, const void *buf, uint32_t offset, uint32_t count) { (void)fm; (void)fid; (void)buf; (void)offset; (void)count; return 0; }
int fm_close_fid(FileManagerState *fm, int fid) { (void)fm; (void)fid; fm->fid = -1; return 0; }

int fm_entry_count(const FileManagerState *fm) {
    return fm ? fm->entry_count : 0;
}
const char *fm_entry_name(const FileManagerState *fm, int i) {
    return (fm && i >= 0 && i < fm->entry_count) ? fm->entries[i].name : "";
}
bool fm_entry_is_dir(const FileManagerState *fm, int i) {
    return (fm && i >= 0 && i < fm->entry_count) ? fm->entries[i].is_dir : false;
}
uint64_t fm_entry_size(const FileManagerState *fm, int i) {
    return (fm && i >= 0 && i < fm->entry_count) ? fm->entries[i].size : 0;
}
const char *fm_get_current_path(const FileManagerState *fm) {
    return fm ? fm->current_path : "";
}
int fm_get_selected_idx(const FileManagerState *fm) {
    return fm ? fm->selected_idx : -1;
}
void fm_set_selected_idx(FileManagerState *fm, int idx) {
    if (fm) fm->selected_idx = idx;
}