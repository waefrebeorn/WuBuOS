/*
 * add_to_library_dialog.c -- see add_to_library_dialog.h. A model over the shared card.
 *
 * C11.
 */
#include "add_to_library_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ChoiceDialog g_card;
static AddToLibraryState g_atl;

bool add_to_library_open(WubuLibraryId id, int fb_w, int fb_h)
{
    add_to_library_close();
    if ((int)id < 0 || (int)id >= WUBU_LIB_COUNT) return false;

    wubu_libraries_load(&g_atl.libs);

    char sugg[WUBU_LIB_MAX_FOLDERS][WUBU_LIB_PATH_MAX];
    int n = wubu_library_suggest(&g_atl.libs, id, sugg, WUBU_LIB_MAX_FOLDERS);
    if (n <= 0) return false;              /* nothing to offer: the caller says so, no empty card */

    char title[CD_TITLE_MAX];
    snprintf(title, sizeof title, "Add folders to %s", wubu_library_name(id));
    /* The surface is fixed at open: the card is positioned and hit-tested from the SAME numbers, so a
     * later resize can leave it off-centre but never leaves the painted card and the clickable card in
     * two different places. No footer tick -- there is no folder-watching feature to promise. */
    cd_open(&g_card, title, "Folders on this machine that are not in the library yet.", true,
            fb_w, fb_h);
    snprintf(g_card.ok_label, sizeof g_card.ok_label, "Add");

    /* A card that does not fit its surface is not a card: refuse instead of drawing it clipped. */
    if (!cd_rect(&g_card, NULL, NULL, NULL, NULL)) {
        cd_close(&g_card);
        return false;
    }

    for (int i = 0; i < n; i++) {
        const char *base = strrchr(sugg[i], '/');
        cd_add(&g_card, base ? base + 1 : sugg[i], sugg[i], true);   /* every suggestion is offered on */
    }

    g_atl.library = id;
    g_atl.open = true;
    g_atl.added = 0;
    g_atl.status[0] = '\0';
    g_atl.card = &g_card;
    return true;
}

int add_to_library_apply(void)
{
    if (!g_atl.open || !g_atl.card) return 0;

    int added = 0;
    for (int i = 0; i < g_card.count; i++) {
        if (!g_card.rows[i].checked) continue;
        /* The detail holds the absolute path; that is what gets stored, not the display name. */
        int rc = wubu_library_add(&g_atl.libs, g_atl.library, g_card.rows[i].detail);
        if (rc == 0) added++;
    }

    if (added > 0) {
        if (wubu_libraries_save(&g_atl.libs) != 0) {
            snprintf(g_atl.status, sizeof g_atl.status,
                     "%d folder(s) added, but the library file could not be written", added);
        } else {
            snprintf(g_atl.status, sizeof g_atl.status, "%d folder(s) added to %s", added,
                     wubu_library_name(g_atl.library));
        }
    } else {
        snprintf(g_atl.status, sizeof g_atl.status, "Nothing was ticked; %s is unchanged",
                 wubu_library_name(g_atl.library));
    }

    g_atl.added = added;
    add_to_library_close();
    return added;
}

void add_to_library_close(void)
{
    g_atl.open = false;
    g_atl.card = NULL;
    cd_close(&g_card);
}

bool add_to_library_is_open(void) { return g_atl.open; }
const AddToLibraryState *add_to_library_state(void) { return &g_atl; }
const char *add_to_library_status(void) { return g_atl.status; }

int add_to_library_click(int x, int y, int fb_w, int fb_h)
{
    if (!g_atl.open || !g_atl.card) return 0;
    (void)fb_w; (void)fb_h;   /* the card remembers the surface it was opened on */

    int hit = cd_hit(&g_card, x, y);
    if (hit >= 0) {
        g_card.selected = hit;
        if (!g_card.rows[hit].disabled) g_card.rows[hit].checked = !g_card.rows[hit].checked;
        return 0;
    }
    if (hit == CD_HIT_FOOTER) { g_card.footer_checked = !g_card.footer_checked; return 0; }
    if (hit == CD_HIT_OK) { (void)add_to_library_apply(); return 1; }
    if (hit == CD_HIT_CANCEL) {
        snprintf(g_atl.status, sizeof g_atl.status, "Nothing was changed");
        add_to_library_close();
        return 2;
    }
    return 0;
}

bool add_to_library_key(uint32_t key, int fb_w, int fb_h)
{
    if (!g_atl.open || !g_atl.card) return false;
    (void)fb_w; (void)fb_h;

    if (key == 0x1C || key == 0xE01C || key == '\r' || key == '\n') {
        (void)add_to_library_apply();
        return true;
    }
    if (key == 27) {
        snprintf(g_atl.status, sizeof g_atl.status, "Nothing was changed");
        add_to_library_close();
        return true;
    }
    if (cd_key(&g_card, key)) return true;
    return true;   /* modal: nothing leaks to the desktop behind it */
}

void add_to_library_render(uint32_t *fb, int fb_w, int fb_h)
{
    if (!g_atl.open || !g_atl.card) return;
    cd_render(&g_card, fb, fb_w, fb_h);
}
