/*
 * wubu_raid.h -- kernel-owned RAID/SAS storage driver routing.
 */
#ifndef WUBU_RAID_H
#define WUBU_RAID_H

#include <stddef.h>

/* W1: probe the RAID topology. */
void wubu_raid_probe(void);

/* W2: accessors */
int  wubu_raid_present(void);
int  wubu_raid_has_sas(void);
int  wubu_raid_has_md(void);
const char *wubu_raid_driver(void);
const char *wubu_raid_name(void);

/* W3: RAID controller driver routing. */
const char *wubu_raid_controller_driver(const char *vendor);

/* W4: summary fragment. */
int wubu_raid_summary(char *out, size_t cap);

#endif /* WUBU_RAID_H */
