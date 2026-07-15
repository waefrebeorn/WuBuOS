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

/* undo recording is done via the public wubu_ed_undo_push() primitive
 * (defined in wubu_editor_undo.c). */

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
    wubu_ed_undo_push(tab, UNDO_INSERT, tab->cursor_line, tab->cursor_col, ustr, 1);
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
        wubu_ed_undo_push(tab, UNDO_DELETE, tab->cursor_line, tab->cursor_col - 1, ustr, 1);
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
        wubu_ed_undo_push(tab, UNDO_DELETE, tab->cursor_line, 0, undo_buf, line->len);
        
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

/* -- Undo/Redo ------------------------------------------------------- */
/* Implemented in wubu_editor_undo.c (wubu_ed_undo / _redo / _can_undo /
 * _can_redo and the wubu_ed_undo_push primitive). */

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

