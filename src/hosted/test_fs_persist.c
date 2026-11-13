/*
 * test_fs_persist.c -- durability and failure behaviour of the guest FS image.
 *
 * The point of this test is not "does save work" but "does load REFUSE
 * something it cannot honour".  A half-loaded namespace is worse than no load:
 * the desktop shows files whose contents do not exist.  So most checks here are
 * negative -- corrupt the image, then assert the live namespace is untouched.
 *
 * Run with a private WUBU_FS_STORE so the real user image is never touched.
 */

#include "hosted.h"
#include "hosted_internal.h"
#include "hosted_fs_persist.h"
#include "../runtime/styx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_checks = 0, g_fails = 0;

#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

static const char *kStorePath = "/tmp/wubu_fs_persist_test.img";

static void fresh(void)
{
    unlink(kStorePath);
    unlink("/tmp/wubu_fs_persist_test.img.tmp");
    fs_reset();
}

/* Byte-level helper: overwrite a range of the image. */
static void poke(size_t off, uint8_t v)
{
    FILE *f = fopen(kStorePath, "r+b");
    if (!f) return;
    fseek(f, (long)off, SEEK_SET);
    fputc(v, f);
    fclose(f);
}

static void truncate_to(size_t n)
{
    if (truncate(kStorePath, (off_t)n) != 0) { /* best effort */ }
}

static size_t file_size(void)
{
    struct stat st;
    if (stat(kStorePath, &st) != 0) return (size_t)-1;
    return (size_t)st.st_size;
}

