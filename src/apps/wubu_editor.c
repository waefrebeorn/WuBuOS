/*
 * wubu_editor.c  --  WuBuOS Code Editor Implementation
 *
 * Cell 396: Tabbed editor with syntax HL, find/replace, folding.
 */
#include "wubu_editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Syntax Detection --------------------------------------------- */

WubuSyntax wubu_ed_detect_syntax(const char *filename) {
    if (!filename) return SYNTAX_NONE;
    size_t len = strlen(filename);
    if (len > 2 && strcmp(filename + len - 2, ".c") == 0)   return SYNTAX_C;
    if (len > 2 && strcmp(filename + len - 2, ".h") == 0)   return SYNTAX_C;
    if (len > 3 && strcmp(filename + len - 3, ".HC") == 0)  return SYNTAX_HOLYC;
    if (len > 3 && strcmp(filename + len - 3, ".py") == 0)  return SYNTAX_PYTHON;
    if (len > 3 && strcmp(filename + len - 3, ".sh") == 0)  return SYNTAX_SHELL;
    if (len > 2 && strcmp(filename + len - 2, ".mk") == 0)  return SYNTAX_MAKEFILE;
    if (len >= 8 && strcmp(filename + len - 8, "Makefile") == 0) return SYNTAX_MAKEFILE;
    if (len > 5 && strcmp(filename + len - 5, ".toml") == 0) return SYNTAX_TOML;
    if (len > 5 && strcmp(filename + len - 5, ".wubu") == 0) return SYNTAX_TOML;
    if (len > 3 && strcmp(filename + len - 3, ".md") == 0)  return SYNTAX_MARKDOWN;
    if (len > 5 && strcmp(filename + len - 5, ".json") == 0) return SYNTAX_JSON;
    if (len > 5 && strcmp(filename + len - 5, ".diff") == 0) return SYNTAX_DIFF;
    if (len > 5 && strcmp(filename + len - 5, ".patch") == 0) return SYNTAX_DIFF;
    return SYNTAX_NONE;
}

/* -- Create/Destroy ----------------------------------------------- */

WubuEditor *wubu_ed_create(void) {
    WubuEditor *ed = (WubuEditor*)calloc(1, sizeof(WubuEditor));
    if (!ed) return NULL;
    ed->n_tabs = 0;
    ed->active_tab = -1;
    ed->show_line_nums = true;
    ed->show_indent_guides = true;
    ed->show_fold_markers = true;
    ed->gutter_width = 48;
    ed->split_ratio = 0.6180339887; /* 1/φ golden split */
    return ed;
}

void wubu_ed_destroy(WubuEditor *ed) {
    if (!ed) return;
    for (int i = 0; i < ed->n_tabs; i++) {
        if (ed->tabs[i].lines) free(ed->tabs[i].lines);
    }
    if (ed->clipboard) free(ed->clipboard);
    if (ed->macro_buf) free(ed->macro_buf);
    free(ed);
}

/* -- New File ----------------------------------------------------- */

void wubu_ed_new_file(WubuEditor *ed) {
    if (!ed || ed->n_tabs >= WUBU_ED_MAX_TABS) return;
    WubuEdTab *tab = &ed->tabs[ed->n_tabs];
    memset(tab, 0, sizeof(WubuEdTab));
    snprintf(tab->title, sizeof(tab->title), "Untitled %d", ed->n_tabs + 1);
    tab->syntax = SYNTAX_NONE;
    tab->tab_size = 4;
    tab->use_spaces = true;
    tab->lines_capacity = 256;
    tab->lines = (WubuEdLine*)calloc(tab->lines_capacity, sizeof(WubuEdLine));
    tab->n_lines = 1;
    tab->lines[0].len = 0;
    tab->lines[0].text[0] = '\0';
    ed->active_tab = ed->n_tabs;
    ed->n_tabs++;
}

/* -- Open File ---------------------------------------------------- */

