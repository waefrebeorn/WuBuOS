/*
 * dosgui_wm_ctxmenu.c  --  Context Menu + Clock + Desktop Ctx
 *
 * Extracted from dosgui_wm.c for modularity.
 */

#include "dosgui_wm_internal.h"

/* ================================================================
 * CLOCK
 * ================================================================ */

void dosgui_taskbar_update_clock(time_t now) {
    g_dwm.last_clock_update = now;
}

char *dosgui_taskbar_get_clock_str(void) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    static char clk[16];
    snprintf(clk, sizeof(clk), "%02d:%02d", tm->tm_hour, tm->tm_min);
    return clk;
}

/* Global context menu stack */
DosGuiContextMenu *g_dosgui_ctx_stack = NULL;

/* -- Context Menu Stack Management -- */

static void ctx_menu_push(DosGuiContextMenu *menu) {
    menu->parent = g_dosgui_ctx_stack;
    g_dosgui_ctx_stack = menu;
}

static void ctx_menu_pop(void) {
    if (g_dosgui_ctx_stack) {
        DosGuiContextMenu *old = g_dosgui_ctx_stack;
        g_dosgui_ctx_stack = old->parent;
        old->parent = NULL;
    }
}

DosGuiContextMenu *dosgui_ctx_menu_create(int x, int y) {
    DosGuiContextMenu *menu = (DosGuiContextMenu*)calloc(1, sizeof(DosGuiContextMenu));
    if (!menu) return NULL;
    menu->x = x;
    menu->y = y;
    menu->visible = false;
    menu->selected_item = -1;
    menu->item_count = 0;
    return menu;
}

void dosgui_ctx_menu_add_item(DosGuiContextMenu *menu, const char *label,
                               void (*action)(void)) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_ACTION;
    item->action = action;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->disabled = false;
    item->checked = false;
    menu->item_count++;
}

void dosgui_ctx_menu_add_separator(DosGuiContextMenu *menu) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SEPARATOR;
    menu->item_count++;
}

DosGuiContextMenu *dosgui_ctx_menu_add_submenu(DosGuiContextMenu *menu, const char *label) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return NULL;
    DosGuiContextMenu *submenu = dosgui_ctx_menu_create(0, 0);
    if (!submenu) return NULL;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SUBMENU;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->submenu = submenu;
    menu->item_count++;
    return submenu;
}

void dosgui_ctx_menu_show(DosGuiContextMenu *menu, int x, int y) {
    if (!menu) return;
    menu->x = x;
    menu->y = y;
    menu->visible = true;
    menu->selected_item = 0;
    /* Find first non-separator item */
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = i;
            break;
        }
    }
    ctx_menu_push(menu);
}

void dosgui_ctx_menu_hide(DosGuiContextMenu *menu) {
    if (!menu) return;
    menu->visible = false;
    if (g_dosgui_ctx_stack == menu) {
        ctx_menu_pop();
    }
}

void dosgui_ctx_menu_handle_mouse(int x, int y, int btn, int kind) {
    if (!g_dosgui_ctx_stack) return;
    
    DosGuiContextMenu *menu = g_dosgui_ctx_stack;
    int item_h = 24;
    int menu_w = 180;
    int menu_x = menu->x;
    int menu_y = menu->y;
    
    /* Check if click is outside menu */
    if (x < menu_x || x >= menu_x + menu_w || y < menu_y || y >= menu_y + menu->item_count * item_h) {
        /* Pop all menus */
        while (g_dosgui_ctx_stack) {
            ctx_menu_pop();
        }
        return;
    }
    
    if (kind == 0) { /* Mouse move */
        int item = (y - menu_y) / item_h;
        if (item >= 0 && item < menu->item_count && menu->items[item].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = item;
        }
    } else if (kind == 1) { /* Mouse down */
        int item = (y - menu_y) / item_h;
        if (item >= 0 && item < menu->item_count) {
            DosGuiCtxItem *it = &menu->items[item];
            if (it->type == CTX_ITEM_ACTION && it->action && !it->disabled) {
                it->action();
                while (g_dosgui_ctx_stack) ctx_menu_pop();
            } else if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                /* Show submenu to the right */
                dosgui_ctx_menu_show(it->submenu, menu_x + menu_w, menu_y + item * item_h);
            }
        }
    }
}

void dosgui_ctx_menu_render(uint32_t *fb, int fb_w, int fb_h) {
    
    DosGuiContextMenu *menu = g_dosgui_ctx_stack;
    while (menu) {
        if (!menu->visible) {
            menu = menu->parent;
            continue;
        }
        
        int item_h = 24;
        int menu_w = 180;
        int menu_h = menu->item_count * item_h;
        int mx = menu->x;
        int my = menu->y;
        
        /* Clamp to screen */
        if (mx + menu_w > fb_w) mx = fb_w - menu_w;
        if (my + menu_h > fb_h) my = fb_h - menu_h;
        if (mx < 0) mx = 0;
        if (my < 0) my = 0;
        menu->x = mx;
        menu->y = my;
        
        /* Draw menu background */
        vbe_fill_rect_rounded(mx, my, menu_w, menu_h, 4, tc()->win_face);
        vbe_3d_sunken_rounded_colors(mx, my, menu_w, menu_h, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        
        /* Draw items */
        for (int i = 0; i < menu->item_count; i++) {
            int y = my + i * item_h;
            DosGuiCtxItem *it = &menu->items[i];
            
            if (it->type == CTX_ITEM_SEPARATOR) {
                vbe_hline(mx + 10, mx + menu_w - 10, y + item_h / 2, tc()->border_dark);
                continue;
            }
            
            /* Highlight selected */
            if (i == menu->selected_item && it->type != CTX_ITEM_SUBMENU) {
                vbe_fill_rect(mx + 2, y, menu_w - 4, item_h, tc()->select_bg);
            }
            
            /* Draw label */
            uint32_t text_color = it->disabled ? 0x808080 : tc()->win_title_text;
            vbe_draw_text(mx + 10, y + (item_h - 8) / 2, it->label, text_color, 1);
            
            /* Draw submenu indicator */
            if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                vbe_draw_text(mx + menu_w - 20, y + (item_h - 8) / 2, ">", text_color, 1);
            }
            
            /* Draw checkmark */
            if (it->checked) {
                vbe_draw_text(mx + 2, y + (item_h - 8) / 2, "*", text_color, 1);
            }
        }
        
        menu = menu->parent;
    }
}



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

