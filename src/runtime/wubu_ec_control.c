/*
 * wubu_ec_control.c -- the HANDHELD EC (fan/thermal/power) controller.
 *
 * The Steam Deck (and the whole handheld class — Aya Neo, Ayaneo,
 * etc.) keeps the fan + thermal policy in an embedded controller
 * (EC) reachable over a register file. Valve's SteamOS uses
 * fancontrol + the EC's PWM registers; mainline Linux models the same
 * class in drivers/platform/x86/ayaneo-ec.c (stolen here):
 *
 *   fan speed : 16-bit, two registers (low + high byte)
 *   PWM duty  : 0-100 percent, stored as 0-255 (percent * 2.55)
 *   PWM mode  : MANUAL (the host sets the duty) vs AUTO (the EC
 *               runs its own thermal curve)
 *
 * WuBuOS's EC controller:
 *   wubu_ec_init()           — probe the EC register file
 *   wubu_ec_read_fan_rpm()   — the current fan speed
 *   wubu_ec_set_pwm()        — manual duty (0-100)
 *   wubu_ec_set_mode()       — MANUAL or AUTO
 *   wubu_ec_get_temp()       — the EC thermal reading (if exposed)
 *
 * The register file is a PLUGGABLE ops table (like the /n control
 * plane pattern): the real Deck EC (i2c/io-port) and the test fake
 * both provide read8/write8.
 *
 * C11, self-contained.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* the register map (matches ayaneo-ec.c + the Deck's EC) */
enum {
    EC_REG_FAN_SPEED     = 0x00,   /* 16-bit fan RPM (low, high) */
    EC_REG_PWM           = 0x02,   /* the duty 0-255 */
    EC_REG_PWM_MODE      = 0x03,   /* 1 = manual, 2 = auto */
    EC_REG_TEMP          = 0x04,   /* the EC thermal reading (optional) */
};

#define EC_MODE_MANUAL 1
#define EC_MODE_AUTO   2

/* the EC backend (pluggable for the tests) */
typedef struct {
    uint8_t (*read8)(void *ctx, uint8_t reg);
    void (*write8)(void *ctx, uint8_t reg, uint8_t val);
    void *ctx;
} wubu_ec_ops_t;

typedef struct {
    int   probed;
    wubu_ec_ops_t ops;
    int   has_fan_control;   /* the EC exposes the PWM registers */
    int   fan_rpm;
    int   pwm;               /* 0-100 */
    int   mode;              /* EC_MODE_MANUAL / EC_MODE_AUTO */
    int   temp_c;            /* -1 if not exposed */
} wubu_ec_t;

static wubu_ec_t g_ec;

/* EC1: init + probe the EC. */
void wubu_ec_init(wubu_ec_ops_t *ops)
{
    memset(&g_ec, 0, sizeof(g_ec));
    if (ops) g_ec.ops = *ops;
    g_ec.probed = 1;
    g_ec.temp_c = -1;
    /* probe: can we read the fan register? The fan control exists iff
     * the PWM-mode register holds a valid mode (1 = manual, 2 = auto);
     * an absent EC reads 0xFF everywhere. */
    if (g_ec.ops.read8) {
        uint8_t lo = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_FAN_SPEED);
        uint8_t hi = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_FAN_SPEED + 1);
        g_ec.fan_rpm = (hi << 8) | lo;
        uint8_t mode = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_PWM_MODE);
        if (mode == EC_MODE_MANUAL || mode == EC_MODE_AUTO) {
            uint8_t pwm = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_PWM);
            g_ec.pwm = (pwm * 100 + 127) / 255;
            g_ec.mode = (mode == EC_MODE_MANUAL) ? EC_MODE_MANUAL : EC_MODE_AUTO;
            uint8_t t = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_TEMP);
            if (t != 0xFF) g_ec.temp_c = t;
            g_ec.has_fan_control = 1;
        }
    }
}

/* EC2: the current fan speed (RPM). */
int wubu_ec_fan_rpm(void)
{
    if (!g_ec.probed || !g_ec.ops.read8) return 0;
    uint8_t lo = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_FAN_SPEED);
    uint8_t hi = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_FAN_SPEED + 1);
    g_ec.fan_rpm = (hi << 8) | lo;
    return g_ec.fan_rpm;
}

/* EC3: set the PWM duty (0-100). Returns 0 on success. */
int wubu_ec_set_pwm(int percent)
{
    if (!g_ec.probed || !g_ec.ops.write8 || !g_ec.has_fan_control)
        return -1;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint8_t v = (uint8_t)((percent * 255 + 50) / 100);
    g_ec.ops.write8(g_ec.ops.ctx, EC_REG_PWM, v);
    g_ec.pwm = percent;
    return 0;
}

/* EC4: set the PWM mode (1 = manual, 2 = auto). */
int wubu_ec_set_mode(int mode)
{
    if (!g_ec.probed || !g_ec.ops.write8 || !g_ec.has_fan_control)
        return -1;
    if (mode != EC_MODE_MANUAL && mode != EC_MODE_AUTO)
        return -1;
    g_ec.ops.write8(g_ec.ops.ctx, EC_REG_PWM_MODE, (uint8_t)mode);
    g_ec.mode = mode;
    return 0;
}

/* EC5: the thermal reading (-1 = not exposed). */
int wubu_ec_temp(void)
{
    if (!g_ec.probed || !g_ec.ops.read8) return -1;
    uint8_t t = g_ec.ops.read8(g_ec.ops.ctx, EC_REG_TEMP);
    if (t == 0xFF) return -1;
    g_ec.temp_c = t;
    return g_ec.temp_c;
}

/* EC6: the test hooks */
typedef struct {
    int probed;
    int has_fan_control;
    int fan_rpm;
    int pwm;
    int mode;
    int temp_c;
} wubu_ec_view_t;

int wubu_ec_get(wubu_ec_view_t *out)
{
    if (!out) return -1;
    out->probed = g_ec.probed;
    out->has_fan_control = g_ec.has_fan_control;
    out->fan_rpm = g_ec.fan_rpm;
    out->pwm = g_ec.pwm;
    out->mode = g_ec.mode;
    out->temp_c = g_ec.temp_c;
    return 0;
}
