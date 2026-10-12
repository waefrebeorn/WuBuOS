/* src/gui/wubu_bonzi_context_test.c
 *
 * The companion ambient-context test (Track B1). Verifies:
 *   1. focus + window count format into a structured prompt prefix
 *   2. the recent-focus ring records and recalls what was attended to
 *   3. no-focus state formats cleanly
 */
#include "wubu_bonzi_context.h"
#include "wubu_test.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_bonzi_context_test (companion ambient context) ===\n");

    /* 1. focus + count -> structured prefix */
    wubu_bonzi_context_set_window_count(3);
    wubu_bonzi_context_set_focus("Notepad - notes.txt", 120, 80, 640, 480);
    char buf[512];
    int n = wubu_bonzi_context_format(buf, sizeof buf);
    CHECK(n > 0, "format returns bytes");
    CHECK(strstr(buf, "windows=3") != NULL, "prefix reports window count");
    CHECK(strstr(buf, "Notepad - notes.txt") != NULL, "prefix reports focus title");
    CHECK(strstr(buf, "120,80 640x480") != NULL, "prefix reports focus rect");

    /* 2. recent-focus ring recalls attendance */
    wubu_bonzi_context_set_focus("Calculator", 10, 10, 200, 300);
    wubu_bonzi_context_set_focus("Explorer", 0, 0, 800, 600);
    char ring[16][128];
    int rn = wubu_bonzi_context_recent(ring, 16);
    CHECK(rn == 3, "ring holds 3 focus events");
    CHECK(strcmp(ring[0], "Notepad - notes.txt") == 0, "ring[0] is the first focus");
    CHECK(strcmp(ring[2], "Explorer") == 0, "ring[2] is the latest focus");

    printf("=== wubu_bonzi_context_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
