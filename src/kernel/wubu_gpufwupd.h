/*
 * wubu_gpufwupd.h -- kernel-owned GPU firmware update routing.
 */
#ifndef WUBU_GPUFWUPD_H
#define WUBU_GPUFWUPD_H

#include <stddef.h>

void wubu_gpufwupd_probe(void);
int  wubu_gpufwupd_present(void);
int  wubu_gpufwupd_match(int current, int expected);
const char *wubu_gpufwupd_status(int code);
void wubu_gpufwupd_summary(char *out, size_t cap);

#endif
