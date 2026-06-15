/*
 * styx_test.c  --  Styx/9P2000 Protocol Test Suite
 *
 * Tests: message building, parsing, server dispatch,
 * fid management, error handling, edge cases.
 */
#include "styx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -- Test Framework ------------------------------------------------ */

static int g_tests = 0, g_passed = 0, g_failed = 0;

#define TEST(name) do { \
    g_tests++; \
    printf("  TEST %-50s ", name); \
    fflush(stdout); \
} while (0)

#define PASS() do { printf("✅\n"); g_passed++; } while(0)

#define FAIL(msg) do { \
    printf("❌ %s\n", msg); g_failed++; \
} while (0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

/* -- Fixture ------------------------------------------------------ */

static uint8_t g_inbuf[STYX_MAX_MSG];
static uint8_t g_outbuf[STYX_MAX_MSG];
static uint32_t g_inlen, g_outlen;
static styx_server_t g_srv;

static void setup(void) {
    memset(g_inbuf, 0xFF, sizeof(g_inbuf));
    memset(g_outbuf, 0xFF, sizeof(g_outbuf));
    g_inlen = g_outlen = 0;
    styx_init(&g_srv);
}

/* -- Helper to verify header -------------------------------------- */

static int check_header(const uint8_t *buf, uint32_t len,
                         uint8_t expected_type, uint16_t expected_tag) {
    if (len < 7) return -1;
    uint32_t size = styx_get32(buf);
    if (size != len) return -2;
    if (buf[4] != expected_type) return -3;
    if (styx_get16(buf + 5) != expected_tag) return -4;
    return 0;
}

/* -- Tversion / Rversion Tests ------------------------------------ */

static void test_version_negotiate(void) {
    TEST("Tversion → Rversion basic");
    setup();
    /* Build Tversion */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    CHECK(g_inlen >= 7, "Expected header");
    CHECK(g_inbuf[4] == STX_TVERSION, "Tversion type");
    
    /* Serve */
    int ret = styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(ret == 0, "Serve should succeed");
    CHECK(g_outbuf[4] == STX_RVERSION, "Rversion type");
    CHECK(g_srv.connected == 1, "Server connected");
    PASS();
}

static void test_version_bad_protocol(void) {
    TEST("Tversion bad protocol → Rerror");
    setup();
    /* Build Tversion with fake version */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P3000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for bad version");
    PASS();
}

static void test_version_parse_response(void) {
    TEST("Parse Rversion response");
    setup();
    uint8_t buf[64];
    uint32_t len;
    uint32_t msize = 0;
    char version[16] = {0};
    
    styx_build_tversion(buf, &len, 4096, "9P2000");
    styx_serve(&g_srv, buf, len, g_outbuf, &g_outlen);
    
    int ret = styx_parse_version(g_outbuf, g_outlen, &msize, version);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(msize > 0, "msize should be > 0");
    CHECK(strcmp(version, "9P2000") == 0, "Version should be 9P2000");
    PASS();
}

/* -- Tattach / Rattach Tests -------------------------------------- */

static void test_attach_no_auth(void) {
    TEST("Tattach → Rattach (no auth needed)");
    setup();
    /* First need version to connect */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Now attach */
    styx_build_tattach(g_inbuf, &g_inlen, 1, 1, 0xFFFFFFFF, "wubu");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RATTACH, "Rattach");
    
    /* Check QID is a directory type */
    CHECK(g_outbuf[7] == STX_QTDIR, "Root QID should be directory");
    PASS();
}

static void test_attach_parse(void) {
    TEST("Parse Tattach message");
    setup();
    uint32_t fid = 0, afid = 0;
    char aname[STYX_MAX_FNAME] = {0};
    
    styx_build_tattach(g_inbuf, &g_inlen, 5, 42, 0xFFFFFFFF, "namespace");
    int ret = styx_parse_attach(g_inbuf, g_inlen, &fid, &afid, aname);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 42, "fid should be 42");
    CHECK(afid == 0xFFFFFFFF, "afid should be NOFID");
    CHECK(strcmp(aname, "namespace") == 0, "aname should match");
    PASS();
}

/* -- Twalk / Rwalk Tests ------------------------------------------ */

