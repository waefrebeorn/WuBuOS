/*
 * taskbar_tasks_test.c -- the taskbar model, headless.
 *
 * A user's taskbar is the one piece of desktop state they notice losing, so the
 * pin model and its persistence are gated: pin, unpin, running state, and a
 * save/load round trip through a real file.
 */
#include "../../egojar.h"
#include "../../egojar_net.h"
#include "taskbar_tasks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* Isolate the test from the developer's real config. */
static void use_temp_home(void) {
    setenv("HOME", "/tmp/wubu_taskbar_test_home", 1);
    mkdir("/tmp/wubu_taskbar_test_home", 0755);
}

static void clear_state(void) {
    /* Remove the config so each scenario starts clean. */
    const char *p = wubu_taskbar_config_path();
    remove(p);
    wubu_taskbar_load();
}

static void test_pin_unpin(void) {
    clear_state();
    CHECK(wubu_taskbar_count() == 0, "starts empty");

    CHECK(wubu_taskbar_pin("editor", "Editor", "wubu-app Editor") == 0, "pin accepted");
    CHECK(wubu_taskbar_is_pinned("editor"), "is pinned");
    CHECK(wubu_taskbar_count() == 1, "one task");
    CHECK(wubu_taskbar_pin("editor", "Editor", "wubu-app Editor") == 0,
          "re-pinning is idempotent");
    CHECK(wubu_taskbar_count() == 1, "still one task after re-pin");

    const WubuTask *t = wubu_taskbar_find("editor");
    CHECK(t && strcmp(t->title, "Editor") == 0, "title stored");
    CHECK(t && strcmp(t->exec, "wubu-app Editor") == 0, "exec stored");

    CHECK(wubu_taskbar_unpin("editor") == 0, "unpin accepted");
    CHECK(!wubu_taskbar_is_pinned("editor"), "no longer pinned");
    CHECK(wubu_taskbar_count() == 0, "a task with no window and no pin is dropped");
    CHECK(wubu_taskbar_unpin("editor") == -1, "unpinning a non-pin fails cleanly");
}

static void test_running_state(void) {
    clear_state();
    wubu_taskbar_set_running("editor", 42, "Untitled - Editor");
    CHECK(wubu_taskbar_is_running(42), "window is running");
    CHECK(wubu_taskbar_running_count() == 1, "one running task");

    /* A running window keeps its task even with no pin. */
    wubu_taskbar_clear_running(42);
    CHECK(!wubu_taskbar_is_running(42), "window is gone");
    CHECK(wubu_taskbar_count() == 0, "an unpinned task with no window is dropped");

    /* A PINNED task survives its window closing -- that is the point. */
    wubu_taskbar_pin("terminal", "Terminal", "wubu-app Terminal");
    wubu_taskbar_set_running("terminal", 7, "Terminal");
    wubu_taskbar_clear_running(7);
    CHECK(wubu_taskbar_is_pinned("terminal"), "pin survives the window closing");
    CHECK(wubu_taskbar_count() == 1, "pinned task remains after close");
}

static void test_persistence(void) {
    clear_state();
    wubu_taskbar_pin("editor", "Editor", "wubu-app Editor");
    wubu_taskbar_pin("terminal", "Terminal", "wubu-app Terminal");
    wubu_taskbar_save();

    const char *path = wubu_taskbar_config_path();
    CHECK(access(path, 0) == 0, "config file written");

    /* A fresh load must reproduce the pins exactly. */
    wubu_taskbar_load();
    CHECK(wubu_taskbar_count() == 2, "both pins reloaded");
    CHECK(wubu_taskbar_is_pinned("editor"), "editor pin reloaded");
    CHECK(wubu_taskbar_is_pinned("terminal"), "terminal pin reloaded");
    const WubuTask *t = wubu_taskbar_find("terminal");
    CHECK(t && strcmp(t->exec, "wubu-app Terminal") == 0, "exec reloaded");

    /* Unpinning and saving must persist the removal. */
    wubu_taskbar_unpin("editor");
    wubu_taskbar_save();
    wubu_taskbar_load();
    CHECK(!wubu_taskbar_is_pinned("editor"), "removal persisted");
    CHECK(wubu_taskbar_is_pinned("terminal"), "unaffected pin survived");
}

static void test_corrupt_and_missing(void) {
    clear_state();
    FILE *f = fopen(wubu_taskbar_config_path(), "w");
    if (f) {
        fprintf(f, "# a comment\n");
        fprintf(f, "pin a|A|A\n");
        fprintf(f, "garbage line without the pin prefix\n");
        fprintf(f, "pin no-separator\n");
        fprintf(f, "pin b|B|B|extra|pipes\n");
        fclose(f);
    }
    wubu_taskbar_load();
    CHECK(wubu_taskbar_is_pinned("a"), "valid line loaded");
    CHECK(!wubu_taskbar_is_pinned("no-separator"), "malformed line ignored");
    /* extra pipes: title is "B", exec becomes "B|extra|pipes" -- the split is
     * on the FIRST two separators, so the rest stays in exec. */
    CHECK(wubu_taskbar_find("b") != NULL, "line with extra separators loaded");
    CHECK(!wubu_taskbar_is_pinned("garbage"), "unknown line ignored");
}

static void test_bounded(void) {
    clear_state();
    for (int i = 0; i < WUBU_TASKBAR_MAX + 5; i++) {
        char key[32];
        snprintf(key, sizeof key, "app%d", i);
        wubu_taskbar_pin(key, key, "");
    }
    CHECK(wubu_taskbar_count() == WUBU_TASKBAR_MAX,
          "the taskbar is bounded");
}

int main(void) {
    use_temp_home();
    printf("=== taskbar tasks ===\n");
    test_pin_unpin();
    test_running_state();
    test_persistence();
    test_corrupt_and_missing();
    test_bounded();
    printf("=== taskbar_tasks: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
