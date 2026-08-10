/*
 * dosgui_controlpanel.c -- the Control Panel MANAGER (Win98/XP
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
#include "dosgui_controlpanel.h"

#include <stdio.h>
#include <string.h>

ControlPanel g_controlpanel;

/* CP1: init the manager + register the applets. */
void dosgui_controlpanel_init(void)
{
    memset(&g_controlpanel, 0, sizeof(g_controlpanel));

    /* the applets: each factory returns a filled CpApplet */
    CpApplet applets[CP_MAX_APPLETS];
    int n = 0;

    applets[n++] = dosgui_cp_create_display_applet();
    applets[n++] = dosgui_cp_create_network_applet();
    applets[n++] = dosgui_cp_create_sound_applet();
    applets[n++] = dosgui_cp_create_theme_applet();
    applets[n++] = dosgui_cp_create_hardware_applet();

    for (int i = 0; i < n && i < CP_MAX_APPLETS; i++) {
        g_controlpanel.applets[i] = applets[i];
        if (g_controlpanel.applets[i].init)
            g_controlpanel.applets[i].init(&g_controlpanel.applets[i]);
        g_controlpanel.applets[i].initialized = true;
    }
    g_controlpanel.applet_count = n;
    g_controlpanel.active_applet = 0;
}

/* CP2: the number of registered applets. */
int dosgui_controlpanel_count(void)
{
    return g_controlpanel.applet_count;
}

/* CP3: select an applet. Returns 0 on success. */
int dosgui_controlpanel_select(int idx)
{
    if (idx < 0 || idx >= g_controlpanel.applet_count) return -1;
    g_controlpanel.active_applet = idx;
    return 0;
}

/* CP4: the active applet. */
CpApplet *dosgui_controlpanel_active(void)
{
    if (g_controlpanel.applet_count == 0) return NULL;
    return &g_controlpanel.applets[g_controlpanel.active_applet];
}

/* CP5: render the sidebar (the applet list) + the active applet. The
 * full glyph rendering is the desktop's; this emits the applet names
 * into a buffer the desktop places (the test asserts the names). */
int dosgui_controlpanel_sidebar(char *out, size_t cap, int *active)
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
void dosgui_controlpanel_mouse(int x, int y, int btn, int kind)
{
    CpApplet *a = dosgui_controlpanel_active();
    if (a && a->mouse) a->mouse(a, x, y, btn, kind);
}

/* CP7: route a key to the active applet. */
void dosgui_controlpanel_key(uint32_t key, uint32_t mods)
{
    CpApplet *a = dosgui_controlpanel_active();
    if (a && a->key) a->key(a, key, mods);
}

/* CP8: the applet lookup by name (the desktop's open-applet path). */
CpApplet *dosgui_controlpanel_find(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_controlpanel.applet_count; i++)
        if (strcmp(g_controlpanel.applets[i].name, name) == 0)
            return &g_controlpanel.applets[i];
    return NULL;
}

/* CP9: register an applet (returns its index, -1 full). */
int dosgui_controlpanel_register_applet(const CpApplet *applet)
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
void dosgui_controlpanel_unregister_applet(CpAppletId id)
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
CpApplet *dosgui_controlpanel_get_applet(CpAppletId id)
{
    for (int i = 0; i < g_controlpanel.applet_count; i++)
        if (g_controlpanel.applets[i].id == id)
            return &g_controlpanel.applets[i];
    return NULL;
}

/* CP12: the window state. */
void dosgui_controlpanel_show(void) { g_controlpanel.win_id = 1; }
void dosgui_controlpanel_hide(void) { g_controlpanel.win_id = 0; }
bool dosgui_controlpanel_is_open(void) { return g_controlpanel.win_id != 0; }
void dosgui_controlpanel_toggle(void)
{
    if (dosgui_controlpanel_is_open()) dosgui_controlpanel_hide();
    else dosgui_controlpanel_show();
}

/* CP13: the input handlers (routed to the active applet). */
void dosgui_controlpanel_handle_key(uint32_t key, uint32_t mods)
{
    dosgui_controlpanel_key(key, mods);
}
void dosgui_controlpanel_handle_mouse(int x, int y, int btn, int kind)
{
    dosgui_controlpanel_mouse(x, y, btn, kind);
}

/* CP14: render — the sidebar strip + the active applet's render.
 * The desktop owns the font; this draws the applet color blocks +
 * calls the active applet's render into its region. */
void dosgui_controlpanel_render(uint32_t *fb, int fb_w, int fb_h)
{
    if (!fb) return;
    /* the sidebar: one color block per applet (the active is
     * brighter); the active applet's render gets the rest */
    for (int i = 0; i < g_controlpanel.applet_count; i++) {
        uint32_t c = g_controlpanel.applets[i].icon_color;
        if (i == g_controlpanel.active_applet)
            c = 0xFFFFFFFF;
        for (int yy = 8; yy < 24 && yy < fb_h; yy++)
            for (int xx = 8 + i * 24; xx < 24 + i * 24 && xx < fb_w; xx++)
                fb[yy * fb_w + xx] = c;
    }
    CpApplet *a = dosgui_controlpanel_active();
    if (a && a->render)
        a->render(a, fb, 0, 32, fb_w, fb_h > 32 ? fb_h - 32 : 1);
}

/* CP15: shutdown — cleanup every applet. */
void dosgui_controlpanel_shutdown(void)
{
    for (int i = 0; i < g_controlpanel.applet_count; i++)
        if (g_controlpanel.applets[i].cleanup)
            g_controlpanel.applets[i].cleanup(&g_controlpanel.applets[i]);
    g_controlpanel.applet_count = 0;
}
