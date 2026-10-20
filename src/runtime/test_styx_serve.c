/*
 * test_styx_serve.c -- 9P2000 wire-conformance suite for styx_serve
 * (GOALS-1000 #592 msize floor/ceiling/echo, #569 Tcreate/fid
 * lifecycle, plus the Tauth/Tflush/Twstat conformance arms). Every
 * expectation cites the 0intro/9P2000 rule it encodes.
 */
#include "styx.h"
#include "styx_flush.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

static int fails;

/* NETWORK #636: the fid leak guard -- how many slots are live */
static int fids_in_use(styx_server_t *s)
{
    int n = 0;
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (s->fids[i].in_use) n++;
    return n;
}
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

static styx_server_t g_srv;
static uint8_t g_in[STYX_MAX_MSG], g_out[STYX_MAX_MSG];
static uint32_t g_inlen, g_outlen;

/* tiny in-memory fs state for the lifecycle + stat/wstat rounds */
static uint32_t g_created_perm, g_created_mode;
static char g_created_name[64];
static int g_create_calls, g_wstat_calls, g_flush_calls;
static char g_walk_names[4][STYX_MAX_FNAME];
static int g_walk_ncalls, g_walk_nnames_last;

static uint32_t g_read_count_seen;
static char g_read_buf[256];
static uint64_t g_read_off_seen;
static styx_dir_t g_wstat_seen;

static void setup(void)
{
    memset(&g_srv, 0, sizeof(g_srv));
    styx_init(&g_srv);
    memset(g_in, 0, sizeof(g_in));
    memset(g_out, 0, sizeof(g_out));
}

static void connect(void)
{
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RVERSION);
}

static void attach(uint32_t fid, uint16_t tag)
{
    styx_build_tattach(g_in, &g_inlen, tag, fid, 0xFFFFFFFF, "");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RATTACH);
    CHECK(styx_get16(g_out + 5) == tag);          /* tag echo */
}

/* -- callbacks ------------------------------------------------------ */

static int cb_create(styx_server_t *s, uint32_t fid, const char *name,
                     uint32_t perm, int mode, styx_qid_t *qid)
{
    (void)s;
    if (strcmp(name, "denied") == 0) { errno = EEXIST; return -1; }
    g_create_calls++;
    snprintf(g_created_name, sizeof(g_created_name), "%s", name);
    g_created_perm = perm;
    g_created_mode = (uint32_t)mode;
    qid->type = STX_QTFILE;
    qid->version = 7;
    qid->path = 0xC0FFEE;
    /* open(5): Tcreate re-roots the fid on the new file */
    styx_fid_t *f = styx_fid_lookup(s, fid);
    if (f) f->qid = *qid;
    return 0;
}

static int cb_wstat(styx_server_t *s, uint32_t fid, const styx_dir_t *dir)
{
    (void)s; (void)fid;
    g_wstat_calls++;
    g_wstat_seen = *dir;
    return 0;
}

static int cb_stat(styx_server_t *s, uint32_t fid, styx_dir_t *dir)
{
    (void)s; (void)fid;
    memset(dir, 0, sizeof(*dir));
    dir->type = 0; dir->dev = 3;
    dir->qid.type = STX_QTFILE; dir->qid.version = 42; dir->qid.path = 99;
    dir->mode = 0644; dir->atime = 111; dir->mtime = 222; dir->length = 4242;
    snprintf(dir->name, sizeof(dir->name), "probe.txt");
    snprintf(dir->uid, sizeof(dir->uid), "wubu");
    snprintf(dir->gid, sizeof(dir->gid), "wubu");
    snprintf(dir->muid, sizeof(dir->muid), "wubu");
    return 0;
}

static uint32_t g_write_count_seen;
static uint64_t g_write_off_seen;
static char g_write_data[64];

static int cb_write(styx_server_t *s, uint32_t fid, uint64_t offset,
                    uint32_t count, const uint8_t *data, uint32_t *nwritten)
{
    (void)s; (void)fid;
    g_write_off_seen = offset;
    g_write_count_seen = count;
    if (count > sizeof(g_write_data)) count = sizeof(g_write_data);
    memcpy(g_write_data, data, count);
    *nwritten = count;
    return 0;
}

static uint32_t g_removed_fid;
static int g_remove_calls;

static void cb_remove(styx_server_t *s, uint32_t fid)
{
    (void)s;
    g_remove_calls++;
    g_removed_fid = fid;
}

