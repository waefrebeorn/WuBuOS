/*
 * dosgui_explorer.h  --  WuBuOS File Manager (Win98/XP Explorer Shell)
 *
 * Phase 5: Full-featured file manager with:
 *   - Tree view sidebar (folder hierarchy, expand/collapse)
 *   - Breadcrumbs address bar (clickable path segments)
 *   - File list view (Details, Icons, List, Tiles)
 *   - Preview pane (text, images, metadata)
 *   - File operations (Copy, Move, Delete, Rename, New Folder)
 *   - Context menus (Open, Properties, Cut, Copy, Paste, Delete, New)
 *   - Zip archive mount/browse (via libzip integration)
 *   - Multi-select (Shift/Ctrl click, rubber band)
 *   - Keyboard shortcuts (F2 rename, Delete, Ctrl+C/V/X, Enter open)
 *   - Column sorting (Name, Size, Type, Date Modified)
 *   - Drive/Volume list (from Styx/9P namespace)
 */

#ifndef WUBU_DOSGUI_EXPLORER_H
#define WUBU_DOSGUI_EXPLORER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- Limits ------------------------------------------------------- */

#define EX_MAX_PATH           4096
#define EX_MAX_ENTRIES        4096
#define EX_MAX_TREE_NODES     512
#define EX_MAX_COLUMNS        8
#define EX_MAX_SELECTION      256
#define EX_MAX_ZIP_ENTRIES    2048
#define EX_COLUMN_NAME_W      220
#define EX_COLUMN_SIZE_W      80
#define EX_COLUMN_TYPE_W      120
#define EX_COLUMN_DATE_W      140
#define EX_ICON_SIZE          32
#define EX_SMALL_ICON         16
#define EX_ROW_H              22
#define EX_TREE_INDENT        16
#define EX_PREVIEW_W          280
#define EX_BREADCRUMB_H       28
#define EX_TOOLBAR_H          36
#define EX_STATUSBAR_H        22

/* Include for pid_t */
#include <sys/types.h>

/* -- View Modes --------------------------------------------------- */

typedef enum {
    EX_VIEW_DETAILS   = 0,  /* Columns: Name, Size, Type, Date */
    EX_VIEW_ICONS     = 1,  /* Large icons, grid layout */
    EX_VIEW_LIST      = 2,  /* Small icons, single column */
    EX_VIEW_TILES     = 3,  /* Medium icons with metadata below */
} ExViewMode;

/* -- Sort Columns ------------------------------------------------- */

typedef enum {
    EX_SORT_NAME      = 0,
    EX_SORT_SIZE      = 1,
    EX_SORT_TYPE      = 2,
    EX_SORT_DATE      = 3,
    EX_SORT_COUNT     = 4,
} ExSortColumn;

/* -- File Entry Types -------------------------------------------- */

typedef enum {
    EX_ENTRY_UNKNOWN  = 0,
    EX_ENTRY_FILE     = 1,
    EX_ENTRY_DIR      = 2,
    EX_ENTRY_SYMLINK  = 3,
    EX_ENTRY_DRIVE    = 4,     /* Root volume/mount point */
    EX_ENTRY_ZIP      = 5,     /* Zip archive (mountable) */
    EX_ENTRY_MOUNT    = 6,     /* Mounted network/device */
    EX_ENTRY_SPECIAL  = 7,     /* /dev, /proc, /sys, etc. */
} ExEntryType;

/* -- Tree Node (Sidebar Folder Hierarchy) ------------------------ */

typedef struct ExTreeNode ExTreeNode;

struct ExTreeNode {
    char            path[EX_MAX_PATH];
    char            display_name[256];
    ExTreeNode     *parent;
    ExTreeNode     *children;
    ExTreeNode     *next_sibling;
    ExTreeNode     *first_child;
    int             child_count;
    bool            expanded;
    bool            is_drive;        /* Root volume */
    bool            is_zip;          /* Zip archive root */
    bool            scanned;         /* Children populated */
    uint32_t        icon_color;      /* Fallback color */
};

/* -- File Entry (Main List View) --------------------------------- */