int wubu_ed_open(WubuEditor *ed, const char *filename) {
    if (!ed || !filename || ed->n_tabs >= WUBU_ED_MAX_TABS) return -1;
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    WubuEdTab *tab = &ed->tabs[ed->n_tabs];
    memset(tab, 0, sizeof(WubuEdTab));
    strncpy(tab->filename, filename, sizeof(tab->filename) - 1);
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    strncpy(tab->title, base, sizeof(tab->title) - 1);
    tab->syntax = wubu_ed_detect_syntax(filename);
    tab->tab_size = 4;
    tab->use_spaces = true;
    tab->lines_capacity = 256;
    tab->lines = (WubuEdLine*)calloc(tab->lines_capacity, sizeof(WubuEdLine));
    tab->n_lines = 0;

    /* Read line by line */
    char buf[WUBU_ED_MAX_LINE_LEN];
    while (fgets(buf, sizeof(buf), f) && tab->n_lines < WUBU_ED_MAX_LINES) {
        if (tab->n_lines >= tab->lines_capacity) {
            tab->lines_capacity *= 2;
            tab->lines = (WubuEdLine*)realloc(tab->lines,
                tab->lines_capacity * sizeof(WubuEdLine));
        }
        WubuEdLine *line = &tab->lines[tab->n_lines];
        int len = (int)strlen(buf);
        /* Strip trailing newline */
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        memcpy(line->text, buf, len);
        line->text[len] = '\0';
        line->len = len;
        tab->n_lines++;
    }
    fclose(f);

    if (tab->n_lines == 0) {
        tab->n_lines = 1;
        tab->lines[0].len = 0;
    }

    ed->active_tab = ed->n_tabs;
    ed->n_tabs++;
    return 0;
}

/* -- Save --------------------------------------------------------- */

int wubu_ed_save(WubuEditor *ed) {
    if (!ed || ed->active_tab < 0) return -1;
    WubuEdTab *tab = &ed->tabs[ed->active_tab];
    if (!tab->filename[0]) return -1;
    FILE *f = fopen(tab->filename, "w");
    if (!f) return -1;
    for (int i = 0; i < tab->n_lines; i++) {
        fwrite(tab->lines[i].text, 1, tab->lines[i].len, f);
        fputc('\n', f);
    }
    fclose(f);
    tab->modified = false;
    return 0;
}

int wubu_ed_save_as(WubuEditor *ed, const char *filename) {
    if (!ed || ed->active_tab < 0 || !filename) return -1;
    WubuEdTab *tab = &ed->tabs[ed->active_tab];
    strncpy(tab->filename, filename, sizeof(tab->filename) - 1);
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    strncpy(tab->title, base, sizeof(tab->title) - 1);
    tab->syntax = wubu_ed_detect_syntax(filename);
    return wubu_ed_save(ed);
}

/* -- Tab Management ----------------------------------------------- */

void wubu_ed_switch_tab(WubuEditor *ed, int tab_idx) {
    if (ed && tab_idx >= 0 && tab_idx < ed->n_tabs)
        ed->active_tab = tab_idx;
}

int  wubu_ed_active_tab(WubuEditor *ed) {
    return ed ? ed->active_tab : -1;
}

int  wubu_ed_tab_count(WubuEditor *ed) {
    return ed ? ed->n_tabs : 0;
}

WubuEdTab *wubu_ed_current_tab(WubuEditor *ed) {
    if (!ed || ed->active_tab < 0 || ed->active_tab >= ed->n_tabs)
        return NULL;
    return &ed->tabs[ed->active_tab];
}

int wubu_ed_close_tab(WubuEditor *ed, int tab_idx) {
    if (!ed || tab_idx < 0 || tab_idx >= ed->n_tabs) return -1;
    if (ed->tabs[tab_idx].lines) free(ed->tabs[tab_idx].lines);
    for (int i = tab_idx; i < ed->n_tabs - 1; i++)
        ed->tabs[i] = ed->tabs[i + 1];
    ed->n_tabs--;
    if (ed->active_tab >= ed->n_tabs)
        ed->active_tab = ed->n_tabs - 1;
    return 0;
}

