/*
 * wubu_tpm.h -- kernel-owned TPM 2.0 full-stack driver routing.
 */
#ifndef WUBU_TPM_H
#define WUBU_TPM_H

#include <stddef.h>

/* W1: probe the TPM topology. */
void wubu_tpm_probe(void);

/* W2: accessors */
int  wubu_tpm_present(void);
int  wubu_tpm_is_tpm2(void);
int  wubu_tpm_has_tss(void);
int  wubu_tpm_has_crb(void);
int  wubu_tpm_has_measured_boot(void);
const char *wubu_tpm_driver(void);

/* W3: TPM driver routing. */
const char *wubu_tpm_driver_for(const char *iface);

/* W4: summary fragment. */
int wubu_tpm_summary(char *out, size_t cap);

#endif /* WUBU_TPM_H */
