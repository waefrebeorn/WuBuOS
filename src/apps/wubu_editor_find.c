/*
 * wubu_editor_find.c -- WuBuOS editor find/replace (split from wubu_editor.c)
 *
 * Self-contained subsystem: incremental find-next / find-prev and
 * replace-next / replace-all over the active tab's lines, with wrap-around
 * and an undo entry per replacement. Mirrors the sibling-module convention
 * (wubu_editor_bookmark.c / _macro.c): include the public wubu_editor.h and
 * implement the wubu_ed_* API directly. Minimal includes, no god header.
 *
 * replace_next records its undo via the public wubu_ed_undo_push() primitive
 * (defined in wubu_editor_undo.c) and re-dispatches to wubu_ed_find_next().
 */

#include "wubu_editor.h"
#include <string.h>

/* -- Find next ------------------------------------------------------- */

int wubu_ed_find_next(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!ed || !ed->find.find_text[0]) return -1;

    int start_line = tab->cursor_line;
    int start_col  = tab->cursor_col + 1; /* Start after cursor */

    for (int pass = 0; pass < 2; pass++) {
        for (int i = start_line; i < tab->n_lines; i++) {
            WubuEdLine *line = &tab->lines[i];
            int col = (i == start_line) ? start_col : 0;
            if (col >= line->len) continue;

            const char *found = strstr(line->text + col, ed->find.find_text);
            if (found) {
                int found_col = (int)(found - line->text);
                tab->cursor_line       = i;
                tab->cursor_col        = found_col;
                ed->find.last_found_line = i;
                ed->find.last_found_col  = found_col;
                return i;
            }
        }
        /* Wrap around */
        start_line = 0;
        start_col  = 0;
    }
    return -1;
}

/* -- Find previous --------------------------------------------------- */

int wubu_ed_find_prev(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!ed || !ed->find.find_text[0]) return -1;

    int start_line = tab->cursor_line;
    int start_col  = tab->cursor_col - 1;

    for (int i = start_line; i >= 0; i--) {
        WubuEdLine *line = &tab->lines[i];
        int col = (i == start_line) ? start_col : line->len;
        if (col < 0 || col >= line->len) continue;

        /* Search backward in this line */
        for (int c = col; c >= 0; c--) {
            if (strncmp(line->text + c, ed->find.find_text,
                        strlen(ed->find.find_text)) == 0) {
                tab->cursor_line        = i;
                tab->cursor_col         = c;
                ed->find.last_found_line = i;
                ed->find.last_found_col  = c;
                return i;
            }
        }
    }
    return -1;
}

/* -- Replace next ---------------------------------------------------- */

int wubu_ed_replace_next(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!ed || !ed->find.find_text[0]) return -1;

    if (ed->find.last_found_line >= 0 && ed->find.last_found_line < tab->n_lines) {
        WubuEdLine *line = &tab->lines[ed->find.last_found_line];
        int flen = (int)strlen(ed->find.find_text);
        int rlen = (int)strlen(ed->find.replace_text);
        if (ed->find.last_found_col + flen <= line->len) {
            /* Record undo */
            char undo_buf[WUBU_ED_MAX_LINE_LEN];
            memcpy(undo_buf, line->text + ed->find.last_found_col, (size_t)flen);
            undo_buf[flen] = '\0';
            wubu_ed_undo_push(tab, UNDO_REPLACE, ed->find.last_found_line,
                              ed->find.last_found_col, undo_buf, flen);

            /* Do replace */
            memmove(&line->text[ed->find.last_found_col + rlen],
                    &line->text[ed->find.last_found_col + flen],
                    line->len - ed->find.last_found_col - flen + 1);
            memcpy(&line->text[ed->find.last_found_col], ed->find.replace_text, (size_t)rlen);
            line->len = line->len - flen + rlen;
            tab->cursor_col = ed->find.last_found_col + rlen;
            tab->modified = true;

            /* Find next */
            return wubu_ed_find_next(ed);
        }
    }
    return wubu_ed_find_next(ed);
}

/* -- Replace all ----------------------------------------------------- */

int wubu_ed_replace_all(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!ed || !ed->find.find_text[0]) return 0;

    int count = 0;
    for (int i = 0; i < tab->n_lines; i++) {
        WubuEdLine *line = &tab->lines[i];
        int flen = (int)strlen(ed->find.find_text);
        int rlen = (int)strlen(ed->find.replace_text);
        char *pos = line->text;
        while ((pos = strstr(pos, ed->find.find_text)) != NULL) {
            int col = (int)(pos - line->text);
            memmove(&line->text[col + rlen], &line->text[col + flen],
                    line->len - col - flen + 1);
            memcpy(&line->text[col], ed->find.replace_text, (size_t)rlen);
            line->len = line->len - flen + rlen;
            pos = line->text + col + rlen;
            count++;
            tab->modified = true;
        }
    }
    return count;
}
