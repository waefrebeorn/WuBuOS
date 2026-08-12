/*
 * wubu_gadget.h -- kernel-owned USB gadget + NVMe endurance routing.
 */
#ifndef WUBU_GADGET_H
#define WUBU_GADGET_H

#include <stddef.h>

/* W1: probe the gadget/endurance topology. */
void wubu_gadget_probe(void);

/* W2: accessors */
int  wubu_gadget_udc(void);
int  wubu_gadget_configfs(void);
int  wubu_gadget_active(void);
int  wubu_gadget_nvme(void);
int  wubu_gadget_smart(void);
const char *wubu_gadget_driver(void);

/* W3: routing. */
const char *wubu_gadget_function_for(const char *fn);
const char *wubu_gadget_nvme_for(const char *health);

/* W4: summary fragment. */
int wubu_gadget_summary(char *out, size_t cap);

#endif /* WUBU_GADGET_H */