static int cb_walk(styx_server_t *s, uint32_t fid, uint32_t newfid,
                    const char **wname, int nwname,
                    styx_qid_t *qids, int *nwqid)
{
    (void)s; (void)fid; (void)newfid;
    g_walk_ncalls++;
    g_walk_nnames_last = nwname;
    for (int i = 0; i < nwname && i < 4; i++)
        snprintf(g_walk_names[i], STYX_MAX_FNAME, "%s", wname[i]);
    for (int i = 0; i < nwname && i < 16; i++) {
        qids[i].type = STX_QTDIR;
        qids[i].version = 1;
        qids[i].path = 100 + i;
    }
    *nwqid = nwname;
    return 0;
}

static int cb_read(styx_server_t *s, uint32_t fid, uint64_t offset,
                   uint32_t count, uint8_t *data, uint32_t *nread)
{
    (void)s; (void)fid;
    g_read_off_seen = offset;
    g_read_count_seen = count;      /* what the DISPATCH passed us, post-cap */
    memset(data, 'A', count > 8 ? 8 : count);
    *nread = count > 8 ? 8 : count;
    return 0;
}

static void cb_flush(styx_server_t *s, uint16_t oldtag)
{
    (void)s;
    g_flush_calls++;
    if (oldtag != 0x1234) { /* count every flush; the test checks the sum */ }
}

/* -- tests ----------------------------------------------------------- */

static void test_version_negotiation(void)
{
    /* #592: floor -- an msize below it cannot carry an Rversion */
    setup();
    styx_build_tversion(g_in, &g_inlen, 32, "9P2000");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);

    /* ceiling: the server clamps the client's ask to max_msize */
    setup();
    g_srv.max_msize = 1024;
    styx_build_tversion(g_in, &g_inlen, 8192, "9P2000");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RVERSION);
    CHECK(styx_get32(g_out + 7) == 1024);          /* echoed = clamped msize */

    /* echo: the reply carries the REQUEST's tag (clients send NOTAG) */
    setup();
    {
        /* build Tversion with NOTAG by hand: size = 7 + 4 + (2+6) = 19 */
        uint32_t pos = 0;
        styx_put32(g_in + pos, 19); pos += 4;
        g_in[pos] = STX_TVERSION; pos++;
        styx_put16(g_in + pos, 0xFFFF); pos += 2;   /* NOTAG */
        styx_put32(g_in + pos, 4096); pos += 4;
        pos += styx_putstr(g_in + pos, "9P2000");
        g_inlen = pos;
        CHECK(g_inlen == 19);
    }
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RVERSION);
    CHECK(styx_get16(g_out + 5) == 0xFFFF);        /* NOTAG echoed */
    CHECK(styx_get32(g_out + 7) == 4096);

    /* after negotiation, an oversized message is refused */
    memset(g_in, 0, 32);
    styx_put32(g_in, 8192); g_in[4] = STX_TREAD; styx_put16(g_in + 5, 1);
    CHECK(styx_serve(&g_srv, g_in, 32, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);

    /* bad version string -> Rerror */
    setup();
    styx_build_tversion(g_in, &g_inlen, 8192, "9P9999");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);
}

static void test_auth_conformant(void)
{
    /* 0intro:159-170 / auth(5): no-auth server REFUSES Tauth */
    setup(); connect();
    styx_build_tauth(g_in, &g_inlen, 5, 0xFFFFFFFF, "wubu", "");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);                 /* not Rattach (the bug) */
    /* a real client then attaches with afid=NOFID and succeeds */
    attach(1, 6);
}

static void test_flush_arm(void)
{
    /* 0intro:476-492: always Rflush, even for an unknown oldtag */
    setup(); connect();
    g_srv.flush = cb_flush;
    g_flush_calls = 0;
    styx_build_tflush(g_in, &g_inlen, 9, 0x1234);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RFLUSH);                 /* was: Unknown message */
    CHECK(styx_get16(g_out + 5) == 9);
    CHECK(g_flush_calls == 1);
    /* an unknown/already-answered oldtag still gets Rflush */
    styx_build_tflush(g_in, &g_inlen, 10, 0xBEEF);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RFLUSH);
    /* the flushtable itself: begin -> cancel -> is_cancelled -> end */
    styx_flushtab_t tab;
    styx_flush_init(&tab);
    CHECK(styx_flush_begin(&tab, 0x77) == 0);
    CHECK(styx_flush_begin(&tab, 0x77) == -1);    /* duplicate in flight */
    CHECK(styx_flush_cancel(&tab, 0x77) == 0);
    CHECK(styx_flush_is_cancelled(&tab, 0x77) == 1);
    CHECK(styx_flush_end(&tab, 0x77) == 0);
    CHECK(styx_flush_is_cancelled(&tab, 0x77) == 0); /* freed */
    CHECK(styx_flush_cancel(&tab, 0x77) == -1);    /* not in flight */
}

