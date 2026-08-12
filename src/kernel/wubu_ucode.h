/*
 * wubu_ucode.h -- kernel-owned CPU microcode loading routing.
 */
#ifndef WUBU_UCODE_H
#define WUBU_UCODE_H

#include <stddef.h>

/* W1: probe the microcode topology. */
void wubu_ucode_probe(void);

/* W2: accessors */
int  wubu_ucode_intel(void);
int  wubu_ucode_amd(void);
int  wubu_ucode_early(void);
int  wubu_ucode_late(void);
int  wubu_ucode_loaded(void);
const char *wubu_ucode_driver(void);

/* W3: microcode loader routing. */
const char *wubu_ucode_loader_for(const char *cpu);
const char *wubu_ucode_loadpath_for(const char *mode);

/* W4: summary fragment. */
int wubu_ucode_summary(char *out, size_t cap);

#endif /* WUBU_UCODE_H */
