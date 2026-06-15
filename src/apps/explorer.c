/*
 * explorer.c  --  WuBuOS File Manager (Win98-style Explorer)
 *
 * Features:
 * - Two-pane (Norton Commander) + Single-pane (Win98) modes
 * - Toolbar: Back, Forward, Up, View (Icons/List/Details)
 * - Address bar (editable path)
 * - 9P namespace integration (Styx mounts visible)
 * - Drag-drop between panes
 * - Context menu: Open, Cut, Copy, Paste, Delete, Rename, Properties
 * - Tree view sidebar (folders only)
 */

#include "../gui/wm.h"
#include "../gui/wubu_wm.h"
#include "../kernel/vbe.h"
#include "../runtime/styx.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define EXPL_WIN_W      900
#define EXPL_WIN_H      600
#define EXPL_TOOLBAR_H  28
#define EXPL_ADDRBAR_H  24
#define EXPL_STATUSBAR_H 22
#define EXPL_SIDEBAR_W  200
#define EXPL_ICON_SIZE  32
#define EXPL_GRID_GAP   4
#define EXPL_MAX_ENTRIES 1024

typedef enum {
    VIEW_ICONS = 0,
    VIEW_LIST,
    VIEW_DETAILS,
    VIEW_TREE,
} ViewMode;

typedef enum {
    PANE_LEFT = 0,
    PANE_RIGHT,
    PANE_SINGLE,
} PaneMode;

typedef struct {
    char        name[256];
    char        path[1024];
    bool        is_dir;
    bool        is_9p_mount;
    uint64_t    size;
    time_t      mtime;
    uint32_t    icon_color;
    int         icon_type;  // 0=folder, 1=file, 2=exe, 3=mount, 4=9p
} FileEntry;

typedef struct {
    FileEntry   entries[EXPL_MAX_ENTRIES];
    int         count;
    int         selected;
    int         hover;
    int         first_visible;
    char        current_path[1024];
    char        history[32][1024];
    int         hist_pos, hist_len;
    ViewMode    view;
    PaneMode    pane_mode;
    bool        focus;  // which pane has focus
    int         sort_col;  // 0=name, 1=size, 2=type, 3=date
    bool        sort_desc;
} PaneState;

typedef struct {
    PaneState   left, right;
    bool        two_pane;
    int         active_pane;  // 0=left, 1=right
    char        addrbar_text[1024];
    int         addrbar_cursor;
    bool        addrbar_focus;
    int         drag_source_pane;
    int         drag_source_idx;
    bool        dragging;
    int         drag_x, drag_y;
} ExplorerState;

static ExplorerState g_expl = {0};

static const char* g_view_names[] = {"Icons", "List", "Details", "Tree"};
static const uint32_t g_type_colors[] = {
    0x00FFD700,  // folder - gold
    0x00FFFFFF,  // file - white
    0x0000FF00,  // exe - green
    0x00FF8C00,  // mount - orange
    0x0000BFFF,  // 9p - deep sky blue
};

static void pane_init(PaneState *p) {
    memset(p, 0, sizeof(*p));
    p->view = VIEW_ICONS;
    p->pane_mode = PANE_SINGLE;
    p->selected = -1;
    p->hover = -1;
    p->first_visible = 0;
    getcwd(p->current_path, sizeof(p->current_path));
    p->hist_pos = 0;
    p->hist_len = 0;
    strncpy(p->history[0], p->current_path, sizeof(p->history[0]));
    p->hist_len = 1;
}

static void pane_add_history(PaneState *p, const char *path) {
    if (p->hist_len > 0 && strcmp(p->history[p->hist_pos], path) == 0) return;
    if (p->hist_pos < p->hist_len - 1) {
        p->hist_len = p->hist_pos + 1;
    }
    if (p->hist_len >= 32) {
        memmove(p->history, p->history + 1, sizeof(p->history) - sizeof(p->history[0]));
        p->hist_len = 31;
    }
    strncpy(p->history[p->hist_len], path, sizeof(p->history[0]));
    p->hist_pos = p->hist_len;
    p->hist_len++;
}

