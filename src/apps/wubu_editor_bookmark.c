/* wubu_editor_bookmark.c -- Editor bookmark subsystem (self-contained).
 *
 * wubu_ed_bookmark_toggle/next/prev. Uses wubu_ed_current_tab (declared in
 * wubu_editor.h) and the WubuEditor/WubuEdTab types. Minimal includes.
 */

#include "wubu_editor.h"

void wubu_ed_bookmark_toggle(WubuEditor *ed, int line) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab || line < 0 || line >= tab->n_lines) return;
    tab->lines[line].bookmark = !tab->lines[line].bookmark;
}

int wubu_ed_bookmark_next(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return -1;
    for (int i = tab->cursor_line + 1; i < tab->n_lines; i++) {
        if (tab->lines[i].bookmark) {
            tab->cursor_line = i;
            tab->cursor_col = 0;
            return i;
        }
    }
    /* Wrap */
    for (int i = 0; i <= tab->cursor_line; i++) {
        if (tab->lines[i].bookmark) {
            tab->cursor_line = i;
            tab->cursor_col = 0;
            return i;
        }
    }
    return -1;
}

int wubu_ed_bookmark_prev(WubuEditor *ed) {
    WubuEdTab *tab = wubu_ed_current_tab(ed);
    if (!tab) return -1;
    for (int i = tab->cursor_line - 1; i >= 0; i--) {
        if (tab->lines[i].bookmark) {
            tab->cursor_line = i;
            tab->cursor_col = 0;
            return i;
        }
    }
    /* Wrap */
    for (int i = tab->n_lines - 1; i >= tab->cursor_line; i--) {
        if (tab->lines[i].bookmark) {
            tab->cursor_line = i;
            tab->cursor_col = 0;
            return i;
        }
    }
    return -1;
}
