/*
 * wubu_ns_ec.c -- the /n/ec control subtree (the handheld EC over the
 * Styx/9P namespace).
 *
 * The OS-source-steal thesis: every other OS reaches the EC with a
 * DIFFERENT mechanism (fancontrol's config, hwmon sysfs, i8kutils).
 * WuBuOS expresses ALL of it through ONE filesystem:
 *
 *   /n/ec/fan    -> the current fan RPM
 *   /n/ec/pwm    -> the manual duty 0-100 (write: set it)
 *   /n/ec/mode   -> 1 manual / 2 auto (write: set it)
 *   /n/ec/temp   -> the EC thermal reading
 *   /n/ec/status -> a one-line summary
 *
 * Each file wraps the REAL wubu_ec_control API (no reimplementation,
 * no new daemon) via ns_mkdir/ns_write from wubu_ns_fs.c.
 */
#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ec_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* publish the /n/ec tree (called after wubu_ns_bridge_create + the EC
 * is probed). Returns 0 on success. */
int wubu_ns_publish_ec(void)
{
    char sub[128];

    if (ns_mkdir("ec") != 0) return -1;

    /* fan */
    snprintf(sub, sizeof(sub), "ec/fan");
    if (ns_write(sub, "0\n") != 0) return -1;
    /* pwm + mode + temp + status (initial values) */
    wubu_ec_view_t v;
    wubu_ec_get(&v);
    snprintf(sub, sizeof(sub), "ec/pwm");
    char buf[64];
    snprintf(buf, sizeof(buf), "%d\n", v.pwm);
    if (ns_write(sub, buf) != 0) return -1;
    snprintf(sub, sizeof(sub), "ec/mode");
    snprintf(buf, sizeof(buf), "%d\n", v.mode);
    if (ns_write(sub, buf) != 0) return -1;
    snprintf(sub, sizeof(sub), "ec/temp");
    snprintf(buf, sizeof(buf), "%d\n", v.temp_c);
    if (ns_write(sub, buf) != 0) return -1;
    snprintf(sub, sizeof(sub), "ec/status");
    snprintf(buf, sizeof(buf), "fan %d rpm, pwm %d%%, mode %s, %dC\n",
             v.fan_rpm, v.pwm,
             v.mode == WUBU_EC_MODE_MANUAL ? "manual" : "auto", v.temp_c);
    if (ns_write(sub, buf) != 0) return -1;

    return 0;
}

/* the /n/ec control ops: `echo 30 > /n/ec/pwm` */

/* set the manual fan duty (0-100) + refresh /n/ec/pwm */
int wubu_ns_ec_set_pwm(int percent)
{
    if (wubu_ec_set_pwm(percent) != 0) return -1;
    wubu_ec_view_t v;
    wubu_ec_get(&v);
    char sub[128], buf[64];
    snprintf(sub, sizeof(sub), "ec/pwm");
    snprintf(buf, sizeof(buf), "%d\n", v.pwm);
    return ns_write(sub, buf);
}

/* set the fan mode (1 manual / 2 auto) + refresh /n/ec/mode */
int wubu_ns_ec_set_mode(int mode)
{
    if (wubu_ec_set_mode(mode) != 0) return -1;
    wubu_ec_view_t v;
    wubu_ec_get(&v);
    char sub[128], buf[64];
    snprintf(sub, sizeof(sub), "ec/mode");
    snprintf(buf, sizeof(buf), "%d\n", v.mode);
    return ns_write(sub, buf);
}

/* refresh /n/ec/fan + /n/ec/temp (the EC values change on their own) */
int wubu_ns_ec_refresh(void)
{
    char sub[128], buf[64];
    snprintf(sub, sizeof(sub), "ec/fan");
    snprintf(buf, sizeof(buf), "%d\n", wubu_ec_fan_rpm());
    if (ns_write(sub, buf) != 0) return -1;
    snprintf(sub, sizeof(sub), "ec/temp");
    snprintf(buf, sizeof(buf), "%d\n", wubu_ec_temp());
    return ns_write(sub, buf);
}
