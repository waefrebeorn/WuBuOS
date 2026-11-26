/*
 * explorer_search.c -- the toolbar search field's model: which entries the list is showing.
 *
 * Windows Explorer filters the list as you type in the toolbar field. This file owns the ONE answer
 * to "which entry is visible row N". Both the renderer and the hit-test read it, because the bug this
 * tree keeps repeating is a component that draws from one expression and hit-tests from another --
 * the address bar tested the top 28 px while it was drawn 36 px lower, and clicking a tree row past
 * the last visible one dereferenced NULL.
 *
 * The mapping is rebuilt lazily (ex_view_sync) and invalidated on every change that can reorder or
 * filter the list: a new directory, a sort, or the query itself.
 *
 * C11.
 */
#include "explorer_internal.h"   /* brings explorer.h and the shell's tc() */

#include <string.h>
#include "../kernel/vbe.h"
#include "wubu_theme.h"
#include "wubu_tokens.h"
#include <stdio.h>

bool ex_entry_matches_search(const ExEntry *e, const char *query)
{
    if (!e) return false;
    if (!query || !*query) return true;
    /* The name is what a user types; the type string ("PNG Image", "Folder") and the extension are
     * what they type next. Matching all three is why "folder" finds folders and "png" finds images. */
    if (str_contains_nocase(e->name, query)) return true;
    if (e->type_str[0] && str_contains_nocase(e->type_str, query)) return true;
    if (e->extension[0] && str_contains_nocase(e->extension, query)) return true;
    return false;
}

void ex_view_invalidate(ExExplorerState *ex)
{
    if (ex) ex->visible_dirty = true;
}

void ex_view_sync(ExExplorerState *ex)
{
    if (!ex) return;
    /* Rebuild on demand OR when the entry set changed under us: a load path that forgot to invalidate
     * must not leave the list showing last directory's rows. */
    if (!ex->visible_dirty && ex->visible_stamp == ex->entry_count) return;

    int n = 0;
    int limit = ex->entry_count < EX_MAX_ENTRIES ? ex->entry_count : EX_MAX_ENTRIES;
    for (int i = 0; i < limit; i++) {
        if (!ex_entry_matches_search(&ex->entries[i], ex->search)) continue;
        ex->visible[n++] = i;
    }
    ex->visible_count = n;
    ex->visible_stamp = ex->entry_count;
    ex->visible_dirty = false;

    /* A selection pointing past the new end would make Delete act on nothing while looking armed. */
    if (ex->focus_idx >= n) ex->focus_idx = n - 1;
    if (ex->focus_idx < -1) ex->focus_idx = -1;
}

int ex_view_count(const ExExplorerState *ex)
{
    if (!ex) return 0;
    return ex->visible_count;
}

int ex_view_entry(const ExExplorerState *ex, int row)
{
    if (!ex || row < 0 || row >= ex->visible_count) return -1;
    return ex->visible[row];
}

void ex_search_set(ExExplorerState *ex, const char *query)
{
    if (!ex) return;
    if (!query) query = "";
    size_t n = strlen(query);
    if (n >= sizeof ex->search) n = sizeof ex->search - 1;
    memcpy(ex->search, query, n);
    ex->search[n] = '\0';
    ex_view_invalidate(ex);
    ex_view_sync(ex);
}

void ex_search_clear(ExExplorerState *ex)
{
    if (!ex) return;
    ex->search[0] = '\0';
    ex->search_active = false;
    ex_view_invalidate(ex);
    ex_view_sync(ex);
}

/* -- The field's box and its drawing -------------------------------------
 * Both live here, with the model, so the renderer calls one function and the click handler asks the
 * same box. They were in explorer_render.c until that file crossed the 600-line budget: a search
 * field is one concern, and it belongs in the file that owns searching. */

bool ex_search_rect(const ExExplorerState *ex, int x, int y, int w, int toolbar_h,
                    int *out_x, int *out_y, int *out_w, int *out_h)
{
    (void)ex;
    /* The field sits between the button strip and the view label, both reserved by tokens. */
    int field_w = WUBU_SEARCH_W;
    int left = x + WUBU_TOOLBAR_BUTTONS_W + WUBU_SPACE_SM;
    int right = x + w - WUBU_VIEW_LABEL_W - WUBU_SPACE_SM;
    if (right - left < field_w) return false;   /* no room: draw nothing, activate nothing */
    int fx = right - field_w;
    if (out_x) *out_x = fx;
    if (out_y) *out_y = y + WUBU_SPACE_XS;
    if (out_w) *out_w = field_w;
    if (out_h) *out_h = toolbar_h - WUBU_SPACE_SM;
    return true;
}

void ex_render_search(ExExplorerState *ex, int x, int y, int w, int toolbar_h)
{
    /* Search field, right of the buttons and left of the view label (Windows puts it there).
     * ex_search_rect() owns the geometry so the click handler cannot disagree with the drawing. */
    int sx, sy, sw, sh;
    if (ex_search_rect(ex, x, y, w, toolbar_h, &sx, &sy, &sw, &sh)) {
        vbe_fill_rect_rounded(sx, sy, sw, sh, WUBU_METRIC_RADIUS_CONTROL,
                              ex->search_active ? tc()->win_face : tc()->btn_face);
        vbe_rect_rounded(sx, sy, sw, sh, WUBU_METRIC_RADIUS_CONTROL,
                         ex->search_active ? tc()->select_bg : tc()->border_face);
        int ty = sy + (sh - 8) / 2;
        if (ex->search[0]) {
            vbe_draw_text(sx + WUBU_SPACE_XS + 2, ty, ex->search, tc()->btn_text, 1);
            int caret = sx + WUBU_SPACE_XS + 2 + vbe_text_width(ex->search, 1) + 1;
            if (ex->search_active) vbe_vline(caret, ty, ty + 9, tc()->select_bg);
        } else {
            vbe_draw_text(sx + WUBU_SPACE_XS + 2, ty, "Search",
                          ex->search_active ? tc()->icon_text : tc()->icon_text_shadow, 1);
        }
        /* The filter result count belongs where the user is looking. */
        if (ex->search[0]) {
            char cnt[32];
            snprintf(cnt, sizeof cnt, "%d", ex_view_count(ex));
            vbe_draw_text(sx + sw - WUBU_SPACE_XS - vbe_text_width(cnt, 1), ty, cnt,
                          tc()->icon_text_shadow, 1);
        }
    }
}

