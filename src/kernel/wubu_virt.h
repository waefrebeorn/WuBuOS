/*
 * wubu_virt.h -- kernel-owned virtualization driver routing.
 */
#ifndef WUBU_VIRT_H
#define WUBU_VIRT_H

#include <stddef.h>

/* W1: probe the hypervisor. */
void wubu_virt_probe(void);

/* W2: accessors */
int  wubu_virt_hypervisor(void);      /* 1=KVM 2=Hyper-V 3=VMware 4=Xen 5=VBox 6=Parallels 0=none */
int  wubu_virt_has_virtio(void);
const char *wubu_virt_pv_driver(void);       /* primary PV driver */
const char *wubu_virt_hypervisor_name(void); /* "KVM"|"Hyper-V"|... */

/* W3: full PV driver set per hypervisor. */
const char *wubu_virt_driver_set(int hyper);

/* W4: summary fragment. */
int wubu_virt_summary(char *out, size_t cap);

#endif /* WUBU_VIRT_H */