static void pane_navigate(PaneState *p, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return;
    pane_add_history(p, p->current_path);
    strncpy(p->current_path, path, sizeof(p->current_path));
    p->selected = -1;
    p->first_visible = 0;
}

static void pane_go_up(PaneState *p) {
    char *slash = strrchr(p->current_path, '/');
    if (slash && slash > p->current_path) {
        *slash = '\0';
        pane_navigate(p, p->current_path);
    } else if (slash == p->current_path) {
        *++slash = '\0';
        pane_navigate(p, p->current_path);
    }
}

static void pane_go_back(PaneState *p) {
    if (p->hist_pos > 0) {
        p->hist_pos--;
        strncpy(p->current_path, p->history[p->hist_pos], sizeof(p->current_path));
        p->selected = -1;
        p->first_visible = 0;
    }
}

static void pane_go_forward(PaneState *p) {
    if (p->hist_pos < p->hist_len - 1) {
        p->hist_pos++;
        strncpy(p->current_path, p->history[p->hist_pos], sizeof(p->current_path));
        p->selected = -1;
        p->first_visible = 0;
    }
}

static int file_type_icon(const char *name, bool is_dir) {
    if (is_dir) return 0;
    const char *ext = strrchr(name, '.');
    if (ext && (strcmp(ext, ".exe") == 0 || strcmp(ext, ".sh") == 0 || strcmp(ext, ".com") == 0)) return 2;
    return 1;
}

static void pane_refresh(PaneState *p) {
    DIR *d = opendir(p->current_path);
    if (!d) return;

    p->count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) && p->count < EXPL_MAX_ENTRIES) {
        if (strcmp(ent->d_name, ".") == 0) continue;

        FileEntry *e = &p->entries[p->count];
        strncpy(e->name, ent->d_name, sizeof(e->name));
        snprintf(e->path, sizeof(e->path), "%s/%s", p->current_path, ent->d_name);

        struct stat st;
        stat(e->path, &st);
        e->is_dir = S_ISDIR(st.st_mode);
        e->size = st.st_size;
        e->mtime = st.st_mtime;
        e->icon_type = file_type_icon(ent->d_name, e->is_dir);
        e->icon_color = g_type_colors[e->icon_type];

        /* Check for 9P mount points */
        e->is_9p_mount = false;
        if (e->is_dir) {
            /* Check if it's a Styx mount */
            if (strstr(e->path, "/mnt/") == e->path || strstr(e->path, "/n/") == e->path) {
                e->is_9p_mount = true;
                e->icon_type = 3;
                e->icon_color = g_type_colors[3];
            }
        }

        p->count++;
    }
    closedir(d);

    /* Sort: dirs first, then by name */
    for (int i = 0; i < p->count - 1; i++) {
        for (int j = i + 1; j < p->count; j++) {
            bool swap = false;
            if (p->entries[i].is_dir != p->entries[j].is_dir) {
                swap = !p->entries[i].is_dir;  // dirs first
            } else {
                swap = strcmp(p->entries[i].name, p->entries[j].name) > 0;
            }
            if (swap) {
                FileEntry tmp = p->entries[i];
                p->entries[i] = p->entries[j];
                p->entries[j] = tmp;
            }
        }
    }
}

