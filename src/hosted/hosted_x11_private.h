/*
 * hosted_x11_private.h -- state shared between the X11 window unit and the surface unit.
 *
 * The window (create/map/close/events) and the SURFACE (image, shared memory, fullscreen, resize) are two
 * concerns; they share this state and nothing else. Keeping the declarations here is what lets the two files
 * stay under the line budget without either one owning a copy of the other's data.
 */
#ifndef HOSTED_X11_PRIVATE_H
#define HOSTED_X11_PRIVATE_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

extern Display *g_xdpy;
extern Window   g_xwin;
extern GC       g_xgc;
extern XImage  *g_ximg;
extern int      g_xw, g_xh;
extern int      g_shm_ok;
extern XShmSegmentInfo g_shm;
extern int      g_xopen;

/* fullscreen / resize state: the window unit records the geometry it returns to, the surface unit does the work */
extern int      g_fullscreen;
extern int      g_win_w, g_win_h, g_win_x, g_win_y;
extern int      g_pending_w, g_pending_h;

/* Create/replace the XImage (shared-memory when the server allows it) at w x h. One owner for the SHM
 * dance: the window opener and the fullscreen resize both need it, and two copies would drift. */
int x11_create_image_surface(int w, int h);

/* release whatever image/segment is attached, without touching the framebuffer we do not own */
void x11_release_image_surface(void);

#endif
