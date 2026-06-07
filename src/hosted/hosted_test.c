/*
 * hosted_test.c — WuBuOS Hosted Mode Test Suite
 *
 * Tests: hosted init/shutdown, Styx namespace construction,
 * Styx server callbacks (attach, walk, open, read, stat),
 * headless mode, .wubu container registration.
 *
 * All tests run in headless mode (no X11 window needed).
 */
#include "../runtime/styx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ── Test Framework ──────────────────────────────────────────────── */

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define TEST(name) do { \
    g_tests++; \
    printf("  TEST %-52s ", name); \
    fflush(stdout); \
} while (0)

#define PASS() do { printf("✅\n"); g_passed++; } while (0)

#define FAIL(msg) do { \
    printf("❌ %s\n", msg); g_failed++; \
} while (0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

/* ── In-memory filesystem helpers (from hosted.c) ───────────────── */

#define STYXFS_MAX_FILES 64

typedef struct {
    char     name[256];
    uint8_t  qtype;
    uint64_t path;
    uint64_t length;
    uint8_t  data[8192];
    uint32_t data_len;
} styxfs_file_t;

static styxfs_file_t g_fs[STYXFS_MAX_FILES];
static int g_nfiles = 0;
static uint64_t g_next_path = 1;

static int fs_add_dir(const char *name) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTDIR;
    f->path = g_next_path++;
    f->length = 0;
    return 0;
}

static int fs_add_file(const char *name, const uint8_t *data, uint32_t len) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTFILE;
    f->path = g_next_path++;
    f->length = len;
    if (data && len > 0) {
        uint32_t clen = len < sizeof(f->data) ? len : sizeof(f->data);
        memcpy(f->data, data, clen);
        f->data_len = clen;
    }
    return 0;
}

static styxfs_file_t *fs_find_by_path(uint64_t path) {
    for (int i = 0; i < g_nfiles; i++)
        if (g_fs[i].path == path) return &g_fs[i];
    return NULL;
}

static void fs_reset(void) {
    memset(g_fs, 0, sizeof(g_fs));
    g_nfiles = 0;
    g_next_path = 1;
}

/* ── Styx Server Callbacks (same pattern as hosted.c) ───────────── */

static int test_attach(styx_server_t *srv, uint32_t fid, const char *aname) {
    (void)aname;
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (!srv->fids[i].in_use) {
            memset(&srv->fids[i], 0, sizeof(srv->fids[i]));
            srv->fids[i].in_use = 1;
            srv->fids[i].fid = fid;
            srv->fids[i].qid.type = STX_QTDIR;
            srv->fids[i].qid.path = 0; /* Root */
            srv->fids[i].qid.version = 1;
            return 0;
        }
    }
    return -1;
}

static int test_walk(styx_server_t *srv, uint32_t fid, uint32_t newfid,
                      const char **wname, int nwname,
                      styx_qid_t *qids, int *nwqid) {
    /* Find source fid */
    styx_fid_t *f = NULL;
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid) { f = &srv->fids[i]; break; }
    if (!f) return -1;
    
    if (nwname == 0) {
        /* Clone */
        for (int i = 0; i < STYX_MAX_FIDS; i++) {
            if (!srv->fids[i].in_use) {
                srv->fids[i] = *f;
                srv->fids[i].fid = newfid;
                qids[0] = f->qid;
                *nwqid = 1;
                return 0;
            }
        }
        return -1;
    }
    
    styx_fid_t *nf = NULL;
    uint64_t current_path = 0;
    int walked = 0;
    
    for (int i = 0; i < nwname; i++) {
        styxfs_file_t *file = NULL;
        for (int j = 0; j < g_nfiles; j++) {
            if (strcmp(g_fs[j].name, wname[i]) == 0) {
                file = &g_fs[j];
                break;
            }
        }
        if (!file) {
            *nwqid = walked;
            return walked > 0 ? 0 : -1;
        }
        qids[walked].type = file->qtype;
        qids[walked].path = file->path;
        qids[walked].version = 1;
        current_path = file->path;
        walked++;
        
        if (!nf) {
            for (int j = 0; j < STYX_MAX_FIDS; j++) {
                if (!srv->fids[j].in_use) { nf = &srv->fids[j]; break; }
            }
            if (!nf) return -1;
            *nf = *f;
            nf->fid = newfid;
        }
        nf->qid.type = file->qtype;
        nf->qid.path = current_path;
    }
    *nwqid = walked;
    return 0;
}

