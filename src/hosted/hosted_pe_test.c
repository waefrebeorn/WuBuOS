/*
 * hosted_pe_test.c -- gate for the DESKTOP PE launch path (hosted_pe_executor).
 *
 * The desktop executor is the path a Windows app takes when launched from
 * the hosted shell: PE bytes -> temp file -> GUI Proton manager -> bwrap +
 * wine in a resolved-or-created "default" prefix. This gate proves:
 *
 *   UNIT (always, no wine needed):
 *     - wubu_proton_get_prefix(NULL) is a safe NULL, not strcmp UB
 *     - the default prefix is resolved-or-created (real wineboot machinery)
 *     - the built wine command carries the PE path as argv[0] after "--",
 *       runs in the default prefix (WINEPREFIX), and is a bwrap sandbox
 *     - an unknown named prefix fails honestly (-1), no silent substitute
 *     - NULL/empty input to the executor fails honestly (-1)
 *
 *   FULL (wine + built demo PE present):
 *     - hosted_pe_executor(demo PE) returns 0 AND the PE writes
 *       WUBU_ERA_WIN.OK through Wine/NT -- the same host-effect marker the
 *       runtime gate (test_era_apps) asserts, but through the DESKTOP path.
 *     - the staged temp PE is cleaned up after the wait.
 *
 * If wine is absent the full leg is skipped WITH A MESSAGE and the unit
 * leg asserts the honest-failure shape instead (no UB, no fake success).
 */

#include "../egojar.h"
#include "hosted_internal.h"
#include "../gui/wubu_proton.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while (0)

static int have_wine(void) {
    return access("/usr/bin/wine", X_OK) == 0 ||
           access("/usr/bin/wine64", X_OK) == 0 ||
           access("/usr/local/bin/wine", X_OK) == 0;
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *sz = (size_t)n;
    return buf;
}

/* Find the token after "--" (the wine binary) and verify the user command
 * follows it with argv[0] == exe. */
static int cmd_shape_ok(const char **cmd, int argc, const char *exe,
                        const char *prefix_path) {
    int sep = -1, wineprefix_ok = 0, bind_ok = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(cmd[i], "--") == 0) sep = i;
        if (strcmp(cmd[i], "WINEPREFIX") == 0 && i + 1 < argc &&
            strcmp(cmd[i + 1], prefix_path) == 0) wineprefix_ok = 1;
        if (strcmp(cmd[i], "--bind") == 0 && i + 2 < argc &&
            strcmp(cmd[i + 1], prefix_path) == 0 &&
            strcmp(cmd[i + 2], prefix_path) == 0) bind_ok = 1;
    }
    if (sep < 0 || sep + 2 >= argc) return 0;
    if (strcmp(cmd[0], "bwrap") != 0) return 0;
    /* cmd[sep+1] is wine/wine64; cmd[sep+2] must be the program path. */
    if (strcmp(cmd[sep + 2], exe) != 0) return 0;
    return wineprefix_ok && bind_ok;
}

int main(void) {
    printf("=== hosted_pe_test (desktop PE executor path) ===\n");
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
    setenv("WUBU_REPO_ROOT", cwd, 1);

    int wine = have_wine();
    printf("  host wine: %s\n", wine ? "present" : "ABSENT (unit leg only)");

    /* --- NULL-prefix safety (the original UB) --- */
    CHECK(wubu_proton_get_prefix(NULL) == NULL,
          "get_prefix(NULL) returns NULL safely (no strcmp UB)");

    /* --- argv construction + default-prefix resolution through the
     *     real builder (creates the prefix via wineboot -u machinery) --- */
    const char *cmd[128];
    const char *user_argv[] = { "/tmp/wubu-pe-probe.exe", NULL };
    int argc = wubu_proton_build_wine_cmd(NULL, user_argv, cmd, 128);
    const ProtonPrefix *def = wubu_proton_get_prefix("default");
    CHECK(def != NULL && def->path[0] != '\0',
          "default prefix resolved-or-created by the launch path");
    if (def) {
        CHECK(file_exists(def->path), "default prefix directory exists on disk");
    }
    if (wine) {
        CHECK(argc > 0, "wine command builds when wine is present");
        CHECK(def && cmd_shape_ok(cmd, argc, user_argv[0], def->path),
              "command is bwrap sandbox, WINEPREFIX=default, PE path is argv[0] after --");
        /* Extra arguments must follow argv[0], not replace it. */
        const char *user_argv2[] = { "/tmp/wubu-pe-probe.exe", "arg1", NULL };
        int argc2 = wubu_proton_build_wine_cmd("default", user_argv2, cmd, 128);
        int found_arg = 0;
        for (int i = 0; i < argc2; i++)
            if (strcmp(cmd[i], "arg1") == 0) found_arg = 1;
        CHECK(argc2 == argc + 1 && found_arg,
              "extra cmdline argument appended after the PE path");
    } else {
        CHECK(argc == -1, "no wine on host -> builder fails honestly (-1)");
    }

    /* --- unknown named prefix: honest failure, no silent default --- */
    int argc_bad = wubu_proton_build_wine_cmd("no-such-prefix-xyz", user_argv, cmd, 128);
    CHECK(argc_bad == -1, "unknown named prefix fails honestly (-1)");

    /* --- executor input validation --- */
    CHECK(hosted_pe_executor(NULL, 0, NULL) == -1,
          "executor rejects NULL/empty payload (-1)");

    /* --- FULL leg: the demo PE runs through the DESKTOP executor --- */
    size_t sz = 0;
    uint8_t *pe = load_file("demos/era/halo_pc/halo.exe", &sz);
    if (pe && sz > 64 && pe[0] == 'M' && pe[1] == 'Z' && wine) {
        unlink("WUBU_ERA_WIN.OK");
        printf("  launching demo PE (%zu bytes) through hosted_pe_executor...\n", sz);
        int rc = hosted_pe_executor(pe, sz, NULL);
        CHECK(rc == 0, "hosted_pe_executor(demo PE) returns 0 (PE ran under wine)");
        CHECK(file_exists("WUBU_ERA_WIN.OK"),
              "demo PE wrote WUBU_ERA_WIN.OK through the DESKTOP path (Wine/NT)");
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "/tmp/wubu-pe-%d.bin", (int)getpid());
        CHECK(!file_exists(tmp), "staged temp PE cleaned up after the wait");
    } else {
        printf("  [SKIP] full PE leg: %s\n",
               !wine ? "wine absent on this host" :
               "demos/era/halo_pc/halo.exe not built (run test_era_apps first)");
        if (pe && sz > 0 && wine) {
            /* A real but invalid PE must fail honestly, never fake success. */
            static const uint8_t junk[] = { 'M', 'Z', 0, 0, 1, 2, 3, 4 };
            int rc = hosted_pe_executor(junk, sizeof(junk), NULL);
            CHECK(rc != 0, "invalid PE through executor fails honestly (rc != 0)");
        }
    }
    free(pe);

    printf("\n%s hosted-pe desktop executor tests\n",
           g_fail == 0 ? "ALL PASSED" : "SOME FAILED");
    return g_fail == 0 ? 0 : 1;
}