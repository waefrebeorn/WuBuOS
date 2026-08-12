/*
 * wubu_mem.h -- kernel-owned memory/ECC driver routing.
 */
#ifndef WUBU_MEM_H
#define WUBU_MEM_H

#include <stddef.h>

/* W1: probe the memory/ECC topology. */
void wubu_mem_probe(void);

/* W2: accessors */
int  wubu_mem_has_edac(void);
int  wubu_mem_has_ecc(void);
int  wubu_mem_has_spd(void);
long wubu_mem_ce_count(void);   /* corrected ECC errors */
long wubu_mem_ue_count(void);   /* uncorrected ECC errors */
const char *wubu_mem_edac_driver(void);

/* W3: EDAC driver routing per memory controller. */
const char *wubu_mem_edac_route(const char *controller);

/* W4: summary fragment. */
int wubu_mem_summary(char *out, size_t cap);

#endif /* WUBU_MEM_H */