typedef struct {
    char            name[256];
    char            full_path[EX_MAX_PATH];
    ExEntryType     type;
    uint64_t        size;            /* Bytes (for files) */
    time_t          modified;        /* Last modified time */
    time_t          created;         /* Creation time */
    char            type_str[64];    /* "Text Document", "Folder", etc. */
    char            extension[16];   /* Lowercase, no dot */
    bool            hidden;
    bool            readonly;
    bool            is_selected;
    uint32_t        icon_color;      /* Fallback color */
    /* For zip entries */
    struct {
        int         zip_index;       /* Index in zip central directory */
        char        zip_path[EX_MAX_PATH]; /* Path to zip file */
    } zip_info;
} ExEntry;

/* -- Column Definition ------------------------------------------- */

typedef struct {
    char            name[32];
    int             width;
    int             min_width;
    ExSortColumn    sort_id;
    bool            visible;
    bool            resizable;
} ExColumn;

/* -- Preview Pane State ------------------------------------------ */

typedef enum {
    EX_PREVIEW_NONE      = 0,
    EX_PREVIEW_TEXT      = 1,
    EX_PREVIEW_IMAGE     = 2,
    EX_PREVIEW_METADATA  = 3,
    EX_PREVIEW_BINARY    = 4,
} ExPreviewType;

typedef struct {
    ExPreviewType   type;
    char            path[EX_MAX_PATH];
    char            text_buffer[8192];    /* For text preview */
    int             text_lines;
    /* For image preview (stub - would use stb_image or similar) */
    int             img_w, img_h;
    uint32_t       *img_pixels;
    /* Metadata */
    uint64_t        file_size;
    time_t          modified;
    char            mime_type[64];
} ExPreviewState;

/* -- Selection Rubber Band --------------------------------------- */

typedef struct {
    bool    active;
    int     start_x, start_y;
    int     end_x, end_y;
} ExSelectionBand;

/* -- File Operation State ---------------------------------------- */

typedef enum {
    EX_OP_NONE    = 0,
    EX_OP_COPY    = 1,
    EX_OP_MOVE    = 2,
    EX_OP_DELETE  = 3,
} ExOpType;

typedef struct {
    ExOpType        type;
    int             count;
    char            paths[EX_MAX_SELECTION][EX_MAX_PATH];
    char            dest_path[EX_MAX_PATH];
    int             current_idx;
    uint64_t        total_bytes;
    uint64_t        copied_bytes;
    bool            in_progress;
    bool            error;
    char            error_msg[256];
    /* For async operation */
    pid_t           worker_pid;
    int             pipe_fd[2];
} ExFileOperation;

/* -- Main Explorer State ----------------------------------------- */

typedef struct {
    /* Current directory */
    char                current_path[EX_MAX_PATH];
    char                previous_path[EX_MAX_PATH];  /* For Back button */
    char                next_path[EX_MAX_PATH];      /* For Forward button */
    int                 history_pos;                 /* Browser-style history */
    char                history[32][EX_MAX_PATH];

    /* Tree view (sidebar) */
    ExTreeNode         *tree_root;
    ExTreeNode         *tree_selected;
    int                 tree_scroll_y;
    int                 tree_w;                      /* Sidebar width */

    /* Breadcrumbs */
    char                breadcrumb_segments[32][256];
    int                 breadcrumb_count;
    int                 breadcrumb_scroll_x;

    /* File list */
    ExEntry             entries[EX_MAX_ENTRIES];
    int                 entry_count;
    int                 list_scroll_y;
    int                 selected_indices[EX_MAX_SELECTION];
    int                 selection_count;
    int                 focus_idx;                   /* Keyboard focus (single) */
    int                 anchor_idx;                  /* Shift-select anchor */
    ExViewMode          view_mode;
    ExSortColumn        sort_column;
    bool                sort_ascending;
    ExColumn            columns[EX_MAX_COLUMNS];
    int                 column_count;

    /* Preview pane */
    ExPreviewState      preview;
    bool                preview_visible;
    int                 preview_w;

    /* UI State */
    ExSelectionBand     rubber_band;
    bool                show_hidden;
    bool                show_extensions;
    bool                single_click_open;

    /* Toolbar */
    bool                toolbar_visible;
    int                 toolbar_mode;    /* 0=standard, 1=compact */

    /* Status bar */
    char                status_text[256];
    int                 status_file_count;
    uint64_t            status_total_size;

    /* File operations */
    ExFileOperation     file_op;

    /* Context menu */
    int                 context_menu_x, context_menu_y;
    int                 context_menu_entry;  /* -1 = background */

    /* Clipboard */
    char                clipboard_paths[EX_MAX_SELECTION][EX_MAX_PATH];
    int                 clipboard_count;
    ExOpType            clipboard_op;    /* COPY or MOVE (for cut) */

    /* Zip archive state */
    char                current_zip_path[EX_MAX_PATH];
    bool                in_zip_archive;

    /* Window ID (for WM integration) */
    int                 win_id;
    
    /* Focus pane for Tab navigation: 0=tree, 1=breadcrumbs, 2=list, 3=preview */
    int                 focus_pane;
} ExExplorerState;

