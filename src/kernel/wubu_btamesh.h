/*
 * wubu_btamesh.h -- kernel-owned Bluetooth mesh routing.
 */
#ifndef WUBU_BTAMESH_H
#define WUBU_BTAMESH_H

#include <stddef.h>

void wubu_btamesh_probe(void);
int  wubu_btamesh_present(void);
int  wubu_btamesh_hops(int ttl);
int  wubu_btamesh_is_relay(int role);
void wubu_btamesh_summary(char *out, size_t cap);

#endif
