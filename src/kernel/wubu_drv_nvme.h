/*
 * wubu_drv_nvme.h -- the NVMe driver.
 */
#ifndef WUBU_DRV_NVME_H
#define WUBU_DRV_NVME_H

#include <stddef.h>
#include <stdint.h>

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_nvme;

/* NV2: set the MMIO window (the tests inject a fake controller). */
void wubu_nvme_set_mmio(volatile void *mmio, size_t len);

/* NV3: the controller is ready? */
int wubu_nvme_ready(void);

/* NV4: the namespace info. */
uint64_t wubu_nvme_nsze(void);
uint32_t wubu_nvme_nsid(void);
uint32_t wubu_nvme_block_size(void);

/* NV5: set the identify result (the tests). */
void wubu_nvme_set_identify(uint64_t nsze, uint32_t nsid, uint32_t blk);

/* NV6: the controller version string. */
const char *wubu_nvme_version(void);

#endif
