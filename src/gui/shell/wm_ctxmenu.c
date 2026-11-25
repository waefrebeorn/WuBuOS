/*
 * wm_ctxmenu.c  --  WuBuOS DosGui WM: context-menu action wiring
 *
 * Facade that binds the generic context-menu engine (wm_ctxmenu_engine.c)
 * to the desktop + icon domains. Owns the default context actions (Open/Play/
 * Rename/Delete/Properties/New (Shortcut|Folder|Doc)/Sort By/Refresh), the modal dialog callbacks
 * (rename/delete/properties/shortcut), the icon + desktop context-menu builders,
 * and the per-frame tick + query accessors.
 *
 * The mechanical engine, the taskbar clock, and window state/modal handling
 * live in their own self-contained modules (C11 opaque-safe, no god headers).
 */

#include "../wm_internal.h"
#include "shell_session.h"   /* desktop_launch_icon: declared, never implicit */
#include "../wubu_tokens.h"
#include "../wubu_trash.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wubu_notify.h"
#include "window_chrome.h"
#include "taskbar_tasks.h"   /* Pin to taskbar: the same pin path the taskbar test proves */
/* WEAK: the WM tests link this file without the desktop unit. A menu that cannot launch
 * must still be buildable; it simply does nothing at that call site. */
__attribute__((weak)) void desktop_launch_exec(const char *exec);

/* -- Default Context Menu Actions -- */

/* The SELECTED icon, which is what keyboard shortcuts act on. The context-menu actions
 * used the mouse position instead, so F2/Delete/Enter had nothing to address. */
static int icon_selected_index(void) {
    for (int i = 0; i < g_dwm.icon_count; i++)
        if (g_dwm.icons[i].alive && g_dwm.icons[i].selected) return i;
    return -1;
}

/* Open must work for a SHORTCUT too: a .desktop icon has no on_execute, only a target
 * file, so the old "call on_execute if present" silently skipped exactly the icons a user
 * double-clicks most. desktop_launch_icon() dispatches on the icon's TYPE. */
void wm_open_icon(int idx) {
    if (idx < 0 || idx >= g_dwm.icon_count || !g_dwm.icons[idx].alive) return;
    g_dwm.focused_id = -1;
    if (g_dwm.icons[idx].on_execute) g_dwm.icons[idx].on_execute();
    else                            desktop_launch_icon(idx);
}

static void ctx_action_open(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx < 0) idx = icon_selected_index();
    wm_open_icon(idx);
}

/* -- Dialog Callback Functions -- */

static DosGuiIcon *g_rename_target = NULL;
static char g_rename_input[32] = {0};
static int g_rename_pos = 0;
static DosGuiIcon *g_delete_target = NULL;
static DosGuiIcon *g_properties_target = NULL;
static bool g_shortcut_pending = false;

/* Two-field shortcut wizard, matching Windows' "New > Shortcut": you type the
 * LOCATION of the item first, then its NAME.  The previous dialog asked only for
 * a name and passed NULL as the target, so every shortcut it created had an empty
 * Exec and clicking it did nothing -- "shortcuts aren't shortcuts". */
static char g_shortcut_target[128] = {0};
static int  g_shortcut_target_pos = 0;
static char g_shortcut_name[64] = {0};
static int  g_shortcut_name_pos = 0;
static bool g_shortcut_adding_name = false;   /* false = editing target */

static void dialog_rename_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == '\r' || key == '\n') {
        if (g_rename_pos > 0 && g_rename_target) {
            strncpy(g_rename_target->name, g_rename_input, sizeof(g_rename_target->name) - 1);
            wubu_notify_simple("Desktop", "Renamed", g_rename_target->name, NULL, 1, WUBU_TOAST_MS_DEFAULT);
        }
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        g_rename_target = NULL;
        wm_destroy(win);
    } else if (key == 27) {  /* Escape */
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        g_rename_target = NULL;
        wm_destroy(win);
    } else if (key == 8 && g_rename_pos > 0) {  /* Backspace */
        g_rename_input[--g_rename_pos] = '\0';
    } else if (key >= 32 && key < 127 && g_rename_pos < 31) {  /* Printable */
        g_rename_input[g_rename_pos++] = (char)key;
        g_rename_input[g_rename_pos] = '\0';
    }
}