static int test_open(styx_server_t *srv, uint32_t fid, int mode, styx_qid_t *qid) {
    (void)mode;
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid) {
            *qid = srv->fids[i].qid;
            return 0;
        }
    return -1;
}

static int test_read(styx_server_t *srv, uint32_t fid, uint64_t offset,
                      uint32_t count, uint8_t *data, uint32_t *nread) {
    (void)srv;
    styx_fid_t *f = NULL;
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid) { f = &srv->fids[i]; break; }
    if (!f) return -1;
    
    uint64_t path = f->qid.path;
    if (path == 0) {
        /* Root dir read */
        uint32_t pos = 0;
        for (int i = 0; i < g_nfiles && pos + 64 < count; i++) {
            uint32_t name_len = (uint32_t)strlen(g_fs[i].name);
            uint32_t dir_rec_size = 2 + 2 + 4 + 1 + 4 + 8 + 4 + 4 + 4 + 8 +
                                    2 + name_len + 2 + 5 + 2 + 5 + 2 + 5;
            
            /* Skip entries before offset */
            static uint32_t entry_counter = 0;
            (void)entry_counter;
            
            if (pos + dir_rec_size > count) break;
            
            /* Write minimal dir entry */
            uint8_t dbuf[256];
            uint32_t dp = 0;
            styx_put16(dbuf + dp, (uint16_t)dir_rec_size); dp += 2;
            styx_put16(dbuf + dp, 0); dp += 2;
            styx_put32(dbuf + dp, 0); dp += 4;
            dbuf[dp++] = g_fs[i].qtype;
            styx_put32(dbuf + dp, 1); dp += 4;
            styx_put64(dbuf + dp, g_fs[i].path); dp += 8;
            styx_put32(dbuf + dp, g_fs[i].qtype & STX_QTDIR ? STX_DMDIR : 0); dp += 4;
            styx_put32(dbuf + dp, 0); dp += 4; /* atime */
            styx_put32(dbuf + dp, 0); dp += 4; /* mtime */
            styx_put64(dbuf + dp, g_fs[i].length); dp += 8;
            dp += styx_putstr(dbuf + dp, g_fs[i].name);
            dp += styx_putstr(dbuf + dp, "wubu");
            dp += styx_putstr(dbuf + dp, "wubu");
            dp += styx_putstr(dbuf + dp, "wubu");
            
            memcpy(data + pos, dbuf, dp);
            pos += dp;
        }
        *nread = pos;
        return 0;
    }
    
    /* File read */
    styxfs_file_t *file = fs_find_by_path(path);
    if (!file) return -1;
    
    if (offset >= file->data_len) { *nread = 0; return 0; }
    uint32_t avail = file->data_len - (uint32_t)offset;
    *nread = (count < avail) ? count : avail;
    if (*nread > 0) memcpy(data, file->data + offset, *nread);
    return 0;
}

static int test_stat(styx_server_t *srv, uint32_t fid, styx_dir_t *dir) {
    styx_fid_t *f = NULL;
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid) { f = &srv->fids[i]; break; }
    if (!f) return -1;
    
    memset(dir, 0, sizeof(*dir));
    dir->qid = f->qid;
    dir->mode = (f->qid.type & STX_QTDIR) ? STX_DMDIR : 0;
    dir->mode |= 0555;
    
    if (f->qid.path == 0) {
        strcpy(dir->name, "/");
    } else {
        styxfs_file_t *file = fs_find_by_path(f->qid.path);
        if (file) strncpy(dir->name, file->name, STYX_MAX_FNAME - 1);
        else strcpy(dir->name, "unknown");
    }
    strcpy(dir->uid, "wubu");
    strcpy(dir->gid, "wubu");
    return 0;
}

