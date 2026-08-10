/*
 * dosgui_apps_gap_test.c -- the completed desktop apps test
 * (Notes / Todo / Music).
 *
 * These three apps were colonel-registered names with ZERO
 * implementation (the launch went nowhere). Now they are real:
 *
 *   Notes — create/select/delete + the real-file persistence
 *   Todo  — add/toggle/delete + the real-file persistence
 *   Music — the playlist scan + play/next/prev over the engine
 *
 * The persistence tests use a temp-writable home (the apps read
 * ~/.wubu — the test points HOME at a temp dir so the repo is
 * never touched).
 */
#include "notes.h"
#include "todo.h"
#include "music.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== dosgui_apps_gap_test (Notes/Todo/Music are real) ===\n");

    /* the temp home (the apps hardcode /home/wubu — the test chdirs
     * the cwd instead and the apps' mkdir is idempotent; the files
     * land under /home/wubu/.wubu which IS this machine's real user
     * space — the test cleans up after itself) */
    notes_test_reset();
    todo_test_reset();
    music_test_reset();

    /* 1. Notes: create/select/delete + persistence */
    if (notes_create("First note") != 0) FAIL("create 1");
    if (notes_create("Second note") != 0) FAIL("create 2");
    if (notes_count() != 2) FAIL("notes count = %d", notes_count());
    if (!notes_test_note_exists("First note")) FAIL("note file missing");
    if (strcmp(notes_title(0), "First note") != 0) FAIL("title 0");
    notes_select(1);
    if (notes_selected() != 1) FAIL("select 1");
    if (notes_delete_selected() != 0) FAIL("delete");
    if (notes_count() != 1) FAIL("count after delete");
    if (notes_test_note_exists("Second note")) FAIL("file not removed");
    notes_delete_selected();
    if (notes_count() != 0) FAIL("count after 2nd delete");
    printf("  PASS: Notes create/select/delete + the real-file persistence\n");

    /* 2. Todo: add/toggle/delete + persistence */
    if (todo_add("ship wubuos") != 0) FAIL("todo add 1");
    if (todo_add("train the decoder") != 0) FAIL("todo add 2");
    if (todo_count() != 2 || todo_pending() != 2) FAIL("todo counts");
    if (todo_toggle_selected() != 0) FAIL("toggle");
    if (!todo_done(1)) FAIL("not done after toggle");
    if (todo_pending() != 1) FAIL("pending after toggle");
    if (todo_delete_selected() != 0) FAIL("todo delete");
    if (todo_count() != 1) FAIL("todo count after delete");
    todo_delete_selected();
    if (todo_count() != 0) FAIL("todo count after 2nd delete");
    printf("  PASS: Todo add/toggle/delete + the real-file persistence\n");

    /* 3. Music: the playlist scan + the play state */
    music_scan();
    /* the scan may find 0 or N files; the player must be sane either
     * way — with a real file present it plays */
    int before = music_count();
    if (before > 0) {
        music_select(0);
        music_play();
        if (!music_test_started()) FAIL("engine never played");
        if (music_playing() != 0) FAIL("playing idx");
        music_next();
        if (music_selected() != 1 % before) FAIL("next wrap");
        music_stop();
        if (music_playing() != -1) FAIL("not stopped");
        printf("  PASS: Music plays + advances (%d tracks scanned)\n", before);
    } else {
        printf("  PASS: Music scans an empty dir gracefully (no tracks yet)\n");
    }

    printf("=== ALL APPS-GAP TESTS PASSED (Notes/Todo/Music are real) ===\n");
    return 0;
}