static void dialog_rename_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = chrome_caption_metrics().bar_h;
    const int bw = chrome_border_width();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    vbe_fill_rect(cx, cy, cw, ch, tc()->win_face);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_LABEL_DY, "New name:", tc()->btn_text, 1);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_INPUT_DY, g_rename_input[0] ? g_rename_input : "(empty)", tc()->btn_text, 1);
}

static void dialog_delete_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == 'y' || key == 'Y') {
        int deleted = 0;
        /* Delete the specifically-targeted icon (single right-click delete). */
        if (g_delete_target) {
            /* Move the underlying ~/Desktop file/folder to the Recycle Bin
             * (ReactOS shell32 CDesktopFolder::Delete / SHFileOperation lesson)
             * so the deletion is recoverable, not just an in-memory hide. */
            if (g_delete_target->target[0])
                wubu_trash_move(g_delete_target->target);
            g_delete_target->alive = false;
            g_delete_target->selected = false;
            deleted++;
            g_delete_target = NULL;
        }
        /* Also delete any multi-selected icons (Ctrl/Shift group select). */
        for (int i = 0; i < g_dwm.icon_count; i++) {
            if (g_dwm.icons[i].alive && g_dwm.icons[i].selected) {
                if (g_dwm.icons[i].target[0])
                    wubu_trash_move(g_dwm.icons[i].target);
                g_dwm.icons[i].alive = false;
                g_dwm.icons[i].selected = false;
                deleted++;
            }
        }
        /* Compact the array so refresh/sort see a dense list. */
        wm_compact_icons();
        wubu_notify_simple("Desktop", "Deleted",
                           deleted ? "Item(s) moved to Recycle Bin" : "Icon removed",
                           NULL, 1, 2000);
        wm_destroy(win);
    } else if (key == 'n' || key == 'N' || key == 27) {
        g_delete_target = NULL;
        wm_destroy(win);
    }
}

static void dialog_delete_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = chrome_caption_metrics().bar_h;
    const int bw = chrome_border_width();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    vbe_fill_rect(cx, cy, cw, ch, tc()->win_face);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_LABEL_DY, "Delete this icon?", tc()->btn_text, 1);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_HINT_DY, "Press Y to confirm, N to cancel", tc()->btn_text, 1);
}

static void dialog_properties_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == 27 || key == 'q' || key == 'Q') {  /* Escape or Q to close */
        g_properties_target = NULL;
        wm_destroy(win);
    }
}

static void dialog_properties_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    DosGuiIcon *ic = g_properties_target;
    if (!ic) return;

    const int tbh = chrome_caption_metrics().bar_h;
    const int bw = chrome_border_width();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;
    int y = cy + WUBU_DIALOG_PROP_TOP;

    vbe_fill_rect(cx, cy, cw, ch, tc()->win_face);

    char buf[256];
    snprintf(buf, sizeof(buf), "Name: %s", ic->name);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, y, buf, tc()->btn_text, 1); y += WUBU_DIALOG_PROP_PITCH;

    const char *type_str[] = {"App", "Shortcut", "Folder", "Drive", "File", "URL"};
    snprintf(buf, sizeof(buf), "Type: %s", ic->type < 6 ? type_str[ic->type] : "Unknown");
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, y, buf, tc()->btn_text, 1); y += WUBU_DIALOG_PROP_PITCH;

    snprintf(buf, sizeof(buf), "Target: %s", ic->target[0] ? ic->target : "(none)");
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, y, buf, tc()->btn_text, 1); y += WUBU_DIALOG_PROP_PITCH;

    snprintf(buf, sizeof(buf), "Position: (%d, %d) grid (%d, %d)", ic->x, ic->y, ic->grid_x, ic->grid_y);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, y, buf, tc()->btn_text, 1); y += WUBU_DIALOG_PROP_PITCH;

    snprintf(buf, sizeof(buf), "Color: #%06X", ic->icon_color & 0xFFFFFF); /* format-mask */
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, y, buf, tc()->btn_text, 1); y += WUBU_DIALOG_PROP_PITCH;

    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, y + WUBU_DIALOG_PROP_HINT_DY, "Press ESC to close", tc()->icon_text_shadow, 1);
}

