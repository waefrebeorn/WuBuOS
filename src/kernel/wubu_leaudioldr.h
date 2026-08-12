/*
 * wubu_leaudioldr.h -- kernel-owned Bluetooth LE Audio routing.
 */
#ifndef WUBU_LEAUDIOLDR_H
#define WUBU_LEAUDIOLDR_H

#include <stddef.h>

void wubu_leaudioldr_probe(void);
int  wubu_leaudioldr_present(void);
int  wubu_leaudioldr_samples(int frame_us);
int  wubu_leaudioldr_is_valid_frame(int samples);
void wubu_leaudioldr_summary(char *out, size_t cap);

#endif
