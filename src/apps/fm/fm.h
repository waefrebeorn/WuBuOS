/*
 * fm.h  --  File Manager (9P/Styx Operations)
 * Directory listing, navigation, 9P file operations
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_FM_H
#define WUBU_FM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

/* File entry */
typedef struct {
    char name[256];
    bool is_dir;
    uint64_t size;
    char mod_time[32];
    char perms[16];
} FMEntry;

/* File Manager state (fields exposed for tests / inspection) */
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

typedef struct FileManagerState FileManagerState;

/* API */
FileManagerState* fm_create(void);
void fm_destroy(FileManagerState *fm);

void fm_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, FileManagerState *fm);
DosGuiWindow* fm_launch(void);

/* Directory operations */
void fm_scan_dir(FileManagerState *fm, const char *path);
void fm_navigate(FileManagerState *fm, const char *path);
void fm_refresh(FileManagerState *fm);

/* File operations (9P/Styx) */
int fm_open_fid(FileManagerState *fm, const char *path);
int fm_read_fid(FileManagerState *fm, int fid, void *buf, uint32_t offset, uint32_t count);
int fm_write_fid(FileManagerState *fm, int fid, const void *buf, uint32_t offset, uint32_t count);
int fm_close_fid(FileManagerState *fm, int fid);

#endif