/* New-Shortcut dialog: reuse the rename input buffer to collect a name,
 * then call the WM helper which writes a real .desktop file to ~/Desktop. */
static void dialog_shortcut_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    if (key == '\r' || key == '\n') {
        if (!g_shortcut_adding_name) {
            /* First Enter accepts the target and moves to the name field. */
            g_shortcut_adding_name = true;
            return;
        }
        if (g_shortcut_name_pos > 0 && g_shortcut_target_pos > 0) {
            int rc = wm_write_desktop_shortcut(g_shortcut_name, g_shortcut_target);
            wubu_notify_simple("Desktop", "Shortcut Created",
                               rc == 0 ? g_shortcut_name : "Failed to create",
                               NULL, 1, 2500);
        }
        g_shortcut_pending = false;
        g_shortcut_target[0] = g_shortcut_target_pos = 0;
        g_shortcut_name[0] = g_shortcut_name_pos = 0;
        g_shortcut_adding_name = false;
        wm_destroy(win);
    } else if (key == 9) {                       /* Tab toggles the field */
        g_shortcut_adding_name = !g_shortcut_adding_name;
    } else if (key == 27) {                      /* Escape cancels */
        g_shortcut_pending = false;
        g_shortcut_target[0] = g_shortcut_target_pos = 0;
        g_shortcut_name[0] = g_shortcut_name_pos = 0;
        g_shortcut_adding_name = false;
        wm_destroy(win);
    } else {
        char *buf = g_shortcut_adding_name ? g_shortcut_name : g_shortcut_target;
        int *pos  = g_shortcut_adding_name ? &g_shortcut_name_pos : &g_shortcut_target_pos;
        size_t cap = g_shortcut_adding_name ? sizeof(g_shortcut_name) : sizeof(g_shortcut_target);
        if (key == 8 && *pos > 0) {
            buf[--*pos] = '\0';
        } else if (key >= 32 && key < 127 && (size_t)*pos + 1 < cap) {
            buf[(*pos)++] = (char)key;
            buf[*pos] = '\0';
        }
    }
}

static void dialog_shortcut_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    const int tbh = chrome_caption_metrics().bar_h;
    const int bw = chrome_border_width();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    vbe_fill_rect(cx, cy, cw, ch, tc()->win_face);

    /* Active field gets an accent bar; inactive stays plain, so it is obvious
     * what the next keystroke goes into without a separate focus widget. */
    uint32_t active_col = tc()->select_bg;
    uint32_t idle_col   = tc()->btn_text;

    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_SC_LBL1_DY, "Type the location of the item:",
                  g_shortcut_adding_name ? idle_col : active_col, 1);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_SC_VAL1_DY,
                  g_shortcut_target[0] ? g_shortcut_target : "(command or path)",
                  g_shortcut_adding_name ? idle_col : active_col, 1);

    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_SC_LBL2_DY, "Type a name for this shortcut:",
                  g_shortcut_adding_name ? active_col : idle_col, 1);
    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_SC_VAL2_DY,
                  g_shortcut_name[0] ? g_shortcut_name : "(shortcut name)",
                  g_shortcut_adding_name ? active_col : idle_col, 1);

    vbe_draw_text(cx + WUBU_DIALOG_PAD_X, cy + WUBU_DIALOG_SC_HINT_DY, "Enter=next   Tab=switch field   Esc=cancel",
                  tc()->icon_text_shadow, 1);
}

/* -- Default Context Menu Actions -- */

