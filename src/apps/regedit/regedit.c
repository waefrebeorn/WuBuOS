/*
 * regedit.c  --  Windows Registry Editor Clone
 *
 * Two-pane registry editor: the key tree on the left (roots HKCR/HKCU/HKLM/
 * HKU/HKCC/HKDD plus their children) and the values of the selected key on the
 * right.
 *
 * This file used to be a blank window: regedit_launch() returned wm_create()
 * and bound no callbacks, and regedit_draw() was a (void)-everything stub, so
 * opening Registry Editor gave an empty frame while the tree model underneath
 * was real. The geometry now has ONE owner (reg_panes/reg_layout): the renderer
 * paints those rows and regedit_mouse() hit-tests the same ones.
 */

#include "regedit.h"
#include "../gui/wm.h"
#include "../gui/wm_internal.h"     /* tc(): the theme's live colours */
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../kernel/input.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* RegKey is defined in regedit.h (public leaf struct). */

struct RegValue {
    char name[REG_MAX_NAME];
    RegType type;
    uint8_t data[REG_MAX_DATA];
    int data_len;
    struct RegKey *owner;   /* the key this value hangs from */
};

#define REG_MAX_ROWS 256
#define REG_ROW_H    14
#define REG_HEADER_H 20

/* One visible tree row. Built by reg_build_rows(), given pixel geometry by
 * reg_layout(), painted by regedit_draw() and hit-tested by regedit_mouse(). */
typedef struct {
    struct RegKey *key;
    int depth;
    int y, h;
} RegRow;

/* Pixel geometry of the two panes, computed once per frame. */
typedef struct {
    int tx, ty, tw, th;   /* key tree   */
    int vx, vy, vw, vh;   /* value pane */
} RegPanes;

struct RegeditState {
    struct RegKey root_keys[6];
    struct RegKey *current_key;

    struct RegValue values[1024];
    int value_count;

    /* Pointer-valued on purpose: these used to be `int`, which truncates a
     * 64-bit pointer, so two keys could compare "equal" after the cast. */
    uintptr_t expanded_keys[REG_MAX_DEPTH];
    int expand_depth;

    char search_text[REG_MAX_NAME];
    bool search_dialog_open;

    RegRow rows[REG_MAX_ROWS];
    int row_count;
    int row_sel;
    RegPanes panes;        /* from the last draw: the mouse reads this */
};

RegeditState* regedit_create(void) {
    return calloc(1, sizeof(RegeditState));
}

void regedit_destroy(RegeditState *reg) {
    free(reg);
}

static void reg_build_rows(RegeditState *reg);

/* -- Expansion state --------------------------------------------- */

static bool reg_key_expanded(const RegeditState *reg, const struct RegKey *k) {
    for (int i = 0; i < reg->expand_depth; i++)
        if ((const struct RegKey *)(uintptr_t)reg->expanded_keys[i] == k)
            return true;
    return false;
}

void regedit_expand_key(RegeditState *reg, void *key) {
    if (!reg || !key) return;
    if (reg_key_expanded(reg, (const struct RegKey *)key)) return;
    if (reg->expand_depth < REG_MAX_DEPTH)
        reg->expanded_keys[reg->expand_depth++] = (uintptr_t)key;
    reg_build_rows(reg);
}

void regedit_collapse_key(RegeditState *reg, void *key) {
    if (!reg) return;
    for (int i = 0; i < reg->expand_depth; i++) {
        if ((void *)(uintptr_t)reg->expanded_keys[i] == key) {
            for (int j = i; j < reg->expand_depth - 1; j++)
                reg->expanded_keys[j] = reg->expanded_keys[j + 1];
            reg->expand_depth--;
            break;
        }
    }
    reg_build_rows(reg);
}

/* -- Visible rows ------------------------------------------------ */

static void reg_push_rows(RegeditState *reg, struct RegKey *k, int depth) {
    while (k && reg->row_count < REG_MAX_ROWS) {
        RegRow *row = &reg->rows[reg->row_count++];
        row->key = k;
        row->depth = depth;
        row->y = 0;
        row->h = 0;
        if (depth < REG_MAX_DEPTH && reg_key_expanded(reg, k))
            reg_push_rows(reg, k->children, depth + 1);
        k = k->next_sibling;
    }
}

static void reg_build_rows(RegeditState *reg) {
    if (!reg) return;
    reg->row_count = 0;
    reg_push_rows(reg, &reg->root_keys[0], 0);
    /* The highlight must be the CURRENT key, not "whatever row 0 is": the
     * editor used to select HKEY_CLASSES_ROOT while the pane showed HKCU. */
    for (int i = 0; i < reg->row_count; i++) {
        if (reg->rows[i].key == reg->current_key) {
            reg->row_sel = i;
            return;
        }
    }
    if (reg->row_sel >= reg->row_count) reg->row_sel = reg->row_count - 1;
    if (reg->row_sel < 0) reg->row_sel = 0;
}

