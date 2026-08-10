/*
 * wubu_ns_steamrt_test.c -- the /n/steamrt subtree test.
 *
 * Asserts the file<->API routing:
 *   1. publish creates the files
 *   2. echo "appid game_lib proton_dist" > /n/steamrt/env writes the
 *      built Proton launch env back
 *   3. echo "lib1,lib2,..." > /n/steamrt/verify writes the missing count
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_steamrt.h"
#include "wubu_steamrt.h"
#include <stdio.h>
#include <string.h>

#define NSROOT "/tmp/ns_steamrt_test"
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return 1;
}

int main(void)
{
    printf("=== wubu_ns_steamrt_test (the /n/steamrt subtree) ===\n");
    system("rm -rf " NSROOT);
    *(const char **)&g_ns_root = NSROOT;

    wubu_steamrt_init("/games/compatdata", "/usr/lib/steamrt/sniper");

    if (wubu_ns_publish_steamrt() != 0) FAIL("publish");

    /* 1. the files exist */
    char p[512], buf[2048];
    snprintf(p, sizeof(p), "%s/steamrt/manifest", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no manifest");
    snprintf(p, sizeof(p), "%s/steamrt/env", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no env");
    snprintf(p, sizeof(p), "%s/steamrt/verify", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no verify");
    printf("  PASS: publish creates /n/steamrt/{manifest,env,verify}\n");

    /* 2. the env build */
    if (wubu_ns_steamrt_refresh_manifest() != 0) FAIL("refresh manifest");
    snprintf(p, sizeof(p), "%s/steamrt/manifest", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "43")) FAIL("manifest count not refreshed: '%s'", buf);
    if (wubu_ns_steamrt_build_env("1245620 /games/common/EldenRing /games/common/Proton-9") != 0)
        FAIL("build env");
    snprintf(p, sizeof(p), "%s/steamrt/env", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "STEAM_COMPAT_DATA_PATH=/games/compatdata/1245620"))
        FAIL("env lacks the compat path");
    if (!strstr(buf, "WINEPREFIX=/games/compatdata/1245620/pfx"))
        FAIL("env lacks the WINEPREFIX");
    if (!strstr(buf, "LD_LIBRARY_PATH"))
        FAIL("env lacks the LD_LIBRARY_PATH");
    printf("  PASS: echo > /n/steamrt/env builds the Proton env\n");

    /* 3. the verify */
    if (wubu_ns_steamrt_verify("libvulkan1,libdrm2") != 0) FAIL("verify ok");
    snprintf(p, sizeof(p), "%s/steamrt/verify", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "0 missing")) FAIL("verify: '%s'", buf);
    if (wubu_ns_steamrt_verify("libvulkan1,libnope9") != 0) FAIL("verify gap");
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "1 missing")) FAIL("verify gap: '%s'", buf);
    printf("  PASS: echo > /n/steamrt/verify reports the gaps\n");

    system("rm -rf " NSROOT);
    printf("=== ALL NS-STEAMRT TESTS PASSED ===\n");
    return 0;
}