static void ctx_action_play(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx < 0) return;
    DosGuiIcon *ic = &g_dwm.icons[idx];

    /* Resolve the target binary (live icon path, or compat title). */
    const char *target = ic->target[0] ? ic->target : ic->name;
    if (!target || !target[0]) return;

    /* Look up a per-title compat profile (ProtonDB-style) so the launch uses
     * the right proton flags / env overrides. Falls back gracefully if none. */
    WubuCompatEntry compat;
    if (wubu_compat_db_get(ic->name, &compat) == 0 && compat.cache_enabled) {
        char cache[512];
        if (wubu_compat_cache_dir(ic->name, cache, sizeof(cache)) == 0) {
            /* Cache dir is created; real shader/proton cache lives here. */
            (void)cache;
        }
    }

    /* Enter a dedicated GAME session and launch via the container/Proton path
     * (SteamOS strategy). The selected icon's binary is read from disk. */
    FILE *bin = fopen(target, "rb");
    if (!bin) {
        wubu_notify_simple("Desktop", "Play", "Cannot open target binary", NULL, 1, WUBU_TOAST_MS_LONG);
        return;
    }
    uint8_t buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), bin);
    fclose(bin);
    if (n == 0) {
        wubu_notify_simple("Desktop", "Play", "Target is empty", NULL, 1, WUBU_TOAST_MS_LONG);
        return;
    }

    hosted_state_t *st = wm_get_hosted_state();
    int pid = wubu_session_launch_game(st, buf, n, ic->name);
    wubu_notify_simple("Desktop", "Play",
                       pid >= 0 ? ic->name : "Launch failed (no Proton/VSL in this build)",
                       NULL, 1, 2500);
}

/* Windows has "Create shortcut" on any item. Ours writes a real .desktop through the
 * same path the New > Shortcut wizard uses, with an Exec the resolver now understands
 * (desktop_exec_app_name handles the "wubu-app <Name>" form). */
static void ctx_action_create_icon_shortcut(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx < 0) return;
    const char *nm = g_dwm.icons[idx].name;
    char exec[256];
    snprintf(exec, sizeof exec, "wubu-app %s", nm);
    if (wm_write_desktop_shortcut(nm, exec) == 0)
        wubu_notify_simple("Desktop", "Shortcut", "Created on the desktop", NULL, 1, 2000);
    else
        wubu_notify_simple("Desktop", "Shortcut", "Could not be created", NULL, 2, 2500);
}

/* Windows pins an app to the taskbar from its context menu. */
static void ctx_action_pin_taskbar(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx < 0) return;
    const char *nm = g_dwm.icons[idx].name;
    char exec[256];
    snprintf(exec, sizeof exec, "wubu-app %s", nm);
    if (wubu_taskbar_pin(nm, nm, exec) == 0)
        wubu_notify_simple("Taskbar", "Pinned", nm, NULL, 1, 2000);
    else
        wubu_notify_simple("Taskbar", "Pin", "Could not pin", NULL, 2, 2500);
}

/* Windows' desktop context menu has Refresh, and so must ours: it re-scans ~/Desktop for
 * .desktop files, which is how a shortcut created outside the shell appears. */
static void ctx_action_refresh_desktop(void) {
    wm_refresh_desktop();
    wubu_notify_simple("Desktop", "Refresh", "Re-scanned shortcuts", NULL, 1, 1500);
}

static void begin_rename(int idx) {
    if (idx >= 0) {
        g_rename_target = &g_dwm.icons[idx];
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        DosGuiWindow *dialog = wm_create_modal(WUBU_DIALOG_ORIGIN_X, WUBU_DIALOG_ORIGIN_Y,
                                             WUBU_DIALOG_RENAME_W, WUBU_DIALOG_RENAME_H, "Rename", NULL);
        if (dialog) {
            dialog->on_key = dialog_rename_on_key;
            dialog->on_draw = dialog_rename_on_draw;
        }
    }
}

/* Keyboard entry points: Windows lets F2/Delete/Enter act on the SELECTED icon, and a
 * context menu that can only be reached with the mouse is half a shell. */
void wm_rename_selected_icon(void) { begin_rename(icon_selected_index()); }

static void ctx_action_rename(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx < 0) idx = icon_selected_index();
    begin_rename(idx);
}