/* -- Geometry (the single owner) --------------------------------- */

static void reg_panes(int cx, int cy, int cw, int ch, RegPanes *p) {
    int tw = (cw * 45) / 100;
    if (tw < 120) tw = cw / 2;
    if (tw > cw - 60) tw = cw - 60;
    p->tx = cx + 1;
    p->ty = cy + REG_HEADER_H;
    p->tw = tw;
    p->th = ch - REG_HEADER_H - 2;
    p->vx = p->tx + tw + 2;
    p->vy = p->ty;
    p->vw = cw - tw - 5;
    p->vh = p->th;
    if (p->tw < 2) p->tw = 2;
    if (p->vw < 2) p->vw = 2;
    if (p->th < 2) p->th = 2;
}

static void reg_layout(RegeditState *reg) {
    for (int i = 0; i < reg->row_count; i++) {
        reg->rows[i].y = reg->panes.ty + 2 + i * REG_ROW_H;
        reg->rows[i].h = REG_ROW_H;
    }
}

/* -- Rendering --------------------------------------------------- */

static const char *reg_type_name(RegType t) {
    switch (t) {
    case REG_SZ:        return "REG_SZ";
    case REG_DWORD:     return "REG_DWORD";
    case REG_QWORD:     return "REG_QWORD";
    case REG_BINARY:    return "REG_BINARY";
    case REG_MULTI_SZ:  return "REG_MULTI_SZ";
    case REG_EXPAND_SZ: return "REG_EXPAND_SZ";
    default:            return "REG_NONE";
    }
}

void regedit_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, RegeditState *reg) {
    if (!win || !reg) return;
    ChromeContentRect content = chrome_draw_window(win, fb, fb_w, fb_h);
    int cx = content.x, cy = content.y, cw = content.w, ch = content.h;
    if (cw < 40 || ch < 40) return;

    reg_build_rows(reg);
    reg_panes(cx, cy, cw, ch, &reg->panes);
    reg_layout(reg);

    /* Header: the path of the selected key. */
    vbe_draw_text(cx + 4, cy + 3, "Computer\\Registry", tc()->win_title_active, 1);

    /* Key tree. */
    vbe_fill_rect(reg->panes.tx, reg->panes.ty, reg->panes.tw, reg->panes.th,
                  tc()->btn_face);
    for (int i = 0; i < reg->row_count; i++) {
        RegRow *row = &reg->rows[i];
        if (row->y + row->h > reg->panes.ty + reg->panes.th) break;
        if (i == reg->row_sel)
            vbe_fill_rect(reg->panes.tx, row->y, reg->panes.tw, row->h,
                          tc()->win_title_active);
        uint32_t fg = (i == reg->row_sel) ? tc()->win_title_text : tc()->btn_text;
        int ind = reg->panes.tx + 3 + row->depth * 12;
        /* +/- affordance so it is visible which keys can be opened. */
        if (row->key->child_count > 0)
            vbe_draw_text(ind, row->y + 2,
                          reg_key_expanded(reg, row->key) ? "-" : "+", fg, 1);
        vbe_draw_text(ind + 10, row->y + 2, row->key->name, fg, 1);
    }

    /* Values of the selected key. */
    vbe_fill_rect(reg->panes.vx, reg->panes.vy, reg->panes.vw, reg->panes.vh,
                  tc()->btn_face);
    int vy = reg->panes.vy + 2;
    struct RegKey *cur = reg->current_key;
    vbe_draw_text(reg->panes.vx + 4, vy, "Name    Type    Data",
                  tc()->win_title_active, 1);
    vy += REG_ROW_H;
    vbe_fill_rect(reg->panes.vx + 2, vy - 1, reg->panes.vw - 4, 1,
                  tc()->border_dark);
    int shown = 0;
    for (int i = 0; i < reg->value_count && vy + REG_ROW_H < reg->panes.vy + reg->panes.vh; i++) {
        struct RegValue *v = &reg->values[i];
        if (v->owner != cur) continue;
        char line[160];
        if (v->type == REG_DWORD) {
            uint32_t d = 0;
            memcpy(&d, v->data, 4);
            snprintf(line, sizeof line, "%s  %s  0x%08x", v->name,
                     reg_type_name(v->type), (unsigned)d);
        } else {
            snprintf(line, sizeof line, "%s  %s  %.*s", v->name,
                     reg_type_name(v->type),
                     v->data_len > 0 ? v->data_len - 1 : 0, (const char *)v->data);
        }
        vbe_draw_text(reg->panes.vx + 4, vy, line, tc()->btn_text, 1);
        vy += REG_ROW_H;
        shown++;
    }
    if (shown == 0)
        vbe_draw_text(reg->panes.vx + 4, vy, "(no values)", tc()->btn_text, 1);

    /* Search hint. */
    if (reg->search_dialog_open) {
        char s[160];
        snprintf(s, sizeof s, "Find: %s", reg->search_text);
        vbe_draw_text(cx + 4, cy + ch - REG_ROW_H - 2, s, tc()->win_title_active, 1);
    }
}

