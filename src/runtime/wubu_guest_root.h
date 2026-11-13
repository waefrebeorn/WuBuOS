/*
 * wubu_guest_root.h -- where the guest namespace lives on disk.
 *
 * A separate leaf so that anything needing to know the sandbox root (File
 * Manager, the desktop directory) does not have to link the whole 9P host layer
 * just to read one path.  Zero dependencies: stdlib only.
 */

#ifndef WUBU_GUEST_ROOT_H
#define WUBU_GUEST_ROOT_H

#include <stddef.h>

/* Absolute path of the guest sandbox root.  Guest "/" resolves here, so no app
 * ever sees the host filesystem unless an operator explicitly mounts one.
 *
 * $WUBU_GUEST_ROOT overrides it.  The value is computed once and cached: a running
 * guest cannot change its own filesystem root underneath itself, and returning a
 * different answer mid-session would leave open handles pointing at two trees. */
void wubu_guest_root(char *out, size_t outsz);

#endif /* WUBU_GUEST_ROOT_H */
