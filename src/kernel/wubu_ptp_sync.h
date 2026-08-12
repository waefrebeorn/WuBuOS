/*
 * wubu_ptp_sync.h -- kernel-owned NIC PTP time sync routing.
 */
#ifndef WUBU_PTP_SYNC_H
#define WUBU_PTP_SYNC_H

#include <stddef.h>

/* W1: probe the PTP topology. */
void wubu_ptp_sync_probe(void);

/* W2: accessors */
int  wubu_ptp_sync_phc(void);
int  wubu_ptp_sync_ptp4l(void);
int  wubu_ptp_sync_phc2sys(void);
int  wubu_ptp_sync_hwts(void);
int  wubu_ptp_sync_synced(void);
const char *wubu_ptp_sync_driver(void);

/* W3: PTP routing. */
const char *wubu_ptp_sync_role_for(const char *role);
const char *wubu_ptp_sync_nic_for(const char *nic);

/* W4: summary fragment. */
int wubu_ptp_sync_summary(char *out, size_t cap);

#endif /* WUBU_PTP_SYNC_H */
