/*
 * wubu_drv_intel.h -- the Intel laptop platform driver.
 */
#ifndef WUBU_DRV_INTEL_H
#define WUBU_DRV_INTEL_H

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_intel_platform;

/* the state */
int wubu_intel_present(void);
int wubu_intel_dptf_ok(void);
int wubu_intel_dptf_zones(void);

#endif
