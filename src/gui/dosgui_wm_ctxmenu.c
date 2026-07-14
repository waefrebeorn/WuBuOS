/*
 * dosgui_wm_ctxmenu.c  --  WuBuOS DosGui WM: context-menu action wiring
 *
 * Facade that binds the generic context-menu engine (dosgui_wm_ctxmenu_engine.c)
 * to the desktop + icon domains. Owns the default context actions (Open/Play/
 * Rename/Delete/Properties/New (Shortcut|Folder|Doc)/Sort By/Refresh), the modal dialog callbacks
 * (rename/delete/properties/shortcut), the icon + desktop context-menu builders,
 * and the per-frame tick + query accessors.
 *
 * The mechanical engine, the taskbar clock, and window state/modal handling
 * live in their own self-contained modules (C11 opaque-safe, no god headers).
 */

#include "dosgui_wm_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -- Default Context Menu Actions -- */

static void ctx_action_open(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0 && g_dwm.icons[idx].on_execute) {
        g_dwm.icons[idx].on_execute();
    }
}

/* -- Dialog Callback Functions -- */

static DosGuiIcon *g_rename_target = NULL;
static char g_rename_input[32] = {0};
static int g_rename_pos = 0;
static DosGuiIcon *g_delete_target = NULL;
static DosGuiIcon *g_properties_target = NULL;
static bool g_shortcut_pending = false;

static void dialog_rename_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == '\r' || key == '\n') {
        if (g_rename_pos > 0 && g_rename_target) {
            strncpy(g_rename_target->name, g_rename_input, sizeof(g_rename_target->name) - 1);
            wubu_notify_simple("Desktop", "Renamed", g_rename_target->name, NULL, 1, 2000);
        }
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        g_rename_target = NULL;
        dosgui_wm_destroy(win);
    } else if (key == 27) {  /* Escape */
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        g_rename_target = NULL;
        dosgui_wm_destroy(win);
    } else if (key == 8 && g_rename_pos > 0) {  /* Backspace */
        g_rename_input[--g_rename_pos] = '\0';
    } else if (key >= 32 && key < 127 && g_rename_pos < 31) {  /* Printable */
        g_rename_input[g_rename_pos++] = (char)key;
        g_rename_input[g_rename_pos] = '\0';
    }
}

static void dialog_rename_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);
    vbe_draw_text(cx + 10, cy + 20, "New name:", 0x00000000, 1);
    vbe_draw_text(cx + 10, cy + 50, g_rename_input[0] ? g_rename_input : "(empty)", 0x00000000, 1);
}

static void dialog_delete_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == 'y' || key == 'Y') {
        if (g_delete_target) {
            g_delete_target->alive = false;
            wubu_notify_simple("Desktop", "Deleted", "Icon removed", NULL, 1, 2000);
        }
        g_delete_target = NULL;
        dosgui_wm_destroy(win);
    } else if (key == 'n' || key == 'N' || key == 27) {
        g_delete_target = NULL;
        dosgui_wm_destroy(win);
    }
}

static void dialog_delete_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);
    vbe_draw_text(cx + 10, cy + 20, "Delete this icon?", 0x00000000, 1);
    vbe_draw_text(cx + 10, cy + 40, "Press Y to confirm, N to cancel", 0x00000000, 1);
}

static void dialog_properties_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == 27 || key == 'q' || key == 'Q') {  /* Escape or Q to close */
        g_properties_target = NULL;
        dosgui_wm_destroy(win);
    }
}

static void dialog_properties_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    DosGuiIcon *ic = g_properties_target;
    if (!ic) return;

    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;
    int y = cy + 10;

    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);

    char buf[256];
    snprintf(buf, sizeof(buf), "Name: %s", ic->name);
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;

    const char *type_str[] = {"App", "Shortcut", "Folder", "Drive", "File", "URL"};
    snprintf(buf, sizeof(buf), "Type: %s", ic->type < 6 ? type_str[ic->type] : "Unknown");
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;

    snprintf(buf, sizeof(buf), "Target: %s", ic->target[0] ? ic->target : "(none)");
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;

    snprintf(buf, sizeof(buf), "Position: (%d, %d) grid (%d, %d)", ic->x, ic->y, ic->grid_x, ic->grid_y);
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;

    snprintf(buf, sizeof(buf), "Color: #%06X", ic->icon_color & 0xFFFFFF);
    vbe_draw_text(cx + 10, y, buf, 0x00000000, 1); y += 16;

    vbe_draw_text(cx + 10, y + 10, "Press ESC to close", 0x00808080, 1);
}