static void begin_delete(int idx) {
    if (idx >= 0) {
        g_delete_target = &g_dwm.icons[idx];
        DosGuiWindow *dialog = wm_create_modal(WUBU_DIALOG_ORIGIN_X, WUBU_DIALOG_ORIGIN_Y,
                                             WUBU_DIALOG_DELETE_W, WUBU_DIALOG_DELETE_H, "Confirm Delete", NULL);
        if (dialog) {
            dialog->on_key = dialog_delete_on_key;
            dialog->on_draw = dialog_delete_on_draw;
        }
    }
}

void wm_delete_selected_icon(void) { begin_delete(icon_selected_index()); }

static void ctx_action_delete(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx < 0) idx = icon_selected_index();
    begin_delete(idx);
}

static void ctx_action_properties(void) {
    int idx = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_properties_target = &g_dwm.icons[idx];
        DosGuiWindow *dialog = wm_create_modal(WUBU_DIALOG_ORIGIN_X, WUBU_DIALOG_ORIGIN_Y,
                                             WUBU_DIALOG_PROPS_W, WUBU_DIALOG_PROPS_H, "Properties", NULL);
        if (dialog) {
            dialog->on_key = dialog_properties_on_key;
            dialog->on_draw = dialog_properties_on_draw;
        }
    }
}

static void ctx_action_create_shortcut(void) {
    /* Prompt for a name via a modal, then write a real .desktop into ~/Desktop.
     * Until the user types a name we ask for it through the rename-style dialog. */
    g_rename_target = NULL;  /* not renaming an icon */
    g_rename_input[0] = '\0';
    g_rename_pos = 0;
    g_shortcut_target[0] = g_shortcut_target_pos = 0;
    g_shortcut_name[0] = g_shortcut_name_pos = 0;
    g_shortcut_adding_name = false;
    g_shortcut_pending = true;
    DosGuiWindow *dialog = wm_create_modal(WUBU_DIALOG_ORIGIN_X, WUBU_DIALOG_ORIGIN_Y,
                                             WUBU_DIALOG_SHORTCUT_W, WUBU_DIALOG_SHORTCUT_H, "New Shortcut", NULL);
    if (dialog) {
        dialog->on_key = dialog_shortcut_on_key;
        dialog->on_draw = dialog_shortcut_on_draw;
    }
}

static void ctx_action_view_desktop(void) {
    /* Toggle Auto-arrange and re-flow the live icons. */
    bool now = !wm_get_auto_arrange();
    wm_set_auto_arrange(now);
    if (now) {
        /* Re-flow current icons into the auto-arrange column. */
        reflow_all_icons_column();
    }
    wubu_notify_simple("Desktop", "View",
                       now ? "Auto-arrange: ON" : "Auto-arrange: OFF",
                       NULL, 1, 2000);
}

static void ctx_action_new_folder(void) {
    if (wm_new_folder() == 0) {
        wubu_notify_simple("Desktop", "New Folder",
                           "Folder created on ~/Desktop", NULL, 1, WUBU_TOAST_MS_SHORT);
    } else {
        wubu_notify_simple("Desktop", "New Folder",
                           "Could not create folder", NULL, 1, WUBU_TOAST_MS_SHORT);
    }
}

static void ctx_action_new_text_doc(void) {
    if (wm_new_text_doc() == 0) {
        wubu_notify_simple("Desktop", "New Text Document",
                           "Text document created on ~/Desktop", NULL, 1, WUBU_TOAST_MS_SHORT);
    } else {
        wubu_notify_simple("Desktop", "New Text Document",
                           "Could not create document", NULL, 1, WUBU_TOAST_MS_SHORT);
    }
}

static void ctx_action_sort_by_size(void) {
    wm_sort_icons(DOSGUI_SORT_SIZE);
    wubu_notify_simple("Desktop", "Sort By Size",
                       "Icons arranged by size", NULL, 1, WUBU_TOAST_MS_SHORT);
}

static void ctx_action_sort_by_type(void) {
    wm_sort_icons(DOSGUI_SORT_TYPE);
    wubu_notify_simple("Desktop", "Sort By Type",
                       "Icons arranged by type", NULL, 1, WUBU_TOAST_MS_SHORT);
}

