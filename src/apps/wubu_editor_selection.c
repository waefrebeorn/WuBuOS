/*
 * wubu_editor_selection.c -- WuBuOS editor selection + clipboard (split from
 * wubu_editor.c)
 *
 * Self-contained subsystem: selection model, get-selection extraction,
 * selection deletion, and cut/copy/paste against the editor clipboard.
 * Mirrors the sibling-module convention (wubu_editor_bookmark.c / _macro.c):
 * include the public wubu_editor.h and implement the wubu_ed_* API directly.
 * Minimal includes, no god header.
 */

#include "wubu_editor.h"
#include <stdlib.h>
#include <string.h>

/* -- Selection: select all ------------------------------------------ */

void wubu_ed_select_all(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    tab->sel_start_line = 0;
    tab->sel_start_col  = 0;
    tab->sel_end_line   = tab->n_lines - 1;
    tab->sel_end_col    = tab->lines[tab->n_lines - 1].len;
}

/* -- Helper: get selected text into a buffer ------------------------ */

static char *get_selection(WubuEdTab *tab, size_t *out_len) {
    if (!tab || tab->sel_start_line < 0 || tab->sel_end_line < 0) return NULL;
    if (tab->sel_start_line > tab->sel_end_line) return NULL;

    /* Calculate total size needed */
    size_t total = 0;
    for (int i = tab->sel_start_line; i <= tab->sel_end_line; i++) {
        if (i >= tab->n_lines) break;
        int start_col = (i == tab->sel_start_line) ? tab->sel_start_col : 0;
        int end_col   = (i == tab->sel_end_line)  ? tab->sel_end_col  : tab->lines[i].len;
        if (start_col > tab->lines[i].len) start_col = tab->lines[i].len;
        if (end_col   > tab->lines[i].len) end_col   = tab->lines[i].len;
        if (end_col > start_col) total += (size_t)(end_col - start_col);
        if (i < tab->sel_end_line) total++; /* newline */
    }

    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;

    size_t pos = 0;
    for (int i = tab->sel_start_line; i <= tab->sel_end_line; i++) {
        if (i >= tab->n_lines) break;
        int start_col = (i == tab->sel_start_line) ? tab->sel_start_col : 0;
        int end_col   = (i == tab->sel_end_line)  ? tab->sel_end_col  : tab->lines[i].len;
        if (start_col > tab->lines[i].len) start_col = tab->lines[i].len;
        if (end_col   > tab->lines[i].len) end_col   = tab->lines[i].len;
        if (end_col > start_col) {
            memcpy(buf + pos, tab->lines[i].text + start_col, (size_t)(end_col - start_col));
            pos += (size_t)(end_col - start_col);
        }
        if (i < tab->sel_end_line) buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    if (out_len) *out_len = pos;
    return buf;
}

/* -- Helper: delete selection --------------------------------------- */

static void delete_selection(WubuEdTab *tab) {
    if (!tab || tab->sel_start_line < 0 || tab->sel_end_line < 0) return;

    int sl = tab->sel_start_line, sc = tab->sel_start_col;
    int el = tab->sel_end_line,   ec = tab->sel_end_col;

    if (sl == el) {
        /* Single line */
        WubuEdLine *line = &tab->lines[sl];
        if (sc > line->len) sc = line->len;
        if (ec > line->len) ec = line->len;
        memmove(&line->text[sc], &line->text[ec], line->len - ec + 1);
        line->len -= (ec - sc);
    } else {
        /* Multi-line: keep start of first line + end of last line */
        WubuEdLine *first = &tab->lines[sl];
        WubuEdLine *last  = &tab->lines[el];
        if (sc > first->len) sc = first->len;
        if (ec > last->len)  ec = last->len;
        /* Truncate first line at selection start */
        first->text[sc] = '\0';
        first->len = sc;
        /* Append end of last line */
        int rest = last->len - ec;
        if (sc + rest < WUBU_ED_MAX_LINE_LEN) {
            memcpy(&first->text[sc], &last->text[ec], (size_t)rest);
            first->len = sc + rest;
            first->text[first->len] = '\0';
        }
        /* Remove lines sl+1 through el */
        int remove_count = el - sl;
        memmove(&tab->lines[sl + 1], &tab->lines[el + 1],
                (tab->n_lines - el - 1) * sizeof(WubuEdLine));
        tab->n_lines -= remove_count;
    }

    tab->cursor_line   = sl;
    tab->cursor_col    = sc;
    tab->sel_start_line = -1;
    tab->sel_end_line   = -1;
    tab->modified = true;
}

/* -- Cut / Copy / Paste --------------------------------------------- */

void wubu_ed_cut(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;

    size_t sel_len;
    char *sel = get_selection(tab, &sel_len);
    if (!sel || sel_len == 0) { free(sel); return; }

    /* Copy to clipboard */
    free(ed->clipboard);
    ed->clipboard      = sel;
    ed->clipboard_size = sel_len;

    /* Delete selection */
    delete_selection(tab);
}

void wubu_ed_copy(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;

    size_t sel_len;
    char *sel = get_selection(tab, &sel_len);
    if (!sel || sel_len == 0) { free(sel); return; }

    free(ed->clipboard);
    ed->clipboard      = sel;
    ed->clipboard_size = sel_len;
}

void wubu_ed_paste(WubuEditor *ed) {
    if (!ed || !ed->clipboard || ed->clipboard_size == 0) return;
    wubu_ed_insert_text(ed, ed->clipboard);
}
