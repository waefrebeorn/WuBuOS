/*
 * wubu_ec_control_test.c -- the handheld EC controller test.
 *
 * A fake EC register file (a byte array + the read8/write8 ops) proves
 * the ayaneo-ec.c-stolen semantics:
 *   1. init probes + decodes the 16-bit fan speed + the pwm + mode
 *   2. set_pwm writes the percent*2.55 encoding back
 *   3. set_mode switches manual/auto
 *   4. the temp register is read
 */
#include "wubu_ec_control.h"
#include "wubu_test.h"
#include <stdio.h>
#include <string.h>

/* FAIL: use wubu_test.h */

/* the fake EC register file */
static uint8_t g_regs[16];

static uint8_t fake_read(void *ctx, uint8_t reg)
{
    (void)ctx;
    return g_regs[reg & 0x0F];
}
static void fake_write(void *ctx, uint8_t reg, uint8_t val)
{
    (void)ctx;
    g_regs[reg & 0x0F] = val;
}

int main(void)
{
    printf("=== wubu_ec_control_test (the handheld EC) ===\n");

    /* a Deck-like EC: fan at 3800 RPM, pwm 50%, auto mode, 61C */
    memset(g_regs, 0, sizeof(g_regs));
    g_regs[0x00] = 0xD8; g_regs[0x01] = 0x0E;   /* 0x0ED8 = 3800 */
    g_regs[0x02] = 127;                          /* pwm 127/255 = 50% */
    g_regs[0x03] = 2;                            /* auto */
    g_regs[0x04] = 61;                           /* 61C */

    wubu_ec_ops_t ops = { fake_read, fake_write, NULL };
    wubu_ec_init(&ops);

    wubu_ec_view_t v;
    wubu_ec_get(&v);
    if (!v.probed || !v.has_fan_control) FAIL("not probed");
    if (v.fan_rpm != 3800) FAIL("fan = %d, want 3800", v.fan_rpm);
    if (v.pwm != 50) FAIL("pwm = %d, want 50", v.pwm);
    if (v.mode != WUBU_EC_MODE_AUTO) FAIL("mode = %d, want auto", v.mode);
    if (v.temp_c != 61) FAIL("temp = %d, want 61", v.temp_c);
    printf("  PASS: init probes the EC (fan %d, pwm %d%%, %dC, mode %d)\n",
           v.fan_rpm, v.pwm, v.temp_c, v.mode);

    /* set_pwm: 30% -> the register holds round(30*255/100) = 77, and
     * the decoder reads it back as 30 */
    if (wubu_ec_set_pwm(30) != 0) FAIL("set_pwm");
    if (g_regs[0x02] != 77) FAIL("pwm reg = %d, want 77", g_regs[0x02]);
    wubu_ec_get(&v);
    if (v.pwm != 30) FAIL("pwm view = %d, want 30", v.pwm);
    printf("  PASS: set_pwm writes the percent*2.55 encoding\n");

    /* set_mode: manual */
    if (wubu_ec_set_mode(WUBU_EC_MODE_MANUAL) != 0) FAIL("set_mode");
    if (g_regs[0x03] != 1) FAIL("mode reg = %d, want 1 (manual)", g_regs[0x03]);
    if (wubu_ec_set_mode(99) == 0) FAIL("bad mode accepted");
    printf("  PASS: set_mode switches manual/auto + rejects bad modes\n");

    /* the live fan read refreshes from the registers */
    g_regs[0x00] = 0x00; g_regs[0x01] = 0x10;   /* 0x1000 = 4096 */
    if (wubu_ec_fan_rpm() != 4096) FAIL("fan refresh = %d, want 4096", wubu_ec_fan_rpm());
    printf("  PASS: fan_rpm refreshes from the EC\n");

    /* a bare EC (no fan control) */
    memset(g_regs, 0xFF, sizeof(g_regs));
    g_regs[0x04] = 0xFF;   /* no temp */
    wubu_ec_init(&ops);
    wubu_ec_get(&v);
    if (v.has_fan_control) FAIL("bare EC reported fan control");
    if (wubu_ec_set_pwm(50) == 0) FAIL("pwm on a bare EC accepted");
    if (wubu_ec_temp() != -1) FAIL("temp on a bare EC != -1");
    printf("  PASS: a bare EC is handled cleanly\n");

    printf("=== ALL EC-CONTROL TESTS PASSED (the handheld EC) ===\n");
    return 0;
}
