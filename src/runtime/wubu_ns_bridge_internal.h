/*
 * wubu_ns_bridge_internal.h -- minimal shared helpers for wubu_ns_bridge
 * submodules (snap, future pkg/kernel/hw). NOT a god header: exposes only
 * the namespace-root handle + the two filesystem primitives that every
 * /n subtree needs. Keep this tiny.
 */

#ifndef WUBU_NS_BRIDGE_INTERNAL_H
#define WUBU_NS_BRIDGE_INTERNAL_H

#include <stddef.h>

/* The /n root captured by wubu_ns_bridge_create(). */
extern const char *g_ns_root;

/* mkdir -p one path component tree under g_ns_root. */
int ns_mkdir(const char *sub);

/* Write a whole file under g_ns_root. */
int ns_write(const char *sub, const char *buf);

#endif /* WUBU_NS_BRIDGE_INTERNAL_H */