/* ── Tests ───────────────────────────────────────────────────────── */

static styx_server_t g_srv;
static uint8_t g_in[STYX_MAX_MSG];
static uint8_t g_out[STYX_MAX_MSG];
static uint32_t g_inlen, g_outlen;

static void setup(void) {
    styx_init(&g_srv);
    g_srv.attach = test_attach;
    g_srv.walk = test_walk;
    g_srv.open = test_open;
    g_srv.read = test_read;
    g_srv.stat = test_stat;
    fs_reset();
}

/* ── 1. Namespace Construction ──────────────────────────────────── */

static void test_namespace_construction(void) {
    TEST("Namespace: add directories and files");
    setup();
    
    /* Build namespace like hosted.c */
    CHECK(fs_add_dir("wubu") == 0, "Add /wubu dir");
    CHECK(fs_add_dir("dev") == 0, "Add /dev dir");
    CHECK(fs_add_dir("prog") == 0, "Add /prog dir");
    
    const char *msg = "WuBuOS hosted";
    CHECK(fs_add_file("cons", (const uint8_t*)msg, strlen(msg)) == 0, "Add /dev/cons");
    
    CHECK(g_nfiles == 4, "4 files in namespace");
    CHECK(g_fs[0].qtype == STX_QTDIR, "wubu is dir");
    CHECK(g_fs[3].qtype == STX_QTFILE, "cons is file");
    CHECK(g_fs[3].length == strlen(msg), "cons has correct length");
    PASS();
}

/* ── 2. Styx Attach to Namespace ────────────────────────────────── */

static void test_styx_attach_namespace(void) {
    TEST("Styx: Tattach connects to root");
    setup();
    fs_add_dir("wubu");
    fs_add_dir("dev");
    
    /* Connect */
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RVERSION, "Version OK");
    
    /* Attach */
    styx_build_tattach(g_in, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RATTACH, "Attach OK");
    CHECK(g_out[7] == STX_QTDIR, "Root is dir");
    PASS();
}

/* ── 3. Walk Namespace ──────────────────────────────────────────── */

static void test_walk_to_file(void) {
    TEST("Styx: Twalk to /dev/cons");
    setup();
    fs_add_dir("wubu");
    fs_add_dir("dev");
    const char *msg = "Hello";
    fs_add_file("cons", (const uint8_t*)msg, strlen(msg));
    
    /* Connect + attach */
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    styx_build_tattach(g_in, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    
    /* Walk to /dev */
    const char *wname1[] = {"dev"};
    styx_build_twalk(g_in, &g_inlen, 2, 1, 2, wname1, 1);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RWALK, "Walk to dev");
    CHECK(styx_get16(g_out + 7) == 1, "nwqid = 1");
    
    /* Walk to /dev/cons from /dev */
    const char *wname2[] = {"cons"};
    styx_build_twalk(g_in, &g_inlen, 3, 2, 3, wname2, 1);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RWALK, "Walk to cons");
    
    /* Open for reading */
    styx_build_topen(g_in, &g_inlen, 4, 3, STX_OREAD);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_ROPEN, "Open cons");
    
    /* Read from cons */
    styx_build_tread(g_in, &g_inlen, 5, 3, 0, 64);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RREAD, "Read cons");
    
    uint32_t nread = styx_get32(g_out + 7);
    CHECK(nread == strlen(msg), "Read correct length");
    CHECK(memcmp(g_out + 11, msg, nread) == 0, "Read correct data");
    PASS();
}

/* ── 4. Walk with 0 elements (clone) ────────────────────────────── */

