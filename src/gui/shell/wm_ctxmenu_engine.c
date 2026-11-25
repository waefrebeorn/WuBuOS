/*
 * wm_ctxmenu_engine.c -- WuBuOS DosGui WM: context-menu engine
 *
 * Self-contained concern split out of wm_ctxmenu.c: the generic
 * context-menu stack, lifecycle (create/add/show/hide), mouse dispatch,
 * and themed rendering. Purely mechanical; knows nothing about the desktop
 * or icon actions that call into it. Depends only on the shared WM state
 * (../wm_internal.h) and the VBE/theme primitives.
 */

#include "../wm_internal.h"

/* Global context menu stack */
DosGuiContextMenu *g_ctx_stack = NULL;

/* -- Context Menu Stack Management -- */

static void ctx_menu_push(DosGuiContextMenu *menu) {
    menu->parent = g_ctx_stack;
    g_ctx_stack = menu;
}

static void ctx_menu_pop(void) {
    if (g_ctx_stack) {
        DosGuiContextMenu *old = g_ctx_stack;
        g_ctx_stack = old->parent;
        old->parent = NULL;
    }
}

DosGuiContextMenu *ctx_menu_create(int x, int y) {
    DosGuiContextMenu *menu = (DosGuiContextMenu*)calloc(1, sizeof(DosGuiContextMenu));
    if (!menu) return NULL;
    menu->x = x;
    menu->y = y;
    menu->visible = false;
    menu->selected_item = -1;
    menu->item_count = 0;
    return menu;
}

void ctx_menu_add_item(DosGuiContextMenu *menu, const char *label,
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

void ctx_menu_add_separator(DosGuiContextMenu *menu) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SEPARATOR;
    menu->item_count++;
}

DosGuiContextMenu *ctx_menu_add_submenu(DosGuiContextMenu *menu, const char *label) {
    if (!menu || menu->item_count >= DOSGUI_MAX_CTX_ITEMS) return NULL;
    DosGuiContextMenu *submenu = ctx_menu_create(0, 0);
    if (!submenu) return NULL;
    DosGuiCtxItem *item = &menu->items[menu->item_count];
    item->type = CTX_ITEM_SUBMENU;
    strncpy(item->label, label, sizeof(item->label) - 1);
    item->submenu = submenu;
    menu->item_count++;
    return submenu;
}

/* Is a menu on screen? A menu that can be opened but never asked about is untestable,
 * and "right-click did nothing" is exactly the failure mode this shell keeps finding. */
bool ctx_menu_visible(void) { return g_ctx_stack != NULL && g_ctx_stack->visible; }

/* Close every menu (test isolation, and window/task destruction). */
void ctx_menu_hide_all(void) {
    while (g_ctx_stack) ctx_menu_pop();
}

void ctx_menu_show(DosGuiContextMenu *menu, int x, int y) {
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

void ctx_menu_hide(DosGuiContextMenu *menu) {
    if (!menu) return;
    menu->visible = false;
    if (g_ctx_stack == menu) {
        ctx_menu_pop();
    }
}

/* Win11 menu metrics. Before this the menu had NO vertical padding (height was literally
 * item_count * 24), 24px rows and a 180px width -- items ran into the card edge, which is the
 * "menu margins are bad" the user reported. One owner, so the hit-test and the renderer cannot
 * disagree about where a row is. */
#define CTX_ITEM_H   30
#define CTX_MENU_W   210
#define CTX_PAD_V    8
#define CTX_PAD_H    6
static inline int ctx_menu_height(int items) { return items * CTX_ITEM_H + 2 * CTX_PAD_V; }

void ctx_menu_handle_mouse(int x, int y, int btn, int kind) {
    if (!g_ctx_stack) return;

    DosGuiContextMenu *menu = g_ctx_stack;
    int item_h = CTX_ITEM_H;
    int menu_w = CTX_MENU_W;
    int menu_x = menu->x;
    int menu_y = menu->y;

    /* Check if click is outside menu */
    if (x < menu_x || x >= menu_x + menu_w ||
        y < menu_y || y >= menu_y + ctx_menu_height(menu->item_count)) {
        /* Pop all menus */
        while (g_ctx_stack) {
            ctx_menu_pop();
        }
        return;
    }

    if (kind == 0) { /* Mouse move */
        int item = (y - menu_y - CTX_PAD_V) / item_h;
        if (y >= menu_y + CTX_PAD_V && item >= 0 && item < menu->item_count &&
            menu->items[item].type != CTX_ITEM_SEPARATOR) {
            menu->selected_item = item;
        }
    } else if (kind == 1) { /* Mouse down */
        int item = (y - menu_y - CTX_PAD_V) / item_h;
        if (y < menu_y + CTX_PAD_V) item = -1;
        if (item >= 0 && item < menu->item_count) {
            DosGuiCtxItem *it = &menu->items[item];
            if (it->type == CTX_ITEM_ACTION && it->action && !it->disabled) {
                it->action();
                while (g_ctx_stack) ctx_menu_pop();
            } else if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                /* Show submenu to the right */
                ctx_menu_show(it->submenu, menu_x + menu_w,
                              menu_y + CTX_PAD_V + item * item_h);
            }
        }
    }
}

void ctx_menu_render(uint32_t *fb, int fb_w, int fb_h) {

    DosGuiContextMenu *menu = g_ctx_stack;
    while (menu) {
        if (!menu->visible) {
            menu = menu->parent;
            continue;
        }

        int item_h = CTX_ITEM_H;
        int menu_w = CTX_MENU_W;
        int menu_h = ctx_menu_height(menu->item_count);
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
            int y = my + CTX_PAD_V + i * item_h;
            DosGuiCtxItem *it = &menu->items[i];

            if (it->type == CTX_ITEM_SEPARATOR) {
                vbe_hline(mx + CTX_PAD_H + 8, mx + menu_w - CTX_PAD_H - 8,
                          y + item_h / 2, tc()->border_dark);
                continue;
            }

            /* Win11 hover: a rounded pill INSET from the card edge, not a full-bleed bar
             * that touches the border. The submenu row is excluded on purpose (its own row
             * opens on hover; highlighting it would compete with the child menu). */
            if (i == menu->selected_item && it->type != CTX_ITEM_SUBMENU) {
                vbe_fill_rect_rounded(mx + CTX_PAD_H, y + 2, menu_w - 2 * CTX_PAD_H,
                                      item_h - 4, 4, tc()->select_bg);
            }

            /* Draw label */
            uint32_t text_color = it->disabled ? 0x808080 : tc()->win_title_text;
            vbe_draw_text(mx + CTX_PAD_H + 8, y + (item_h - 8) / 2, it->label, text_color, 1);

            /* Draw submenu indicator */
            if (it->type == CTX_ITEM_SUBMENU && it->submenu) {
                vbe_draw_text(mx + menu_w - CTX_PAD_H - 14, y + (item_h - 8) / 2, ">",
                              text_color, 1);
            }

            /* Draw checkmark */
            if (it->checked) {
                vbe_draw_text(mx + 2, y + (item_h - 8) / 2, "*", text_color, 1);
            }
        }

        menu = menu->parent;
    }
}
