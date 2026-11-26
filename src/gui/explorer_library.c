/*
 * explorer_library.c -- libraries in Explorer: shortcuts in the nav pane, and a merged view.
 *
 * Windows puts Documents/Pictures/Music/Videos in the nav pane above the folder tree, and opening one
 * shows every folder the library holds as ONE list. WuBuOS had libraries in src/runtime since this
 * session; this file is the Explorer half:
 *   - the four shortcuts, drawn above the tree (ex_render_library_shortcuts) with their hit-test in the
 *     same expression (ex_hit_library_shortcut) -- a shortcut you cannot click is a decoration;
 *   - the merged fill (ex_load_library): the list is a normal entries[] array, so render, hit-test,
 *     open, copy and the context menu need no library mode of their own;
 *   - the anchor path is the library's first EXISTING folder, so breadcrumbs and Up stay real, and the
 *     status bar says the list is a union (ex_library_status) instead of pretending it is one folder.
 *
 * C11.
 */
#include "explorer.h"
#include "explorer_internal.h"
#include "../runtime/wubu_libraries.h"
#include "../kernel/vbe.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <string.h>

#define EX_LIB_SLOT_H 22

int ex_library_block_h(void) { return WUBU_LIB_COUNT * EX_LIB_SLOT_H; }

/* -- The merged fill ------------------------------------------------------------------------------ */

int ex_load_library(ExExplorerState *ex, int lib_id)
{
    if (!ex || lib_id < 0 || lib_id >= WUBU_LIB_COUNT) return -1;

    WubuLibraries libs;
    wubu_libraries_load(&libs);
    if (!wubu_library_get(&libs, (WubuLibraryId)lib_id)) return -1;

    static WubuLibraryItem items[EX_MAX_ENTRIES];
    int n = wubu_library_list(&libs, (WubuLibraryId)lib_id, items, EX_MAX_ENTRIES);
    if (n < 0) return -1;

    ex->entry_count = 0;
    for (int i = 0; i < n && ex->entry_count < EX_MAX_ENTRIES; i++) {
        const WubuLibraryItem *it = &items[i];
        ExEntry *e = &ex->entries[ex->entry_count];
        memset(e, 0, sizeof(*e));
        snprintf(e->name, sizeof e->name, "%s", it->name);
        snprintf(e->full_path, sizeof e->full_path, "%s", it->path);
        e->type = it->is_dir ? EX_ENTRY_DIR : EX_ENTRY_FILE;
        e->size = (uint64_t)(it->size > 0 ? it->size : 0);
        e->modified = it->modified;
        /* The Type column names the FOLDER the item came from: that is the only thing that makes a
         * merged list readable, and it is true (the plain type string would hide it). */
        snprintf(e->type_str, sizeof e->type_str, "%s", it->source);
        const char *dot = strrchr(it->name, '.');
        if (dot && dot != it->name) {
            size_t k = 0;
            for (const char *p = dot + 1; *p && k + 1 < sizeof e->extension; p++)
                e->extension[k++] = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
            e->extension[k] = '\0';
        }
        ex->entry_count++;
    }

    ex->library = lib_id;
    ex_view_invalidate(ex);
    return ex->entry_count;
}

bool explorer_open_library(int lib_id)
{
    ExExplorerState *ex = explorer_state();
    if (!ex || lib_id < 0 || lib_id >= WUBU_LIB_COUNT) return false;

    /* Anchor on the first folder that exists, so the breadcrumb and Up are real paths. When none
     * exists the library is still openable: an empty list plus a status line that says why is better
     * than refusing to show a library the user just configured. */
    WubuLibraries libs;
    wubu_libraries_load(&libs);
    const WubuLibrary *lib = wubu_library_get(&libs, (WubuLibraryId)lib_id);
    if (!lib) return false;

    const char *anchor = NULL;
    for (int i = 0; i < lib->count; i++)
        if (lib->folders[i].exists) { anchor = lib->folders[i].path; break; }

    if (anchor) snprintf(ex->current_path, sizeof ex->current_path, "%s", anchor);
    else        snprintf(ex->current_path, sizeof ex->current_path, "/");

    if (ex_load_library(ex, lib_id) < 0) return false;
    ex_update_breadcrumbs(ex);
    ex_add_to_history(ex, ex->current_path);
    ex->preview.type = EX_PREVIEW_NONE;
    ex->preview.path[0] = '\0';
    return true;
}

int explorer_library_id(void)
{
    ExExplorerState *ex = explorer_state();
    return ex ? ex->library : -1;
}

bool explorer_in_library(void) { return explorer_library_id() >= 0; }

/* The line the status bar shows. Empty when this is a plain folder. */
void ex_library_status(ExExplorerState *ex, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!ex || ex->library < 0) return;

    WubuLibraries libs;
    wubu_libraries_load(&libs);
    const WubuLibrary *lib = wubu_library_get(&libs, (WubuLibraryId)ex->library);
    if (!lib) return;

    int missing = 0;
    for (int i = 0; i < lib->count; i++) if (!lib->folders[i].exists) missing++;

    if (missing > 0)
        snprintf(out, cap, "%s library \u00b7 %d folder%s \u00b7 %d missing",
                 lib->name, lib->count, lib->count == 1 ? "" : "s", missing);
    else
        snprintf(out, cap, "%s library \u00b7 %d folder%s merged",
                 lib->name, lib->count, lib->count == 1 ? "" : "s");
}

/* -- The nav-pane shortcuts ----------------------------------------------------------------------- */

static int lib_slot_y(const ExExplorerState *ex, int tree_y, int i)
{
    (void)ex;
    return tree_y + i * EX_LIB_SLOT_H;
}

void ex_render_library_shortcuts(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h,
                                 int x, int y, int w, int *drawn_h)
{
    const WubuThemeColors *c = tc();
    int total = WUBU_LIB_COUNT * EX_LIB_SLOT_H;

    if (drawn_h) *drawn_h = total;

    for (int i = 0; i < WUBU_LIB_COUNT; i++) {
        int sy = lib_slot_y(ex, y, i);
        bool active = (ex->library == i);
        if (active)
            vbe_fill_rect_rounded(x, sy, w, EX_LIB_SLOT_H, WUBU_METRIC_RADIUS_CONTROL, c->select_bg);

        /* A small folder mark, then the name: the nav pane has no icon system for libraries yet, and a
         * coloured square would be decoration. */
        uint32_t fg = active ? c->select_text : c->body_text;
        int tx = x + EX_TREE_INDENT + 14;
        vbe_fill_rect(x + EX_TREE_INDENT, sy + 7, 8, 6, fg);
        vbe_fill_rect(x + EX_TREE_INDENT, sy + 6, 4, 2, fg);
        vbe_draw_text(tx, sy + 6, wubu_library_name((WubuLibraryId)i), fg, 1);
    }
}

int ex_hit_library_shortcut(ExExplorerState *ex, int tree_y, int mx, int my)
{
    (void)ex;
    for (int i = 0; i < WUBU_LIB_COUNT; i++) {
        int sy = lib_slot_y(ex, tree_y, i);
        if (my >= sy && my < sy + EX_LIB_SLOT_H) return i;
    }
    return -1;
}
