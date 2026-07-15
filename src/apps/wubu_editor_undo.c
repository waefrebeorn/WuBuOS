/*
 * wubu_editor_undo.c -- WuBuOS editor undo/redo engine (split from wubu_editor.c)
 *
 * Self-contained subsystem: the per-tab undo ring (push + undo/redo replay).
 * Mirrors the sibling-module convention used by wubu_editor_bookmark.c and
 * wubu_editor_macro.c: include the public wubu_editor.h and implement the
 * wubu_ed_* API directly. No god header, minimal includes.
 *
 * The push primitive (wubu_ed_undo_push) is exposed publicly because the
 * editing ops in wubu_editor.c and replace_next in wubu_editor_find.c both
 * record undo entries.
 */

#include "wubu_editor.h"
#include <stdlib.h>
#include <string.h>

/* -- Undo stack push ------------------------------------------------- */

void wubu_ed_undo_push(WubuEdTab *tab, WubuUndoKind kind, int line, int col,
                       const char *text, int text_len) {
    if (!tab || tab->undo_count >= WUBU_ED_MAX_UNDO) return;
    WubuUndo *u = &tab->undo_stack[tab->undo_pos];
    u->kind = kind;
    u->line = line;
    u->col  = col;
    if (text && text_len > 0) {
        int len = text_len < WUBU_ED_MAX_LINE_LEN ? text_len : WUBU_ED_MAX_LINE_LEN - 1;
        memcpy(u->text, text, (size_t)len);
        u->text[len] = '\0';
        u->text_len  = len;
    } else {
        u->text[0]   = '\0';
        u->text_len  = 0;
    }
    tab->undo_pos   = (tab->undo_pos + 1) % WUBU_ED_MAX_UNDO;
    tab->undo_count++;
}

/* -- Undo ------------------------------------------------------------ */

void wubu_ed_undo(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab || tab->undo_count == 0) return;

    tab->undo_pos = (tab->undo_pos - 1 + WUBU_ED_MAX_UNDO) % WUBU_ED_MAX_UNDO;
    WubuUndo *u = &tab->undo_stack[tab->undo_pos];

    switch (u->kind) {
    case UNDO_INSERT: {
        /* Undo an insert = delete the inserted text */
        if (u->line >= tab->n_lines) break;
        WubuEdLine *line = &tab->lines[u->line];
        if (u->col + u->text_len > line->len) break;
        memmove(&line->text[u->col], &line->text[u->col + u->text_len],
                line->len - u->col - u->text_len + 1);
        line->len -= u->text_len;
        tab->cursor_line = u->line;
        tab->cursor_col  = u->col;
        break;
    }
    case UNDO_DELETE: {
        /* Undo a delete = re-insert the deleted text */
        if (u->line >= tab->n_lines) break;
        WubuEdLine *line = &tab->lines[u->line];
        if (line->len + u->text_len >= WUBU_ED_MAX_LINE_LEN) break;
        memmove(&line->text[u->col + u->text_len], &line->text[u->col],
                line->len - u->col + 1);
        memcpy(&line->text[u->col], u->text, (size_t)u->text_len);
        line->len += u->text_len;
        tab->cursor_line = u->line;
        tab->cursor_col  = u->col + u->text_len;
        break;
    }
    case UNDO_REPLACE: {
        /* Undo a replace = restore original text */
        if (u->line >= tab->n_lines) break;
        WubuEdLine *line = &tab->lines[u->line];
        if (u->col + u->text_len > line->len) break;
        memcpy(&line->text[u->col], u->text, (size_t)u->text_len);
        tab->cursor_line = u->line;
        tab->cursor_col  = u->col;
        break;
    }
    }

    tab->undo_count--;
    tab->modified = true;
}

/* -- Redo ------------------------------------------------------------ */

void wubu_ed_redo(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;

    /* Redo is complex; for now, simple redo of inserts */
    int redo_idx = tab->undo_pos;
    if (redo_idx >= WUBU_ED_MAX_UNDO) return;

    WubuUndo *u = &tab->undo_stack[redo_idx];
    if (u->kind == UNDO_INSERT && u->line < tab->n_lines) {
        WubuEdLine *line = &tab->lines[u->line];
        if (line->len + u->text_len < WUBU_ED_MAX_LINE_LEN) {
            memmove(&line->text[u->col + u->text_len], &line->text[u->col],
                    line->len - u->col + 1);
            memcpy(&line->text[u->col], u->text, (size_t)u->text_len);
            line->len += u->text_len;
            tab->cursor_line = u->line;
            tab->cursor_col  = u->col + u->text_len;
            tab->undo_pos    = (tab->undo_pos + 1) % WUBU_ED_MAX_UNDO;
            tab->undo_count++;
            tab->modified = true;
        }
    }
}

/* -- Undo/redo availability ----------------------------------------- */

bool wubu_ed_can_undo(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    return tab && tab->undo_count > 0;
}

bool wubu_ed_can_redo(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return false;
    int redo_idx = tab->undo_pos;
    return redo_idx < WUBU_ED_MAX_UNDO && tab->undo_stack[redo_idx].kind != 0;
}
