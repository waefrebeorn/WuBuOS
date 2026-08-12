/*
 * wubu_qos.h -- kernel-owned Ethernet switch QoS/ACL routing.
 */
#ifndef WUBU_QOS_H
#define WUBU_QOS_H

#include <stddef.h>

/* W1: probe the QoS/ACL topology. */
void wubu_qos_probe(void);

/* W2: accessors */
int  wubu_qos_tc(void);
int  wubu_qos_offload(void);
int  wubu_qos_flower(void);
int  wubu_qos_shaping(void);
int  wubu_qos_ecn(void);
const char *wubu_qos_driver(void);

/* W3: QoS driver routing. */
const char *wubu_qos_driver_for(const char *hw);

/* W4: summary fragment. */
int wubu_qos_summary(char *out, size_t cap);

#endif /* WUBU_QOS_H */