static void pane_draw_icons(PaneState *p, WmWindow *win, int x, int y, int w, int h, bool focused) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int cols = (w - EXPL_GRID_GAP) / (EXPL_ICON_SIZE + EXPL_GRID_GAP);
    if (cols < 1) cols = 1;

    int row = 0, col = 0;
    for (int i = p->first_visible; i < p->count; i++) {
        int ix = x + col * (EXPL_ICON_SIZE + EXPL_GRID_GAP);
        int iy = y + row * (EXPL_ICON_SIZE + EXPL_GRID_GAP + 18);  // +18 for label

        if (iy + EXPL_ICON_SIZE + 18 > y + h) break;

        FileEntry *e = &p->entries[i];
        bool sel = (i == p->selected);
        bool hov = (i == p->hover);

        /* Selection highlight */
        if (sel || hov) {
            uint32_t bg = sel ? tc->select_bg : tc->btn_hover;
            vbe_fill_rect(ix - 2, iy - 2, EXPL_ICON_SIZE + 4, EXPL_ICON_SIZE + 20, bg);
        }

        /* Icon */
        vbe_fill_rect(ix, iy, EXPL_ICON_SIZE, EXPL_ICON_SIZE, e->icon_color);
        if (e->is_9p_mount) {
            /* Draw 9P indicator */
            vbe_fill_rect(ix + 2, iy + 2, EXPL_ICON_SIZE - 4, EXPL_ICON_SIZE - 4, 0x00000000);
        } else if (e->is_dir) {
            /* Folder icon */
            vbe_fill_rect(ix + 4, iy + 8, EXPL_ICON_SIZE - 8, EXPL_ICON_SIZE - 12, 0x00FFD700);
        }

        /* Label (first 12 chars) */
        char label[16];
        strncpy(label, e->name, 12);
        label[12] = '\0';
        int lx = ix + (EXPL_ICON_SIZE - strlen(label) * 4) / 2;
        int ly = iy + EXPL_ICON_SIZE + 2;
        if (sel) vbe_fill_rect(lx - 1, ly, strlen(label) * 8 + 2, 14, tc->select_bg);

        row++;
        if (row * (EXPL_ICON_SIZE + 18) > h) {
            row = 0;
            col++;
            if (col >= cols) break;
        }
    }
}

static void pane_draw_list(PaneState *p, WmWindow *win, int x, int y, int w, int h, bool focused) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int row_h = 20;
    int visible = h / row_h;

    for (int i = 0; i < visible && (p->first_visible + i) < p->count; i++) {
        int idx = p->first_visible + i;
        FileEntry *e = &p->entries[idx];
        int iy = y + i * row_h;

        if (idx == p->selected) {
            vbe_fill_rect(x, iy, w, row_h, tc->select_bg);
        } else if (idx == p->hover) {
            vbe_fill_rect(x, iy, w, row_h, tc->btn_hover);
        }

        /* Icon */
        vbe_fill_rect(x + 4, iy + 2, 16, 16, e->icon_color);

        /* Name */
        /* Size */
        if (!e->is_dir) {
            char sz[32];
            if (e->size < 1024) snprintf(sz, sizeof(sz), "%lu B", e->size);
            else if (e->size < 1024*1024) snprintf(sz, sizeof(sz), "%.1f KB", e->size / 1024.0);
            else snprintf(sz, sizeof(sz), "%.1f MB", e->size / (1024.0*1024.0));
        }

        /* Date */
        struct tm *tm = localtime(&e->mtime);
        char date[32];
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
    }
}

static void pane_draw_details(PaneState *p, WmWindow *win, int x, int y, int w, int h, bool focused) {
    pane_draw_list(p, win, x, y, w, h, focused);
    /* Details adds columns: Name | Size | Type | Date Modified */
}

static void pane_draw(PaneState *p, WmWindow *win, int x, int y, int w, int h, bool focused) {
    const WubuThemeColors *tc = wubu_theme_colors();

    /* Background */
    vbe_fill_rect(x, y, w, h, focused ? 0x00FFFFFF : 0x00F0F0F0);
    if (focused) vbe_3d_sunken(x, y, w, h);
    else vbe_3d_raised(x, y, w, h);

    switch (p->view) {
        case VIEW_ICONS: pane_draw_icons(p, win, x, y, w, h, focused); break;
        case VIEW_LIST: pane_draw_list(p, win, x, y, w, h, focused); break;
        case VIEW_DETAILS: pane_draw_details(p, win, x, y, w, h, focused); break;
        case VIEW_TREE: pane_draw_list(p, win, x, y, w, h, focused); break;
    }
}

