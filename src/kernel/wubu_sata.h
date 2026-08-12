/*
 * wubu_sata.h -- kernel-owned advanced SATA/NCQ driver routing.
 */
#ifndef WUBU_SATA_H
#define WUBU_SATA_H

#include <stddef.h>

/* W1: probe the SATA topology. */
void wubu_sata_probe(void);

/* W2: accessors */
int  wubu_sata_present(void);
int  wubu_sata_has_ahci(void);
int  wubu_sata_has_ncq(void);
int  wubu_sata_has_hotplug(void);
int  wubu_sata_has_pmp(void);
int  wubu_sata_has_smart(void);
const char *wubu_sata_driver(void);

/* W3: SATA controller driver routing. */
const char *wubu_sata_controller_driver(const char *ctrl);

/* W4: summary fragment. */
int wubu_sata_summary(char *out, size_t cap);

#endif /* WUBU_SATA_H */