static void test_walk_zero_elements(void) {
    TEST("Twalk with 0 elements (fid clone)");
    setup();
    /* Connect + attach */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    styx_build_tattach(g_inbuf, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Walk with 0 elements (clone fid 1 to fid 2) */
    const char *empty[] = {};
    styx_build_twalk(g_inbuf, &g_inlen, 2, 1, 2, empty, 0);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RWALK, "Rwalk for clone");
    
    /* nwqid should be 1 (root QID returned) */
    uint16_t nwqid = styx_get16(g_outbuf + 7);
    CHECK(nwqid == 1, "nwqid should be 1 for clone");
    PASS();
}

/* -- Topen / Ropen Tests ------------------------------------------ */

static void test_open_file(void) {
    TEST("Topen → Ropen");
    setup();
    /* Connect + attach */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    styx_build_tattach(g_inbuf, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Open fid 1 for reading */
    styx_build_topen(g_inbuf, &g_inlen, 3, 1, STX_OREAD);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_ROPEN, "Ropen");
    
    /* Check iounit  --  offset 20 (7 header + 13 QID) */
    uint32_t iounit = styx_get32(g_outbuf + 20);
    CHECK(iounit > 0, "iounit should be > 0");
    PASS();
}

static void test_open_parse(void) {
    TEST("Parse Topen message");
    setup();
    uint32_t fid = 0;
    int mode = 0;
    
    styx_build_topen(g_inbuf, &g_inlen, 7, 12, STX_ORDWR | STX_OTRUNC);
    int ret = styx_parse_open(g_inbuf, g_inlen, &fid, &mode);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 12, "fid should be 12");
    CHECK(mode == (STX_ORDWR | STX_OTRUNC), "mode should be ORDWR|OTRUNC");
    PASS();
}

/* -- Tread / Rread Tests ------------------------------------------ */

static void test_read_empty(void) {
    TEST("Tread → Rread (empty, default handler)");
    setup();
    /* Connect + attach + open */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    styx_build_tattach(g_inbuf, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    styx_build_topen(g_inbuf, &g_inlen, 3, 1, STX_OREAD);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Read 1024 bytes */
    styx_build_tread(g_inbuf, &g_inlen, 4, 1, 0, 1024);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RREAD, "Rread");
    
    /* Count = 0 (EOF for default handler) */
    uint32_t count = styx_get32(g_outbuf + 7);
    CHECK(count == 0, "Count should be 0 for empty default handler");
    PASS();
}

static void test_read_parse(void) {
    TEST("Parse Tread message");
    setup();
    uint32_t fid = 0, count = 0;
    uint64_t offset = 0;
    
    styx_build_tread(g_inbuf, &g_inlen, 5, 3, 1024, 512);
    int ret = styx_parse_read(g_inbuf, g_inlen, &fid, &offset, &count);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 3, "fid should be 3");
    CHECK(offset == 1024, "offset should be 1024");
    CHECK(count == 512, "count should be 512");
    PASS();
}

/* -- Twrite / Rwrite Tests ---------------------------------------- */

static void test_write_parse(void) {
    TEST("Parse Twrite message");
    setup();
    uint32_t fid = 0, count = 0;
    uint64_t offset = 0;
    const uint8_t *data = NULL;
    
    uint8_t test_data[] = "Hello, Styx!";
    styx_build_twrite(g_inbuf, &g_inlen, 6, 5, 0, 12, test_data);
    int ret = styx_parse_write(g_inbuf, g_inlen, &fid, &offset,
                                &count, &data);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 5, "fid should be 5");
    CHECK(count == 12, "count should be 12");
    CHECK(memcmp(data, "Hello, Styx!", 12) == 0, "data should match");
    PASS();
}

/* -- Tclunk / Rclunk Tests ---------------------------------------- */

static void test_clunk_fid(void) {
    TEST("Tclunk → Rclunk (freed)");
    setup();
    /* Connect + attach */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    styx_build_tattach(g_inbuf, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Clunk */
    styx_build_tclunk(g_inbuf, &g_inlen, 7, 1);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RCLUNK, "Rclunk");
    
    /* Fid should be freed now */
    styx_build_tstat(g_inbuf, &g_inlen, 8, 1);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for freed fid");
    PASS();
}

static void test_clunk_parse(void) {
    TEST("Parse Tclunk message");
    setup();
    uint32_t fid = 0;
    
    styx_build_tclunk(g_inbuf, &g_inlen, 9, 7);
    int ret = styx_parse_clunk(g_inbuf, g_inlen, &fid);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 7, "fid should be 7");
    PASS();
}

/* -- Tstat / Rstat Tests ------------------------------------------ */

