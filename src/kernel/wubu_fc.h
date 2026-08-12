/*
 * wubu_fc.h -- kernel-owned ethernet flow control (pause) routing.
 */
#ifndef WUBU_FC_H
#define WUBU_FC_H

#include <stddef.h>

/* W1: probe the FC topology. */
void wubu_fc_probe(void);

/* W2: accessors */
int  wubu_fc_supported(void);
int  wubu_fc_pause(void);
int  wubu_fc_pfc(void);
int  wubu_fc_autoneg(void);
int  wubu_fc_ethtool(void);
const char *wubu_fc_driver(void);

/* W3: FC routing. */
const char *wubu_fc_mode_for(const char *mode);
const char *wubu_fc_autoneg_for(const char *mode);

/* W4: summary fragment. */
int wubu_fc_summary(char *out, size_t cap);

#endif /* WUBU_FC_H */
