#include "hosted.h"
#include "hosted_internal.h"
#include "wayland_state.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_theme.h"
#include "../bridge/bridge.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "xdg-shell-client.header"
#include "primary-selection-client.header"

/* === hosted_wayland.c -- launcher-core facade for the Wayland subsystem ===
 * Thin orchestration: owns the public hosted_wl_* entry points declared in
 * hosted_internal.h. SHM pool, input listeners, and surface/xdg wiring live
 * in hosted_wayland_shm.c / _input.c / _surface.c respectively, each exposing
 * a wl_<subsys>_init / wl_<subsys>_term pair. */

/* Cross-module entry points owned by the sub-modules. */
void wl_shm_init(hosted_state_t *state);
void wl_shm_term(void);
void wl_input_init(hosted_state_t *state);
void wl_input_term(void);
void wl_surface_init(hosted_state_t *state);
void wl_surface_term(void);

/* g_hosted_state is defined in hosted.c; this module reads it. */
extern hosted_state_t *g_hosted_state;

/* Strong definition of the Wayland connection state. The header declares it
 * extern; hosted.c and the GUI screenshot/clipboard TUs reference it. Owned
 * here (facade) since this module is always linked into the hosted binary. */
wayland_state_t g_wl;

void dosgui_platform_shutdown(void) {
    if (g_hosted_state) g_hosted_state->running = false;
}

int hosted_wl_connect(hosted_state_t *state) {
    memset(&g_wl, 0, sizeof(g_wl));

    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) {
        fprintf(stderr, "No Wayland display. Use -h for headless.\n");
        return -1;
    }
    g_wl.display = dpy;

    /* Bind Wayland globals + create the xdg surface/toplevel (hosted_wayland_surface.c). */
    wl_surface_init(state);
    /* Attach keyboard/pointer/seat listeners (hosted_wayland_input.c). */
    wl_input_init(state);
    /* Allocate the double-buffered SHM pool + wire it into g_wl (hosted_wayland_shm.c). */
    wl_shm_init(state);

    fprintf(stderr, "WuBuOS: Wayland %dx%d window\n", state->width, state->height);
    return 0;
}

void hosted_wl_dispatch(void) {
    if (g_wl.display) {
        wl_display_dispatch_pending(g_wl.display);
        wl_display_flush(g_wl.display);
    }
}

void hosted_wl_disconnect(void) {
    /* Tear down in reverse dependency order: surface first, then input, then SHM. */
    wl_surface_term();
    wl_input_term();
    wl_shm_term();

    if (g_wl.compositor) wl_compositor_destroy(g_wl.compositor);
    if (g_wl.display)    { wl_display_flush(g_wl.display); wl_display_disconnect(g_wl.display); }
    memset(&g_wl, 0, sizeof(g_wl));
}
