/*
 * wubu_adreno700.h -- kernel-owned Qualcomm Adreno 700 GPU routing.
 */
#ifndef WUBU_ADRENO700_H
#define WUBU_ADRENO700_H

#include <stddef.h>

void wubu_adreno700_probe(void);
int  wubu_adreno700_present(void);
int  wubu_adreno700_uses_freedreno(int freedreno_available);
int  wubu_adreno700_gen(int gen);
void wubu_adreno700_summary(char *out, size_t cap);

#endif
