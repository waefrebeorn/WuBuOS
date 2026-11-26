/*
 * control_panel.c -- the Control Panel MANAGER (Win98/XP
 * vibes, the applet registry).
 *
 * The header declared g_controlpanel + the applet contract for ages;
 * the MANAGER was never built (form-without-function — the applets
 * had no registry to live in). This is the manager:
 *
 *   - instantiates the applets (sound + hardware; the display/network/
 *     theme stubs are declared for future applets)
 *   - the sidebar: the applet list + the active selection
 *   - the routing: render/mouse/key go to the ACTIVE applet
 *   - control_panel_open() runs the manager (the desktop calls it)
 *
 * The test asserts the applet registry + the switching.
 * C11.
 */
#include "control_panel.h"
#include "../kernel/wubu_world.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "wubu_gui_metrics.h"
#include "wm.h"   /* DosGuiWindow: the launcher creates one */

/* The panel's own chrome: a labelled sidebar and a page beside it. */
#define CP_SIDEBAR_W   152
#define CP_HEADER_H    32
#define CP_SIDE_ROW_H  26
#define CP_SIDE_PAD    10

   /* the snapshot the Display/Network pages report */

/* Weak: a panel test graph has the applets without the world model. */
extern const wubu_world_t *wubu_world_snapshot(void) __attribute__((weak));

#include <stdio.h>
#include <string.h>

/* The sync applet is optional at link time: targets that compile this manager
 * without the sync leaf simply do not show the applet (weak reference), so no
 * GUI test target gains a hard dependency on HTTPS/mbedTLS. The hosted build
 * links wubu_pkgmgr_sync_panel.o and gets the real control. */
extern CpApplet cp_create_pkg_sync_applet(void) __attribute__((weak));

ControlPanel g_controlpanel;

/* CP1: init the manager + register the applets. */
void controlpanel_init(void)
{
    memset(&g_controlpanel, 0, sizeof(g_controlpanel));

    /* the applets: each factory returns a filled CpApplet */
    CpApplet applets[CP_MAX_APPLETS];
    int n = 0;

    applets[n++] = cp_create_display_applet();
    applets[n++] = cp_create_network_applet();
    applets[n++] = cp_create_sound_applet();
    applets[n++] = cp_create_theme_applet();
    applets[n++] = cp_create_hardware_applet();
    applets[n++] = cp_create_firmware_applet();
    applets[n++] = cp_create_mirror_applet();
    applets[n++] = cp_create_account_applet();
    applets[n++] = cp_create_limits_applet();
    applets[n++] = cp_create_signin_applet();
    if (cp_create_pkg_sync_applet && n < CP_MAX_APPLETS)
        applets[n++] = cp_create_pkg_sync_applet();

    for (int i = 0; i < n && i < CP_MAX_APPLETS; i++) {
        g_controlpanel.applets[i] = applets[i];
        if (g_controlpanel.applets[i].init)
            g_controlpanel.applets[i].init(&g_controlpanel.applets[i]);
        g_controlpanel.applets[i].initialized = true;
    }
    g_controlpanel.applet_count = n;

    /* The pages that report machine state need a provider, and nothing in the shell ever installed one:
     * Display and Network rendered "no snapshot" on every real install while the tests injected fakes and
     * passed. Wire the real snapshot once, here, where the panel is set up. */
    if (wubu_world_snapshot) {
        cp_display_set_snapshot(wubu_world_snapshot);
        cp_network_set_snapshot(wubu_world_snapshot);
    }
    g_controlpanel.active_applet = 0;
}

/* CP2: the number of registered applets. */
int controlpanel_count(void)
{
    return g_controlpanel.applet_count;
}

/* CP3: select an applet. Returns 0 on success. */
int controlpanel_select(int idx)
{
    if (idx < 0 || idx >= g_controlpanel.applet_count) return -1;
    g_controlpanel.active_applet = idx;
    return 0;
}

/* CP4: the active applet. */
CpApplet *controlpanel_active(void)
{
    if (g_controlpanel.applet_count == 0) return NULL;
    return &g_controlpanel.applets[g_controlpanel.active_applet];
}

/* CP5: render the sidebar (the applet list) + the active applet. The
 * full glyph rendering is the desktop's; this emits the applet names
 * into a buffer the desktop places (the test asserts the names). */
int controlpanel_sidebar(char *out, size_t cap, int *active)
{
    if (!out || cap == 0) return -1;
    size_t off = 0;
    for (int i = 0; i < g_controlpanel.applet_count; i++) {
        int w = snprintf(out + off, cap - off, "%s%c",
                         g_controlpanel.applets[i].name,
                         i == g_controlpanel.active_applet ? '*' : ' ');
        if (w < 0) break;
        off += (size_t)w;
    }
    if (active) *active = g_controlpanel.active_applet;
    return g_controlpanel.applet_count;
}