int main(void)
{
    /* Must happen before the first fs_persist_path() call: it caches. */
    setenv("WUBU_FS_STORE", kStorePath, 1);
    TEST(strcmp(fs_persist_path(), kStorePath) == 0, "store honours WUBU_FS_STORE");

    /* ---- 1. first boot: no image is not an error --------------------- */
    fresh();
    TEST(fs_persist_load(1) == 0, "missing image loads as zero entries");
    TEST(g_nfiles == 0, "missing image leaves namespace empty");

    /* ---- 2. round trip ---------------------------------------------- */
    fresh();
    TEST(fs_add_dir("wubu") == 0, "add dir");
    TEST(fs_add_dir("dev") == 0, "add dir 2");
    TEST(fs_add_file("cons", (const uint8_t *)"hello world\n", 12) == 0,
          "add small file");
    TEST(fs_add_file("empty", NULL, 0) == 0, "add empty file");

    /* A full-capacity file, to prove nothing truncates silently. */
    static uint8_t big[8192];
    for (size_t i = 0; i < sizeof(big); i++) big[i] = (uint8_t)(i * 7 + 3);
    TEST(fs_add_file("big.bin", big, sizeof(big)) == 0, "add max-size file");
    TEST(g_nfiles == 5, "five entries before save");

    TEST(fs_persist_save() == 0, "save succeeds");
    TEST(file_size() > 0, "image exists and is non-empty");

    /* Rewrite every entry, then reload: nothing may survive from the old
     * state unless the image carried it. */
    fs_reset();
    TEST(g_nfiles == 0, "reset clears the namespace");
    TEST(fs_persist_load(1) == 5, "load restores five entries");

    TEST(strcmp(g_fs[0].name, "wubu") == 0 && (g_fs[0].qtype & STX_QTDIR),
          "dir restored with its type");
    TEST(strcmp(g_fs[2].name, "cons") == 0, "file name restored");
    TEST(g_fs[2].data_len == 12, "file length restored");
    TEST(memcmp(g_fs[2].data, "hello world\n", 12) == 0, "file bytes restored");
    TEST(g_fs[3].data_len == 0, "empty file stays empty");
    TEST(g_fs[4].data_len == sizeof(big), "max-size file not truncated");
    TEST(memcmp(g_fs[4].data, big, sizeof(big)) == 0, "max-size bytes exact");

    /* ---- 3. append mode does not clear ------------------------------ */
    fs_add_file("extra", (const uint8_t *)"x", 1);
    TEST(g_nfiles == 6, "append adds a sixth entry");
    fs_reset();
    fs_persist_load(1);
    TEST(g_nfiles == 5, "replace-mode load restores exactly the image");
    fs_reset();
    fs_persist_load(0);
    TEST(g_nfiles == 5, "non-replace load onto an empty namespace also yields five");

    /* ---- 4. corruption must refuse, and leave state untouched ------- */
    uint64_t keep_path = g_fs[0].path;

    /* bad magic */
    fresh();
    fs_add_file("sentinel", (const uint8_t *)"keep", 4);
    fs_persist_save();
    poke(0, 'X');
    fs_reset();
    fs_add_file("survivor", (const uint8_t *)"still here", 10);
    TEST(fs_persist_load(1) == -1, "bad magic refused");
    TEST(g_nfiles == 1 && strcmp(g_fs[0].name, "survivor") == 0,
          "refused load leaves the live namespace untouched");

    /* truncated header */
    fresh();
    fs_add_file("a", (const uint8_t *)"b", 1);
    fs_persist_save();
    size_t full = file_size();
    truncate_to(4);
    TEST(fs_persist_load(1) == -1, "truncated header refused");
    TEST(file_size() == 4, "truncation itself did not change");

    /* truncated tail */
    fresh();
    fs_add_file("a", (const uint8_t *)"payload", 7);
    fs_persist_save();
    truncate_to(full >= 4 ? full / 2 : 4);
    TEST(fs_persist_load(1) == -1, "truncated record refused");
    (void)full;

    /* absurd record count */
    fresh();
    fs_add_file("a", (const uint8_t *)"b", 1);
    fs_persist_save();
    {
        FILE *f = fopen(kStorePath, "r+b");
        TEST(f != NULL, "reopen image for surgery");
        if (f) {
            fseek(f, 8, SEEK_SET);
            unsigned char cnt[4] = { 0xff, 0xff, 0xff, 0x7f };
            fwrite(cnt, 1, 4, f);
            fclose(f);
        }
    }
    fs_reset();
    TEST(fs_persist_load(1) == -1, "count beyond capacity refused");

    /* ---- 5. failed save must not destroy the old image -------------- */
    fresh();
    fs_add_file("precious", (const uint8_t *)"do not lose me", 14);
    TEST(fs_persist_save() == 0, "initial save works");

    /* Point the store at a path whose parent cannot be created. */
    setenv("WUBU_FS_STORE", "/proc/definitely/not/writable/fs.img", 1);
    TEST(fs_persist_save() == -1, "unwritable store reports failure");

    setenv("WUBU_FS_STORE", kStorePath, 1);
    TEST(fs_persist_size() > 0, "previous image survived the failed save");

    /* ---- 6. writes through the Styx callback ------------------------ */
    fresh();
    fs_add_file("scratch", (const uint8_t *)"aaaa", 4);
    uint64_t p = g_fs[0].path;

    styx_server_t srv;
    memset(&srv, 0, sizeof(srv));
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    srv.open = styx_open_cb;
    srv.read = styx_read_cb;
    srv.write = styx_write_cb;
    srv.clunk = styx_clunk_cb;
    srv.remove = styx_remove_cb;
    srv.stat = styx_stat_cb;

    TEST(srv.attach(&srv, 0, "") == 0, "attach");
    const char *wn[] = { "scratch" };
    styx_qid_t qids[4]; int nwqid = 0;
    TEST(srv.walk(&srv, 0, 1, wn, 1, qids, &nwqid) == 0 && nwqid == 1,
          "walk to scratch");
    styx_qid_t oqid;
    TEST(srv.open(&srv, 1, 1, &oqid) == 0, "open scratch");

    uint32_t nw = 0;
    TEST(srv.write(&srv, 1, 4, 2, (const uint8_t *)"bb", &nw) == 0 && nw == 2,
          "append past EOF extends the file");
    TEST(g_fs[0].data_len == 6, "length grew to six");
    TEST(memcmp(g_fs[0].data, "aaaabb", 6) == 0, "bytes written correctly");

    /* Refuse anything that would not fit exactly.  Note 8192 bytes at offset 0
     * FITS (the buffer is exactly 8192), so the boundary must be probed at
     * 8193 rather than assumed. */
    nw = 0;
    TEST(srv.write(&srv, 1, 0, sizeof(big) + 1, big, &nw) == -1,
          "oversized write refused");
    TEST(g_fs[0].data_len == 6, "refused write changed nothing");

    /* Offset beyond the array is refused, not wrapped. */
    nw = 0;
    TEST(srv.write(&srv, 1, 9999, 1, (const uint8_t *)"z", &nw) == -1,
          "far offset refused");

    /* Directories are not writable. */
    TEST(srv.walk(&srv, 0, 2, NULL, 0, qids, &nwqid) == 0, "clone root fid");
    TEST(srv.open(&srv, 2, 1, &oqid) == 0, "open root");
    nw = 0;
    TEST(srv.write(&srv, 2, 0, 1, (const uint8_t *)"z", &nw) == -1,
          "directory write refused");

    /* Unknown fid. */
    nw = 0;
    TEST(srv.write(&srv, 4242, 0, 1, (const uint8_t *)"z", &nw) == -1,
          "unknown fid refused");

    /* Read still works after the writes. */
    uint8_t buf[16]; uint32_t nr = 0;
    TEST(srv.read(&srv, 1, 0, sizeof(buf), buf, &nr) == 0 && nr == 6,
          "read returns the written six bytes");

    /* Stat must now report a real length -- it used to always say zero,
     * which made every reader believe files were empty. */
    styx_dir_t dir;
    TEST(srv.stat(&srv, 1, &dir) == 0, "stat");
    TEST(dir.length == 6, "stat reports the real length");

    /* Clunk frees the fid. */
    TEST(srv.clunk(&srv, 1) == 0, "clunk");
    nw = 0;
    TEST(srv.write(&srv, 1, 0, 1, (const uint8_t *)"z", &nw) == -1,
          "write after clunk refused");

    /* ---- 7. remove ------------------------------------------------- */
    TEST(srv.attach(&srv, 10, "") == 0, "attach for remove");
    TEST(srv.walk(&srv, 10, 11, wn, 1, qids, &nwqid) == 0, "walk scratch again");
    int before = g_nfiles;
    TEST(srv.remove(&srv, 11) == 0, "remove a file");
    TEST(g_nfiles == before - 1, "namespace shrank by one");
    TEST(fs_find("scratch") < 0, "removed file is gone");

    /* Removing a directory is refused rather than recursively deleting. */
    TEST(srv.attach(&srv, 20, "") == 0, "attach for dir remove");
    TEST(srv.remove(&srv, 20) == -1, "removing root/dir refused");

    /* ---- 8. flush persists pending changes ------------------------- */
    fresh();
    fs_add_file("dirty", (const uint8_t *)"v1", 2);
    /* Simulate what a Styx write does, without a server. */
    g_fs[0].data_len = 3;
    memcpy(g_fs[0].data, "v1x", 3);
    hosted_fs_mark_dirty();
    TEST(hosted_fs_flush() == 0, "flush succeeds");
    fs_reset();
    TEST(fs_persist_load(1) == 1, "entry survived reset");
    TEST(g_fs[0].data_len == 3, "flushed bytes are durable");

    printf("=== fs_persist: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