/* -- Global Instance ---------------------------------------------- */

extern ExExplorerState g_explorer;

/* -- Public API --------------------------------------------------- */

/* Lifecycle */
int  dosgui_explorer_init(void);
void dosgui_explorer_shutdown(void);

/* Window management */
void dosgui_explorer_show(void);
void dosgui_explorer_hide(void);
bool dosgui_explorer_is_open(void);
void dosgui_explorer_toggle(void);

/* Navigation */
void dosgui_explorer_navigate(const char *path);
void dosgui_explorer_go_up(void);
void dosgui_explorer_go_back(void);
void dosgui_explorer_go_forward(void);
void dosgui_explorer_refresh(void);

/* Tree view */
void dosgui_explorer_tree_expand(ExTreeNode *node);
void dosgui_explorer_tree_collapse(ExTreeNode *node);
void dosgui_explorer_tree_select(ExTreeNode *node);
ExTreeNode *dosgui_explorer_tree_find(const char *path);
void dosgui_explorer_tree_scan(ExTreeNode *node);

/* Selection */
void dosgui_explorer_select_all(void);
void dosgui_explorer_clear_selection(void);
void dosgui_explorer_toggle_selection(int idx);
void dosgui_explorer_select_range(int start, int end);
bool dosgui_explorer_is_selected(int idx);

/* View */
void dosgui_explorer_set_view_mode(ExViewMode mode);
void dosgui_explorer_set_sort(ExSortColumn col, bool ascending);
void dosgui_explorer_toggle_hidden(void);
void dosgui_explorer_toggle_extensions(void);
void dosgui_explorer_toggle_preview(void);
void dosgui_explorer_toggle_toolbar(void);

/* File Operations */
void dosgui_explorer_copy(void);
void dosgui_explorer_cut(void);
void dosgui_explorer_paste(void);
void dosgui_explorer_delete(bool permanent);
void dosgui_explorer_rename(int idx);
void dosgui_explorer_new_folder(void);
void dosgui_explorer_new_file(const char *template_name);

/* Zip Archives */
bool dosgui_explorer_mount_zip(const char *zip_path);
void dosgui_explorer_unmount_zip(void);
bool dosgui_explorer_is_in_zip(void);

/* Properties */
void dosgui_explorer_show_properties(int idx);

/* Input handling */
void dosgui_explorer_handle_key(uint32_t key, uint32_t mods);
void dosgui_explorer_handle_mouse(int x, int y, int btn, int kind);

/* Rendering (called from WM) */
void dosgui_explorer_render(uint32_t *fb, int fb_w, int fb_h);

/* State accessors */
ExExplorerState *dosgui_explorer_state(void);
const char *dosgui_explorer_current_path(void);

/* Helpers */
const char *dosgui_explorer_type_str(ExEntryType type);
const char *dosgui_explorer_view_mode_name(ExViewMode mode);
uint32_t dosgui_explorer_type_color(ExEntryType type);
void dosgui_explorer_format_size(uint64_t bytes, char *buf, int buf_size);
void dosgui_explorer_format_time(time_t t, char *buf, int buf_size);

/* Drive/Volume enumeration (from Styx/9P) */
int  dosgui_explorer_enumerate_drives(char paths[][EX_MAX_PATH], char labels[][64], int max);
void dosgui_explorer_update_drive_list(void);

#endif /* WUBU_DOSGUI_EXPLORER_H */