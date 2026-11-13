/*
 * test_revolver.c -- the clamping rules must hold, not just parse.
 *
 * A revolver is a configuration primitive that sizes allocations.  Its whole
 * value is refusing to hand back a number nobody asked for, so these checks are
 * mostly NEGATIVE: garbage input, out-of-range input, and typos must leave the
 * effective value alone rather than quietly becoming a small limit.
 */

#include "../runtime/wubu_revolver.h"
#include "../hosted/hosted_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0, g_fails = 0;
static char g_last_warn[256];
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

static void warn_capture(const char *msg, const char *env_name, const char *value)
{
    (void)env_name; (void)value;
    snprintf(g_last_warn, sizeof(g_last_warn), "%s", msg ? msg : "");
}

static void set_env(const char *k, const char *v)
{
    if (v) setenv(k, v, 1); else unsetenv(k);
}

int main(void)
{
    wubu_revolver_t rev;

    /* ---- 1. defaults ------------------------------------------------ */
    wubu_revolver_init(&rev, "t_default", 64, 4096, 1);
    TEST(wubu_revolver_get(&rev) == 64, "default is adopted");
    TEST(!rev.from_env, "default is not marked as coming from the environment");

    /* ---- 2. a valid override is honoured ---------------------------- */
    set_env("WUBU_TEST_REV", "128");
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == 128, "valid override applied");
    TEST(rev.from_env, "override is marked as from the environment");

    /* ---- 3. above the ceiling: clamped, and said so ----------------- */
    g_last_warn[0] = '\0';
    set_env("WUBU_TEST_REV", "999999");
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == 4096, "above-ceiling value is clamped to the ceiling");
    TEST(strstr(g_last_warn, "ceiling") != NULL, "clamping is reported, not silent");

    /* ---- 4. garbage is refused, value unchanged ---------------------- */
    long before = wubu_revolver_get(&rev);
    set_env("WUBU_TEST_REV", "6O4");           /* letter O, not zero */
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == before,
         "a typo like '6O4' does not become a smaller limit");

    set_env("WUBU_TEST_REV", "64K");           /* unit suffix */
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == before,
         "a unit suffix does not get partially parsed");

    set_env("WUBU_TEST_REV", "-5");
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == before, "negative values are refused");

    set_env("WUBU_TEST_REV", "0");
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == before, "zero is refused (never a zero-size bound)");

    /* ---- 5. an unset variable changes nothing ------------------------ */
    set_env("WUBU_TEST_REV", NULL);
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == before, "absent variable leaves the value alone");

    /* ---- 6. scale is applied then clamped ---------------------------- */
    wubu_revolver_init(&rev, "t_scale", 64, 1024 * 1024, 1024);
    TEST(wubu_revolver_get(&rev) == 64 * 1024, "scale multiplies the default");
    set_env("WUBU_TEST_REV", "100");
    wubu_revolver_set_from_env(&rev, "WUBU_TEST_REV", warn_capture);
    TEST(wubu_revolver_get(&rev) == 100 * 1024, "scale multiplies the override");

    /* ---- 7. a NULL/empty revolver or env cannot crash ---------------- */
    wubu_revolver_set_from_env(NULL, "WUBU_TEST_REV", warn_capture);
    wubu_revolver_set_from_env(&rev, NULL, warn_capture);
    wubu_revolver_set_from_env(&rev, "", warn_capture);
    TEST(wubu_revolver_get(NULL) == 0, "NULL revolver reads as zero");
    TEST(wubu_revolver_get(&rev) == 100 * 1024, "bad arguments changed nothing");

    /* ---- 8. the guest FS slot limit honours WUBU_FS_FILES ------------ */
    TEST(styxfs_max_files() >= 1, "FS slot limit is at least 1");
    TEST(styxfs_max_files() <= STYXFS_MAX_FILES_LIMIT,
         "FS slot limit never exceeds the compile-time array extent");

    printf("=== test_revolver: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
