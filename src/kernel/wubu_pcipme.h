/*
 * wubu_pcipme.h -- kernel-owned PCI PME routing.
 */
#ifndef WUBU_PCIPME_H
#define WUBU_PCIPME_H

#include <stddef.h>

void wubu_pcipme_probe(void);
int  wubu_pcipme_present(void);
int  wubu_pcipme_pmcsr(void);
int  wubu_pcipme_acpi(void);
int  wubu_pcipme_wake(void);
int  wubu_pcipme_pmeint(void);
const char *wubu_pcipme_driver(void);
const char *wubu_pcipme_state_for(const char *s);
const char *wubu_pcipme_event_for(const char *e);
int wubu_pcipme_summary(char *out, size_t cap);

#endif