/* -- Input ------------------------------------------------------- */

void regedit_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    RegeditState *reg = win ? (RegeditState *)win->user_data : NULL;
    if (!reg) return;
    reg_build_rows(reg);

    switch (key) {
    case 0xE048:   /* Up */
        if (reg->row_sel > 0) reg->row_sel--;
        break;
    case 0xE050:   /* Down */
        if (reg->row_sel + 1 < reg->row_count) reg->row_sel++;
        break;
    case 0xE04D:   /* Right: expand */
    case '\r':
    case '\n':
        if (reg->row_sel < reg->row_count)
            regedit_expand_key(reg, reg->rows[reg->row_sel].key);
        reg_build_rows(reg);
        break;
    case 0xE04B:   /* Left: collapse */
        if (reg->row_sel < reg->row_count)
            regedit_collapse_key(reg, reg->rows[reg->row_sel].key);
        reg_build_rows(reg);
        break;
    case 0xE035:   /* F2: toggle the find bar (no keymap surprises) */
        reg->search_dialog_open = !reg->search_dialog_open;
        break;
    default:
        /* Typing while the find bar is open writes the query. */
        if (reg->search_dialog_open && key >= 0x20 && key < 0x7f) {
            size_t n = strlen(reg->search_text);
            if (n + 1 < sizeof(reg->search_text)) {
                reg->search_text[n] = (char)key;
                reg->search_text[n + 1] = '\0';
            }
        }
        break;
    }
    if (reg->row_sel < reg->row_count)
        reg->current_key = reg->rows[reg->row_sel].key;
}

void regedit_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    RegeditState *reg = win ? (RegeditState *)win->user_data : NULL;
    if (!reg) return;
    if (kind != 1 && !(btn & 1)) return;   /* press, not release */

    const RegPanes *p = &reg->panes;
    if (x < p->tx || x >= p->tx + p->tw) return;      /* only the tree pane */
    for (int i = 0; i < reg->row_count; i++) {
        RegRow *row = &reg->rows[i];
        if (y < row->y || y >= row->y + row->h) continue;
        reg->row_sel = i;
        reg->current_key = row->key;
        /* A click on the +/- column opens or closes the key. */
        int afford_x = p->tx + 3 + row->depth * 12;
        if (row->key->child_count > 0 && x >= afford_x && x < afford_x + 9) {
            if (reg_key_expanded(reg, row->key)) regedit_collapse_key(reg, row->key);
            else regedit_expand_key(reg, row->key);
            reg_build_rows(reg);
        }
        return;
    }
}

/* -- Launcher ---------------------------------------------------- */

static void regedit_draw_wm(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    regedit_draw(win, fb, fb_w, fb_h, win ? (RegeditState *)win->user_data : NULL);
}

void regedit_seed_default_hive(RegeditState *reg) {
    if (!reg || reg->value_count > 0) return;
    struct RegKey *hklm = &reg->root_keys[2];
    struct RegKey *sw = (struct RegKey *)regedit_create_key(reg, hklm, "Software");
    struct RegKey *os = sw ? (struct RegKey *)regedit_create_key(reg, sw, "WuBuOS") : NULL;
    struct RegKey *cur = reg->current_key;

    reg->current_key = os ? os : hklm;
    regedit_set_string(reg, "Version", "WuBuOS 1.0");
    regedit_set_dword(reg, "Build", 20261124);
    regedit_set_string(reg, "Kernel", "wubukernel");

    reg->current_key = &reg->root_keys[1];
    regedit_set_string(reg, "Wallpaper", "default");

    reg->current_key = cur;
    regedit_expand_key(reg, hklm);
    if (sw) regedit_expand_key(reg, sw);
    reg_build_rows(reg);
}

DosGuiWindow* regedit_launch(void) {
    DosGuiWindow *win = wm_create(80, 60, 800, 600, "Registry Editor");
    if (!win) return NULL;
    RegeditState *reg = regedit_create();
    if (!reg) return win;
    regedit_init_roots(reg);
    regedit_seed_default_hive(reg);
    win->user_data = reg;
    win->on_draw = regedit_draw_wm;
    win->on_key = regedit_key;
    win->on_mouse = regedit_mouse;
    return win;
}

/* -- Model ------------------------------------------------------- */

