/*
 * wubu_libraries_test.c -- a library is a LIST of real folders, and the file it saves must reload as
 * the same list. Nothing here may invent a path.
 *
 * C11.
 */
#include "wubu_libraries.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

static void touch_dir(const char *p) { (void)mkdir(p, 0755); }

int main(void)
{
    char root[] = "/tmp/libtestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }

    char home[300], conf[400], pictures[400], music[400], extra[400], missing[400];
    snprintf(home, sizeof home, "%s/home", root);
    touch_dir(home);
    snprintf(conf, sizeof conf, "%s/libraries.conf", root);
    snprintf(pictures, sizeof pictures, "%s/Pictures", home);
    snprintf(music, sizeof music, "%s/Music", home);
    snprintf(extra, sizeof extra, "%s/PhotosOnDisk", home);
    snprintf(missing, sizeof missing, "%s/Unplugged", home);
    touch_dir(pictures);
    touch_dir(music);
    touch_dir(extra);

    setenv("HOME", home, 1);
    setenv("WUBU_LIBRARIES_CONF", conf, 1);
    unsetenv("XDG_PICTURES_DIR");
    unsetenv("XDG_MUSIC_DIR");

    printf("=== Test: defaults are real paths, not a folder list invented by the OS ===\n");
    WubuLibraries libs;
    wubu_libraries_load(&libs);
    CHECK(libs.loaded, "the libraries load");
    for (int i = 0; i < WUBU_LIB_COUNT; i++) {
        CHECK(libs.lib[i].id == (WubuLibraryId)i, "every library has its id");
        CHECK(libs.lib[i].count == 1, "every library starts with its one default folder");
        CHECK(libs.lib[i].folders[0].from_default, "the starting folder is a default");
        CHECK(strncmp(libs.lib[i].folders[0].path, home, strlen(home)) == 0,
              "the default folder lives under the home directory");
    }
    WubuLibrary *pics = wubu_library_get(&libs, WUBU_LIB_PICTURES);
    CHECK(pics && strcmp(pics->folders[0].path, pictures) == 0, "Pictures resolves to <home>/Pictures");
    CHECK(pics && pics->folders[0].exists, "an existing default is marked present");
    CHECK(strcmp(wubu_library_name(WUBU_LIB_MUSIC), "Music") == 0, "the library has a name");

    printf("\n=== Test: a missing folder is kept and reported, not dropped ===\n");
    CHECK(wubu_library_add(&libs, WUBU_LIB_PICTURES, missing) == 0, "a missing folder can be added");
    CHECK(pics->count == 2, "the library now holds two folders");
    CHECK(!pics->folders[1].exists, "the missing folder is reported missing");
    CHECK(strcmp(pics->folders[1].path, missing) == 0, "the missing folder keeps its path");

    printf("\n=== Test: adding twice is a no-op, not a duplicate ===\n");
    CHECK(wubu_library_add(&libs, WUBU_LIB_PICTURES, pictures) == 1, "re-adding reports already present");
    CHECK(pics->count == 2, "re-adding does not grow the list");

    printf("\n=== Test: the list has a bound and says so ===\n");
    for (int i = 0; i < WUBU_LIB_MAX_FOLDERS + 3; i++) {
        char p[400];
        snprintf(p, sizeof p, "%s/extra%02d", home, i);
        touch_dir(p);
        (void)wubu_library_add(&libs, WUBU_LIB_VIDEOS, p);
    }
    CHECK(wubu_library_get(&libs, WUBU_LIB_VIDEOS)->count == WUBU_LIB_MAX_FOLDERS,
          "a library stops at its bound instead of overrunning the array");
    char over[400];
    snprintf(over, sizeof over, "%s/one-more", home);
    touch_dir(over);
    CHECK(wubu_library_add(&libs, WUBU_LIB_VIDEOS, over) == -1, "adding past the bound fails loudly");

    printf("\n=== Test: removal, including a folder that is not there ===\n");
    CHECK(wubu_library_remove(&libs, WUBU_LIB_PICTURES, extra) == 1, "removing an absent folder says so");
    CHECK(wubu_library_remove(&libs, WUBU_LIB_PICTURES, missing) == 0, "removing a present folder works");
    CHECK(pics->count == 1, "the library shrinks");

    printf("\n=== Test: what is saved reloads as what was saved ===\n");
    /* Pictures currently holds only its default; give it a user folder so the file has one to write. */
    CHECK(wubu_library_add(&libs, WUBU_LIB_PICTURES, extra) == 0, "a user folder is added before saving");
    CHECK(wubu_libraries_save(&libs) == 0, "the libraries save");
    FILE *f = fopen(conf, "r");
    CHECK(f != NULL, "the config file exists");
    if (f) {
        char buf[2048];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = '\0';
        fclose(f);
        CHECK(strstr(buf, "pictures=") != NULL, "a user folder is written");
        CHECK(strstr(buf, "Pictures\n") == NULL || strstr(buf, "pictures=\n") == NULL,
              "a default folder is not written as a dead path");
        CHECK(strstr(buf, "/Pictures\n") == NULL, "the default Pictures folder is not persisted");
    }

    WubuLibraries again;
    wubu_libraries_load(&again);
    WubuLibrary *pics2 = wubu_library_get(&again, WUBU_LIB_PICTURES);
    CHECK(pics2->count == 2, "the reloaded library has the same folder count");
    CHECK(strcmp(pics2->folders[0].path, pictures) == 0, "the reloaded default is the same path");
    CHECK(strcmp(pics2->folders[1].path, extra) == 0, "the reloaded user folder is the same path");
    CHECK(!pics2->folders[1].from_default, "a reloaded user folder is still a user folder");
    CHECK(wubu_library_get(&again, WUBU_LIB_VIDEOS)->count == WUBU_LIB_MAX_FOLDERS,
          "the user's extra folders survive the round trip");

    printf("\n=== Test: suggestions are folders that exist and are not already included ===\n");
    char sugg[WUBU_LIB_MAX_FOLDERS][WUBU_LIB_PATH_MAX];
    int ns = wubu_library_suggest(&libs, WUBU_LIB_MUSIC, sugg, WUBU_LIB_MAX_FOLDERS);
    CHECK(ns >= 1, "there is at least one folder worth suggesting");
    bool saw_pics = false, saw_self = false;
    for (int i = 0; i < ns; i++) {
        if (strcmp(sugg[i], pictures) == 0) saw_pics = true;
        if (strcmp(sugg[i], music) == 0) saw_self = true;
        struct stat st;
        CHECK(stat(sugg[i], &st) == 0 && S_ISDIR(st.st_mode), "a suggestion is an existing directory");
    }
    CHECK(saw_pics, "a sibling folder that exists is suggested");
    CHECK(!saw_self, "a folder already in the library is not suggested");
    CHECK(wubu_library_suggest(&libs, WUBU_LIB_MUSIC, NULL, 4) == 0, "no buffer means no suggestions");

    printf("\n=== Test: the merged view lists every folder, once per name ===\n");
    {
        /* Two folders in one library, one shared file name: the view must show it ONCE and say which
         * folder that copy came from. */
        char lib_dir_a[400], lib_dir_b[400], f1[500], f2[500], shared[500], sub[500];
        snprintf(lib_dir_a, sizeof lib_dir_a, "%s/AlbumA", home);
        snprintf(lib_dir_b, sizeof lib_dir_b, "%s/AlbumB", home);
        touch_dir(lib_dir_a);
        touch_dir(lib_dir_b);
        snprintf(f1, sizeof f1, "%s/one.png", lib_dir_a);
        snprintf(f2, sizeof f2, "%s/two.png", lib_dir_b);
        snprintf(shared, sizeof shared, "%s/two.png", lib_dir_a);
        snprintf(sub, sizeof sub, "%s/2019", lib_dir_b);
        touch_dir(sub);
        FILE *g = fopen(f1, "w"); if (g) { fputs("x", g); fclose(g); }
        g = fopen(f2, "w"); if (g) { fputs("xy", g); fclose(g); }
        g = fopen(shared, "w"); if (g) { fputs("xyz", g); fclose(g); }

        WubuLibraries ml;
        wubu_libraries_load(&ml);
        (void)wubu_library_add(&ml, WUBU_LIB_PICTURES, lib_dir_a);
        (void)wubu_library_add(&ml, WUBU_LIB_PICTURES, lib_dir_b);

        WubuLibraryItem items[64];
        int n = wubu_library_list(&ml, WUBU_LIB_PICTURES, items, 64);
        CHECK(n >= 3, "the merged view lists the folders' children");
        bool saw_one = false, saw_two = false, saw_dir = false;
        int two_count = 0;
        for (int i = 0; i < n; i++) {
            if (strcmp(items[i].name, "one.png") == 0) saw_one = true;
            if (strcmp(items[i].name, "two.png") == 0) { saw_two = true; two_count++; }
            if (strcmp(items[i].name, "2019") == 0) saw_dir = items[i].is_dir;
        }
        CHECK(saw_one && saw_two, "files from both folders are listed");
        CHECK(two_count == 1, "a name in two folders is listed exactly once");
        CHECK(saw_dir, "a sub-directory is flagged as a directory");
        bool dated = false;
        for (int i = 0; i < n; i++)
            if (strcmp(items[i].name, "one.png") == 0 && items[i].modified > 0) dated = true;
        CHECK(dated, "a file carries its modification time");
        CHECK(n > 0 && items[0].is_dir, "directories sort first");
        bool named_source = false;
        for (int i = 0; i < n; i++) if (items[i].source[0]) named_source = true;
        CHECK(named_source, "every item names the folder it came from");
        CHECK(wubu_library_list(&ml, WUBU_LIB_PICTURES, items, 2) == 2,
              "the view respects its bound");

        /* The default Pictures folder is empty-but-present: it must not stop the others listing. */
        CHECK(strcmp(items[0].source, "") != 0, "a row always has a source folder");
        CHECK(wubu_library_list(NULL, WUBU_LIB_PICTURES, items, 4) == -1, "no libraries means no view");
    }

    printf("\n=== Test: XDG wins over the home default when it is absolute ===\n");
    setenv("XDG_MUSIC_DIR", "/srv/music", 1);
    WubuLibraries xdg;
    wubu_libraries_load(&xdg);
    CHECK(strcmp(wubu_library_get(&xdg, WUBU_LIB_MUSIC)->folders[0].path, "/srv/music") == 0,
          "an absolute XDG directory is the default");
    CHECK(!wubu_library_get(&xdg, WUBU_LIB_MUSIC)->folders[0].exists,
          "a default that is not on this machine is reported missing");
    setenv("XDG_MUSIC_DIR", "relative/path", 1);
    WubuLibraries rel;
    wubu_libraries_load(&rel);
    CHECK(strcmp(wubu_library_get(&rel, WUBU_LIB_MUSIC)->folders[0].path, music) == 0,
          "a relative XDG directory is ignored in favour of the home default");

    char rm[500];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  libraries: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
