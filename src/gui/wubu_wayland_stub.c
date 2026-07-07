/*
 * wubu_wayland_stub.c  --  weak Wayland-state default for standalone binaries.
 *
 * src/gui/wubu_screenshot.c and wubu_clipboard.c reference the global
 * `g_wl` (wayland_state_t), which is fully populated only by the hosted
 * binary (src/hosted/hosted.c).  Standalone app binaries (paint, doom, ...)
 * do not bring up a Wayland connection, so they link this weak zeroed
 * default instead.  Functions that touch g_wl already guard on
 * `g_wl.display == NULL` / `g_wl.shm_buffer == NULL` and degrade to no-ops,
 * so a zeroed struct is safe.
 *
 * The real definition in hosted.c is a strong symbol and overrides this
 * weak one at link time (no duplicate-symbol error).
 */

#include "wayland_state.h"

__attribute__((weak))
wayland_state_t g_wl;   /* zero-initialised; .display == NULL => no Wayland */
