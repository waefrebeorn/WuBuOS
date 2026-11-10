/*
 * getting_started.h -- the "hello world" path, as an in-shell document.
 *
 * A first-boot user needs a map, not a wiki.  This window is the product
 * saying where to start: the terminal, the in-tree compiler, a Lua .wet
 * applet, and the VWS designer.
 */
#ifndef GETTING_STARTED_H
#define GETTING_STARTED_H

#include "../gui/wm.h"

#ifdef __cplusplus
extern "C" {
#endif

DosGuiWindow *gs_launch(void);

#ifdef __cplusplus
}
#endif

#endif /* GETTING_STARTED_H */
