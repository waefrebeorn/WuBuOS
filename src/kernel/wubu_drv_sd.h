/*
 * wubu_drv_sd.h -- the SD/MMC driver.
 */
#ifndef WUBU_DRV_SD_H
#define WUBU_DRV_SD_H

#include <stdint.h>

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_sd;

/* the test hooks */
void wubu_sd_set_mmio(volatile void *mmio);
void wubu_sd_set_card(int card_present, const uint8_t *cid, uint64_t capacity_mb);

/* the state */
int wubu_sd_present(void);
int wubu_sd_card_present(void);
uint64_t wubu_sd_capacity_mb(void);
const char *wubu_sd_model(void);

#endif