static void test_stat_fid(void) {
    TEST("Tstat → Rstat");
    setup();
    /* Connect + attach */
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    styx_build_tattach(g_inbuf, &g_inlen, 1, 1, 0xFFFFFFFF, "");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Stat */
    styx_build_tstat(g_inbuf, &g_inlen, 10, 1);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RSTAT, "Rstat");
    PASS();
}

static void test_stat_parse(void) {
    TEST("Parse Tstat message");
    setup();
    uint32_t fid = 0;
    
    styx_build_tstat(g_inbuf, &g_inlen, 11, 9);
    int ret = styx_parse_stat(g_inbuf, g_inlen, &fid);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 9, "fid should be 9");
    PASS();
}

/* -- Bad Fid Tests ------------------------------------------------ */

static void test_bad_fid_walk(void) {
    TEST("Twalk with bad fid → Rerror");
    setup();
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    const char *empty[] = {};
    styx_build_twalk(g_inbuf, &g_inlen, 1, 999, 2, empty, 0);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for bad fid");
    PASS();
}

static void test_bad_fid_open(void) {
    TEST("Topen with bad fid → Rerror");
    setup();
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    styx_build_topen(g_inbuf, &g_inlen, 1, 42, STX_OREAD);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for bad fid");
    PASS();
}

static void test_bad_fid_read(void) {
    TEST("Tread with bad fid → Rerror");
    setup();
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    styx_build_tread(g_inbuf, &g_inlen, 1, 99, 0, 64);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for bad fid");
    PASS();
}

static void test_bad_fid_stat(void) {
    TEST("Tstat with bad fid → Rerror");
    setup();
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    styx_build_tstat(g_inbuf, &g_inlen, 1, 77);
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for bad fid");
    PASS();
}

/* -- Edge Cases --------------------------------------------------- */

static void test_truncated_message(void) {
    TEST("Truncated message → handled gracefully");
    setup();
    uint8_t tiny[4] = {0, 0, 0, 4};
    int ret = styx_serve(&g_srv, tiny, 4, g_outbuf, &g_outlen);
    CHECK(ret == -1, "Truncated message returns -1");
    PASS();
}

static void test_unknown_message_type(void) {
    TEST("Unknown message type → Rerror");
    setup();
    styx_build_tversion(g_inbuf, &g_inlen, 8192, "9P2000");
    styx_serve(&g_srv, g_inbuf, g_inlen, g_outbuf, &g_outlen);
    
    /* Craft a message with invalid type */
    memset(g_inbuf, 0, 7);
    styx_put32(g_inbuf, 7);
    g_inbuf[4] = 255; /* Invalid type */
    styx_put16(g_inbuf + 5, 0);
    styx_serve(&g_srv, g_inbuf, 7, g_outbuf, &g_outlen);
    CHECK(g_outbuf[4] == STX_RERROR, "Rerror for unknown type");
    PASS();
}

/* -- Endian-ness and Encoding Tests ------------------------------- */

static void test_put_get_16(void) {
    TEST("styx_put16/styx_get16 round-trip");
    uint8_t buf[2];
    styx_put16(buf, 0xABCD);
    CHECK(buf[0] == 0xCD, "Low byte first (LE)");
    CHECK(buf[1] == 0xAB, "High byte second (LE)");
    CHECK(styx_get16(buf) == 0xABCD, "Get matches put");
    PASS();
}

static void test_put_get_32(void) {
    TEST("styx_put32/styx_get32 round-trip");
    uint8_t buf[4];
    styx_put32(buf, 0xDEADBEEF);
    CHECK(styx_get32(buf) == 0xDEADBEEF, "32-bit round-trip");
    PASS();
}

static void test_put_get_64(void) {
    TEST("styx_put64/styx_get64 round-trip");
    uint8_t buf[8];
    uint64_t val = 0x1234567890ABCDEFULL;
    styx_put64(buf, val);
    CHECK(styx_get64(buf) == val, "64-bit round-trip");
    PASS();
}

/* -- Message Name Lookup ------------------------------------------ */

static void test_msg_names(void) {
    TEST("styx_msg_name returns correct names");
    CHECK(strcmp(styx_msg_name(100), "Tversion") == 0, "100=Tversion");
    CHECK(strcmp(styx_msg_name(101), "Rversion") == 0, "101=Rversion");
    CHECK(strcmp(styx_msg_name(110), "Twalk") == 0, "110=Twalk");
    CHECK(strcmp(styx_msg_name(117), "Rread") == 0, "117=Rread");
    CHECK(strcmp(styx_msg_name(99), "Unknown") == 0, "99=Unknown");
    CHECK(strcmp(styx_msg_name(128), "Unknown") == 0, "128=Unknown");
    PASS();
}

