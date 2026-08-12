/*
 * wubu_nvidia_fermi.h -- kernel-owned NVIDIA Fermi legacy routing.
 */
#ifndef WUBU_NVIDIA_FERMI_H
#define WUBU_NVIDIA_FERMI_H

#include <stddef.h>

void wubu_nvidia_fermi_probe(void);
int  wubu_nvidia_fermi_present(void);
int  wubu_nvidia_fermi_needs_legacy(int fermi);
int  wubu_nvidia_fermi_eol_status(void);
void wubu_nvidia_fermi_summary(char *out, size_t cap);

#endif
