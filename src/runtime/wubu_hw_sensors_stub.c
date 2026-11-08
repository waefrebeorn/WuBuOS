/*
 * wubu_hw_sensors_stub.c -- hosted no-hardware shim for wubu_world.c sensor refs.
 *
 * wubu_world.c (kernel AGI world-model) reads GPU/battery/thermal state via
 * wubu_drv_gpu.c / wubu_drv_battery.c / wubu_drv_thermal.c -- kernel-mode
 * drivers that manage real device MMIO. Hosted runtime tests (test_era_apps,
 * etc.) link wubu_world but must not pull in kernel device code. This stub
 * provides the sensor getters wubu_world actually calls with safe
 * "no hardware" values, mirroring the established wubu_hw_stub.c isolation
 * pattern. Same signatures as the kernel drivers so the link binds cleanly.
 *
 * C11, self-contained.
 */
#include <stdint.h>

int wubu_gpu_present(void)       { return 0; }
int wubu_gpu_present_wsl(void)   { return 0; }
int wubu_gpu_connector(void)     { return 0; }
int wubu_gpu_width(void)         { return 0; }
int wubu_gpu_height(void)        { return 0; }
uint64_t wubu_gpu_vram_mb(void)  { return 0; }

int wubu_battery_present(void)   { return 0; }
int wubu_battery_charging(void)  { return 0; }
int wubu_battery_percent(void)   { return 0; }

int wubu_thermal_temp(int zone)          { (void)zone; return 0; }
int wubu_thermal_fan_duty(void)          { return 0; }
int wubu_thermal_throttled(void)         { return 0; }