static void ctx_action_sort_by_name(void) {
    /* Real alphabetical re-flow. */
    dosgui_wm_sort_icons_by_name();
    wubu_notify_simple("Desktop", "Sort By Name",
                       "Icons arranged alphabetically", NULL, 1, 1500);
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
    dosgui_ctx_menu_add_item(newmenu, "Folder", NULL);
    dosgui_ctx_menu_add_item(newmenu, "Text Document", NULL);
    
    dosgui_ctx_menu_add_separator(menu);
    dosgui_ctx_menu_add_item(menu, "View", ctx_action_view_desktop);
    
    DosGuiContextMenu *viewmenu = dosgui_ctx_menu_add_submenu(menu, "Sort By");
    dosgui_ctx_menu_add_item(viewmenu, "Name", ctx_action_sort_by_name);
    dosgui_ctx_menu_add_item(viewmenu, "Size", NULL);
    dosgui_ctx_menu_add_item(viewmenu, "Type", NULL);
    dosgui_ctx_menu_add_item(viewmenu, "Date Modified", NULL);
    
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

/* -- Window Resize and State Management ----------------------------- */

void dosgui_wm_resize(DosGuiWindow *win, int w, int h) {
    if (!win) return;
    if (w < 100) w = 100;
    if (h < 50) h = 50;
    if (w > g_dwm.screen_w) w = g_dwm.screen_w;
    if (h > g_dwm.screen_h - dosgui_taskbar_height()) h = g_dwm.screen_h - dosgui_taskbar_height();
    win->w = w;
    win->h = h;
    if (win->on_resize) {
        win->on_resize(win, w, h);
    }
}

void dosgui_wm_move(DosGuiWindow *win, int x, int y) {
    if (!win) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + win->w > g_dwm.screen_w) x = g_dwm.screen_w - win->w;
    if (y + win->h > g_dwm.screen_h - dosgui_taskbar_height()) y = g_dwm.screen_h - dosgui_taskbar_height() - win->h;
    win->x = x;
    win->y = y;
}

void dosgui_wm_maximize(DosGuiWindow *win) {
    if (!win) return;
    if (win->flags & DOSGUI_WIN_MAXIMIZED) return;
    win->min_x = win->x;
    win->min_y = win->y;
    win->min_w = win->w;
    win->min_h = win->h;
    win->x = 0;
    win->y = 0;
    win->w = g_dwm.screen_w;
    win->h = g_dwm.screen_h - dosgui_taskbar_height();
    win->flags |= DOSGUI_WIN_MAXIMIZED;
    if (win->on_resize) {
        win->on_resize(win, win->w, win->h);
    }
}

void dosgui_wm_minimize(DosGuiWindow *win) {
    if (!win) return;
    win->flags |= DOSGUI_WIN_MINIMIZED;
}

void dosgui_wm_restore(DosGuiWindow *win) {
    if (!win) return;
    if (win->flags & DOSGUI_WIN_MAXIMIZED) {
        win->x = win->min_x;
        win->y = win->min_y;
        win->w = win->min_w;
        win->h = win->min_h;
        win->flags &= ~DOSGUI_WIN_MAXIMIZED;
        if (win->on_resize) {
            win->on_resize(win, win->w, win->h);
        }
    }
    win->flags &= ~DOSGUI_WIN_MINIMIZED;
}

bool dosgui_wm_is_maximized(DosGuiWindow *win) {
    return win && (win->flags & DOSGUI_WIN_MAXIMIZED);
}

bool dosgui_wm_is_minimized(DosGuiWindow *win) {
    return win && (win->flags & DOSGUI_WIN_MINIMIZED);
}

/* -- Modal Dialog Support ------------------------------------------- */

DosGuiWindow *dosgui_wm_create_modal(int x, int y, int w, int h,
                                      const char *title,
                                      DosGuiWindow *parent) {
    DosGuiWindow *win = dosgui_wm_create(x, y, w, h, title);
    if (!win) return NULL;
    win->is_modal = true;
    win->parent = parent;
    /* Raise above parent */
    if (parent) {
        int parent_idx = -1;
        for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
            if (&g_dwm.windows[i] == parent) { parent_idx = i; break; }
        }
        if (parent_idx >= 0) {
            /* Insert modal just above parent in z-order */
            for (int j = g_dwm.nz - 1; j >= 0; j--) {
                if (g_dwm.zorder[j] == parent_idx) {
                    if (j + 1 < g_dwm.nz) {
                        memmove(&g_dwm.zorder[j + 2], &g_dwm.zorder[j + 1],
                                (g_dwm.nz - j - 1) * sizeof(int));
                    }
                    g_dwm.zorder[j + 1] = win - g_dwm.windows;
                    break;
                }
            }
        }
    }
    return win;
}

bool dosgui_wm_is_modal(DosGuiWindow *win) {
    return win && win->is_modal;
}