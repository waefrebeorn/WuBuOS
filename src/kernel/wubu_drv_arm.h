/*
 * wubu_drv_arm.h -- the ARM platform drivers.
 */
#ifndef WUBU_DRV_ARM_H
#define WUBU_DRV_ARM_H

/* the ARM models */
enum {
    WUBU_ARM_RPI4 = 0,
    WUBU_ARM_RPI5,
    WUBU_ARM_SNAPDRAGON,
    WUBU_ARM_APPLE,
};

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_arm_platform;

/* A2: probe the platform blocks for a model. */
void wubu_arm_probe_blocks(int model);

/* the state */
int wubu_arm_present(void);
int wubu_arm_uart_ok(void);
int wubu_arm_timer_ok(void);
int wubu_arm_gic_ok(void);
int wubu_arm_gpio_ok(void);
int wubu_arm_model(void);
const char *wubu_arm_model_name(int m);

#endif
