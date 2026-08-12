/*
 * wubu_probe.h -- the UNIFIED HARDWARE DISCOVERY dispatcher.
 *
 * One entry point discovers the ENTIRE machine (GPU, audio, storage,
 * network, input, power, virtual) and publishes the full matrix to KV-FS.
 * "We run everything and run on everything."
 */
#ifndef WUBU_PROBE_H
#define WUBU_PROBE_H

#include <stddef.h>

/* W1: discover everything. Call once at kernel init. */
void wubu_probe_all(void);

/* W2: build the machine-matrix string (human-readable). */
void wubu_probe_build_matrix(void);

/* W3: the driver-registry matrix fragment. */
const char *wubu_probe_drv_matrix(void);

/* W4: publish the matrix to KV-FS (/kv/world/hw_matrix). */
void wubu_probe_publish(void);

/* W5: accessor — the built matrix string. */
const char *wubu_probe_matrix(void);

#endif /* WUBU_PROBE_H */
