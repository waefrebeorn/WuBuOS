/*
 * dosgui_wm_ctxmenu_engine.c -- WuBuOS DosGui WM: context-menu engine
 *
 * Self-contained concern split out of dosgui_wm_ctxmenu.c: the generic
 * context-menu stack, lifecycle (create/add/show/hide), mouse dispatch,
 * and themed rendering. Purely mechanical; knows nothing about the desktop
 * or icon actions that call into it. Depends only on the shared WM state
 * (dosgui_wm_internal.h) and the VBE/theme primitives.
 */

#include "dosgui_wm_internal.h"

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
