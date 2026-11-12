/*
 * egojar_net.h -- the networking shelf of the egojar room.
 *
 *   "e goes in the e jar, that's where the ego."
 *
 * Sockets live here, in their own jar, because putting them in the core jar
 * broke the build: <sys/socket.h> drags in <net/if.h>, which redefines
 * struct ifreq / ifmap / IFF_* and collides with <linux/if.h> -- and half the
 * tree legitimately includes <linux/if.h>.
 *
 * So the room has shelves:
 *
 *   egojar.h       core POSIX: files, processes, threads, memory, signals
 *   egojar_net.h   this one: sockets, addresses, network devices
 *
 * A file that talks to the network includes BOTH (the core first, then the
 * net shelf). A file that only reads files or spawns processes includes just
 * the core, and never pays for a socket header it does not use.
 *
 * Standard C headers are still not here -- see egojar.h.
 */

#ifndef WUBU_EGOJAR_NET_H
#define WUBU_EGOJAR_NET_H

#include "egojar.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#endif /* WUBU_EGOJAR_NET_H */