static void explorer_draw_sidebar(WmWindow *win, void *fb, int fb_w, int fb_h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 2;
    int y = win->y + WM_TITLE_HEIGHT + EXPL_TOOLBAR_H + EXPL_ADDRBAR_H + 2;
    int w = EXPL_SIDEBAR_W;
    int h = win->h - WM_TITLE_HEIGHT - EXPL_TOOLBAR_H - EXPL_ADDRBAR_H - EXPL_STATUSBAR_H - 6;

    vbe_fill_rect(x, y, w, h, tc->win_face);
    vbe_3d_sunken(x, y, w, h);

    /* Tree view - simplified: show parent directories */
    char path[1024];
    strncpy(path, g_expl.left.current_path, sizeof(path));
    char *parts[32];
    int nparts = 0;
    char *tok = strtok(path, "/");
    while (tok && nparts < 32) {
        parts[nparts++] = tok;
        tok = strtok(NULL, "/");
    }

    int ty = y + 4;
    for (int i = 0; i < nparts; i++) {
        char full[1024] = "/";
        for (int j = 0; j <= i; j++) {
            strcat(full, parts[j]);
            if (j < i) strcat(full, "/");
        }
        /* Draw tree item */
        vbe_fill_rect(x + 4, ty, w - 8, 18, (i == nparts - 1) ? tc->select_bg : tc->win_face);
        /* Expand/collapse indicator */
        vbe_fill_rect(x + 6, ty + 4, 10, 10, tc->btn_face);
        vbe_3d_raised(x + 6, ty + 4, 10, 10);
        ty += 20;
    }

    /* 9P mounts section */
    ty += 4;
    vbe_fill_rect(x + 4, ty, w - 8, 18, tc->select_bg);
    ty += 22;
    /* List known 9P mounts: /mnt/host, /mnt/wubu, /n/... */
}

static void explorer_draw_toolbar(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 2;
    int y = win->y + WM_TITLE_HEIGHT + 2;
    int w = win->w - 4;
    int h = EXPL_TOOLBAR_H;

    vbe_fill_rect(x, y, w, h, tc->win_face);
    vbe_3d_raised(x, y, w, h);

    /* Buttons: Back, Forward, Up, View, New Folder */
    const char *btns[] = {"\x1B[D", "\x1B[C", "\x1B[A", "View", "New"};
    int bx = x + 4;
    for (int i = 0; i < 5; i++) {
        int bw = (i == 3) ? 60 : 32;
        vbe_fill_rect(bx, y + 3, bw, h - 6, tc->btn_face);
        vbe_3d_raised(bx, y + 3, bw, h - 6);
        bx += bw + 4;
    }
}

static void explorer_draw_addrbar(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 2;
    int y = win->y + WM_TITLE_HEIGHT + EXPL_TOOLBAR_H + 2;
    int w = win->w - 4 - EXPL_SIDEBAR_W - 2;
    int h = EXPL_ADDRBAR_H;

    vbe_fill_rect(x, y, w, h, 0x00FFFFFF);
    vbe_3d_sunken(x, y, w, h);

    /* Path text - editable */
    PaneState *p = g_expl.two_pane ? (g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right) : &g_expl.left;
}

static void explorer_draw_statusbar(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 2;
    int y = win->y + win->h - EXPL_STATUSBAR_H - 2;
    int w = win->w - 4;
    int h = EXPL_STATUSBAR_H;

    vbe_fill_rect(x, y, w, h, tc->win_face);
    vbe_3d_sunken(x, y, w, h);

    /* Item count, free space */
}