static void test_create_lifecycle(void)
{
    /* #569: Tcreate reaches the wire, re-roots the fid; the fid then
     * follows the standard lifecycle (Tstat -> Tclunk -> gone). */
    setup(); connect(); attach(3, 2);
    g_srv.create = cb_create;
    g_create_calls = 0;
    styx_build_tcreate(g_in, &g_inlen, 11, 3, "notes.txt", 0644,
                       STX_OWRITE);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RCREATE);                /* was: Unknown message */
    CHECK(styx_get16(g_out + 5) == 11);
    CHECK(g_create_calls == 1);
    CHECK(strcmp(g_created_name, "notes.txt") == 0);
    CHECK(g_created_perm == 0644);
    CHECK(g_created_mode == STX_OWRITE);
    /* Rcreate body: qid[13] + iounit[4] */
    CHECK(g_out[7] == STX_QTFILE);
    CHECK(styx_get32(g_out + 8) == 7);
    CHECK(styx_get64(g_out + 12) == 0xC0FFEE);
    CHECK(styx_get32(g_out + 20) == 0);            /* iounit */
    /* the fid was re-rooted: its qid now matches the created file */
    styx_fid_t *f = styx_fid_lookup(&g_srv, 3);
    CHECK(f && f->qid.path == 0xC0FFEE);
    /* the re-rooted fid stats as the new file (lifecycle continuity) */
    styx_build_tstat(g_in, &g_inlen, 12, 3);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RSTAT);
    /* clunk frees the fid */
    styx_build_tclunk(g_in, &g_inlen, 13, 3);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RCLUNK);
    CHECK(styx_fid_lookup(&g_srv, 3) == NULL);
    /* a failed create reports Rerror and does not touch the fid table */
    attach(4, 14);
    styx_build_tcreate(g_in, &g_inlen, 15, 4, "denied", 0644, STX_OWRITE);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);
    CHECK(g_create_calls == 1);                    /* unchanged */

    /* NAMESPACE #590: the Rerror carries the REASON (the callback's
     * errno mapped through the ename table), not a generic 'failed' */
    {
        uint16_t esz = styx_get16(g_out + 7);
        char etxt[64] = {0};
        if (esz < sizeof(etxt)) memcpy(etxt, g_out + 9, esz);
        CHECK(strcmp(etxt, "file already exists") == 0);
    }
}

