/*
 * wubu_multigig.h -- kernel-owned Ethernet multi-gig PHY routing.
 */
#ifndef WUBU_MULTIGIG_H
#define WUBU_MULTIGIG_H

#include <stddef.h>

/* W1: probe the multi-gig topology. */
void wubu_multigig_probe(void);

/* W2: accessors */
int  wubu_multigig_present(void);
int  wubu_multigig_2g5(void);
int  wubu_multigig_5g(void);
int  wubu_multigig_10g(void);
const char *wubu_multigig_driver(void);
const char *wubu_multigig_name(void);

/* W3: multi-gig driver routing. */
const char *wubu_multigig_driver_for(const char *phy);

/* W4: summary fragment. */
int wubu_multigig_summary(char *out, size_t cap);

#endif /* WUBU_MULTIGIG_H */