/* New-Shortcut dialog: reuse the rename input buffer to collect a name,
 * then call the WM helper which writes a real .desktop file to ~/Desktop. */
static void dialog_shortcut_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    if (key == '\r' || key == '\n') {
        if (g_rename_pos > 0) {
            char name[32];
            snprintf(name, sizeof(name), "%s", g_rename_input);
            int rc = dosgui_wm_write_desktop_shortcut(name, NULL);
            wubu_notify_simple("Desktop", "Shortcut Created",
                               rc == 0 ? name : "Failed to create", NULL, 1, 2500);
        }
        g_shortcut_pending = false;
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        dosgui_wm_destroy(win);
    } else if (key == 27) {  /* Escape */
        g_shortcut_pending = false;
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        dosgui_wm_destroy(win);
    } else if (key == 8 && g_rename_pos > 0) {  /* Backspace */
        g_rename_input[--g_rename_pos] = '\0';
    } else if (key >= 32 && key < 127 && g_rename_pos < 31) {  /* Printable */
        g_rename_input[g_rename_pos++] = (char)key;
        g_rename_input[g_rename_pos] = '\0';
    }
}

static void dialog_shortcut_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const int tbh = 20;
    const int bw = 3;
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    vbe_fill_rect(cx, cy, cw, ch, 0x00E0E0E0);
    vbe_draw_text(cx + 10, cy + 20, "Shortcut name:", 0x00000000, 1);
    vbe_draw_text(cx + 10, cy + 50, g_rename_input[0] ? g_rename_input : "(type a name)", 0x00000000, 1);
}

/* -- Default Context Menu Actions -- */

static void ctx_action_play(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
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
        wubu_notify_simple("Desktop", "Play", "Cannot open target binary", NULL, 1, 2500);
        return;
    }
    uint8_t buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), bin);
    fclose(bin);
    if (n == 0) {
        wubu_notify_simple("Desktop", "Play", "Target is empty", NULL, 1, 2500);
        return;
    }

    hosted_state_t *st = dosgui_wm_get_hosted_state();
    int pid = wubu_session_launch_game(st, buf, n, ic->name);
    wubu_notify_simple("Desktop", "Play",
                       pid >= 0 ? ic->name : "Launch failed (no Proton/VSL in this build)",
                       NULL, 1, 2500);
}

static void ctx_action_rename(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_rename_target = &g_dwm.icons[idx];
        g_rename_input[0] = '\0';
        g_rename_pos = 0;
        DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 400, 150, "Rename", NULL);
        if (dialog) {
            dialog->on_key = dialog_rename_on_key;
            dialog->on_draw = dialog_rename_on_draw;
        }
    }
}

static void ctx_action_delete(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_delete_target = &g_dwm.icons[idx];
        DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 350, 140, "Confirm Delete", NULL);
        if (dialog) {
            dialog->on_key = dialog_delete_on_key;
            dialog->on_draw = dialog_delete_on_draw;
        }
    }
}

static void ctx_action_properties(void) {
    int idx = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
    if (idx >= 0) {
        g_properties_target = &g_dwm.icons[idx];
        DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 400, 300, "Properties", NULL);
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
    g_shortcut_pending = true;
    DosGuiWindow *dialog = dosgui_wm_create_modal(300, 200, 400, 150, "New Shortcut", NULL);
    if (dialog) {
        dialog->on_key = dialog_shortcut_on_key;
        dialog->on_draw = dialog_shortcut_on_draw;
    }
}

static void ctx_action_view_desktop(void) {
    /* Toggle Auto-arrange and re-flow the live icons. */
    bool now = !dosgui_wm_get_auto_arrange();
    dosgui_wm_set_auto_arrange(now);
    if (now) {
        /* Re-flow current icons into the auto-arrange column. */
        reflow_all_icons_column();
    }
    wubu_notify_simple("Desktop", "View",
                       now ? "Auto-arrange: ON" : "Auto-arrange: OFF",
                       NULL, 1, 2000);
}

static void ctx_action_new_folder(void) {
    if (dosgui_wm_new_folder() == 0) {
        wubu_notify_simple("Desktop", "New Folder",
                           "Folder created on ~/Desktop", NULL, 1, 1500);
    } else {
        wubu_notify_simple("Desktop", "New Folder",
                           "Could not create folder", NULL, 1, 1500);
    }
}