/* -- Undo/Redo Internal -------------------------------------------- */

static void undo_push(WubuEdTab *tab, WubuUndoKind kind, int line, int col, const char *text, int text_len);

/* -- Insert Character --------------------------------------------- */

void wubu_ed_insert_char(WubuEditor *ed, char ch) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    if (tab->cursor_line >= tab->n_lines) return;
    WubuEdLine *line = &tab->lines[tab->cursor_line];
    if (line->len >= WUBU_ED_MAX_LINE_LEN - 1) return;
    if (tab->cursor_col > line->len) tab->cursor_col = line->len;
    /* Record undo: the inserted character */
    char ustr[2] = {ch, '\0'};
    undo_push(tab, UNDO_INSERT, tab->cursor_line, tab->cursor_col, ustr, 1);
    memmove(&line->text[tab->cursor_col + 1],
            &line->text[tab->cursor_col],
            line->len - tab->cursor_col + 1);
    line->text[tab->cursor_col] = ch;
    line->len++;
    tab->cursor_col++;
    tab->modified = true;
}

void wubu_ed_insert_newline(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab || tab->n_lines >= WUBU_ED_MAX_LINES) return;
    if (tab->cursor_line >= tab->n_lines) return;

    /* Ensure capacity */
    if (tab->n_lines >= tab->lines_capacity) {
        tab->lines_capacity *= 2;
        tab->lines = (WubuEdLine*)realloc(tab->lines,
            tab->lines_capacity * sizeof(WubuEdLine));
    }

    /* Split current line at cursor */
    WubuEdLine *cur = &tab->lines[tab->cursor_line];
    int col = tab->cursor_col;
    if (col > cur->len) col = cur->len;

    /* Move lines down */
    memmove(&tab->lines[tab->cursor_line + 2],
            &tab->lines[tab->cursor_line + 1],
            (tab->n_lines - tab->cursor_line - 1) * sizeof(WubuEdLine));

    /* New line gets text after cursor */
    WubuEdLine *next = &tab->lines[tab->cursor_line + 1];
    int rest = cur->len - col;
    memcpy(next->text, &cur->text[col], rest);
    next->text[rest] = '\0';
    next->len = rest;

    /* Current line truncated at cursor */
    cur->text[col] = '\0';
    cur->len = col;

    tab->cursor_line++;
    tab->cursor_col = 0;
    tab->n_lines++;
    tab->modified = true;
}

void wubu_ed_delete_char(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab || tab->cursor_line >= tab->n_lines) return;
    WubuEdLine *line = &tab->lines[tab->cursor_line];

    if (tab->cursor_col > 0) {
        /* Delete character before cursor */
        char deleted = line->text[tab->cursor_col - 1];
        char ustr[2] = {deleted, '\0'};
        undo_push(tab, UNDO_DELETE, tab->cursor_line, tab->cursor_col - 1, ustr, 1);
        tab->cursor_col--;
        memmove(&line->text[tab->cursor_col],
                &line->text[tab->cursor_col + 1],
                line->len - tab->cursor_col);
        line->len--;
        line->text[line->len] = '\0';
    } else if (tab->cursor_line > 0) {
        /* Join with previous line */
        WubuEdLine *prev = &tab->lines[tab->cursor_line - 1];
        int old_prev_len = prev->len;
        /* Record undo: store the joined state */
        char undo_buf[WUBU_ED_MAX_LINE_LEN];
        memcpy(undo_buf, line->text, (size_t)line->len);
        undo_buf[line->len] = '\0';
        undo_push(tab, UNDO_DELETE, tab->cursor_line, 0, undo_buf, line->len);
        
        tab->cursor_col = prev->len;
        memcpy(&prev->text[prev->len], line->text, (size_t)line->len);
        prev->len += line->len;
        prev->text[prev->len] = '\0';
        /* Remove current line */
        memmove(&tab->lines[tab->cursor_line],
                &tab->lines[tab->cursor_line + 1],
                (tab->n_lines - tab->cursor_line - 1) * sizeof(WubuEdLine));
        tab->n_lines--;
        tab->cursor_line--;
    }
    tab->modified = true;
}

