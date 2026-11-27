/* startmenu_search.c -- Start menu search + recent apps.
 *
 * Self-contained subsystem extracted from startmenu.c. Operates on the
 * shared g_search / g_recent / g_program_db state (declared extern in
 * ./startmenu_internal.h). Minimal includes.
 */

#include "../wubu_theme.h"
#include "./startmenu_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Selection, for the arrow keys and for tests. Resets whenever the query changes: a stale index would launch a
 * different app than the one highlighted. */
void startmenu_search_select(int idx) {
    if (idx < 0) idx = 0;
    if (g_search.result_count > 0 && idx > g_search.result_count - 1) idx = g_search.result_count - 1;
    g_search.selected = idx;
}
int startmenu_search_selected(void) { return g_search.selected; }

void startmenu_search_init(void) {
    memset(&g_search, 0, sizeof(g_search));
    g_search.cursor_pos = 0;
}

void startmenu_search_update(const char *query) {
    if (!query) {
        g_search.query[0] = '\0';
        g_search.cursor_pos = 0;
        g_search.result_count = 0;
        g_search.selected = 0;
        g_search.scroll = 0;
        g_search.active = false;
        return;
    }
    
    strncpy(g_search.query, query, DOSGUI_SEARCH_MAX_LEN - 1);
    g_search.query[DOSGUI_SEARCH_MAX_LEN - 1] = '\0';
    g_search.cursor_pos = strlen(g_search.query);
    g_search.active = true;
    g_search.result_count = 0;
    
    if (g_search.query[0] == '\0') return;
    
    /* Lowercase the query.  lower_query must be NUL-terminated at the query
     * length: leaving it to chance made strstr() match against stack garbage
     * and every search came back empty. */
    char lower_query[DOSGUI_SEARCH_MAX_LEN];
    int qlen = (int)strlen(g_search.query);
    if (qlen > DOSGUI_SEARCH_MAX_LEN - 1) qlen = DOSGUI_SEARCH_MAX_LEN - 1;
    for (int i = 0; i < qlen; i++)
        lower_query[i] = (char)tolower((unsigned char)g_search.query[i]);
    lower_query[qlen] = '\0';
    
    /* RANK the matches: exact name, then prefix, then substring, each in database order.
     *
     * A plain strstr scan in database order is not a search, it is a filter: typing the app's FULL name
     * ("File Manager") returned "File Renamer" first because it sits earlier in the database, and pressing
     * Enter opened the wrong entry -- measured in a live session, where the name the user could read on the
     * desktop found something that was not it. Ranking is only parity if the exact answer cannot lose. */
    g_search.selected = 0;      /* a new query starts at the best match (which is now ranked first) */
    g_search.scroll = 0;
    static unsigned char taken[DOSGUI_MAX_PROGRAM_ENTRIES];
    memset(taken, 0, sizeof taken);
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < g_program_db.count && g_search.result_count < DOSGUI_MAX_PROGRAM_ENTRIES; i++) {
            if (taken[i]) continue;
            SmProgramEntry *e = &g_program_db.entries[i];
            char lower_name[48];
            int nlen = (int)strlen(e->name);
            if (nlen > 47) nlen = 47;
            for (int j = 0; j < nlen; j++)
                lower_name[j] = (char)tolower((unsigned char)e->name[j]);
            lower_name[nlen] = '\0';

            int hit;
            if (pass == 0)      hit = (strcmp(lower_name, lower_query) == 0);
            else if (pass == 1) hit = (strncmp(lower_name, lower_query, (size_t)qlen) == 0);
            else                hit = (strstr(lower_name, lower_query) != NULL);
            if (hit) {
                g_search.results[g_search.result_count++] = e;
                taken[i] = 1;
            }
        }
    }
}

void startmenu_search_clear(void) {
    g_search.query[0] = '\0';
    g_search.cursor_pos = 0;
    g_search.result_count = 0;
    g_search.active = false;
}

int startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count) {
    if (!g_search.active) return -1;
    if (out_results) *out_results = g_search.results;
    if (out_count) *out_count = g_search.result_count;
    return 0;
}

/* -- Recently Used ------------------------------------------------ */

void startmenu_recent_add(const char *app_name) {
    if (!app_name) return;
    
    for (int i = 0; i < g_recent_count; i++) {
        if (strcmp(g_recent[i].name, app_name) == 0) {
            SmProgramEntry temp = g_recent[i];
            memmove(&g_recent[1], &g_recent[0], i * sizeof(SmProgramEntry));
            g_recent[0] = temp;
            temp.last_run = time(NULL);
            temp.run_count++;
            return;
        }
    }
    
    for (int i = 0; i < g_program_db.count; i++) {
        if (strcmp(g_program_db.entries[i].name, app_name) == 0) {
            if (g_recent_count < DOSGUI_MAX_RECENT) {
                g_recent[g_recent_count++] = g_program_db.entries[i];
            } else {
                memmove(&g_recent[1], &g_recent[0], (DOSGUI_MAX_RECENT - 1) * sizeof(SmProgramEntry));
            }
            g_recent[0] = g_program_db.entries[i];
            g_recent[0].last_run = time(NULL);
            g_recent[0].run_count++;
            break;
        }
    }
}

int startmenu_recent_get(SmProgramEntry **out_entries, int max) {
    int count = g_recent_count < max ? g_recent_count : max;
    for (int i = 0; i < count; i++) {
        out_entries[i] = &g_recent[i];
    }
    return count;
}
