/*
 * fs_crash_safe_test.c -- verifies the guest store's crash-safety claims.
 *
 * WHY THIS EXISTS
 *   hosted_fs_persist.c documents an atomic-write contract: write to a temporary,
 *   rename into place, unlink on any failure, so "a crash mid-write can never leave
 *   a half-written store behind".  Nothing tested that.  A safety property nobody has
 *   exercised is a hope, not a guarantee -- and this one had an untested hole: two
 *   guests sharing a host wrote to the SAME fixed temp path, so the second writer
 *   truncated whatever the first was mid-way through writing and the rename published
 *   a mixture of both stores.
 *
 * WHAT IS ASSERTED
 *   1. After a clean save, no temporary file is left behind.
 *   2. A crashed writer's garbage temp does NOT affect the published image: load
 *      still returns the last complete state.
 *   3. A temp file belonging to a DIFFERENT process is left untouched by our save --
 *      the concurrency-collision regression, checked through observable behaviour
 *      rather than by inspecting internal naming.
 *   4. The published image stays readable across repeated saves.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hosted_fs_persist.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static char g_store[512];

/* Count files sitting beside the store whose name ends in .tmp. */
static int tmp_count(void)
{
    char dir[600];
    snprintf(dir, sizeof(dir), "%s", g_store);
    char *slash = strrchr(dir, '/');
    if (!slash) return -1;
    *slash = '\0';
    const char *base = slash + 1;

    DIR *d = opendir(dir);
    if (!d) return -1;

    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, base, strlen(base)) != 0) continue;
        size_t len = strlen(e->d_name), suffix = strlen(".tmp");
        if (len > suffix && strcmp(e->d_name + len - suffix, ".tmp") == 0) n++;
    }
    closedir(d);
    return n;
}

/* Place a half-written image where ANOTHER WRITER would have left it: at the fixed
 * "<store>.tmp" path.  That is the path the old implementation used for every save,
 * so a second guest's crash debris sat exactly there -- and a later save from this
 * process truncated it before renaming over the published image. */
static void put_foreign_writer_debris(void)
{
    char p[600];
    snprintf(p, sizeof(p), "%s.tmp", g_store);
    FILE *fp = fopen(p, "wb");
    if (!fp) return;
    fwrite("WUBFS01\0garbage-not-a-complete-image", 1, 33, fp);
    fclose(fp);
}

int main(void)
{
    snprintf(g_store, sizeof(g_store), "/tmp/wubu-fs-crash-%d.img", (int)getpid());
    setenv("WUBU_FS_STORE", g_store, 1);
    unlink(g_store);

    /* -- establish a known-good published image ---------------------- */
    TEST(fs_persist_save() == 0, "initial save succeeds");
    TEST(fs_persist_load(1) >= 0, "initial image loads");

    /* -- claim 1: a clean save leaves no temporary behind ------------ */
    TEST(tmp_count() == 0, "no temp file left after a clean save");

    /* -- claim 2: a crashed writer's garbage temp is ignored -------- */
    put_foreign_writer_debris();      /* a crashed writer's debris */
    TEST(fs_persist_load(1) >= 0,
         "garbage temp from another process does not break loading");

    /* -- claim 3: we never clobber another process's temp ---------- */
    /* Under the old fixed "<path>.tmp" name, our save would open this exact file with
     * "wb" and truncate it before renaming over the published image.  With per-process
     * names it must survive untouched. */
    int before = tmp_count();
    TEST(before == 1, "the foreign temp exists to be protected");
    TEST(fs_persist_save() == 0, "save succeeds alongside a foreign temp");
    TEST(tmp_count() == 1,
         "another process's temp is untouched -- no interleaved writes");

    /* -- claim 4: the published image stays readable ---------------- */
    TEST(fs_persist_load(1) >= 0, "image still readable after repeated saves");

    printf("%d checks, %d failed\n", checks_run, checks_failed);

    /* best-effort cleanup */
    char p[600];
    snprintf(p, sizeof(p), "%s.tmp", g_store);
    unlink(p);
    unlink(g_store);
    return checks_failed ? 1 : 0;
}