void wubu_ed_delete_forward(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    /* Move cursor right then backspace */
    WubuEdLine *line = &tab->lines[tab->cursor_line];
    if (tab->cursor_col < line->len) {
        tab->cursor_col++;
        wubu_ed_delete_char(ed);
    } else if (tab->cursor_line < tab->n_lines - 1) {
        tab->cursor_line++;
        tab->cursor_col = 0;
        wubu_ed_delete_char(ed);
    }
}

void wubu_ed_insert_text(WubuEditor *ed, const char *text) {
    if (!ed || !text) return;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') wubu_ed_insert_newline(ed);
        else wubu_ed_insert_char(ed, *p);
    }
}

/* -- Undo/Redo ---------------------------------------------------- */

static void undo_push(WubuEdTab *tab, WubuUndoKind kind, int line, int col, const char *text, int text_len) {
    if (!tab || tab->undo_count >= WUBU_ED_MAX_UNDO) return;
    WubuUndo *u = &tab->undo_stack[tab->undo_pos];
    u->kind = kind;
    u->line = line;
    u->col = col;
    if (text && text_len > 0) {
        int len = text_len < WUBU_ED_MAX_LINE_LEN ? text_len : WUBU_ED_MAX_LINE_LEN - 1;
        memcpy(u->text, text, (size_t)len);
        u->text[len] = '\0';
        u->text_len = len;
    } else {
        u->text[0] = '\0';
        u->text_len = 0;
    }
    tab->undo_pos = (tab->undo_pos + 1) % WUBU_ED_MAX_UNDO;
    tab->undo_count++;
}

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
        /* Remove the inserted characters */
        memmove(&line->text[u->col], &line->text[u->col + u->text_len],
                line->len - u->col - u->text_len + 1);
        line->len -= u->text_len;
        tab->cursor_line = u->line;
        tab->cursor_col = u->col;
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
        tab->cursor_col = u->col + u->text_len;
        break;
    }
    case UNDO_REPLACE: {
        /* Undo a replace = restore original text */
        if (u->line >= tab->n_lines) break;
        WubuEdLine *line = &tab->lines[u->line];
        if (u->col + u->text_len > line->len) break;
        memcpy(&line->text[u->col], u->text, (size_t)u->text_len);
        tab->cursor_line = u->line;
        tab->cursor_col = u->col;
        break;
    }
    }
    
    tab->undo_count--;
    tab->modified = true;
}

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
            tab->cursor_col = u->col + u->text_len;
            tab->undo_pos = (tab->undo_pos + 1) % WUBU_ED_MAX_UNDO;
            tab->undo_count++;
            tab->modified = true;
        }
    }
}

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

/* -- Selection ------------------------------------------------------ */

void wubu_ed_select_all(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    tab->sel_start_line = 0;
    tab->sel_start_col = 0;
    tab->sel_end_line = tab->n_lines - 1;
    tab->sel_end_col = tab->lines[tab->n_lines - 1].len;
}

