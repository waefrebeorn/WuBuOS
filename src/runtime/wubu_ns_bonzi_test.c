/* src/runtime/wubu_ns_bonzi_test.c
 *
 * The /n/bonzi subtree test. Asserts the companion's live state is mirrored
 * into the /n namespace over a REAL ns bridge root (g_ns_root set directly;
 * links wubu_ns_fs.o + wubu_ns_bonzi.c, NOT the wubu_bonzi renderer):
 *   1. sync creates /n/bonzi with the mirror's values
 *   2. set_pos/set_size/enabled/agi/bubble update the files
 *   3. the mirror read-back matches (single source of truth)
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_test.h"
#include "wubu_ns_bonzi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/ns_bonzi_test"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == ' ')) out[--n] = '\0';
    return 1;
}

static void rm_rf(const char *path)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

int main(void)
{
    printf("=== wubu_ns_bonzi_test (the /n/bonzi companion subtree) ===\n");
    rm_rf(NSROOT);
    mkdir(NSROOT, 0755);
    g_ns_root = NSROOT;

    /* 1. sync before the root is set -> -1 (no crash, no tree) */
    g_ns_root = NULL;
    CHECK(wubu_ns_bonzi_sync() == -1, "sync without ns root returns -1");
    g_ns_root = NSROOT;

    /* 2. push live state, then sync -> files appear */
    wubu_ns_bonzi_set_enabled(1);
    wubu_ns_bonzi_set_pos(120, 340);
    wubu_ns_bonzi_set_size(96, 128);
    wubu_ns_bonzi_set_agi(0);
    wubu_ns_bonzi_set_bubble("Hello, I am WuBu.");
    CHECK(wubu_ns_bonzi_sync() == 0, "sync /n/bonzi");

    char buf[256];
    CHECK(read_file(NSROOT "/bonzi/enabled", buf, sizeof buf) &&
          strcmp(buf, "1") == 0, "/n/bonzi/enabled == 1");
    CHECK(read_file(NSROOT "/bonzi/pos", buf, sizeof buf) &&
          strcmp(buf, "120 340") == 0, "/n/bonzi/pos == 120 340");
    CHECK(read_file(NSROOT "/bonzi/size", buf, sizeof buf) &&
          strcmp(buf, "96 128") == 0, "/n/bonzi/size == 96 128");
    CHECK(read_file(NSROOT "/bonzi/agi", buf, sizeof buf) &&
          strcmp(buf, "0") == 0, "/n/bonzi/agi == 0");
    CHECK(read_file(NSROOT "/bonzi/bubble", buf, sizeof buf) &&
          strcmp(buf, "Hello, I am WuBu.") == 0, "/n/bonzi/bubble text");

    /* 3. state changes flow through on the next sync */
    wubu_ns_bonzi_set_pos(200, 400);
    wubu_ns_bonzi_set_agi(1);
    wubu_ns_bonzi_set_bubble("I can see your screen now.");
    CHECK(wubu_ns_bonzi_sync() == 0, "re-sync /n/bonzi");
    CHECK(read_file(NSROOT "/bonzi/pos", buf, sizeof buf) &&
          strcmp(buf, "200 400") == 0, "/n/bonzi/pos updated -> 200 400");
    CHECK(read_file(NSROOT "/bonzi/agi", buf, sizeof buf) &&
          strcmp(buf, "1") == 0, "/n/bonzi/agi updated -> 1 (panel open)");
    CHECK(read_file(NSROOT "/bonzi/bubble", buf, sizeof buf) &&
          strstr(buf, "see your screen") != NULL, "/n/bonzi/bubble updated");

    /* 4. mirror read-back is the single source of truth */
    CHECK(wubu_ns_bonzi_agi_open() == 1, "mirror agi_open == 1");

    rm_rf(NSROOT);
    printf("=== wubu_ns_bonzi_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
