/* src/gui/wubu_bonzi_greet_test.c
 *
 * The companion summon-greeting test (Track B3). Verifies:
 *   1. empty memory -> first-meeting greeting
 *   2. non-empty memory -> "welcome back" naming recent activity
 *   3. singular/plural handling
 */
#include "wubu_bonzi_greet.h"
#include "wubu_test.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_bonzi_greet_test (companion summon greeting) ===\n");
    char buf[512];

    /* 1. empty memory -> first meeting */
    int n = wubu_bonzi_greet_format(buf, sizeof buf, 0, NULL, 0);
    CHECK(n > 0, "empty greeting returns bytes");
    CHECK(strstr(buf, "don't have any memories") != NULL,
          "empty memory -> first-meeting greeting");

    /* 2. one memory -> singular "welcome back" */
    const char *r1[] = { "editing notes.txt" };
    n = wubu_bonzi_greet_format(buf, sizeof buf, 1, r1, 1);
    CHECK(strstr(buf, "1 thing") != NULL, "singular 'thing' for 1 memory");
    CHECK(strstr(buf, "Welcome back") != NULL, "non-empty -> welcome back");
    CHECK(strstr(buf, "editing notes.txt") != NULL, "names the recent activity");

    /* 3. many memories + two recents -> plural + both named */
    const char *r3[] = { "exploring the kernel", "training the model" };
    n = wubu_bonzi_greet_format(buf, sizeof buf, 42, r3, 2);
    CHECK(strstr(buf, "42 things") != NULL, "plural 'things' for 42 memories");
    CHECK(strstr(buf, "exploring the kernel") != NULL, "names most-recent activity");
    CHECK(strstr(buf, "training the model") != NULL, "names the prior activity too");

    printf("=== wubu_bonzi_greet_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