/* Helper: get selected text into a buffer, returns allocated string */
static char *get_selection(WubuEdTab *tab, size_t *out_len) {
    if (!tab || tab->sel_start_line < 0 || tab->sel_end_line < 0) return NULL;
    if (tab->sel_start_line > tab->sel_end_line) return NULL;
    
    /* Calculate total size needed */
    size_t total = 0;
    for (int i = tab->sel_start_line; i <= tab->sel_end_line; i++) {
        if (i >= tab->n_lines) break;
        int start_col = (i == tab->sel_start_line) ? tab->sel_start_col : 0;
        int end_col = (i == tab->sel_end_line) ? tab->sel_end_col : tab->lines[i].len;
        if (start_col > tab->lines[i].len) start_col = tab->lines[i].len;
        if (end_col > tab->lines[i].len) end_col = tab->lines[i].len;
        if (end_col > start_col) total += (size_t)(end_col - start_col);
        if (i < tab->sel_end_line) total++; /* newline */
    }
    
    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;
    
    size_t pos = 0;
    for (int i = tab->sel_start_line; i <= tab->sel_end_line; i++) {
        if (i >= tab->n_lines) break;
        int start_col = (i == tab->sel_start_line) ? tab->sel_start_col : 0;
        int end_col = (i == tab->sel_end_line) ? tab->sel_end_col : tab->lines[i].len;
        if (start_col > tab->lines[i].len) start_col = tab->lines[i].len;
        if (end_col > tab->lines[i].len) end_col = tab->lines[i].len;
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

/* Helper: delete selection */
static void delete_selection(WubuEditor *ed, WubuEdTab *tab) {
    if (!tab || tab->sel_start_line < 0 || tab->sel_end_line < 0) return;
    
    int sl = tab->sel_start_line, sc = tab->sel_start_col;
    int el = tab->sel_end_line, ec = tab->sel_end_col;
    
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
        WubuEdLine *last = &tab->lines[el];
        if (sc > first->len) sc = first->len;
        if (ec > last->len) ec = last->len;
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
    
    tab->cursor_line = sl;
    tab->cursor_col = sc;
    tab->sel_start_line = -1;
    tab->sel_end_line = -1;
    tab->modified = true;
}

void wubu_ed_cut(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    
    size_t sel_len;
    char *sel = get_selection(tab, &sel_len);
    if (!sel || sel_len == 0) { free(sel); return; }
    
    /* Copy to clipboard */
    free(ed->clipboard);
    ed->clipboard = sel;
    ed->clipboard_size = sel_len;
    
    /* Delete selection */
    delete_selection(ed, tab);
}

void wubu_ed_copy(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    
    size_t sel_len;
    char *sel = get_selection(tab, &sel_len);
    if (!sel || sel_len == 0) { free(sel); return; }
    
    free(ed->clipboard);
    ed->clipboard = sel;
    ed->clipboard_size = sel_len;
}

void wubu_ed_paste(WubuEditor *ed) {
    if (!ed || !ed->clipboard || ed->clipboard_size == 0) return;
    wubu_ed_insert_text(ed, ed->clipboard);
}

/* -- Find/Replace ---------------------------------------------------- */

int wubu_ed_find_next(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!ed || !ed->find.find_text[0]) return -1;
    
    int start_line = tab->cursor_line;
    int start_col = tab->cursor_col + 1; /* Start after cursor */
    
    for (int pass = 0; pass < 2; pass++) {
        for (int i = start_line; i < tab->n_lines; i++) {
            WubuEdLine *line = &tab->lines[i];
            int col = (i == start_line) ? start_col : 0;
            if (col >= line->len) continue;
            
            const char *found = strstr(line->text + col, ed->find.find_text);
            if (found) {
                int found_col = (int)(found - line->text);
                tab->cursor_line = i;
                tab->cursor_col = found_col;
                ed->find.last_found_line = i;
                ed->find.last_found_col = found_col;
                return i;
            }
        }
        /* Wrap around */
        start_line = 0;
        start_col = 0;
    }
    return -1;
}

int wubu_ed_find_prev(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!ed || !ed->find.find_text[0]) return -1;
    
    int start_line = tab->cursor_line;
    int start_col = tab->cursor_col - 1;
    
    for (int i = start_line; i >= 0; i--) {
        WubuEdLine *line = &tab->lines[i];
        int col = (i == start_line) ? start_col : line->len;
        if (col < 0 || col >= line->len) continue;
        
        /* Search backward in this line */
        for (int c = col; c >= 0; c--) {
            if (strncmp(line->text + c, ed->find.find_text,
                        strlen(ed->find.find_text)) == 0) {
                tab->cursor_line = i;
                tab->cursor_col = c;
                ed->find.last_found_line = i;
                ed->find.last_found_col = c;
                return i;
            }
        }
    }
    return -1;
}

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
            undo_push(tab, UNDO_REPLACE, ed->find.last_found_line,
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

/* -- Code Folding --------------------------------------------------- */

/* Helper: find the matching fold end for a fold start at given line */
static int find_fold_end(WubuEdTab *tab, int start_line) {
    if (start_line < 0 || start_line >= tab->n_lines) return -1;
    int level = tab->lines[start_line].fold_level;
    for (int i = start_line + 1; i < tab->n_lines; i++) {
        if (tab->lines[i].fold_level == level && tab->lines[i].fold_start)
            return i; /* Nested fold start at same level */
        if (tab->lines[i].fold_level < level)
            return i - 1; /* End of this fold block */
    }
    return tab->n_lines - 1;
}

void wubu_ed_fold_toggle(WubuEditor *ed, int line) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab || line < 0 || line >= tab->n_lines) return;
    
    if (!tab->lines[line].fold_start) return;
    
    if (tab->lines[line].folded) {
        /* Unfold: mark this line and all children as not folded */
        tab->lines[line].folded = false;
        int end = find_fold_end(tab, line);
        for (int i = line + 1; i <= end && i < tab->n_lines; i++) {
            tab->lines[i].folded = false;
        }
    } else {
        /* Fold: mark children as folded */
        int end = find_fold_end(tab, line);
        for (int i = line + 1; i <= end && i < tab->n_lines; i++) {
            tab->lines[i].folded = true;
        }
    }
}