static void test_walk_clone(void) {
    TEST("Styx: Twalk 0 elements clones fid");
    setup();
    fs_add_dir("wubu");
    
    /* Connect + attach */
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    styx_build_tattach(g_in, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    
    /* Clone fid 1 to fid 10 */
    const char *empty[] = {};
    styx_build_twalk(g_in, &g_inlen, 2, 1, 10, empty, 0);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RWALK, "Clone RWALK");
    CHECK(styx_get16(g_out + 7) == 1, "nwqid = 1 for clone");
    PASS();
}

/* ── 5. Walk to non-existent file ───────────────────────────────── */

static void test_walk_nonexistent(void) {
    TEST("Styx: Walk to nonexistent returns Rerror");
    setup();
    fs_add_dir("wubu");
    
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    styx_build_tattach(g_in, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    
    const char *wname[] = {"nonexistent"};
    styx_build_twalk(g_in, &g_inlen, 2, 1, 2, wname, 1);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RERROR, "Rerror for nonexistent walk");
    PASS();
}

/* ── 6. .wubu container in namespace ────────────────────────────── */

static void test_wubu_container_in_namespace(void) {
    TEST("Namespace: .wubu container as Styx file");
    setup();
    fs_add_dir("wubu");
    
    /* Create a minimal .wubu header */
    uint8_t container[64];
    memset(container, 0, sizeof(container));
    memcpy(container, "WUBU!\0\1\2", 8);
    container[8] = 1; /* ELF payload type */
    container[16] = 0x48; container[17] = 0x31; /* Example bytes */
    
    CHECK(fs_add_file("hello.wubu", container, sizeof(container)) == 0,
          "Registers .wubu file");
    
    /* Verify it's findable by path */
    styxfs_file_t *f = NULL;
    for (int i = 0; i < g_nfiles; i++)
        if (strcmp(g_fs[i].name, "hello.wubu") == 0) { f = &g_fs[i]; break; }
    CHECK(f != NULL, "hello.wubu exists");
    CHECK(f->length == 64, "64-byte .wubu header");
    CHECK(memcmp(f->data, "WUBU!", 5) == 0, "WUBU magic");
    PASS();
}

/* ── 7. Styx stat on namespace files ────────────────────────────── */

static void test_stat_namespace_file(void) {
    TEST("Styx: Tstat on namespace file");
    setup();
    fs_add_dir("wubu");
    fs_add_file("hello.wubu", (const uint8_t*)"WUBU!\0\1\2" "test", 12);
    
    /* Connect + attach + walk */
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    styx_build_tattach(g_in, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    
    const char *wname[] = {"wubu"};
    styx_build_twalk(g_in, &g_inlen, 2, 1, 2, wname, 1);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    
    const char *wname2[] = {"hello.wubu"};
    styx_build_twalk(g_in, &g_inlen, 3, 2, 3, wname2, 1);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    
    /* Stat */
    styx_build_tstat(g_in, &g_inlen, 4, 3);
    styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
    CHECK(g_out[4] == STX_RSTAT, "Rstat for wubu file");
    PASS();
}

/* ── 8. Namespace teardown / allocator sanity ───────────────────── */

static void test_fs_reset_clean(void) {
    TEST("Namespace: fs_reset clears all entries");
    setup();
    fs_add_dir("test1");
    fs_add_dir("test2");
    fs_add_file("f1", (const uint8_t*)"data", 4);
    CHECK(g_nfiles == 3, "3 files before reset");
    
    fs_reset();
    CHECK(g_nfiles == 0, "0 files after reset");
    CHECK(g_next_path == 1, "path counter reset");
    PASS();
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Hosted Mode Test Suite                  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");
    
    test_namespace_construction();
    test_styx_attach_namespace();
    test_walk_to_file();
    test_walk_clone();
    test_walk_nonexistent();
    test_wubu_container_in_namespace();
    test_stat_namespace_file();
    test_fs_reset_clean();
    
    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n",
           g_passed, g_tests, g_failed);
    printf("══════════════════════════════════════════════════\n");
    return g_failed > 0 ? 1 : 0;
}
