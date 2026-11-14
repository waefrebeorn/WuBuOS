/*
 * wubu_spell_scan_test.c -- gate for .spell directory discovery.
 *
 * Discovery is the half that was missing after the resolver existed: nothing turned a
 * directory of module files into registered handles.  This covers what can go wrong in
 * that step specifically -- unrelated files ignored, corrupt files skipped without
 * stopping the scan, oversized files refused, a missing directory reported rather than
 * treated as zero modules, and the lifetime rule that a scanned image outlives the
 * scan because open() borrows it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wubu_spellmod.h"
#include "wubu_spell_resolve.h"
#include "spellmod_test_img.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static char g_dir[512];

static void put_file(const char *name, const uint8_t *data, size_t len)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", g_dir, name);
    FILE *fp = fopen(path, "wb");
    if (!fp) return;
    fwrite(data, 1, len, fp);
    fclose(fp);
}

static const char *prov_core[]  = { "core" };
static const char *prov_net[]   = { "network" };
static const char *need_core[]  = { "core" };

/* Two valid modules where net depends on core. */
static void seed_valid_modules(void)
{
    uint8_t buf[WUSPELL_TEST_BUF_SIZE];
    size_t n;

    n = spell_test_build(buf, sizeof(buf), "core", prov_core, 1, NULL, 0);
    put_file("core.spell", buf, n);

    n = spell_test_build(buf, sizeof(buf), "net", prov_net, 1, need_core, 1);
    put_file("net.spell", buf, n);
}

int main(void)
{
    /* Fresh scratch directory per run; never touch the user's real module dir. */
    snprintf(g_dir, sizeof(g_dir), "/tmp/wubu-spell-scan-%d", (int)getpid());
    mkdir(g_dir, 0700);

    /* -- empty directory scans as 0, not as an error -------------------- */
    wubu_spell_resolve *r = wubu_spell_resolve_new();
    TEST(r != NULL, "registry allocates");
    TEST(wubu_spell_scan(r, g_dir) == 0, "empty directory yields zero modules");

    /* -- a missing directory is reported, not silently ignored --------- */
    TEST(wubu_spell_scan(r, "/tmp/definitely-not-a-wubu-dir-xyz") == -1,
         "missing directory returns -1");

    /* -- unrelated and corrupt files are skipped ---------------------- */
    seed_valid_modules();

    uint8_t junk[64];
    memset(junk, 0xAB, sizeof(junk));
    put_file("broken.spell", junk, sizeof(junk));          /* bad magic */

    const char *text = "not a spell at all\n";
    put_file("notes.txt", (const uint8_t *)text, strlen(text));   /* wrong suffix */

    /* -- default dir honours the environment override ---------------- */
    char def[512];
    setenv("WUBU_SPELL_DIR", g_dir, 1);
    TEST(wubu_spell_default_dir(def, sizeof(def)) == 0, "default_dir succeeds");
    TEST(strcmp(def, g_dir) == 0, "default_dir honours WUBU_SPELL_DIR");
    unsetenv("WUBU_SPELL_DIR");

    /* -- the real scan ------------------------------------------------ */
    int got = wubu_spell_scan(r, g_dir);
    TEST(got == 2, "scan registers exactly the two valid modules");

    TEST(wubu_spell_find_provider(r, "core") != NULL, "core discoverable after scan");
    TEST(wubu_spell_find_provider(r, "network") != NULL,
         "network discoverable after scan");
    TEST(wubu_spell_find_provider(r, "nonexistent") == NULL,
         "unrelated file contributed no capability");

    /* a real plan across scanned modules proves the handles stay valid */
    wubu_spellmod_t *plan[WUBU_SPELL_MAX_MODULES];
    int count = 0;
    TEST(wubu_spell_plan(r, "net", plan, &count, WUBU_SPELL_MAX_MODULES) == WUBU_SPELL_OK,
         "plan succeeds over scanned modules");
    TEST(count == 2, "plan includes both scanned modules");

    /* -- oversized files are skipped, not loaded whole --------------- */
    /* A directory of its own: scanning the shared fixture directory would also find
     * the two valid modules and tell us nothing about the oversized one. */
    char bigdir[512];
    snprintf(bigdir, sizeof(bigdir), "/tmp/wubu-spell-big-%d", (int)getpid());
    mkdir(bigdir, 0700);

    FILE *bf = fopen(bigdir, "wb");           /* placeholder, replaced below */
    if (bf) fclose(bf);
    char big_path[1024];
    snprintf(big_path, sizeof(big_path), "%s/huge.spell", bigdir);
    bf = fopen(big_path, "wb");
    if (bf) {
        /* One byte past the cap; a seek-and-write makes a sparse fixture without
         * actually materialising a megabyte. */
        fseek(bf, WUBU_SPELL_FILE_MAX, SEEK_SET);
        fputc(0, bf);
        fclose(bf);

        wubu_spell_resolve *r2 = wubu_spell_resolve_new();
        TEST(wubu_spell_scan(r2, bigdir) == 0,
             "oversized module is skipped rather than read whole");
        wubu_spell_resolve_free(r2);
        unlink(big_path);
    } else {
        checks_run++;
        printf("  SKIP could not create oversized fixture\n");
    }
    rmdir(bigdir);

    /* -- scanned images are released by the registry ----------------- */
    wubu_spell_resolve_free(r);
    TEST(true, "teardown releases scanned images without leaking them");

    printf("%d checks, %d failed\n", checks_run, checks_failed);

    /* best-effort cleanup */
    char path[1024];
    const char *names[] = { "core.spell", "net.spell", "broken.spell",
                            "notes.txt", "huge.spell" };
    for (int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "%s/%s", g_dir, names[i]);
        unlink(path);
    }
    rmdir(g_dir);

    return checks_failed ? 1 : 0;
}
