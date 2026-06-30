/*
 * wubu_compositor.h  --  WuBuOS Wayland Compositor Header
 *
 * Wayland-native compositor with Vulkan rendering, 9P IPC
 */

#ifndef WUBU_COMPOSITOR_H
#define WUBU_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>

/* Forward declarations */
typedef struct WuBuCompositor WuBuCompositor;
typedef struct WuBuWindow WuBuWindow;
typedef struct WuBuOutput WuBuOutput;

/* ================================================================
 * WuBuObject - Opaque object system (better than GObject)
 * ================================================================ */

#define WUBU_OBJECT_HEADER \
    const char *type_name; \
    void (*fini)(void *self);

/* All WuBu objects embed this header */
#define WUBU_OBJECT_CAST(obj) ((struct { WUBU_OBJECT_HEADER } *)(obj))

/* ================================================================
 * Window Types (match xdg_shell roles)
 * ================================================================ */

typedef enum {
    WUBU_WINDOW_TOPLEVEL    = 0,  /* xdg_toplevel - regular window */
    WUBU_WINDOW_POPUP       = 1,  /* xdg_popup - menus, tooltips */
    WUBU_WINDOW_LAYER       = 2,  /* layer_shell - panel, wallpaper, OSD */
} WuBuWindowType;

/* ================================================================
 * Window State (managed by compositor)
 * ================================================================ */

typedef struct {
    double x, y;
    double width, height;
    double scale;          /* Fractional scale (1.0, 1.5, 2.0...) */
    float opacity;         /* 0.0 - 1.0 */
    float transform[9];    /* 3x3 column-major matrix */
} WuBuWindowGeometry;

/* Window capabilities (for 9P exposure) */
typedef struct {
    bool can_minimize;
    bool can_maximize;
    bool can_fullscreen;
    bool can_resize;
    bool can_close;
    bool accepts_input;
} WuBuWindowCaps;

/* ================================================================
 * Output / Monitor
 * ================================================================ */

typedef struct {
    char name[64];
    int x, y;              /* Position in global coordinates
    int width, height;      /* Physical pixels */
    int phys_width, phys_height; /* mm */
    double scale;           /* Fractional scale */
    int refresh_rate;       /* mHz */
    bool enabled;
} WuBuOutputInfo;

/* ================================================================
 * Compositor API
 * ================================================================ */

/* Create/destroy */
WuBuCompositor *wubu_compositor_create(void);
void wubu_compositor_destroy(WuBuCompositor *comp);

/* Run main loop */
int wubu_compositor_run(WuBuCompositor *comp);

/* Get Wayland display for client connections */
struct wl_display *wubu_compositor_get_display(WuBuCompositor *comp);

/* Window management (called by shell) */
WuBuWindow *wubu_window_create(WuBuCompositor *comp, WuBuWindowType type);
void wubu_window_destroy(WuBuWindow *win);

void wubu_window_set_title(WuBuWindow *win, const char *title);
void wubu_window_set_geometry(WuBuWindow *win, const WuBuWindowGeometry *geom);
void wubu_window_get_geometry(WuBuWindow *win, WuBuWindowGeometry *geom);
void wubu_window_set_opacity(WuBuWindow *win, float opacity);
void wubu_window_set_minimized(WuBuWindow *win, bool minimized);
void wubu_window_set_maximized(WuBuWindow *win, bool maximized);
void wubu_window_set_fullscreen(WuBuWindow *win, bool fullscreen, WuBuOutput *output);
void wubu_window_activate(WuBuWindow *win);           /* Focus */
void wubu_window_close(WuBuWindow *win);              /* Request close */

/* Output management */
int wubu_compositor_get_outputs(WuBuCompositor *comp, WuBuOutputInfo *out, int max);
WuBuOutput *wubu_compositor_get_output(WuBuCompositor *comp, const char *name);

/* Damage / rendering */
void wubu_compositor_damage_window(WuBuCompositor *comp, WuBuWindow *win, int x, int y, int w, int h);
void wubu_compositor_damage_output(WuBuCompositor *comp, WuBuOutput *out, int x, int y, int w, int h);

/* Screenshot capability (for 9P) */
int wubu_compositor_screenshot(WuBuCompositor *comp, WuBuOutput *out, void **data, size_t *size);

/* Cursor */
void wubu_compositor_set_cursor(WuBuCompositor *comp, struct wlr_surface *surface, int hotspot_x, int hotspot_y);

/* 9P namespace path */
const char *wubu_compositor_get_9p_path(WuBuCompositor *comp);

/* ================================================================
 * Shell Integration (for dosgui_shell)
 * ================================================================ */

/* Called by shell when it creates a toplevel */
struct wlr_xdg_toplevel *wubu_shell_create_toplevel(WuBuCompositor *comp, const char *app_id);

/* Called by shell for layer surfaces (panel, wallpaper) */
struct wlr_layer_surface_v1 *wubu_shell_create_layer(WuBuCompositor *comp, struct wlr_output *output, uint32_t layer, const char *namespace_);

/* Input method (text input) */
void wubu_compositor_set_text_input_rect(WuBuCompositor *comp, struct wlr_surface *surface, struct wlr_box *box);

/* ================================================================
 * VSL GPU Integration
 * ================================================================ */

bool wubu_compositor_gpu_init(WuBuCompositor *comp);  /* Called after VSL init */
void wubu_compositor_gpu_fini(WuBuCompositor *comp);

/* ================================================================
 * Accessibility (built-in, not AT-SPI2)
 * ================================================================ */

typedef enum {
    WUBU_A11Y_ROLE_WINDOW,
    WUBU_A11Y_ROLE_BUTTON,
    WUBU_A11Y_ROLE_MENU,
    WUBU_A11Y_ROLE_MENU_ITEM,
    WUBU_A11Y_ROLE_TEXT,
    WUBU_A11Y_ROLE_SLIDER,
    WUBU_A11Y_ROLE_LIST,
    WUBU_A11Y_ROLE_LIST_ITEM,
} WuBuA11yRole;

typedef struct {
    WuBuA11yRole role;
    char name[128];
    char description[256];
    int x, y, width, height;
    bool focused;
    bool selected;
    bool enabled;
} WuBuA11yNode;

void wubu_compositor_a11y_tree_get_root(WuBuCompositor *comp, WuBuA11yNode *root);
void wubu_compositor_a11y_node_get_children(WuBuA11yNode *parent, WuBuA11yNode *children, int max);
void wubu_compositor_a11y_announce(WuBuCompositor *comp, const char *message);  /* Screen reader */

#endif /* WUBU_COMPOSITOR_H */