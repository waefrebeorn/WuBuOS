/*
 * wubu_ns_steaminput_test.c -- the /n/steaminput subtree test.
 *
 * Asserts the file<->API routing:
 *   1. publish creates /n/steaminput/map + status
 *   2. writing a real Deck report (hex) as /n/steaminput/report emits
 *      the mapped input (A -> Space) through the kernel queue
 *   3. a short/corrupt report is refused
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_steaminput.h"
#include "wubu_steaminput.h"
#include "input.h"
#include <stdio.h>
#include <string.h>

#define NSROOT "/tmp/ns_si_test"
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n')) out[--n] = '\0';
    return 1;
}

int main(void)
{
    printf("=== wubu_ns_steaminput_test (the /n/steaminput subtree) ===\n");
    system("rm -rf " NSROOT);
    *(const char **)&g_ns_root = NSROOT;

    wubu_si_init();

    if (wubu_ns_publish_steaminput() != 0) FAIL("publish");

    /* 1. the files exist */
    char p[512], buf[256];
    snprintf(p, sizeof(p), "%s/steaminput/map", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no map");
    if (!strstr(buf, "A=Space")) FAIL("map lacks A=Space");
    snprintf(p, sizeof(p), "%s/steaminput/status", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no status");
    printf("  PASS: publish creates /n/steaminput/map + status\n");

    /* 2. a real Deck report: A pressed (byte 8 = 0x80) */
    KeyEvent ev;
    while (input_key_poll(&ev)) {}
    char hex[256];
    memset(hex, '0', sizeof(hex) - 1);
    hex[0] = '8'; hex[1] = '0';          /* byte 8 = 0x80: A down */
    hex[sizeof(hex) - 1] = '\0';
    /* byte 8 is at hex offset 16 (2 chars per byte) */
    memset(hex, '0', 16);                /* bytes 0-7 = 0 */
    hex[16] = '8'; hex[17] = '0';        /* byte 8 = 0x80 */
    int n = wubu_ns_steaminput_report(hex);
    if (n < 1) FAIL("report emitted %d events", n);
    int got_a = 0;
    while (input_key_poll(&ev)) if (ev.scancode == 0x39) got_a = 1;
    if (!got_a) FAIL("deck report did not emit Space");
    printf("  PASS: echo deck-report > /n/steaminput/report emits input\n");

    /* 3. a short report is refused */
    if (wubu_ns_steaminput_report("80") >= 0) FAIL("short report accepted");
    if (wubu_ns_steaminput_report(NULL) >= 0) FAIL("null report accepted");
    printf("  PASS: a short report is refused\n");

    /* 4. the battery file + refresh */
    snprintf(p, sizeof(p), "%s/steaminput/battery", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no battery");
    uint8_t batt[15];
    memset(batt, 0, sizeof(batt));
    batt[12] = 0x3C; batt[13] = 0x0F;   /* 3900 mV */
    batt[14] = 78;
    wubu_si_parse_battery(batt, sizeof(batt));
    if (wubu_ns_steaminput_refresh_battery() != 0) FAIL("refresh battery");
    if (!read_file(p, buf, sizeof(buf))) FAIL("no battery after refresh");
    if (!strstr(buf, "3900")) FAIL("battery file lacks 3900 mV: '%s'", buf);
    printf("  PASS: the battery file refreshes\n");

    system("rm -rf " NSROOT);
    printf("=== ALL NS-STEAMINPUT TESTS PASSED ===\n");
    return 0;
}