void regedit_init_roots(RegeditState *reg) {
    const char *names[6] = {"HKEY_CLASSES_ROOT","HKEY_CURRENT_USER","HKEY_LOCAL_MACHINE","HKEY_USERS","HKEY_CURRENT_CONFIG","HKEY_DYN_DATA"};
    for (int i = 0; i < 6; i++) {
        strncpy(reg->root_keys[i].name, names[i], REG_MAX_NAME - 1);
        reg->root_keys[i].parent = NULL;
        reg->root_keys[i].children = NULL;
        reg->root_keys[i].next_sibling = (i < 5) ? &reg->root_keys[i + 1] : NULL;
        reg->root_keys[i].child_count = 0;
    }
    reg->current_key = &reg->root_keys[1];
    reg_build_rows(reg);
}

void* regedit_create_key(RegeditState *reg, void *parent, const char *name) {
    struct RegKey *p = (struct RegKey*)parent;
    if (!reg || !p || !name) return NULL;
    if (p->child_count >= 100) return NULL;
    struct RegKey *child = calloc(1, sizeof(struct RegKey));
    if (!child) return NULL;
    strncpy(child->name, name, REG_MAX_NAME - 1);
    child->parent = p;
    child->children = NULL;
    child->next_sibling = p->children;
    child->child_count = 0;
    p->children = child;
    p->child_count++;
    return child;
}

int regedit_set_string(RegeditState *reg, const char *name, const char *data) {
    if (!reg || !name || !data) return -1;
    if (reg->value_count >= 1024) return -1;
    struct RegValue *v = &reg->values[reg->value_count++];
    strncpy(v->name, name, REG_MAX_NAME - 1);
    v->type = REG_SZ;
    size_t n = strlen(data);
    if (n >= REG_MAX_DATA) n = REG_MAX_DATA - 1;
    memcpy(v->data, data, n);
    v->data[n] = '\0';
    v->data_len = (int)n + 1;
    v->owner = reg->current_key;
    return 0;
}

int regedit_set_dword(RegeditState *reg, const char *name, uint32_t data) {
    if (!reg || !name) return -1;
    if (reg->value_count >= 1024) return -1;
    struct RegValue *v = &reg->values[reg->value_count++];
    strncpy(v->name, name, REG_MAX_NAME - 1);
    v->type = REG_DWORD;
    memcpy(v->data, &data, 4);
    v->data_len = 4;
    v->owner = reg->current_key;
    return 0;
}

/* -- Inspection (tests / debug) ---------------------------------- */

int regedit_get_expand_depth(const RegeditState *reg) {
    return reg ? reg->expand_depth : 0;
}
bool regedit_search_open(const RegeditState *reg) {
    return reg ? reg->search_dialog_open : false;
}
const char *regedit_get_search_text(const RegeditState *reg) {
    return reg ? reg->search_text : "";
}
const RegKey *regedit_get_current_key(const RegeditState *reg) {
    return reg ? reg->current_key : NULL;
}
const RegKey *regedit_get_root_key(const RegeditState *reg, int i) {
    return (reg && i >= 0 && i < 6) ? &reg->root_keys[i] : NULL;
}

/* Values of the SELECTED key (the pane shows exactly these). */
int regedit_value_count(const RegeditState *reg) {
    if (!reg) return 0;
    int n = 0;
    for (int i = 0; i < reg->value_count; i++)
        if (reg->values[i].owner == reg->current_key) n++;
    return n;
}

void regedit_set_search_text(RegeditState *reg, const char *text) {
    if (!reg || !text) return;
    strncpy(reg->search_text, text, sizeof(reg->search_text) - 1);
    reg->search_text[sizeof(reg->search_text) - 1] = '\0';
}
void regedit_set_search_open(RegeditState *reg, bool open) {
    if (reg) reg->search_dialog_open = open;
}

/* Visible tree rows, so a test can assert the tree and the click map agree. */
int regedit_row_count(const RegeditState *reg) {
    return reg ? reg->row_count : 0;
}
const RegKey *regedit_row_key(const RegeditState *reg, int i) {
    if (!reg || i < 0 || i >= reg->row_count) return NULL;
    return reg->rows[i].key;
}
int regedit_row_depth(const RegeditState *reg, int i) {
    if (!reg || i < 0 || i >= reg->row_count) return -1;
    return reg->rows[i].depth;
}
int regedit_row_selected(const RegeditState *reg) {
    return reg ? reg->row_sel : -1;
}
/* The rect the last draw gave row i -- the same rect regedit_mouse() hits. */
bool regedit_row_rect(const RegeditState *reg, int i, int *x, int *y, int *w, int *h) {
    if (!reg || i < 0 || i >= reg->row_count) return false;
    if (x) *x = reg->panes.tx;
    if (y) *y = reg->rows[i].y;
    if (w) *w = reg->panes.tw;
    if (h) *h = reg->rows[i].h;
    return true;
}
