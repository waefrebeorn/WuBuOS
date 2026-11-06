/*
 * wubu_bus.h -- a message bus for the /n/background runtime (P3).
 *
 * Models the D-Bus activation contract from the Mint baseline study
 * (os-studies/linuxmint_background_baseline.md §4): a service publishes a
 * WELL-KNOWN NAME on the bus; a client that calls that name triggers the bus
 * to ACTIVATE the owning service on first use (on-demand launch), exactly as
 * dbus-daemon starts a .service file "to provide a particular well-known bus
 * name" the first time that name is called.
 *
 * This is a NAME-OWNERSHIP + ACTIVATION bus: the unit of binding is a
 * well-known name (e.g. "org.wubu.Power" or "org.wubu.Session"), not a raw
 * address. The bus keeps a table of owned names -> connection; requesting a
 * message for an UNOWNED name fires the activate hook and queues the request
 * until (or unless) the name comes alive.
 *
 * C11, allocator-based (host runtime), thread-free: the API takes no locks —
 * call it from a single driver loop or the same thread that owns the bus.
 * Thread-safety (a reader/writer lock around the name table) is a later step;
 * the D-Bus activation SEMANTICS are what this unit nails.
 */
#ifndef WUBU_BUS_H
#define WUBU_BUS_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_BUS_MAX_NAME   128
#define WUBU_BUS_MAX_CONNS  64
#define WUBU_BUS_MAX_MSG    256
#define WUBU_BUS_MAX_PENDING 128

/* a connection handle is an opaque index; a connection OWNS zero-or-more
 * well-known names after it registers them. */
typedef struct wubu_bus wubu_bus_t;

/* Callback: bus asks the runtime to start the service that owns `name`
 * (on-demand activation). Return 0 if launch was initiated; the request is
 * then QUEUED until the name is claimed (or WUBU_BUS_MAX_PENDING fills).
 * Return nonzero to reject (call fails immediately). */
typedef int (*wubu_bus_activate_fn)(wubu_bus_t *bus,
                                    const char *name, void *ud);

typedef struct {
    char message[WUBU_BUS_MAX_MSG];
    uint32_t sender;          /* connection index that sent it */
    uint64_t serial;
} wubu_bus_msg_t;

/* Lifecycle --------------------------------------------------------------- */
wubu_bus_t *wubu_bus_create(void);
void wubu_bus_destroy(wubu_bus_t *bus);

/* Connection management: register a client, get its handle, drop it. Dropping
 * also un-owns every name that connection claimed. Returns -1 on full. */
int32_t wubu_bus_connect(wubu_bus_t *bus);
int  wubu_bus_disconnect(wubu_bus_t *bus, int32_t conn);

/* Name ownership (the D-Bus well-known-name contract) ---------------------- */
/* Claim a well-known name. -1 if invalid name or the name is already owned.
 * The name string is copied. */
int  wubu_bus_request_name(wubu_bus_t *bus, int32_t conn, const char *name);
/* Release a name. 0 on success. */
int  wubu_bus_release_name(wubu_bus_t *bus, int32_t conn, const char *name);
/* Who owns `name` right now? -1 if unowned. */
int32_t wubu_bus_owner_of(wubu_bus_t *bus, const char *name);
/* How many distinct names are claimed? */
int wubu_bus_name_count(const wubu_bus_t *bus);
/* The i-th claimed name (0..name_count-1); NULL out of range or unowned.
 * Enables enumerating the live session bus (e.g. for /n/background/bus/names). */
const char *wubu_bus_name_at(const wubu_bus_t *bus, int i);
/* How many live connections the bus has right now. */
int wubu_bus_conn_count(const wubu_bus_t *bus);

/* Activation -------------------------------------------------------------- */
void wubu_bus_set_activator(wubu_bus_t *bus, wubu_bus_activate_fn fn, void *ud);

/* Messaging ----------------------------------------------------------------- */
/* Send `msg` to the connection that owns `name`. If the name is unowned, the
 * bus first asks the activator to start it and QUEUES the message pending the
 * name's arrival (the D-Bus on-demand-activation path). Returns the number of
 * pending messages (>=0); -1 on invalid sender, or if the name has no owner,
 * activation is refused, and the queue is full. */
int wubu_bus_send_to_name(wubu_bus_t *bus, int32_t sender,
                          const char *name, const char *msg);
/* Deliver all messages currently addressed to `conn` (FIFO). Returns how many
 * were delivered. */
int wubu_bus_recv(wubu_bus_t *bus, int32_t conn,
                  wubu_bus_msg_t *out, int max);

#endif /* WUBU_BUS_H */
