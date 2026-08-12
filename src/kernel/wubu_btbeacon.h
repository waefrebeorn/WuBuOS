/*
 * wubu_btbeacon.h -- kernel-owned Bluetooth beacon routing.
 */
#ifndef WUBU_BTBEACON_H
#define WUBU_BTBEACON_H

#include <stddef.h>

void wubu_btbeacon_probe(void);
int  wubu_btbeacon_present(void);
int  wubu_btbeacon_type(int rssi);
int  wubu_btbeacon_valid_uuid(const char *uuid_prefix);
void wubu_btbeacon_summary(char *out, size_t cap);

#endif
