/* test_colonel.c -- the everything-through-the-Colonel dispatcher. */
#include <stdio.h>
#include <string.h>
#include "wubu_colonel.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* the test eval: a REAL HolyC-style arithmetic evaluator substitute --
 * the actual hc_eval is linked by the hosted binary; here the routing
 * + the parse are verified with a deterministic fake. */
static int64_t fake_eval(const char *src)
{
    /* 1+2+3 -> 6, else 42 */
    if (src && strcmp(src, "1+2+3") == 0) return 6;
    return 42;
}

int main(void)
{
    printf("=== test_colonel ===\n");
    wubu_colonel_t c;

    /* the parse classes */
    CHECK(wubu_colonel_parse("run calc", &c) == WUBU_COLONEL_OK, "run parses");
    CHECK(c.class == WUBU_COL_CMD_APP && strcmp(c.cmd, "calc") == 0, "run class+name");
    CHECK(wubu_colonel_parse("eval 1+2+3", &c) == WUBU_COLONEL_OK, "eval parses");
    CHECK(c.class == WUBU_COL_CMD_EVAL && strcmp(c.arg, "1+2+3") == 0, "eval arg");
    CHECK(wubu_colonel_parse("os shutdown", &c) == WUBU_COLONEL_OK &&
          c.class == WUBU_COL_CMD_OS, "os class");
    CHECK(wubu_colonel_parse("sys reboot", &c) == WUBU_COLONEL_OK &&
          c.class == WUBU_COL_CMD_SYS, "sys class");
    CHECK(wubu_colonel_parse("agi close-gap", &c) == WUBU_COLONEL_OK &&
          c.class == WUBU_COL_CMD_AGI, "agi class");
    CHECK(wubu_colonel_parse("load wasm /tmp/app.wasm", &c) == WUBU_COLONEL_OK &&
          c.class == WUBU_COL_CMD_LOAD && strcmp(c.cmd, "wasm") == 0, "load class");
    CHECK(wubu_colonel_parse("calc", &c) == WUBU_COLONEL_OK &&
          c.class == WUBU_COL_CMD_APP, "bare token -> app");
    CHECK(wubu_colonel_parse("   ", &c) == WUBU_COLONEL_EMPTY, "empty");
    CHECK(wubu_colonel_parse(NULL, &c) == WUBU_COLONEL_BAD, "null");

    /* the dispatch: eval routes through the engine */
    CHECK(wubu_colonel_dispatch("eval 1+2+3", &c, fake_eval) == WUBU_COLONEL_OK,
          "eval dispatched");
    CHECK(c.value == 6, "eval result routed");
    CHECK(wubu_colonel_dispatch("eval 9*9", &c, fake_eval) == WUBU_COLONEL_OK &&
          c.value == 42, "fallback value");

    /* the app registry (the GUI-launch validation) */
    CHECK(wubu_colonel_app_known("calc") == 1, "calc known");
    CHECK(wubu_colonel_app_known("bonzi") == 1, "bonzi known");
    CHECK(wubu_colonel_app_known("comfy") == 1, "comfy known");
    CHECK(wubu_colonel_app_known("not-a-real-app") == 0, "unknown rejected");
    CHECK(wubu_colonel_dispatch("run not-a-real-app", &c, NULL) == WUBU_COLONEL_UNKNOWN,
          "unknown app -> UNKNOWN");
    CHECK(wubu_colonel_dispatch("run calc", &c, NULL) == WUBU_COLONEL_OK,
          "known app -> OK (GUI launches)");

    /* the verb classes route */
    CHECK(wubu_colonel_dispatch("os shutdown", &c, NULL) == WUBU_COLONEL_OK,
          "os verb routes");
    CHECK(wubu_colonel_dispatch("load wasm /tmp/app.wasm", &c, NULL) == WUBU_COLONEL_OK,
          "load routes");
    CHECK(wubu_colonel_dispatch("os ", &c, NULL) == WUBU_COLONEL_UNKNOWN,
          "empty os verb -> UNKNOWN");

    if (failures == 0) printf("ALL COLONEL TESTS PASSED\n");
    else printf("%d COLONEL FAILURES\n", failures);
    return failures ? 1 : 0;
}
