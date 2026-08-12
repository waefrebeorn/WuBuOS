/*
 * wubu_phy.h -- kernel-owned Ethernet PHY/MDIO driver routing.
 */
#ifndef WUBU_PHY_H
#define WUBU_PHY_H

#include <stddef.h>

/* W1: probe the PHY topology. */
void wubu_phy_probe(void);

/* W2: accessors */
int  wubu_phy_present(void);
int  wubu_phy_has_mdio(void);
int  wubu_phy_link_up(void);
const char *wubu_phy_driver(void);
const char *wubu_phy_name(void);

/* W3: PHY driver routing. */
const char *wubu_phy_driver_for(const char *phy);

/* W4: summary fragment. */
int wubu_phy_summary(char *out, size_t cap);

#endif /* WUBU_PHY_H */
