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
    applets[n++] = cp_create_ai_applet();
    applets[n++] = cp_create_people_applet();
    applets[n++] = cp_create_programs_applet();
    if (cp_create_pkg_sync_applet && n < CP_MAX_APPLETS)
        applets[n++] = cp_create_pkg_sync_applet();

    for (int i = 0; i < n && i < CP_MAX_APPLETS; i++) {
        g_controlpanel.applets[i] = applets[i];
        if (g_controlpanel.applets[i].init)
            g_controlpanel.applets[i].init(&g_controlpanel.applets[i]);
        g_controlpanel.applets[i].initialized = true;
    }
    g_controlpanel.applet_count = n;

    /* The pages that report machine state resolve the LIVE world themselves (weak wubu_world_snapshot),
     * and the injected provider is a TEST override only. This used to install wubu_world_snapshot through
     * cp_*_set_snapshot() here, which stored a pointer the applet did not own: measured in a live session,
     * the applet read 0x47a4ec (screen=3843x51383) while the real world sat at 0x360d320 (1920x1080). */
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

void controlpanel_render_at(uint32_t *fb, int fb_w, int fb_h,
                           int ox, int oy, int ow, int oh)
{
    if (!fb || ow <= 0 || oh <= 0) return;
    const WubuThemeColors *c = wubu_theme_colors();

    /* The panel is a window's CONTENT area: paint at the origin it was given and clip to it. Drawing at
     * (0,0) with the framebuffer's size was the reason the Control Panel window opened empty -- its
     * painted surface landed outside the chrome's clip. */
    cp_fill(fb, fb_w, fb_h, ox, oy, ow, oh, c->win_face);

    int sw = CP_SIDEBAR_W < ow ? CP_SIDEBAR_W : ow;
    cp_fill(fb, fb_w, fb_h, ox, oy, sw, oh, c->taskbar_bg);
    cp_fill(fb, fb_w, fb_h, ox + sw - 1, oy, 1, oh, c->border_dark);
    cp_draw_text(fb, fb_w, fb_h, ox + CP_SIDE_PAD, oy + 10, "Settings", c->win_title_text, 1);

    for (int i = 0; i < g_controlpanel.applet_count; i++) {
        int x, y, w, h;
        if (!controlpanel_sidebar_row_rect(i, &x, &y, &w, &h)) continue;
        bool active = (i == g_controlpanel.active_applet);
        if (active) cp_fill(fb, fb_w, fb_h, ox + x + 4, oy + y, w - 12, h, c->select_bg);
        cp_fill(fb, fb_w, fb_h, ox + x + CP_SIDE_PAD, oy + y + 9, 8, 8,
                g_controlpanel.applets[i].icon_color);
        cp_draw_text(fb, fb_w, fb_h, ox + x + CP_SIDE_PAD + 16, oy + y + 9,
                     g_controlpanel.applets[i].name,
                     active ? c->select_text : c->body_text, 1);
    }

    CpApplet *a = controlpanel_active();
    int px = ox + sw, py = oy + CP_HEADER_H;
    int pw = ow - sw, ph = oh - CP_HEADER_H;
    if (a && a->render && pw > 0 && ph > 0)
        a->render(a, fb, fb_w, fb_h, px, py, pw, ph);
}

void controlpanel_render(uint32_t *fb, int fb_w, int fb_h)
{
    controlpanel_render_at(fb, fb_w, fb_h, 0, 0, fb_w, fb_h);
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

static void cp_on_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)
{
    /* Draw into the surface that vbe_* ACTUALLY writes.
     *
     * vbe_row() -- the backing for every vbe_fill_rect / vbe_draw_text -- is
     * (uint8_t*)g_vbe.back + y * g_vbe.pitch. The `fb` parameter is NULL in the hosted frame loop
     * (desktop_render(NULL, ...)), and vbe_state()->fb is a DIFFERENT buffer from g_vbe.back. Both were
     * tried and measured: a full-rect magenta fill and a title change through this callback left the
     * presented frame untouched, while the chrome (which goes through vbe_*) always appeared. That is
     * exactly the shape of "chrome draws, content does not". */
    int sw = fb_w, sh = fb_h;
    {
        /* Weak: the kernel's vbe unit is not linked into a panel test graph. */
        extern uint32_t *vbe_draw_surface(int *, int *) __attribute__((weak));
        if (vbe_draw_surface) {
            int stride = 0, hgt = 0;
            uint32_t *surf = vbe_draw_surface(&stride, &hgt);
            if (surf) {
                fb = surf;
                sw = stride > 0 ? stride : fb_w;
                sh = hgt > 0 ? hgt : fb_h;
            }
        }
    }
    if (!fb) return;
    fb_w = sw; fb_h = sh;
    /* wm_render clips to the content rect and then calls us with the WHOLE framebuffer, so the panel
     * must paint at the content ORIGIN in framebuffer coordinates. Painting at (0,0) put every pixel
     * outside the clip and the window opened empty -- twice, because the first fix changed the size but
     * kept the origin at zero. */
    int cx = 0, cy = 0, cw = fb_w, ch = fb_h;
    /* Weak: a panel test graph has the applets without the window manager. */
    extern void wm_content_rect(const DosGuiWindow *, int *, int *, int *, int *) __attribute__((weak));
    if (win && wm_content_rect) wm_content_rect(win, &cx, &cy, &cw, &ch);
    if (cw <= 0 || ch <= 0) { cw = fb_w; ch = fb_h; cx = 0; cy = 0; }
    controlpanel_render_at(fb, fb_w, fb_h, cx, cy, cw, ch);

}

static void cp_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)win;
    controlpanel_handle_key(key, mods);
}

static void cp_on_mouse(DosGuiWindow *win, int mx, int my, int btn, int kind)
{
    /* Mouse coordinates arrive in window space; the sidebar's rows live in content space. */
    int cx = 0, cy = 0, cw = 0, ch = 0;
    extern void wm_content_rect(const DosGuiWindow *, int *, int *, int *, int *) __attribute__((weak));
    if (wm_content_rect) wm_content_rect(win, &cx, &cy, &cw, &ch);
    (void)cw; (void)ch;
    controlpanel_handle_mouse(mx - cx, my - cy, btn, kind);
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