static void explorer_draw(WmWindow *win, void *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;

    explorer_draw_toolbar(win);
    explorer_draw_addrbar(win);
    explorer_draw_sidebar(win, fb, fb_w, fb_h);

    /* Main pane(s) */
    int content_x = win->x + EXPL_SIDEBAR_W + 4;
    int content_y = win->y + WM_TITLE_HEIGHT + EXPL_TOOLBAR_H + EXPL_ADDRBAR_H + 4;
    int content_w = win->w - EXPL_SIDEBAR_W - 6;
    int content_h = win->h - WM_TITLE_HEIGHT - EXPL_TOOLBAR_H - EXPL_ADDRBAR_H - EXPL_STATUSBAR_H - 8;

    if (g_expl.two_pane) {
        int half_w = content_w / 2 - 2;
        pane_draw(&g_expl.left, win, content_x, content_y, half_w, content_h, g_expl.active_pane == 0);
        pane_draw(&g_expl.right, win, content_x + half_w + 4, content_y, half_w, content_h, g_expl.active_pane == 1);
        /* Divider */
        vbe_vline(content_x + half_w + 2, content_y, content_y + content_h, 0x00808080);
    } else {
        pane_draw(&g_expl.left, win, content_x, content_y, content_w, content_h, true);
    }

    explorer_draw_statusbar(win);
}

static void explorer_handle_mouse(WmWindow *win, int x, int y, int btn, int kind) {
    /* Toolbar clicks */
    int ty = win->y + WM_TITLE_HEIGHT + 2;
    if (y >= ty && y < ty + EXPL_TOOLBAR_H && kind == 1) {
        int bx = x - win->x - 4;
        if (bx >= 0 && bx < 32) { /* Back */
            PaneState *p = g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right;
            pane_go_back(p);
        } else if (bx >= 36 && bx < 68) { /* Forward */
            PaneState *p = g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right;
            pane_go_forward(p);
        } else if (bx >= 72 && bx < 104) { /* Up */
            PaneState *p = g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right;
            pane_go_up(p);
        } else if (bx >= 108 && bx < 168) { /* View */
            PaneState *p = g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right;
            p->view = (p->view + 1) % 4;
        }
        wm_invalidate(win);
        return;
    }

    /* Pane clicks */
    if (kind == 1) {
        PaneState *p = g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right;
        int px, py, pw, ph;
        if (g_expl.two_pane) {
            int content_x = win->x + EXPL_SIDEBAR_W + 4;
            int content_y = win->y + WM_TITLE_HEIGHT + EXPL_TOOLBAR_H + EXPL_ADDRBAR_H + 4;
            int half_w = (win->w - EXPL_SIDEBAR_W - 6) / 2 - 2;
            if (g_expl.active_pane == 0) {
                px = content_x; py = content_y; pw = half_w; ph = win->h - WM_TITLE_HEIGHT - EXPL_TOOLBAR_H - EXPL_ADDRBAR_H - EXPL_STATUSBAR_H - 8;
            } else {
                px = content_x + half_w + 4; py = content_y; pw = half_w; ph = win->h - WM_TITLE_HEIGHT - EXPL_TOOLBAR_H - EXPL_ADDRBAR_H - EXPL_STATUSBAR_H - 8;
            }
        } else {
            px = win->x + EXPL_SIDEBAR_W + 4;
            py = win->y + WM_TITLE_HEIGHT + EXPL_TOOLBAR_H + EXPL_ADDRBAR_H + 4;
            pw = win->w - EXPL_SIDEBAR_W - 6;
            ph = win->h - WM_TITLE_HEIGHT - EXPL_TOOLBAR_H - EXPL_ADDRBAR_H - EXPL_STATUSBAR_H - 8;
        }

        if (x >= px && x < px + pw && y >= py && y < py + ph) {
            int rel_x = x - px;
            int rel_y = y - py;
            int idx = -1;

            if (p->view == VIEW_ICONS) {
                int cols = (pw - EXPL_GRID_GAP) / (EXPL_ICON_SIZE + EXPL_GRID_GAP);
                if (cols < 1) cols = 1;
                int col = rel_x / (EXPL_ICON_SIZE + EXPL_GRID_GAP);
                int row = rel_y / (EXPL_ICON_SIZE + EXPL_GRID_GAP + 18);
                idx = p->first_visible + row * cols + col;
            } else {
                int row_h = 20;
                int row = rel_y / row_h;
                idx = p->first_visible + row;
            }

            if (idx >= 0 && idx < p->count) {
                p->selected = idx;
                p->hover = idx;

                if (btn == 1) { /* Left click - could be double-click for open */
                    FileEntry *e = &p->entries[idx];
                    if (e->is_dir) {
                        pane_navigate(p, e->path);
                        pane_refresh(p);
                    } else {
                        /* Open file - launch associated app */
                    }
                } else if (btn == 3) { /* Right click - context menu */
                    /* TODO: context menu */
                }
                wm_invalidate(win);
            }
        }
    }
}

