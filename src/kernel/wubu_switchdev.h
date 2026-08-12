/*
 * wubu_switchdev.h -- kernel-owned network switch fabric routing.
 */
#ifndef WUBU_SWITCHDEV_H
#define WUBU_SWITCHDEV_H

#include <stddef.h>

/* W1: probe the switch topology. */
void wubu_switchdev_probe(void);

/* W2: accessors */
int  wubu_switchdev_present(void);
int  wubu_switchdev_has_dsa(void);
int  wubu_switchdev_has_asic(void);
const char *wubu_switchdev_driver(void);
const char *wubu_switchdev_name(void);

/* W3: switch driver routing. */
const char *wubu_switchdev_driver_for(const char *chip);

/* W4: summary fragment. */
int wubu_switchdev_summary(char *out, size_t cap);

#endif /* WUBU_SWITCHDEV_H */