void wubu_ed_fold_all(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    for (int i = 0; i < tab->n_lines; i++) {
        if (tab->lines[i].fold_start) {
            int end = find_fold_end(tab, i);
            for (int j = i + 1; j <= end && j < tab->n_lines; j++) {
                tab->lines[j].folded = true;
            }
        }
    }
}

void wubu_ed_unfold_all(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return;
    for (int i = 0; i < tab->n_lines; i++) {
        tab->lines[i].folded = false;
    }
}

/* -- Bookmarks ------------------------------------------------------ */




/* -- View Toggles ------------------------------------------------- */

void wubu_ed_toggle_line_nums(WubuEditor *ed)   { if (ed) ed->show_line_nums = !ed->show_line_nums; }
void wubu_ed_toggle_word_wrap(WubuEditor *ed)    { WubuEdTab *t = wubu_ed_current_tab(ed); if (t) t->word_wrap = !t->word_wrap; }
void wubu_ed_toggle_split(WubuEditor *ed)        { if (ed) ed->split = !ed->split; }
void wubu_ed_toggle_whitespace(WubuEditor *ed)   { WubuEdTab *t = wubu_ed_current_tab(ed); if (t) t->show_whitespace = !t->show_whitespace; }

/* -- Macro ---------------------------------------------------------- */

static void macro_record(WubuEditor *ed, char ch) {
    if (!ed || !ed->macro_recording) return;
    if (ed->macro_len + 1 >= ed->macro_size) {
        size_t new_size = ed->macro_size ? ed->macro_size * 2 : 256;
        char *new_buf = (char *)realloc(ed->macro_buf, new_size);
        if (!new_buf) return;
        ed->macro_buf = new_buf;
        ed->macro_size = new_size;
    }
    ed->macro_buf[ed->macro_len++] = ch;
}

void wubu_ed_macro_start(WubuEditor *ed) {
    if (!ed) return;
    ed->macro_recording = true;
    ed->macro_len = 0;
}

