/* dosgui_startmenu_search.c -- Start menu search + recent apps.
 *
 * Self-contained subsystem extracted from dosgui_startmenu.c. Operates on the
 * shared g_search / g_recent / g_program_db state (declared extern in
 * dosgui_startmenu_internal.h). Minimal includes.
 */

#include "wubu_theme.h"
#include "dosgui_startmenu_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void dosgui_startmenu_search_init(void) {
    memset(&g_search, 0, sizeof(g_search));
    g_search.cursor_pos = 0;
}

void dosgui_startmenu_search_update(const char *query) {
    if (!query) {
        g_search.query[0] = '\0';
        g_search.cursor_pos = 0;
        g_search.result_count = 0;
        g_search.active = false;
        return;
    }
    
    /* Copy through a local first: callers pass g_search.query itself, and
     * strncpy(dst, dst, n) is UB that -O3 turns into corruption. */
    char in[DOSGUI_SEARCH_MAX_LEN];
    size_t in_len = strlen(query);
    if (in_len >= sizeof(in)) in_len = sizeof(in) - 1;
    memcpy(in, query, in_len);
    in[in_len] = '\0';
    memcpy(g_search.query, in, in_len + 1);
    g_search.cursor_pos = (int)in_len;
    g_search.active = true;
    g_search.result_count = 0;
    
    if (g_search.query[0] == '\0') return;
    
    char lower_query[DOSGUI_SEARCH_MAX_LEN];
    int qn = 0;
    for (; g_search.query[qn] && qn < DOSGUI_SEARCH_MAX_LEN - 1; qn++)
        lower_query[qn] = (char)tolower((unsigned char)g_search.query[qn]);
    lower_query[qn] = '\0';   /* never strlen() an uninitialized buffer */
    
    /* Two passes: prefix matches first, then substring matches. A user who
     * types "cal" expects Calculator above "Local Calendar", not whichever
     * the DB happened to store first. */
    size_t qlen = strlen(lower_query);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < g_program_db.count && g_search.result_count < DOSGUI_MAX_PROGRAM_ENTRIES; i++) {
            SmProgramEntry *e = &g_program_db.entries[i];
            char lower_name[48];
            size_t nl = strlen(e->name);
            if (nl >= sizeof(lower_name)) nl = sizeof(lower_name) - 1;
            for (size_t j = 0; j < nl; j++) lower_name[j] = (char)tolower((unsigned char)e->name[j]);
            lower_name[nl] = '\0';

            int is_prefix = (qlen > 0 && strncmp(lower_name, lower_query, qlen) == 0);
            int is_sub = (strstr(lower_name, lower_query) != NULL);
            if ((pass == 0 && is_prefix) || (pass == 1 && is_sub && !is_prefix))
                g_search.results[g_search.result_count++] = e;
        }
    }
}

void dosgui_startmenu_search_clear(void) {
    g_search.query[0] = '\0';
    g_search.cursor_pos = 0;
    g_search.result_count = 0;
    g_search.active = false;
}

int dosgui_startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count) {
    if (!g_search.active) return -1;
    if (out_results) *out_results = g_search.results;
    if (out_count) *out_count = g_search.result_count;
    return 0;
}

/* -- Recently Used ------------------------------------------------ */

void dosgui_startmenu_recent_add(const char *app_name) {
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

int dosgui_startmenu_recent_get(SmProgramEntry **out_entries, int max) {
    int count = g_recent_count < max ? g_recent_count : max;
    for (int i = 0; i < count; i++) {
        out_entries[i] = &g_recent[i];
    }
    return count;
}
