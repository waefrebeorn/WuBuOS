/*
 * wubu_gpushader.h -- kernel-owned GPU shader model routing.
 */
#ifndef WUBU_GPUSHADER_H
#define WUBU_GPUSHADER_H

#include <stddef.h>

void wubu_gpushader_probe(void);
int  wubu_gpushader_present(void);
int  wubu_gpushader_model_int(int major, int minor);
const char *wubu_gpushader_model_str(int level);
void wubu_gpushader_summary(char *out, size_t cap);

#endif