/* CP6: route a mouse event to the active applet. */
void controlpanel_mouse(int x, int y, int btn, int kind)
{
    CpApplet *a = controlpanel_active();
    if (a && a->mouse) a->mouse(a, x, y, btn, kind);
}

/* CP7: route a key to the active applet. */
void controlpanel_key(uint32_t key, uint32_t mods)
{
    CpApplet *a = controlpanel_active();
    if (a && a->key) a->key(a, key, mods);
}

/* CP8: the applet lookup by name (the desktop's open-applet path). */
CpApplet *controlpanel_find(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_controlpanel.applet_count; i++)
        if (strcmp(g_controlpanel.applets[i].name, name) == 0)
            return &g_controlpanel.applets[i];
    return NULL;
}

/* CP9: register an applet (returns its index, -1 full). */
int controlpanel_register_applet(const CpApplet *applet)
{
    if (!applet || g_controlpanel.applet_count >= CP_MAX_APPLETS) return -1;
    int i = g_controlpanel.applet_count++;
    g_controlpanel.applets[i] = *applet;
    if (g_controlpanel.applets[i].init)
        g_controlpanel.applets[i].init(&g_controlpanel.applets[i]);
    g_controlpanel.applets[i].initialized = true;
    return i;
}

/* CP10: unregister an applet by id (compacts the list). */
void controlpanel_unregister_applet(CpAppletId id)
{
    for (int i = 0; i < g_controlpanel.applet_count; i++) {
        if (g_controlpanel.applets[i].id != id) continue;
        if (g_controlpanel.applets[i].cleanup)
            g_controlpanel.applets[i].cleanup(&g_controlpanel.applets[i]);
        for (int j = i; j < g_controlpanel.applet_count - 1; j++)
            g_controlpanel.applets[j] = g_controlpanel.applets[j + 1];
        g_controlpanel.applet_count--;
        if (g_controlpanel.active_applet >= g_controlpanel.applet_count)
            g_controlpanel.active_applet = 0;
        return;
    }
}

/* CP11: the applet lookup by id. */
CpApplet *controlpanel_get_applet(CpAppletId id)
{
    for (int i = 0; i < g_controlpanel.applet_count; i++)
        if (g_controlpanel.applets[i].id == id)
            return &g_controlpanel.applets[i];
    return NULL;
}

/* CP12: the window state. */
void controlpanel_show(void) { g_controlpanel.win_id = 1; }
void controlpanel_hide(void) { g_controlpanel.win_id = 0; }
bool controlpanel_is_open(void) { return g_controlpanel.win_id != 0; }
void controlpanel_toggle(void)
{
    if (controlpanel_is_open()) controlpanel_hide();
    else controlpanel_show();
}

/* CP13: the input handlers (routed to the active applet). */
void controlpanel_handle_key(uint32_t key, uint32_t mods)
{
    if (key == 0xE050 || key == 0xE048) {          /* Down / Up cycle the pages */
        int n = g_controlpanel.applet_count;
        if (n > 0) g_controlpanel.active_applet =
            (g_controlpanel.active_applet + (key == 0xE050 ? 1 : n - 1)) % n;
        return;
    }
    controlpanel_key(key, mods);
}
void controlpanel_handle_mouse(int x, int y, int btn, int kind)
{
    if (kind == 1 && btn == 1) {
        int hit = controlpanel_sidebar_hit(x, y);
        if (hit >= 0) { g_controlpanel.active_applet = hit; return; }
    }
    /* The page sees coordinates RELATIVE to its own origin: an applet that hit-tests in page space must
     * not have to know where the sidebar ends. */
    if (x >= CP_SIDEBAR_W)
        controlpanel_mouse(x - CP_SIDEBAR_W, y - CP_HEADER_H, btn, kind);
}

/* CP14: render -- a real sidebar with NAMES and a page beside it, not colour blocks.
 *
 * The old version painted 24-px blocks with no labels and handed every applet the full width, so the
 * page ran under its own sidebar and a user could not tell a page name from a colour. This is the chrome
 * the applet pages were always meant to sit inside. */
#define CP_SIDEBAR_W 152
#define CP_HEADER_H  32
#define CP_SIDE_ROW_H 26
#define CP_SIDE_PAD  10

int controlpanel_sidebar_w(void) { return CP_SIDEBAR_W; }
int controlpanel_header_h(void) { return CP_HEADER_H; }

