/*
 * wubu_ptp.h -- kernel-owned Ethernet PTP/TSN + input haptics routing.
 */
#ifndef WUBU_PTP_H
#define WUBU_PTP_H

#include <stddef.h>

/* W1: probe the PTP/TSN + haptics topology. */
void wubu_ptp_probe(void);

/* W2: accessors */
int  wubu_ptp_present(void);
int  wubu_ptp_has_tsn(void);
int  wubu_ptp_has_haptic(void);
int  wubu_ptp_phc_clocks(void);
const char *wubu_ptp_driver(void);
const char *wubu_ptp_haptic_driver(void);

/* W3: driver routing. */
const char *wubu_ptp_driver_for(const char *nic);
const char *wubu_ptp_haptic_for(const char *dev);

/* W4: summary fragment. */
int wubu_ptp_summary(char *out, size_t cap);

#endif /* WUBU_PTP_H */