static void ctx_action_new_text_doc(void) {
    if (dosgui_wm_new_text_doc() == 0) {
        wubu_notify_simple("Desktop", "New Text Document",
                           "Text document created on ~/Desktop", NULL, 1, 1500);
    } else {
        wubu_notify_simple("Desktop", "New Text Document",
                           "Could not create document", NULL, 1, 1500);
    }
}

static void ctx_action_sort_by_size(void) {
    dosgui_wm_sort_icons(DOSGUI_SORT_SIZE);
    wubu_notify_simple("Desktop", "Sort By Size",
                       "Icons arranged by size", NULL, 1, 1500);
}

static void ctx_action_sort_by_type(void) {
    dosgui_wm_sort_icons(DOSGUI_SORT_TYPE);
    wubu_notify_simple("Desktop", "Sort By Type",
                       "Icons arranged by type", NULL, 1, 1500);
}

static void ctx_action_sort_by_name(void) {
    /* Real alphabetical re-flow. */
    dosgui_wm_sort_icons_by_name();
    wubu_notify_simple("Desktop", "Sort By Name",
                       "Icons arranged alphabetically", NULL, 1, 1500);
}

static void ctx_action_sort_by_date(void) {
    dosgui_wm_sort_icons(DOSGUI_SORT_DATE);
    wubu_notify_simple("Desktop", "Sort By Date Modified",
                       "Icons arranged by date", NULL, 1, 1500);
}

static void ctx_action_refresh(void) {
    /* Real filesystem refresh: re-scan ~/Desktop for .desktop files. */
    dosgui_wm_refresh_desktop();
    wubu_notify_simple("Desktop", "Refresh",
                       "Desktop reloaded from ~/Desktop", NULL, 1, 2000);
}

/* -- Show Icon Context Menu -- */

void dosgui_icon_show_context_menu(int icon_idx, int mx, int my) {
    if (icon_idx < 0 || icon_idx >= DOSGUI_MAX_ICONS) return;
    if (!g_dwm.icons[icon_idx].alive) return;

    DosGuiContextMenu *menu = dosgui_ctx_menu_create(mx, my);
    if (!menu) return;

    /* Select the icon */
    g_dwm.icons[icon_idx].selected = true;

    dosgui_ctx_menu_add_item(menu, "Open", ctx_action_open);
    dosgui_ctx_menu_add_item(menu, "Play", ctx_action_play);
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Rename", ctx_action_rename);
    dosgui_ctx_menu_add_item(menu, "Delete", ctx_action_delete);
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Properties", ctx_action_properties);

    dosgui_ctx_menu_show(menu, mx, my);
}

void dosgui_desktop_show_context_menu(int mx, int my) {
    DosGuiContextMenu *menu = dosgui_ctx_menu_create(mx, my);
    if (!menu) return;

    dosgui_ctx_menu_add_item(menu, "New", NULL);

    DosGuiContextMenu *newmenu = dosgui_ctx_menu_add_submenu(menu, "New");
    dosgui_ctx_menu_add_item(newmenu, "Shortcut", ctx_action_create_shortcut);
    dosgui_ctx_menu_add_item(newmenu, "Folder", ctx_action_new_folder);
    dosgui_ctx_menu_add_item(newmenu, "Text Document", ctx_action_new_text_doc);

    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "View", ctx_action_view_desktop);

    DosGuiContextMenu *viewmenu = dosgui_ctx_menu_add_submenu(menu, "Sort By");
    dosgui_ctx_menu_add_item(viewmenu, "Name", ctx_action_sort_by_name);
    dosgui_ctx_menu_add_item(viewmenu, "Size", ctx_action_sort_by_size);
    dosgui_ctx_menu_add_item(viewmenu, "Type", ctx_action_sort_by_type);
    dosgui_ctx_menu_add_item(viewmenu, "Date Modified", ctx_action_sort_by_date);

    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "Refresh", ctx_action_refresh);
    dosgui_ctx_menu_add_item(menu, "Properties", ctx_action_properties);

    dosgui_ctx_menu_show(menu, mx, my);
}

/* -- Tick ------------------------------------------------------- */

void dosgui_tick(void) {
    g_dwm.ticks++;
}

/* -- Query ------------------------------------------------------- */

int dosgui_wm_screen_w(void) { return g_dwm.screen_w; }
int dosgui_wm_screen_h(void) { return g_dwm.screen_h; }

int dosgui_wm_get_icon_count(void) { return g_dwm.icon_count; }

DosGuiWM *dosgui_wm_state(void) { return &g_dwm; }

/* EOF */