static void ctx_action_sort_by_name(void) {
    /* Real alphabetical re-flow. */
    wm_sort_icons_by_name();
    wubu_notify_simple("Desktop", "Sort By Name",
                       "Icons arranged alphabetically", NULL, 1, WUBU_TOAST_MS_SHORT);
}

static void ctx_action_sort_by_date(void) {
    wm_sort_icons(DOSGUI_SORT_DATE);
    wubu_notify_simple("Desktop", "Sort By Date Modified",
                       "Icons arranged by date", NULL, 1, WUBU_TOAST_MS_SHORT);
}

static void ctx_action_refresh(void) {
    /* Real filesystem refresh: re-scan ~/Desktop for .desktop files. */
    wm_refresh_desktop();
    wubu_notify_simple("Desktop", "Refresh",
                       "Desktop reloaded from ~/Desktop", NULL, 1, WUBU_TOAST_MS_DEFAULT);
}

/* -- Show Icon Context Menu -- */

void icon_show_context_menu(int icon_idx, int mx, int my) {
    if (icon_idx < 0 || icon_idx >= DOSGUI_MAX_ICONS) return;
    if (!g_dwm.icons[icon_idx].alive) return;

    DosGuiContextMenu *menu = ctx_menu_create(mx, my);
    if (!menu) return;

    /* Select the icon */
    g_dwm.icons[icon_idx].selected = true;

    ctx_menu_add_item(menu, "Open", ctx_action_open);
    ctx_menu_add_item(menu, "Play", ctx_action_play);
    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "Rename", ctx_action_rename);
    ctx_menu_add_item(menu, "Delete", ctx_action_delete);
    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "Create shortcut", ctx_action_create_icon_shortcut);
    ctx_menu_add_item(menu, "Pin to taskbar", ctx_action_pin_taskbar);
    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "Properties", ctx_action_properties);

    ctx_menu_show(menu, mx, my);
}

/* ---- Window system menu (Alt+Space, as every Windows has had) ---------------------
 * Windows' menu is Restore / Move / Size / Minimize / Maximize / Close. Move and Size are
 * keyboard modal modes we do not have yet, so they are NOT offered as dead items -- the
 * menu ships the four that work, which is the difference between a menu and a decoration. */
static int g_sysmenu_win = -1;

static DosGuiWindow *sysmenu_target(void) {
    if (g_sysmenu_win < 0 || g_sysmenu_win >= DOSGUI_MAX_WINDOWS) return NULL;
    DosGuiWindow *w = &g_dwm.windows[g_sysmenu_win];
    return w->alive ? w : NULL;
}

static void ctx_action_sys_restore(void)  { DosGuiWindow *w = sysmenu_target(); if (w) wm_restore(w); }
static void ctx_action_sys_minimize(void) { DosGuiWindow *w = sysmenu_target(); if (w) wm_minimize(w); }
static void ctx_action_sys_maximize(void) { DosGuiWindow *w = sysmenu_target(); if (w) wm_maximize(w); }
static void ctx_action_sys_close(void)    { DosGuiWindow *w = sysmenu_target(); if (w) wm_destroy(w); }

void window_show_system_menu(int win_idx, int mx, int my) {
    g_sysmenu_win = win_idx;
    DosGuiWindow *w = sysmenu_target();
    if (!w) return;
    DosGuiContextMenu *menu = ctx_menu_create(mx, my);
    if (!menu) return;
    ctx_menu_add_item(menu, "Restore",  ctx_action_sys_restore);
    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "Minimize", ctx_action_sys_minimize);
    ctx_menu_add_item(menu, "Maximize", ctx_action_sys_maximize);
    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "Close   Alt+F4", ctx_action_sys_close);
    ctx_menu_show(menu, mx, my);
}

/* ---- Taskbar jump list (Windows: right-click a taskbar button) ------------------
 * Windows shows what you can DO with that app plus what you did with it. We have no
 * per-app recency yet, so this ships the honest half: open/focus, close, pin/unpin. */
