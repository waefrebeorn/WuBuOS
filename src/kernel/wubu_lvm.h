/*
 * wubu_lvm.h -- kernel-owned storage LVM routing.
 */
#ifndef WUBU_LVM_H
#define WUBU_LVM_H

#include <stddef.h>

void wubu_lvm_probe(void);
int  wubu_lvm_present(void);
const char *wubu_lvm_uuid_for(const char *dev);
int  wubu_lvm_health(int size_mb, int used_mb);
void wubu_lvm_summary(char *out, size_t cap);

#endif
