/*
 * wubu_raid5.h -- kernel-owned storage RAID5 routing.
 */
#ifndef WUBU_RAID5_H
#define WUBU_RAID5_H

#include <stddef.h>

void wubu_raid5_probe(void);
int  wubu_raid5_present(void);
int  wubu_raid5_stripe(void);
int  wubu_raid5_layout(void);
int  wubu_raid5_parity(void);
int  wubu_raid5_disks(void);
const char *wubu_raid5_driver(void);
const char *wubu_raid5_layout_for(const char *l);
const char *wubu_raid5_parity_for(const char *p);
int wubu_raid5_summary(char *out, size_t cap);

#endif