/* -- String Helpers ----------------------------------------------- */

static void test_put_get_str(void) {
    TEST("styx_putstr/styx_getstr round-trip");
    uint8_t buf[64];
    char out[64] = {0};
    const char *hello = "/wubu/apps/editor";
    
    int n = styx_putstr(buf, hello);
    CHECK(n > 0, "Put should return > 0");
    
    const uint8_t *p = styx_getstr(buf, out, sizeof(out));
    CHECK(p != NULL, "Get should return pointer");
    CHECK(strcmp(out, hello) == 0, "Strings should match");
    PASS();
}

static void test_string_empty(void) {
    TEST("Empty string put/get");
    uint8_t buf[64];
    char out[64] = "garbage";
    
    int n = styx_putstr(buf, "");
    CHECK(n == 2, "Empty string puts 2 bytes (length=0)");
    CHECK(buf[0] == 0 && buf[1] == 0, "Length should be 0");
    
    styx_getstr(buf, out, sizeof(out));
    CHECK(strcmp(out, "") == 0, "Empty string round-trip");
    PASS();
}

static void test_string_truncation(void) {
    TEST("String truncation on oversize");
    uint8_t buf[64];
    char out[8] = {0};
    
    char longstr[300];
    memset(longstr, 'A', 299);
    longstr[299] = '\0';
    
    int n = styx_putstr(buf, longstr);
    CHECK(n <= STYX_MAX_FNAME + 2, "String should be capped");
    
    styx_getstr(buf, out, sizeof(out));
    CHECK(strlen(out) < 8, "Output should be truncated to buffer size");
    PASS();
}

/* -- Formatted Output Test (Styled like Inferno's /dev/cons) ---- */

static void test_walk_parse(void) {
    TEST("Parse Twalk with 2 elements");
    setup();
    uint32_t fid = 0, newfid = 0;
    int nwname = 0;
    char wnames[16][STYX_MAX_FNAME];
    
    const char *paths[] = {"wubu", "apps"};
    styx_build_twalk(g_inbuf, &g_inlen, 5, 1, 2, paths, 2);
    int ret = styx_parse_walk(g_inbuf, g_inlen, &fid, &newfid,
                               wnames, &nwname);
    CHECK(ret == 0, "Parse should succeed");
    CHECK(fid == 1, "fid should be 1");
    CHECK(newfid == 2, "newfid should be 2");
    CHECK(nwname == 2, "nwname should be 2");
    CHECK(strcmp(wnames[0], "wubu") == 0, "First element: wubu");
    CHECK(strcmp(wnames[1], "apps") == 0, "Second element: apps");
    PASS();
}

/* -- Main ---------------------------------------------------------- */

int main(void) {
    printf("+==================================================+\n");
    printf("|  WuBuOS Styx/9P2000 Protocol Test Suite         |\n");
    printf("+==================================================+\n\n");
    
    /* Tversion/Rversion */
    test_version_negotiate();
    test_version_bad_protocol();
    test_version_parse_response();
    
    /* Tattach/Rattach */
    test_attach_no_auth();
    test_attach_parse();
    
    /* Twalk/Rwalk */
    test_walk_zero_elements();
    test_walk_parse();
    
    /* Topen/Ropen */
    test_open_file();
    test_open_parse();
    
    /* Tread/Rread */
    test_read_empty();
    test_read_parse();
    
    /* Twrite/Rwrite */
    test_write_parse();
    
    /* Tclunk/Rclunk */
    test_clunk_fid();
    test_clunk_parse();
    
    /* Tstat/Rstat */
    test_stat_fid();
    test_stat_parse();
    
    /* Edge cases */
    test_bad_fid_walk();
    test_bad_fid_open();
    test_bad_fid_read();
    test_bad_fid_stat();
    test_truncated_message();
    test_unknown_message_type();
    
    /* Encoding */
    test_put_get_16();
    test_put_get_32();
    test_put_get_64();
    test_put_get_str();
    test_string_empty();
    test_string_truncation();
    
    /* Message names */
    test_msg_names();
    
    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n",
           g_passed, g_tests, g_failed);
    printf("==================================================\n");
    return g_failed > 0 ? 1 : 0;
}