static void explorer_handle_key(WmWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    PaneState *p = g_expl.active_pane == 0 ? &g_expl.left : &g_expl.right;

    if (key == 0xE048) { /* Up */
        if (p->selected > 0) p->selected--;
        else if (p->first_visible > 0) p->first_visible--;
    } else if (key == 0xE050) { /* Down */
        if (p->selected < p->count - 1) p->selected++;
        else if (p->first_visible + 20 < p->count) p->first_visible++;
    } else if (key == 0xE04B) { /* Left */
        if (g_expl.two_pane) {
            g_expl.active_pane = 0;
        } else {
            pane_go_back(p);
        }
    } else if (key == 0xE04D) { /* Right */
        if (g_expl.two_pane) {
            g_expl.active_pane = 1;
        } else {
            pane_go_forward(p);
        }
    } else if (key == 0x1C) { /* Enter */
        if (p->selected >= 0 && p->selected < p->count) {
            FileEntry *e = &p->entries[p->selected];
            if (e->is_dir) {
                pane_navigate(p, e->path);
                pane_refresh(p);
            }
        }
    } else if (key == 0x0E) { /* Backspace */
        pane_go_up(p);
        pane_refresh(p);
    } else if (key == 0x3B) { /* F1 - toggle two-pane */
        g_expl.two_pane = !g_expl.two_pane;
        if (g_expl.two_pane) {
            pane_init(&g_expl.right);
            strncpy(g_expl.right.current_path, g_expl.left.current_path, sizeof(g_expl.right.current_path));
            pane_refresh(&g_expl.right);
        }
    } else if (key == 0x3C) { /* F2 - rename */
        /* TODO */
    } else if (key == 0x3D) { /* F3 - view mode */
        p->view = (p->view + 1) % 4;
    } else if (key == 0x3E) { /* F4 - new folder */
        /* TODO */
    } else if (key == 0x3F) { /* F5 - copy */
        /* TODO */
    } else if (key == 0x40) { /* F6 - move */
        /* TODO */
    } else if (key == 0x41) { /* F7 - mkdir */
        /* TODO */
    } else if (key == 0x42) { /* F8 - delete */
        /* TODO */
    }
    wm_invalidate(win);
}

void explorer_open(void) {
    pane_init(&g_expl.left);
    pane_refresh(&g_expl.left);
    g_expl.two_pane = false;
    g_expl.active_pane = 0;
    g_expl.dragging = false;

    WmWindow *win = wm_create_window(50, 50, EXPL_WIN_W, EXPL_WIN_H, "File Manager");
    if (win) {
        win->on_draw = explorer_draw;
        win->on_mouse = explorer_handle_mouse;
        win->on_key = explorer_handle_key;
    }
}

void explorer_init(void) { memset(&g_expl, 0, sizeof(g_expl)); }
void explorer_shutdown(void) { }