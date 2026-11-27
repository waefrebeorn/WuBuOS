/*
 * wubu_timesync_test.c -- the clock correction, proven with canned packets.
 *
 * The three things that can be wrong here are all testable without a network:
 * the wire parse (short/echo/kiss-o'-death), the fixed-point conversion, and the
 * offset arithmetic. A wrong clock is a daily annoyance (#69), so the arithmetic
 * gets pinned exactly rather than approximately.
 */
#include "wubu_timesync.h"
#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(c, m) do { if (c) { printf("  [PASS] %s\n", m); g_pass++; } \
                         else { printf("  [FAIL] %s (%s:%d)\n", m, __FILE__, __LINE__); g_fail++; } } while (0)

#define NTP_DELTA 2208988800ULL
static void put64(unsigned char *p, uint64_t v) { for (int i = 7; i >= 0; i--) { p[i] = (unsigned char)(v & 0xFF); v >>= 8; } }

static void make_reply(unsigned char *p, int mode, int stratum, uint64_t t2, uint64_t t3) {
    memset(p, 0, 48);
    p[0] = (unsigned char)((0 << 6) | (3 << 3) | (mode & 7));   /* LI=0 VN=3 mode */
    p[1] = (unsigned char)stratum;
    put64(p + 32, t2);
    put64(p + 40, t3);
}

static void test_parse(void) {
    unsigned char pkt[48];
    WubuNtpResult r;
    const uint64_t unix_s = 1795000000ULL;
    const uint64_t t2 = ((unix_s + NTP_DELTA) << 32) | 0x80000000ULL;   /* .5 s */
    const uint64_t t3 = t2 + (2ULL << 32);                              /* +2 s */

    make_reply(pkt, 4, 2, t2, t3);
    CHECK(wubu_ntp_parse(pkt, 48, &r) == 0, "a valid server reply parses");
    CHECK(r.stratum == 2, "stratum is reported");
    CHECK(r.version == 3, "NTP version is reported");
    CHECK(r.server_recv_us == (int64_t)unix_s * 1000000LL + 500000,
          "T2 converts to unix microseconds exactly (no rounding drift)");
    CHECK(r.server_send_us == (int64_t)unix_s * 1000000LL + 2500000, "T3 likewise");

    make_reply(pkt, 4, 2, t2, t3);
    CHECK(wubu_ntp_parse(pkt, 47, &r) == -2, "a 47-byte packet is refused, not guessed");

    make_reply(pkt, 3, 2, t2, t3);                 /* mode 3: a client echo */
    CHECK(wubu_ntp_parse(pkt, 48, &r) == -3, "a non-reply mode is refused");

    make_reply(pkt, 4, 0, t2, t3);                 /* kiss-o'-death */
    memcpy(pkt + 12, "RATE", 4);
    CHECK(wubu_ntp_parse(pkt, 48, &r) == -4, "stratum 0 is a refusal");
    CHECK(strcmp(r.refusal, "RATE") == 0, "and the reason survives for the operator");

    make_reply(pkt, 4, 2, 1000, 2000);             /* before 1970 in NTP terms */
    CHECK(wubu_ntp_parse(pkt, 48, &r) == -5, "a pre-1970 timestamp is refused");
}

static void test_math(void) {
    /* Symmetric path: the server is 2 s ahead, round trip 1 ms. */
    int64_t t1 = 1000000, t2 = 3000500, t3 = 3000500, t4 = 1001000;
    CHECK(wubu_ntp_offset_us(t1, t2, t3, t4) == 2000000, "offset = server - client when symmetric");
    CHECK(wubu_ntp_delay_us(t1, t2, t3, t4) == 1000, "delay = round trip minus server processing");

    /* One-way asymmetry must not change the sign. */
    int64_t o = wubu_ntp_offset_us(0, 500000, 700000, 1000000);
    CHECK(o == 100000, "asymmetric path still yields the standard estimate");

    /* A zero offset is a real answer (clock already right). */
    CHECK(wubu_ntp_offset_us(10, 10, 10, 10) == 0, "a correct clock measures zero offset");
}

static void test_epoch_conversion(void) {
    /* unix 0 == NTP 2208988800 */
    CHECK(wubu_ntp_to_unix_us(NTP_DELTA << 32) == 0, "the epoch itself converts to 0");
    CHECK(wubu_ntp_to_unix_us((NTP_DELTA - 1) << 32) < 0, "an earlier time is refused");
    /* Half a second of fraction must be 500000 us, not 0 and not 1 s. */
    CHECK(wubu_ntp_to_unix_us((NTP_DELTA << 32) | 0x80000000ULL) == 500000,
          "the 32-bit fraction becomes microseconds");
}

int main(void) {
    printf("=== wubu_timesync_test ===\n");
    test_parse();
    test_math();
    test_epoch_conversion();
    printf("\n=== timesync: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
