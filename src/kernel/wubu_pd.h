/*
 * wubu_pd.h -- kernel-owned USB PD + NIC flow steering routing.
 */
#ifndef WUBU_PD_H
#define WUBU_PD_H

#include <stddef.h>

/* W1: probe the USB-PD/flow-steering topology. */
void wubu_pd_probe(void);

/* W2: accessors */
int  wubu_pd_typec(void);
int  wubu_pd_supported(void);
int  wubu_pd_tcpm(void);
int  wubu_pd_rfs(void);
int  wubu_pd_arfs(void);
const char *wubu_pd_driver(void);

/* W3: routing. */
const char *wubu_pd_contract_for(const char *role);
const char *wubu_pd_flow_for(const char *nic);

/* W4: summary fragment. */
int wubu_pd_summary(char *out, size_t cap);

#endif /* WUBU_PD_H */
