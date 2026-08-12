/*
 * wubu_nicoffload.h -- kernel-owned NIC offload + multi-queue routing.
 */
#ifndef WUBU_NICOFFLOAD_H
#define WUBU_NICOFFLOAD_H

#include <stddef.h>

/* W1: probe the NIC offload topology. */
void wubu_nicoffload_probe(void);

/* W2: accessors */
int  wubu_nicoffload_present(void);
int  wubu_nicoffload_tso(void);
int  wubu_nicoffload_gro(void);
int  wubu_nicoffload_rss(void);
int  wubu_nicoffload_multi_queue(void);
int  wubu_nicoffload_queues(void);
const char *wubu_nicoffload_driver(void);

/* W3: offload driver routing. */
const char *wubu_nicoffload_driver_for(const char *nic);

/* W4: summary fragment. */
int wubu_nicoffload_summary(char *out, size_t cap);

#endif /* WUBU_NICOFFLOAD_H */