/* Where applet row i is, as one expression shared by the renderer and the hit-test. */
bool controlpanel_sidebar_row_rect(int i, int *x, int *y, int *w, int *h)
{
    if (i < 0 || i >= g_controlpanel.applet_count) return false;
    if (x) *x = 0;
    if (y) *y = CP_HEADER_H + 4 + i * CP_SIDE_ROW_H;
    if (w) *w = CP_SIDEBAR_W;
    if (h) *h = CP_SIDE_ROW_H;
    return true;
}

void controlpanel_handle_mouse(int x, int y, int btn, int kind);

int controlpanel_active_index(void) { return g_controlpanel.active_applet; }

int controlpanel_sidebar_hit(int mx, int my)
{
    for (int i = 0; i < g_controlpanel.applet_count; i++) {
        int x, y, w, h;
        if (!controlpanel_sidebar_row_rect(i, &x, &y, &w, &h)) continue;
        if (mx >= x && mx < x + w && my >= y && my < y + h) return i;
    }
    return -1;
}

void controlpanel_render(uint32_t *fb, int fb_w, int fb_h)
{
    if (!fb || fb_w <= 0 || fb_h <= 0) return;
    const WubuThemeColors *c = wubu_theme_colors();

    /* Drawing goes through control_panel_text's helpers, which CLIP to the surface: this renderer is
     * called with the window's surface and must never write outside it (the canary in the applet gate
     * exists because a panel once did). */
    cp_fill(fb, fb_w, fb_h, 0, 0, fb_w, fb_h, c->win_face);

    /* The sidebar: its own surface and a NAME per row. */
    cp_fill(fb, fb_w, fb_h, 0, 0, CP_SIDEBAR_W, fb_h, c->taskbar_bg);
    cp_fill(fb, fb_w, fb_h, CP_SIDEBAR_W - 1, 0, 1, fb_h, c->border_dark);
    cp_draw_text(fb, fb_w, fb_h, CP_SIDE_PAD, 10, "Settings", c->win_title_text, 1);

    for (int i = 0; i < g_controlpanel.applet_count; i++) {
        int x, y, w, h;
        if (!controlpanel_sidebar_row_rect(i, &x, &y, &w, &h)) continue;
        bool active = (i == g_controlpanel.active_applet);
        if (active) cp_fill(fb, fb_w, fb_h, x + 4, y, w - 12, h, c->select_bg);
        /* The applet's colour becomes a small mark, so the sidebar is not a wall of text. */
        cp_fill(fb, fb_w, fb_h, x + CP_SIDE_PAD, y + 9, 8, 8, g_controlpanel.applets[i].icon_color);
        cp_draw_text(fb, fb_w, fb_h, x + CP_SIDE_PAD + 16, y + 9, g_controlpanel.applets[i].name,
                     active ? c->select_text : c->body_text, 1);
    }

    CpApplet *a = controlpanel_active();
    if (a && a->render)
        a->render(a, fb, fb_w, fb_h, CP_SIDEBAR_W, CP_HEADER_H,
                  fb_w - CP_SIDEBAR_W, fb_h > CP_HEADER_H ? fb_h - CP_HEADER_H : 1);
}

/* CP15: shutdown — cleanup every applet. */
void controlpanel_shutdown(void)
{
    for (int i = 0; i < g_controlpanel.applet_count; i++)
        if (g_controlpanel.applets[i].cleanup)
            g_controlpanel.applets[i].cleanup(&g_controlpanel.applets[i]);
    g_controlpanel.applet_count = 0;
}

/* -- The launcher ---------------------------------------------------------------------------------
 * The engine had no window of its own: the desktop's Settings icon opened a DIFFERENT panel, so every
 * applet page here was unreachable in a real session. This creates the window, wires the engine to it,
 * and installs the machine-state providers. Weak wm_create: a panel test graph has the applets without
 * the window manager. */
extern DosGuiWindow *wm_create(int x, int y, int w, int h, const char *title) __attribute__((weak));

static void cp_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h)
{
    (void)win; (void)x; (void)y;
    if (!fb) return;
    (void)w; (void)h;
    controlpanel_render(fb, fb_w, fb_h);
}

static void cp_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)win;
    controlpanel_handle_key(key, mods);
}

static void cp_on_mouse(DosGuiWindow *win, int mx, int my, int btn, int kind)
{
    (void)win;
    controlpanel_handle_mouse(mx, my, btn, kind);
}

DosGuiWindow *control_panel_launch(void)
{
    if (!wm_create) return NULL;
    DosGuiWindow *win = wm_create(80, 60, 860, 620, "Control Panel");
    if (!win) return NULL;
    controlpanel_init();
    win->on_draw = cp_on_draw;
    win->on_key = cp_on_key;
    win->on_mouse = cp_on_mouse;
    return win;
}
