/*
 * startmenu_files_test.c -- Start search must find FILES, not just apps.
 *
 * A user who just saved a document looks for it in the same place they look for
 * everything else.  Searching only the app registry meant that after saving
 * "notes.txt" the fastest way to open it was to remember where you put it.
 *
 * This drives the real guest filesystem and the real indexer: create files,
 * index them, then require them to appear in results for a matching query --
 * and require deleted files to disappear on the next index rather than
 * lingering as ghosts.
 */

#include "shell/startmenu_internal.h"
#include "shell/startmenu.h"
#include "../apps/guest_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void startmenu_index_files(void);

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

/* True when a search result row is the given file entry. */
static bool results_contain(const char *name)
{
    SmProgramEntry **res = NULL;
    int n = 0;
    if (startmenu_search_get_results(&res, &n) != 0) return false;
    for (int i = 0; i < n; i++)
        if (res[i] && strcmp(res[i]->name, name) == 0) return true;
    return false;
}

static int count_file_entries(void)
{
    int n = 0;
    for (int i = 0; i < g_program_db.count; i++)
        if (g_program_db.entries[i].opens_file) n++;
    return n;
}

int main(void)
{
    /* A test that writes into ~/.local/share/wubu/fs.img has corrupted the
     * user's real desktop for every later boot -- which is exactly what the
     * first version of this file did.  Point the store at a scratch file and
     * remove it on the way out. */
    char store[256];
    snprintf(store, sizeof(store),
             "/home/wubu/.hermes/cache/scratch/wubu-fs-test-%d.img", (int)getpid());
    setenv("WUBU_FS_STORE", store, 1);
    unlink(store);

    /* ---- 1. files become findable ---------------------------------- */
    guest_file_write("zebra-notes.txt", (const uint8_t *)"hello", 5);
    guest_file_write("alpha.txt",       (const uint8_t *)"world", 5);
    guest_file_write("run-output.txt",  (const uint8_t *)"$ x",   3);

    startmenu_index_files();
    TEST(count_file_entries() == 3, "three files indexed");

    const SmProgramEntry *f = NULL;
    for (int i = 0; i < g_program_db.count; i++)
        if (strcmp(g_program_db.entries[i].name, "zebra-notes.txt") == 0)
            f = &g_program_db.entries[i];
    TEST(f != NULL, "the file entry exists");
    TEST(f && f->opens_file, "entry is marked as opening a file");
    TEST(f && strcmp(f->category, "File") == 0, "category says File");
    TEST(f && strcmp(f->executable, "zebra-notes.txt") == 0,
         "executable carries the path to open");

    /* ---- 2. search returns them ------------------------------------ */
    startmenu_search_update("zebra");
    TEST(results_contain("zebra-notes.txt"),
         "searching 'zebra' finds the saved document");
    TEST(!results_contain("alpha.txt"), "unrelated file is not returned");

    startmenu_search_clear();

    /* ---- 3. re-indexing replaces rather than duplicates ------------ */
    startmenu_index_files();
    TEST(count_file_entries() == 3, "re-indexing does not duplicate entries");

    /* ---- 4. deleted files stop being ghosts ------------------------ */
    guest_file_delete("zebra-notes.txt");
    startmenu_index_files();
    TEST(count_file_entries() == 2, "a deleted file is no longer indexed");
    startmenu_search_update("zebra");
    TEST(!results_contain("zebra-notes.txt"),
         "a deleted file no longer appears in results");
    startmenu_search_clear();

    /* ---- 5. a file never shadows a real application ---------------- */
    /* "Notepad" is an app; indexing a file of that exact name must not
     * overwrite or duplicate it. */
    int apps_before = g_program_db.count - count_file_entries();
    guest_file_write("Notepad", (const uint8_t *)"x", 1);
    startmenu_index_files();
    TEST(g_program_db.count - count_file_entries() == apps_before,
         "a file named after an app does not disturb the app count");
    int notepad_rows = 0;
    for (int i = 0; i < g_program_db.count; i++)
        if (strcmp(g_program_db.entries[i].name, "Notepad") == 0) notepad_rows++;
    TEST(notepad_rows == 1, "exactly one Notepad row (the app)");

    guest_file_flush();
    unlink(store);

    printf("=== startmenu_files: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
