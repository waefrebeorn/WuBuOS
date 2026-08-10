/*
 * wubu_ec_control.h -- the handheld EC (fan/thermal/power) controller.
 */
#ifndef WUBU_EC_CONTROL_H
#define WUBU_EC_CONTROL_H

#include <stdint.h>

/* the EC backend ops (the tests provide a fake register file) */
typedef struct {
    uint8_t (*read8)(void *ctx, uint8_t reg);
    void (*write8)(void *ctx, uint8_t reg, uint8_t val);
    void *ctx;
} wubu_ec_ops_t;

/* the PWM modes */
enum { WUBU_EC_MODE_MANUAL = 1, WUBU_EC_MODE_AUTO = 2 };

/* EC1: init + probe the EC register file. */
void wubu_ec_init(wubu_ec_ops_t *ops);

/* EC2: the current fan speed (RPM). */
int wubu_ec_fan_rpm(void);

/* EC3: set the PWM duty (0-100). Returns 0 on success. */
int wubu_ec_set_pwm(int percent);

/* EC4: set the PWM mode (manual/auto). */
int wubu_ec_set_mode(int mode);

/* EC5: the thermal reading (-1 = not exposed). */
int wubu_ec_temp(void);

/* EC6: the test hooks. */
typedef struct {
    int probed;
    int has_fan_control;
    int fan_rpm;
    int pwm;
    int mode;
    int temp_c;
} wubu_ec_view_t;
int wubu_ec_get(wubu_ec_view_t *out);

#endif
