/*
 * add_to_library_dialog_test.c -- the suggestions card must offer real folders, and accepting must
 * write what it offered.
 *
 * C11.
 */
#include "add_to_library_dialog.h"

#include "../../kernel/vbe.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

static const WubuThemeColors kTheme = {
    .win_face = 0xFF202020u, .border_dark = 0xFF3A3A3A, .win_title_text = 0xFFFFFFFFu,
    .body_text = 0xFFE0E0E0u, .body_text_ina = 0xFF9A9A9Au, .select_bg = 0xFF0078D4u,
    .select_text = 0xFFFFFFFFu, .btn_face = 0xFF2D2D2Du, .btn_text = 0xFFE0E0E0u,
    .win_shadow = 0xFF000000u,
};
const WubuThemeColors *wubu_theme_colors(void) { return &kTheme; }

static void touch_dir(const char *p) { (void)mkdir(p, 0755); }

int main(void)
{
    vbe_init(1600, 900);
    char root[] = "/tmp/atltestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }

    char home[300], conf[400], pics[400], music[400], docs[400];
    snprintf(home, sizeof home, "%s/home", root);
    snprintf(conf, sizeof conf, "%s/libraries.conf", root);
    snprintf(pics, sizeof pics, "%s/Pictures", home);
    snprintf(music, sizeof music, "%s/Music", home);
    snprintf(docs, sizeof docs, "%s/Documents", home);
    touch_dir(home); touch_dir(pics); touch_dir(music); touch_dir(docs);

    setenv("HOME", home, 1);
    setenv("WUBU_LIBRARIES_CONF", conf, 1);
    unsetenv("XDG_MUSIC_DIR"); unsetenv("XDG_PICTURES_DIR"); unsetenv("XDG_DOCUMENTS_DIR");

    printf("=== Test: the card offers the folders that exist and are missing ===\n");
    CHECK(!add_to_library_is_open(), "nothing is open before it is opened");
    CHECK(!add_to_library_open(WUBU_LIB_PICTURES, 60, 40), "a surface too small refuses the card");
    CHECK(add_to_library_open(WUBU_LIB_PICTURES, 1600, 900), "the card opens for Pictures");
    CHECK(add_to_library_is_open(), "the card reports itself open");

    const AddToLibraryState *st = add_to_library_state();
    ChoiceDialog *card = st->card;
    CHECK(card != NULL, "the card exists");
    CHECK(card && card->multi, "the suggestions are a multi-select");
    CHECK(card && card->count >= 2, "at least Music and Documents are offered");
    int saw_music = 0, saw_docs = 0, saw_pics = 0;
    for (int i = 0; card && i < card->count; i++) {
        if (strcmp(card->rows[i].detail, music) == 0) saw_music = 1;
        if (strcmp(card->rows[i].detail, docs) == 0) saw_docs = 1;
        if (strcmp(card->rows[i].detail, pics) == 0) saw_pics = 1;
        CHECK(card->rows[i].detail[0] == '/', "every offered folder is an absolute path");
        CHECK(card->rows[i].checked, "suggestions start ticked");
    }
    CHECK(saw_music && saw_docs, "the sibling folders are offered");
    CHECK(!saw_pics, "the folder already in the library is not offered");

    printf("\n=== Test: cancelling changes nothing ===\n");
    int bx, by, bw, bh;
    CHECK(cd_cancel_rect(card, &bx, &by, &bw, &bh), "the Cancel button has a rect");
    int rc = add_to_library_click(bx + bw / 2, by + bh / 2, 1600, 900);
    CHECK(rc == 2, "Cancel reports a cancel");
    CHECK(!add_to_library_is_open(), "Cancel closes the card");
    FILE *f = fopen(conf, "r");
    CHECK(f == NULL, "Cancel wrote no library file");
    if (f) fclose(f);

    printf("\n=== Test: accepting writes what it offered ===\n");
    CHECK(add_to_library_open(WUBU_LIB_PICTURES, 1600, 900), "the card reopens");
    st = add_to_library_state();
    card = st->card;
    int okx, oky, okw, okh;
    CHECK(cd_ok_rect(card, &okx, &oky, &okw, &okh), "the Add button has a rect");
    rc = add_to_library_click(okx + okw / 2, oky + okh / 2, 1600, 900);
    CHECK(rc == 1, "Add reports an accept");
    CHECK(!add_to_library_is_open(), "Add closes the card");
    CHECK(add_to_library_state()->added >= 2, "both offered folders were added");

    f = fopen(conf, "r");
    CHECK(f != NULL, "accepting wrote the library file");
    if (f) {
        char buf[1024];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = '\0';
        fclose(f);
        CHECK(strstr(buf, music) != NULL, "the Music folder is in the file");
        CHECK(strstr(buf, docs) != NULL, "the Documents folder is in the file");
    }

    /* Reload from disk: the library now holds the default plus what was ticked. */
    WubuLibraries reloaded;
    wubu_libraries_load(&reloaded);
    WubuLibrary *lib = wubu_library_get(&reloaded, WUBU_LIB_PICTURES);
    CHECK(lib->count >= 3, "the reloaded library holds the default and both added folders");

    printf("\n=== Test: a card with nothing ticked changes nothing and says so ===\n");
    (void)remove(conf);   /* the accepts above filled the library; suggestions must exist again */
    CHECK(add_to_library_open(WUBU_LIB_PICTURES, 1600, 900), "the card opens again");
    st = add_to_library_state();
    card = st->card;
    for (int i = 0; card && i < card->count; i++) card->rows[i].checked = false;
    CHECK(add_to_library_apply() == 0, "nothing ticked adds nothing");
    CHECK(strstr(add_to_library_status(), "Nothing was ticked") != NULL,
          "the status says nothing was ticked");

    printf("\n=== Test: the keyboard accepts and cancels, and swallows everything ===\n");
    /* The accepts above filled the library, so start from an empty config again: what is being tested
     * here is key routing, not suggestion arithmetic. */
    (void)remove(conf);
    CHECK(add_to_library_open(WUBU_LIB_PICTURES, 1600, 900), "the card opens for the key test");
    CHECK(add_to_library_key('z', 1600, 900), "an ordinary key is swallowed by the modal");
    CHECK(add_to_library_is_open(), "and does not close the card");
    CHECK(add_to_library_key(0xE050, 1600, 900), "Down is consumed");
    CHECK(add_to_library_key(27, 1600, 900), "Esc is consumed");
    CHECK(!add_to_library_is_open(), "Esc closes the card");
    CHECK(strstr(add_to_library_status(), "Nothing was changed") != NULL, "Esc says nothing changed");

    printf("\n=== Test: nothing to offer is not an empty card ===\n");
    /* Put every home folder into the library, then ask for suggestions: the card must refuse. */
    WubuLibraries full;
    wubu_libraries_load(&full);
    (void)wubu_library_add(&full, WUBU_LIB_PICTURES, music);
    (void)wubu_library_add(&full, WUBU_LIB_PICTURES, docs);
    CHECK(wubu_libraries_save(&full) == 0, "the library is saved with every folder");
    CHECK(!add_to_library_open(WUBU_LIB_PICTURES, 1600, 900),
          "with nothing left to suggest the card does not open");
    CHECK(!add_to_library_is_open(), "and nothing is left open");

    char rm[500];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  add_to_library: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