void wubu_ed_macro_stop(WubuEditor *ed) {
    if (ed) ed->macro_recording = false;
}

void wubu_ed_macro_play(WubuEditor *ed) {
    if (!ed || !ed->macro_buf || ed->macro_len == 0) return;
    ed->macro_playing = true;
    for (size_t i = 0; i < ed->macro_len; i++) {
        char ch = ed->macro_buf[i];
        if (ch == '\n') wubu_ed_insert_newline(ed);
        else wubu_ed_insert_char(ed, ch);
    }
    ed->macro_playing = false;
}

/* -- Session -------------------------------------------------------- */

int wubu_ed_session_save(WubuEditor *ed, const char *path) {
    if (!ed || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "# WuBuOS Editor Session\n");
    fprintf(f, "tabs=%d\n", ed->n_tabs);
    fprintf(f, "active_tab=%d\n", ed->active_tab);
    
    for (int i = 0; i < ed->n_tabs; i++) {
        WubuEdTab *tab = &ed->tabs[i];
        fprintf(f, "\n[tab %d]\n", i);
        fprintf(f, "filename=%s\n", tab->filename);
        fprintf(f, "cursor_line=%d\n", tab->cursor_line);
        fprintf(f, "cursor_col=%d\n", tab->cursor_col);
        fprintf(f, "n_lines=%d\n", tab->n_lines);
        for (int j = 0; j < tab->n_lines; j++) {
            fprintf(f, "%s\n", tab->lines[j].text);
        }
        fprintf(f, "[end tab %d]\n", i);
    }
    
    fclose(f);
    return 0;
}

int wubu_ed_session_load(WubuEditor *ed, const char *path) {
    if (!ed || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[WUBU_ED_MAX_LINE_LEN + 2];
    int current_tab = -1;
    int line_idx = 0;
    
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        
        if (strncmp(line, "tabs=", 5) == 0) {
            ed->n_tabs = atoi(line + 5);
        } else if (strncmp(line, "active_tab=", 11) == 0) {
            ed->active_tab = atoi(line + 11);
        } else if (strncmp(line, "[tab ", 5) == 0) {
            current_tab = atoi(line + 5);
            line_idx = 0;
            if (current_tab >= 0 && current_tab < WUBU_ED_MAX_TABS) {
                memset(&ed->tabs[current_tab], 0, sizeof(WubuEdTab));
            }
        } else if (strncmp(line, "[end tab ", 9) == 0) {
            if (current_tab >= 0 && current_tab < WUBU_ED_MAX_TABS) {
                ed->tabs[current_tab].n_lines = line_idx;
            }
            current_tab = -1;
        } else if (current_tab >= 0 && current_tab < WUBU_ED_MAX_TABS) {
            WubuEdTab *tab = &ed->tabs[current_tab];
            if (strncmp(line, "filename=", 9) == 0) {
                strncpy(tab->filename, line + 9, sizeof(tab->filename) - 1);
            } else if (strncmp(line, "cursor_line=", 12) == 0) {
                tab->cursor_line = atoi(line + 12);
            } else if (strncmp(line, "cursor_col=", 11) == 0) {
                tab->cursor_col = atoi(line + 11);
            } else if (strncmp(line, "n_lines=", 8) == 0) {
                int n = atoi(line + 8);
                tab->lines_capacity = n + 64;
                tab->lines = (WubuEdLine *)calloc((size_t)tab->lines_capacity, sizeof(WubuEdLine));
            } else if (tab->lines && line_idx < WUBU_ED_MAX_LINES) {
                WubuEdLine *tl = &tab->lines[line_idx];
                strncpy(tl->text, line, WUBU_ED_MAX_LINE_LEN - 1);
                tl->len = (int)strlen(tl->text);
                line_idx++;
            }
        }
    }
    
    fclose(f);
    return 0;
}

