/*
 * wubu_ima.h -- kernel-owned IMA/EVM measured boot routing.
 */
#ifndef WUBU_IMA_H
#define WUBU_IMA_H

#include <stddef.h>

/* W1: probe the IMA/EVM topology. */
void wubu_ima_probe(void);

/* W2: accessors */
int  wubu_ima_present(void);
int  wubu_ima_evm(void);
int  wubu_ima_measure(void);
int  wubu_ima_appraise(void);
int  wubu_ima_pcr(void);
const char *wubu_ima_driver(void);

/* W3: IMA/EVM routing. */
const char *wubu_ima_mode_for(const char *mode);
const char *wubu_ima_policy_for(const char *policy);

/* W4: summary fragment. */
int wubu_ima_summary(char *out, size_t cap);

#endif /* WUBU_IMA_H */
