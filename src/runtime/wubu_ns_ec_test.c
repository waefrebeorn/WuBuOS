/*
 * wubu_ns_ec_test.c -- the /n/ec subtree test.
 *
 * Asserts the file<->API routing over a REAL ns bridge root:
 *   1. publish creates the /n/ec files with the initial values
 *   2. wubu_ns_ec_set_pwm(30) writes the duty through the EC API AND
 *      refreshes /n/ec/pwm
 *   3. wubu_ns_ec_set_mode(1) switches manual + refreshes /n/ec/mode
 *   4. wubu_ns_ec_refresh() updates fan + temp
 */
#include "wubu_ns_bridge.h"
#include "wubu_test.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_ec.h"
#include "wubu_ec_control.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/ns_ec_test"
/* FAIL: use wubu_test.h */

/* the fake EC register file */
static uint8_t g_regs[16];
static uint8_t fake_read(void *ctx, uint8_t reg) { (void)ctx; return g_regs[reg & 0x0F]; }
static void fake_write(void *ctx, uint8_t reg, uint8_t val) { (void)ctx; g_regs[reg & 0x0F] = val; }

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

int main(void)
{
    printf("=== wubu_ns_ec_test (the /n/ec control subtree) ===\n");
    system("rm -rf " NSROOT);

    /* a Deck-like EC: fan 3800, pwm 50%, auto, 61C */
    memset(g_regs, 0, sizeof(g_regs));
    g_regs[0x00] = 0xD8; g_regs[0x01] = 0x0E;   /* 3800 */
    g_regs[0x02] = 127;
    g_regs[0x03] = 2;
    g_regs[0x04] = 61;
    wubu_ec_ops_t ops = { fake_read, fake_write, NULL };
    wubu_ec_init(&ops);

    /* the bridge root (skip the heavy wubu_ns_bridge_create: the fs
     * helpers only need g_ns_root) */
    *(const char **)&g_ns_root = NSROOT;
    if (wubu_ns_publish_ec() != 0) FAIL("publish ec");
    /* 1. the files exist with the initial values */
    char p[512], buf[128];
    snprintf(p, sizeof(p), "%s/ec/pwm", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no /n/ec/pwm");
    if (strcmp(buf, "50") != 0) FAIL("pwm = '%s', want 50", buf);
    snprintf(p, sizeof(p), "%s/ec/mode", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "2") != 0) FAIL("mode = '%s', want 2 (auto)", buf);
    snprintf(p, sizeof(p), "%s/ec/fan", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "0") != 0) FAIL("fan = '%s', want 0 (initial)", buf);
    printf("  PASS: publish creates /n/ec with the initial values\n");

    /* 2. set_pwm -> EC + the file */
    if (wubu_ns_ec_set_pwm(30) != 0) FAIL("ns set_pwm");
    if (g_regs[0x02] != 77) FAIL("EC pwm reg = %d, want 77", g_regs[0x02]);
    snprintf(p, sizeof(p), "%s/ec/pwm", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "30") != 0) FAIL("file pwm = '%s', want 30", buf);
    printf("  PASS: echo > /n/ec/pwm drives the EC + refreshes the file\n");

    /* 3. set_mode -> EC + the file */
    if (wubu_ns_ec_set_mode(WUBU_EC_MODE_MANUAL) != 0) FAIL("ns set_mode");
    if (g_regs[0x03] != 1) FAIL("EC mode reg = %d, want 1", g_regs[0x03]);
    snprintf(p, sizeof(p), "%s/ec/mode", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "1") != 0) FAIL("file mode = '%s', want 1", buf);
    printf("  PASS: echo > /n/ec/mode drives the EC + refreshes the file\n");

    /* 4. refresh updates fan + temp */
    g_regs[0x00] = 0x00; g_regs[0x01] = 0x10;   /* 4096 */
    g_regs[0x04] = 66;
    if (wubu_ns_ec_refresh() != 0) FAIL("refresh");
    snprintf(p, sizeof(p), "%s/ec/fan", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "4096") != 0) FAIL("fan = '%s', want 4096", buf);
    snprintf(p, sizeof(p), "%s/ec/temp", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "66") != 0) FAIL("temp = '%s', want 66", buf);
    printf("  PASS: refresh updates /n/ec/fan + /n/ec/temp\n");

    system("rm -rf " NSROOT);
    printf("=== ALL NS-EC TESTS PASSED (the /n/ec control subtree) ===\n");
    return 0;
}