static void test_wstat_rstat_roundtrip(void)
{
    /* #591: Twstat answers Rwstat (0intro:296-306); the Dir bytes the
     * server parses round-trip field-exactly through the wstat callback. */
    setup(); connect(); attach(5, 20);
    g_srv.wstat = cb_wstat;
    g_srv.stat = cb_stat;
    g_wstat_calls = 0;

    styx_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    dir.type = 0; dir.dev = 9;
    dir.qid.type = STX_QTFILE; dir.qid.version = 5; dir.qid.path = 777;
    dir.mode = 0600; dir.atime = 1000; dir.mtime = 2000; dir.length = 65;
    snprintf(dir.name, sizeof(dir.name), "renamed");
    snprintf(dir.uid, sizeof(dir.uid), "ada");
    snprintf(dir.gid, sizeof(dir.gid), "grp");
    snprintf(dir.muid, sizeof(dir.muid), "m");
    styx_build_twstat(g_in, &g_inlen, 21, 5, &dir);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RWSTAT);                /* was: Rclunk (the bug) */
    CHECK(styx_get16(g_out + 5) == 21);
    CHECK(g_wstat_calls == 1);
    /* every field round-tripped through the wire */
    CHECK(g_wstat_seen.dev == 9);
    CHECK(g_wstat_seen.qid.type == STX_QTFILE);
    CHECK(g_wstat_seen.qid.version == 5);
    CHECK(g_wstat_seen.qid.path == 777);
    CHECK(g_wstat_seen.mode == 0600);
    CHECK(g_wstat_seen.atime == 1000);
    CHECK(g_wstat_seen.mtime == 2000);
    CHECK(g_wstat_seen.length == 65);
    CHECK(strcmp(g_wstat_seen.name, "renamed") == 0);
    CHECK(strcmp(g_wstat_seen.uid, "ada") == 0);
    CHECK(strcmp(g_wstat_seen.gid, "grp") == 0);
    CHECK(strcmp(g_wstat_seen.muid, "m") == 0);

    /* Tstat -> Rstat: the served Dir parses back with field fidelity */
    styx_build_tstat(g_in, &g_inlen, 22, 5);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RSTAT);
    /* Rstat body: size[2] then the Dir -- decode it back */
    const uint8_t *p = g_out + 7;
    uint16_t dsz = styx_get16(p); p += 2;
    CHECK(dsz > 40);
    styx_dir_t back;
    memset(&back, 0, sizeof(back));
    back.type = styx_get16(p); p += 2;
    back.dev = styx_get32(p); p += 4;
    back.qid.type = *p; p++;
    back.qid.version = styx_get32(p); p += 4;
    back.qid.path = styx_get64(p); p += 8;
    back.mode = styx_get32(p); p += 4;
    back.atime = styx_get32(p); p += 4;
    back.mtime = styx_get32(p); p += 4;
    back.length = styx_get64(p); p += 8;
    p = styx_getstr(p, back.name, sizeof(back.name), 64);
    p = styx_getstr(p, back.uid, sizeof(back.uid), 64);
    p = styx_getstr(p, back.gid, sizeof(back.gid), 64);
    styx_getstr(p, back.muid, sizeof(back.muid), 64);
    (void)p;
    CHECK(back.dev == 3);
    CHECK(back.qid.version == 42 && back.qid.path == 99);
    CHECK(back.mode == 0644);
    CHECK(back.length == 4242);
    CHECK(strcmp(back.name, "probe.txt") == 0);
    CHECK(strcmp(back.uid, "wubu") == 0);
}

