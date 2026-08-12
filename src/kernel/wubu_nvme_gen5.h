/*
 * wubu_nvme_gen5.h -- kernel-owned NVMe Gen5 routing.
 */
#ifndef WUBU_NVME_GEN5_H
#define WUBU_NVME_GEN5_H

#include <stddef.h>

void wubu_nvme_gen5_probe(void);
int  wubu_nvme_gen5_present(void);
int  wubu_nvme_gen5_speed_gbps(int gen, int lanes);
int  wubu_nvme_gen5_is_fast(int speed_gbps);
void wubu_nvme_gen5_summary(char *out, size_t cap);

#endif