static int g_jump_task = -1;

static const WubuTask *jump_task(void) {
    return (g_jump_task >= 0) ? wubu_taskbar_get(g_jump_task) : NULL;
}

static void ctx_action_jump_open(void) {
    const WubuTask *t = jump_task();
    if (!t) return;
    if (t->win_id >= 0) {
        for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
            if (g_dwm.windows[i].alive && g_dwm.windows[i].id == t->win_id) {
                wm_restore(&g_dwm.windows[i]);
                wm_set_focus(&g_dwm.windows[i]);
                return;
            }
    }
    const char *x = t->exec[0] ? t->exec : t->title;
    if (desktop_launch_exec) desktop_launch_exec(x);
}

static void ctx_action_jump_close(void) {
    const WubuTask *t = jump_task();
    if (!t || t->win_id < 0) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (g_dwm.windows[i].alive && g_dwm.windows[i].id == t->win_id) {
            wm_destroy(&g_dwm.windows[i]);
            return;
        }
}

static void ctx_action_jump_pin(void) {
    const WubuTask *t = jump_task();
    if (!t) return;
    if (t->pinned) wubu_taskbar_unpin(t->key);
    else           wubu_taskbar_pin(t->key, t->title, t->exec);
    wubu_notify_simple("Taskbar", t->pinned ? "Unpinned" : "Pinned", t->title, NULL, 1, 1800);
}

void taskbar_show_jump_list(int task_idx, int mx, int my) {
    g_jump_task = task_idx;
    const WubuTask *t = jump_task();
    if (!t) return;
    DosGuiContextMenu *menu = ctx_menu_create(mx, my);
    if (!menu) return;
    ctx_menu_add_item(menu, t->win_id >= 0 ? "Focus" : "Open", ctx_action_jump_open);
    if (t->win_id >= 0) {
        ctx_menu_add_separator(menu);
        ctx_menu_add_item(menu, "Close window", ctx_action_jump_close);
    }
    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, t->pinned ? "Unpin from taskbar" : "Pin to taskbar",
                      ctx_action_jump_pin);
    ctx_menu_show(menu, mx, my);
}

void desktop_show_context_menu(int mx, int my) {
    DosGuiContextMenu *menu = ctx_menu_create(mx, my);
    if (!menu) return;

    ctx_menu_add_item(menu, "New", NULL);

    DosGuiContextMenu *newmenu = ctx_menu_add_submenu(menu, "New");
    ctx_menu_add_item(newmenu, "Shortcut", ctx_action_create_shortcut);
    ctx_menu_add_item(newmenu, "Folder", ctx_action_new_folder);
    ctx_menu_add_item(newmenu, "Text Document", ctx_action_new_text_doc);

    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "View", ctx_action_view_desktop);
    ctx_menu_add_item(menu, "Refresh", ctx_action_refresh_desktop);

    DosGuiContextMenu *viewmenu = ctx_menu_add_submenu(menu, "Sort By");
    ctx_menu_add_item(viewmenu, "Name", ctx_action_sort_by_name);
    ctx_menu_add_item(viewmenu, "Size", ctx_action_sort_by_size);
    ctx_menu_add_item(viewmenu, "Type", ctx_action_sort_by_type);
    ctx_menu_add_item(viewmenu, "Date Modified", ctx_action_sort_by_date);

    ctx_menu_add_separator(menu);
    ctx_menu_add_item(menu, "Refresh", ctx_action_refresh);
    ctx_menu_add_item(menu, "Properties", ctx_action_properties);

    ctx_menu_show(menu, mx, my);
}

/* -- Tick ------------------------------------------------------- */

void tick(void) {
    g_dwm.ticks++;
}

/* -- Query ------------------------------------------------------- */

int wm_screen_w(void) { return g_dwm.screen_w; }
int wm_screen_h(void) { return g_dwm.screen_h; }

int wm_get_icon_count(void) { return g_dwm.icon_count; }

DosGuiWM *wm_state(void) { return &g_dwm; }

/* EOF */