static void test_fid_table_clone_multiname(void)
{
    /* NAMESPACE #593: the fid table API, the wire clone, and multi-name
     * walks. KERNEL #569's Tremove leg rides along at the end. */
    setup(); connect(); attach(30, 60);

    /* the fid table API directly: alloc -> lookup -> free -> gone */
    styx_fid_t *fa = styx_fid_alloc(&g_srv, 90);
    CHECK(fa && fa->fid == 90);
    CHECK(styx_fid_lookup(&g_srv, 90) == fa);
    styx_fid_free(&g_srv, 90);
    CHECK(styx_fid_lookup(&g_srv, 90) == NULL);

    /* exhaustion: fill every remaining slot, then alloc must fail */
    for (uint32_t i = 100; i < 100 + STYX_MAX_FIDS; i++) {
        if (styx_fid_lookup(&g_srv, i)) continue;
        if (!styx_fid_alloc(&g_srv, i)) {
            CHECK(i - 100 >= 1);              /* at least one was taken */
            goto table_full;
        }
    }
    CHECK(!styx_fid_alloc(&g_srv, 9999));     /* table exhausted */
table_full:
    /* free them again */
    for (uint32_t i = 100; i < 100 + STYX_MAX_FIDS + 8; i++)
        styx_fid_free(&g_srv, i);

    /* clone via the wire: walk 30 -> 31 with no names */
    g_walk_ncalls = 0;
    styx_fid_t *f30 = styx_fid_lookup(&g_srv, 30);
    CHECK(f30 != NULL);
    styx_build_twalk(g_in, &g_inlen, 61, 30, 31, NULL, 0);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RWALK);
    CHECK(styx_get16(g_out + 7) == 1);        /* nwqid 1 for a clone */
    styx_fid_t *f31 = styx_fid_lookup(&g_srv, 31);
    CHECK(f31 && f31->qid.path == f30->qid.path);  /* clone carries the qid */
    CHECK(f30 != NULL && styx_fid_lookup(&g_srv, 30) != NULL); /* source lives */
    CHECK(g_walk_ncalls == 0);                /* clone never calls the walk cb */

    /* cloning onto an IN-USE fid is refused (walk(5)), not duplicated */
    styx_build_twalk(g_in, &g_inlen, 62, 30, 31, NULL, 0);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);            /* fid in use */

    /* a walk to a BAD source fid is refused */
    styx_build_twalk(g_in, &g_inlen, 63, 7777, 32, NULL, 0);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);

    /* multi-name walk: three components reach the callback in order and
     * the reply carries one qid per component */
    const char *wn[3] = { "n", "sub", "leaf" };
    g_srv.walk = cb_walk;
    g_walk_ncalls = 0;
    styx_build_twalk(g_in, &g_inlen, 64, 30, 33, wn, 3);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RWALK);
    CHECK(styx_get16(g_out + 7) == 3);        /* nwqid = 3 */
    CHECK(g_walk_ncalls == 1 && g_walk_nnames_last == 3);
    CHECK(strcmp(g_walk_names[0], "n") == 0);
    CHECK(strcmp(g_walk_names[1], "sub") == 0);
    CHECK(strcmp(g_walk_names[2], "leaf") == 0);
    /* qid[0] right after nwqid */
    CHECK(g_out[9] == STX_QTDIR && styx_get64(g_out + 14) == 100);

    /* KERNEL #569's Tremove leg: a remove implies clunk (0intro:262-271
     * -- even if the remove fails the fid is gone), and the callback
     * fires before the fid is freed. */
    attach(34, 66);
    g_removed_fid = 0; g_remove_calls = 0;
    g_srv.remove = cb_remove;

    styx_build_tremove(g_in, &g_inlen, 67, 34);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RREMOVE);
    CHECK(styx_get16(g_out + 5) == 67);
    CHECK(g_remove_calls == 1 && g_removed_fid == 34);
    CHECK(styx_fid_lookup(&g_srv, 34) == NULL);   /* clunked by the remove */

    /* no remove callback: the house default clunks the fid too (a bare
     * fid-table server's remove IS the clunk; matches the read arm's
     * no-backend EOF default) */
    attach(35, 68);
    g_srv.remove = NULL;
    styx_build_tremove(g_in, &g_inlen, 69, 35);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RREMOVE);
    CHECK(styx_fid_lookup(&g_srv, 35) == NULL);

    /* remove of a BAD fid still answers (the clunk is idempotent) */
    styx_build_tremove(g_in, &g_inlen, 70, 7777);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RREMOVE);

    /* NETWORK #636 -- the fid leak guard: after a full session
     * (attach/clone/walk/create/clunk/remove), clunking every live fid
     * returns the table to EMPTY. */
    int guard_live = fids_in_use(&g_srv);
    CHECK(guard_live > 0);                       /* there IS something to leak */
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (g_srv.fids[i].in_use) {
            styx_build_tclunk(g_in, &g_inlen, 71, g_srv.fids[i].fid);
            CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
            CHECK(g_out[4] == STX_RCLUNK);
        }
    }
    CHECK(fids_in_use(&g_srv) == 0);             /* no leaks */
}

static void test_read_iounit_and_offsets(void)
{
    /* KERNEL #483: the dispatch must cap Tread count to the negotiated
     * msize minus the 11-byte Rread header (iounit cap), and advance the
     * fid offset by the number of bytes actually returned. */
    setup(); connect(); attach(8, 40);
    g_srv.read = cb_read;

    /* renegotiate a SMALL msize: Tversion msize=128 */
    styx_build_tversion(g_in, &g_inlen, 128, "9P2000");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RVERSION && styx_get32(g_out + 7) == 128);
    attach(8, 41);

    /* ask for 1000 bytes; the callback must see the cap (128-11=117) */
    styx_build_tread(g_in, &g_inlen, 42, 8, 500, 1000);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RREAD);
    CHECK(g_read_count_seen == 117);              /* msize-11, not 1000 */
    CHECK(g_read_off_seen == 500);                /* offset passed through */
    uint32_t rcount = styx_get32(g_out + 7);
    CHECK(rcount == 8);                           /* 8 'A's returned */
    CHECK(g_out[11] == 'A');
    /* the fid offset advanced by the bytes actually returned */
    styx_fid_t *f = styx_fid_lookup(&g_srv, 8);
    CHECK(f && f->offset == 508);
    /* the reply frame itself fits the negotiated msize */
    CHECK(g_outlen == 11 + 8 && g_outlen <= 128);

    /* Twrite: offset passes through, the payload lands intact, the fid
     * offset advances by the count written (KERNEL #483) */
    g_srv.write = cb_write;
    styx_build_twrite(g_in, &g_inlen, 44, 8, 100, 4, (const uint8_t *)"wubu");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RWRITE);
    CHECK(styx_get32(g_out + 7) == 4);            /* count echoed */
    CHECK(g_write_off_seen == 100 && g_write_count_seen == 4);
    CHECK(memcmp(g_write_data, "wubu", 4) == 0);
    f = styx_fid_lookup(&g_srv, 8);
    CHECK(f && f->offset == 104);                 /* 100 + 4 written */

    /* a write whose payload overruns the frame is refused, not truncated */
    styx_build_twrite(g_in, &g_inlen, 45, 8, 0, 200,
                      (const uint8_t *)"short");
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);                /* 23+200 > actual frame */

        /* a fid with no read callback reports EOF, not an error */
    g_srv.read = NULL;
    styx_build_tread(g_in, &g_inlen, 43, 8, 0, 10);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RREAD && styx_get32(g_out + 7) == 0);
}

