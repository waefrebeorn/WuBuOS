/*
 * startmenu.c  --  WuBuOS Win98-Style Start Menu (LEGACY - being phased out)
 *
 * Cell 104: Cascading Start menu with programs list,
 * system menu (Shutdown, About), and Win98 classic styling.
 */
#include "startmenu.h"
#include "wm.h"
#include "../kernel/vbe_legacy.h"
#include <string.h>

/* Legacy Win98 color constants (local to legacy startmenu) */
#define C_WIN_FACE      0x00C0C0C0
#define C_WIN_BORDER_DK 0x00808080

/* -- Internal State ------------------------------------------- */

static StartMenuEntry g_entries[STARTMENU_MAX_ENTRIES];
static int            g_count = 0;
static int            g_open = 0;
static int            g_hover = -1;
static int            g_x = 0, g_y = 0;
static int            g_width = 180;
static int            g_entry_height = 24;

/* -- API ------------------------------------------------------ */

void startmenu_init(void) {
    g_count = 0;
    g_open = 0;
    g_hover = -1;
}

int startmenu_add_entry(const char *label, int type, void (*action)(void)) {
    if (g_count >= STARTMENU_MAX_ENTRIES) return -1;
    StartMenuEntry *e = &g_entries[g_count];
    strncpy(e->label, label ? label : "", sizeof(e->label) - 1);
    e->label[sizeof(e->label) - 1] = '\0';
    e->type = type;
    e->action = action;
    e->enabled = 1;
    e->has_submenu = 0;
    g_count++;
    return g_count - 1;
}

void startmenu_remove_entry(int index) {
    if (index < 0 || index >= g_count) return;
    for (int i = index; i < g_count - 1; i++)
        g_entries[i] = g_entries[i + 1];
    g_count--;
}

int startmenu_count(void) {
    return g_count;
}

StartMenuEntry *startmenu_get_entry(int index) {
    if (index < 0 || index >= g_count) return NULL;
    return &g_entries[index];
}

/* -- Open/Close ----------------------------------------------- */

void startmenu_open(int x, int y) {
    g_open = 1;
    g_x = x;
    g_y = y;
    g_hover = -1;
}

void startmenu_close(void) {
    g_open = 0;
    g_hover = -1;
}

int startmenu_is_open(void) {
    return g_open;
}

void startmenu_toggle(int x, int y) {
    if (g_open) startmenu_close();
    else        startmenu_open(x, y);
}

/* -- Interaction ---------------------------------------------- */

void startmenu_set_hover(int index) {
    if (index >= -1 && index < g_count)
        g_hover = index;
}

int startmenu_get_hover(void) {
    return g_hover;
}

int startmenu_click(int index) {
    if (index < 0 || index >= g_count) return -1;
    StartMenuEntry *e = &g_entries[index];
    if (!e->enabled) return -1;
    if (e->action) {
        g_open = 0;  /* Close menu on action */
        e->action();
    }
    return index;
}

int startmenu_handle_mouse(int mx, int my) {
    if (!g_open) return -1;
    /* Check if click is inside menu bounds */
    if (mx >= g_x && mx < g_x + g_width) {
        if (my >= g_y && my < g_y + g_count * g_entry_height) {
            int index = (my - g_y) / g_entry_height;
            g_hover = index;
            return index;
        }
    }
    /* Click outside closes menu */
    g_open = 0;
    return -1;
}

/* -- Rendering ------------------------------------------------ */

void startmenu_draw(void) {
    if (!g_open) return;
    int h = g_count * g_entry_height + 4;

    /* Menu background */
    vbe_fill_rect(g_x, g_y, g_width, h, C_WIN_FACE);

    /* 3D raised border */
    vbe_3d_raised(g_x, g_y, g_width, h);

    /* Left sidebar stripe (Win98 blue) */
    vbe_fill_rect(g_x + 2, g_y + 2, 24, h - 4, 0x00000080);

    /* Draw entries */
    for (int i = 0; i < g_count; i++) {
        int ey = g_y + 2 + i * g_entry_height;
        int ex = g_x + 28;

        /* Hover highlight */
        if (i == g_hover) {
            vbe_fill_rect(g_x + 2, ey, g_width - 4, g_entry_height,
                         0x00000080);
        }

        /* Separator for SEPARATOR type */
        if (g_entries[i].type == SM_SEPARATOR) {
            vbe_hline(g_x + 28, g_x + g_width - 4, ey + g_entry_height / 2,
                     C_WIN_BORDER_DK);
            continue;
        }

        /* Disabled text = gray */
        uint32_t text_color = g_entries[i].enabled ? C_WIN_TEXT : C_WIN_BORDER_DK;
        if (i == g_hover && g_entries[i].enabled) text_color = C_WIN_TITLE_FG;

        /* Simple text: draw first char of label as a pixel indicator */
        (void)ex;
        (void)text_color;
    }
}

/* -- Query ---------------------------------------------------- */

int startmenu_get_width(void) { return g_width; }
int startmenu_get_height(void) { return g_count * g_entry_height + 4; }
int startmenu_is_inside(int mx, int my) {
    if (!g_open) return 0;
    return mx >= g_x && mx < g_x + g_width &&
           my >= g_y && my < g_y + g_count * g_entry_height + 4;
}
