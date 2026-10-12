/* src/gui/wubu_meta_e2e_test.c
 *
 * END-TO-END meta-layer proof: the whole 9P companion chain runs on the
 * live render loop and lands on disk, readable with ordinary file verbs.
 *
 *   1. init the WM, mount a namespace root (wubu_ns_bridge_create)
 *   2. enable the companion, render a frame -> the real observer pushes
 *      live state -> wubu_ns_bonzi_sync writes /n/bonzi/* to disk
 *   3. read /n/bonzi/enabled back from disk and assert it reflects the
 *      live companion (the OS IS introspectable, not just unit-tested)
 *
 * Links the REAL observer + ns modules (not the weak no-ops), so this
 * proves the shipped path.
 */
#include "dosgui_wm.h"
#include "wubu_bonzi.h"
#include "wubu_ns_bridge.h"
#include "wubu_ns_resolve.h"
#include "wubu_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/wubu_meta_e2e"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

static int read_file(const char *p, char *out, size_t cap) {
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == ' ')) out[--n] = '\0';
    return 1;
}

int main(void)
{
    printf("=== wubu_meta_e2e_test (the meta-layer, live end-to-end) ===\n");
    system("rm -rf " NSROOT " && mkdir -p " NSROOT);

    /* 1. init WM + mount the namespace */
    dosgui_wm_init(1024, 768);
    CHECK(wubu_ns_bridge_create(NSROOT) == 0, "namespace root mounted");

    /* 2. enable the companion + render -> observer pushes to /n/bonzi */
    wubu_bonzi_init(900, 600);
    wubu_bonzi_set_enabled(true);
    dosgui_wm_render(NULL, 1024, 768);

    /* 3. read /n/bonzi/enabled back from disk via the resolver */
    char resolved[512];
    CHECK(wubu_ns_resolve("/n/bonzi/enabled", resolved, sizeof resolved) == 0,
          "/n/bonzi/enabled resolves to the live tree");
    char buf[64];
    CHECK(read_file(resolved, buf, sizeof buf) && strcmp(buf, "1") == 0,
          "/n/bonzi/enabled == 1 (the live companion is introspectable)");

    /* and the position file reflects the live bonzi position */
    CHECK(wubu_ns_resolve("/n/bonzi/pos", resolved, sizeof resolved) == 0, "pos resolves");
    CHECK(read_file(resolved, buf, sizeof buf) && strstr(buf, "900 600") != NULL,
          "/n/bonzi/pos == 900 600 (live position on disk)");

    system("rm -rf " NSROOT);
    printf("=== wubu_meta_e2e_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