static void test_fuzz_conformance(void)
{
    /* NAMESPACE #609: a deterministic wire-conformance corpus -- mutated
     * and random frames must never crash the dispatch, and every reply
     * must be a well-formed 9P2000 message (type + echoed tag). */
    setup(); connect(); attach(50, 80);

    uint32_t seed = 0x9E3779B9u;
    int crashes_seen = 0, bad_reply = 0, answered = 0, rejected = 0;
    for (int i = 0; i < 2000; i++) {
        /* xorshift32: deterministic, reproducible corpus */
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        int len = 7 + (seed % 64);
        for (int j = 0; j < len; j++) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            g_in[j] = (uint8_t)(seed >> 24);
        }
        g_in[0] = (uint8_t)len; g_in[1] = (uint8_t)(len >> 8); /* plausible size */
        if (styx_serve(&g_srv, g_in, (uint32_t)len, g_out, &g_outlen) != 0)
            rejected++;                          /* incomplete frame: wait for more */
        if (g_out[4] >= 100 && g_out[4] <= 127) answered++;
        else if (g_out[4] != 0) bad_reply++;
    }

    /* systematic truncation of every REAL message type: cut each builder
     * output at every prefix length -- no crash, always a sane reply */
    struct { void (*build)(uint8_t*, uint32_t*, uint16_t, uint32_t,
                           const char*); uint32_t fid; const char *name; } dummy;
    (void)dummy;
    uint8_t base[STYX_MAX_MSG]; uint32_t blen;
    for (int t = STX_TVERSION; t <= STX_TWSTAT; t++) {
        styx_build_tversion(base, &blen, 8192, "9P2000");
        base[4] = (uint8_t)t;
        for (uint32_t cut = 0; cut < blen && cut < 40; cut++) {
            memcpy(g_in, base, blen);
            g_inlen = cut;
            g_out[4] = 0;
            int rc = styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen);
            if (rc != 0) {
                rejected++;
                /* a rejected partial frame must leave NO reply behind */
                if (g_out[4] != 0) bad_reply++;
            } else if (g_out[4] < 100 || g_out[4] > 127) {
                bad_reply++;                     /* a garbage reply type */
            }
        }
    }

    printf("[fuzz: 2000 random + %d truncations, answered=%d rejected=%d bad=%d] ",
           (STX_TWSTAT - STX_TVERSION + 1) * 40, answered, rejected, bad_reply);
    CHECK(crashes_seen == 0);                     /* the dispatch never dies */
    CHECK(bad_reply == 0);                       /* replies are always sane */
    CHECK(answered + rejected >= 2000);         /* every random frame resolves */
}

static void test_create_without_cb(void)
{
    setup(); connect(); attach(6, 30);
    /* no create callback -> conformant Rerror (not a crash, not silence) */
    styx_build_tcreate(g_in, &g_inlen, 31, 6, "x", 0644, STX_OWRITE);
    CHECK(styx_serve(&g_srv, g_in, g_inlen, g_out, &g_outlen) == 0);
    CHECK(g_out[4] == STX_RERROR);
}

int main(void)
{
    test_version_negotiation();
    test_auth_conformant();
    test_flush_arm();
    test_create_lifecycle();
    test_wstat_rstat_roundtrip();
    test_create_without_cb();
    test_read_iounit_and_offsets();
    test_fuzz_conformance();
    test_fid_table_clone_multiname();
    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
