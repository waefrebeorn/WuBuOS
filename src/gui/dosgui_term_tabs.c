/* dosgui_term_tabs.c -- Terminal tab-management subsystem.
 *
 * Self-contained: tab creation/close/switch/move + active-tab accessors and the
 * default-shell resolver. Uses TermState/TermTab (dosgui_term.h) and g_term via
 * dosgui_term_state(). term_tab_bar_layout (render module) declared in
 * dosgui_term_internal.h. Minimal includes.
 */

#include "dosgui_term_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *term_default_shell(void) {
    const char *shell = getenv("SHELL");
    return shell ? shell : "/bin/bash";
}

int dosgui_term_new_tab(TermSessionType type, const char *label, const char *shell) {
    if (g_term.tab_count >= TERM_MAX_TABS) return -1;

    int idx = g_term.tab_count;
    TermTab *tab = &g_term.tabs[idx];
    memset(tab, 0, sizeof(TermTab));
    
    tab->type = type;
    tab->active = false;
    tab->dirty = true;

    if (label) {
        strncpy(tab->label, label, TERM_TAB_LABEL_LEN - 1);
    } else {
        switch (type) {
            case TERM_SESSION_SHELL: snprintf(tab->label, sizeof(tab->label), "Shell %d", idx + 1); break;
            case TERM_SESSION_HOLYC: snprintf(tab->label, sizeof(tab->label), "HolyC %d", idx + 1); break;
            case TERM_SESSION_CONTAINER: snprintf(tab->label, sizeof(tab->label), "Container %d", idx + 1); break;
        }
    }

    /* Initialize session */
    switch (type) {
        case TERM_SESSION_SHELL:
            if (term_pty_spawn(shell ? shell : term_default_shell(), getenv("HOME"), &tab->session.pty, NULL) < 0) {
                return -1;
            }
            break;
        case TERM_SESSION_HOLYC:
            /* E4: embed the wubu_holyd HolyC REPL as a real PTY-backed
             * process so the Desktop terminal hosts a live interactive REPL. */
            {
                static const char *holy_argv[] = { "--repl", NULL };
                if (term_pty_spawn("wubu_holyd", getenv("HOME"), &tab->session.holyc.pty, holy_argv) < 0) {
                    return -1;
                }
            }
            break;
        case TERM_SESSION_CONTAINER:
            if (shell) {
                strncpy(tab->session.container.shell, shell, sizeof(tab->session.container.shell) - 1);
            } else {
                strncpy(tab->session.container.shell, "/bin/bash", sizeof(tab->session.container.shell) - 1);
            }
            if (term_container_spawn(&tab->session.container, tab->session.container.container_name) < 0) {
                return -1;
            }
            break;
    }

    g_term.tab_count++;
    
    /* Make it active */
    if (g_term.tab_count == 1) {
        g_term.active_tab = 0;
        tab->active = true;
    }

    return idx;
}

void dosgui_term_close_tab(int idx) {
    if (idx < 0 || idx >= g_term.tab_count) return;

    TermTab *tab = &g_term.tabs[idx];
    
    if (tab->type == TERM_SESSION_SHELL) {
        term_pty_cleanup(&tab->session.pty);
    } else if (tab->type == TERM_SESSION_CONTAINER) {
        term_container_cleanup(&tab->session.container);
    }

    /* Shift remaining tabs left */
    for (int i = idx; i < g_term.tab_count - 1; i++) {
        g_term.tabs[i] = g_term.tabs[i + 1];
    }
    g_term.tab_count--;

    /* Adjust active tab */
    if (g_term.active_tab >= g_term.tab_count) {
        g_term.active_tab = g_term.tab_count - 1;
    }
    if (g_term.active_tab >= 0 && g_term.tab_count > 0) {
        g_term.tabs[g_term.active_tab].active = true;
    }

    /* If no tabs left, close window */
    if (g_term.tab_count == 0) {
        dosgui_term_hide();
    }
}

void dosgui_term_switch_tab(int idx) {
    if (idx < 0 || idx >= g_term.tab_count) return;
    if (idx == g_term.active_tab) return;

    g_term.tabs[g_term.active_tab].active = false;
    g_term.active_tab = idx;
    g_term.tabs[g_term.active_tab].active = true;
    g_term.tabs[g_term.active_tab].dirty = true;
}

void dosgui_term_move_tab(int from, int to) {
    if (from < 0 || from >= g_term.tab_count) return;
    if (to < 0 || to >= g_term.tab_count) return;
    if (from == to) return;

    TermTab tab = g_term.tabs[from];
    
    if (from < to) {
        for (int i = from; i < to; i++) {
            g_term.tabs[i] = g_term.tabs[i + 1];
        }
    } else {
        for (int i = from; i > to; i--) {
            g_term.tabs[i] = g_term.tabs[i - 1];
        }
    }
    g_term.tabs[to] = tab;

    /* Update active_tab if it moved */
    if (g_term.active_tab == from) {
        g_term.active_tab = to;
    } else if (from < g_term.active_tab && g_term.active_tab <= to) {
        g_term.active_tab--;
    } else if (to <= g_term.active_tab && g_term.active_tab < from) {
        g_term.active_tab++;
    }
}

int dosgui_term_get_active_tab(void) {
    return g_term.active_tab;
}

TermTab *dosgui_term_get_tab(int idx) {
    if (idx < 0 || idx >= g_term.tab_count) return NULL;
    return &g_term.tabs[idx];
}
