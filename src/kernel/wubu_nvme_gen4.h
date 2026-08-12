/*
 * wubu_nvme_gen4.h -- kernel-owned NVMe Gen4 routing.
 */
#ifndef WUBU_NVME_GEN4_H
#define WUBU_NVME_GEN4_H

#include <stddef.h>

void wubu_nvme_gen4_probe(void);
int  wubu_nvme_gen4_present(void);
int  wubu_nvme_gen4_speed_gbps(int lanes);
int  wubu_nvme_gen4_is_fast(int speed_gbps);
void wubu_nvme_gen4_summary(char *out, size_t cap);

#endif
