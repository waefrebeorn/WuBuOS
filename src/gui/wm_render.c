/*
 * wm_render.c -- WuBuOS WM rendering (single module)
 *
 * ONE entry point: wm_render(fb, w, h). It draws the desktop
 * background, icons, every live window (chrome + on_draw content), the
 * taskbar, the notification center and the cursor.
 *
 * The previous design had THREE render entry points (wm_render,
 * wm_render_desktop, desktop_render) plus a dead
 * dirty-region tracking system that nothing consumed (the hosted loop
 * redraws every frame). All of that is collapsed here: the two
 * *_desktop_render names remain as 1-line wrappers for back-compat
 * callers (hosted.c, tests), and the dirty tracking is gone.
 *
 * Self-contained: depends only on the public WM API (wm.h)
 * and the shared internal state/helpers (wm_internal.h).
 */

#include "wm.h"
#include "wm_internal.h"
#include "shell/wubu_osd.h"
#include "shell/wubu_screensaver.h"
#include "shell/wubu_lock_screen.h"
#include "shell/win11_shell.h"
#include "shell/run_dialog.h"
#include "shell/power_menu.h"
#include "wubu_a11y.h"
#include "buddy_sprite.h"
#include "../kernel/vbe.h"
#include "../runtime/wubu_effect.h"   /* the pure effect engine (no deps) */
#include "../hosted/wubu_vk_effect.h" /* the GPU leg of the same engine */

#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>

/* The effect stack is owned by the runtime's /n/effect publisher.  Declared
 * weak so this module keeps its self-contained design and links with or
 * without the runtime object; in a full build the real symbol resolves and a
 * program's /n/effect/set becomes visible on the desktop. */
const WubuEffectStack *wubu_ns_effect_stack(void) __attribute__((weak));

/* The GPU effect pass, when this build has Vulkan.  Weak for the same reason:
 * a GUI-only test link has no Vulkan and the symbol resolves to 0.  TESTED
 * before it is called -- a weak function with no definition cannot be called
 * directly (the call is relocated to 0). */
int wubu_vk_effect_desktop_apply(const WubuEffectStack *s, uint32_t *px,
                                 int w, int h) __attribute__((weak));

/* Elevation shadow for the focused window (THEME_DARK). Weak: minimal WM
 * test links carry no shadow module; the product and test_wm link the real
 * one (src/gui/shell/wm_shadow.c). */
void wm_shadow_render(const DosGuiWindow *w) __attribute__((weak));

/* -- Single window draw ------------------------------------------- */

static void draw_window(int idx, uint32_t *fb, int fb_w, int fb_h) {
    DosGuiWindow *w = &g_dwm.windows[idx];
    if (!w->alive) return;

    /* Use standardized chrome to draw window frame + title bar + buttons.
     * Returns content rect for app to draw within. */
    /* Elevation shadow first: it is what makes a rounded corner READ as rounded. The module
     * existed and was tested but nothing ever called it, so every window sat flat on the
     * wallpaper -- which is most of why the chrome looked boxy. */
    {
        extern void wm_shadow_render(const DosGuiWindow *) __attribute__((weak));
        static int no_shadow = -1;
        if (no_shadow < 0) no_shadow = getenv("WUBU_NO_SHADOW") ? 1 : 0;
        if (wm_shadow_render && !no_shadow) wm_shadow_render(w);
    }
    /* Finish any close animation whose time is up before drawing this frame. */
    wm_close_tick();

    /* Open animation: grow the window from WUBU_ANIM_OPEN_FROM_PCT of its height to full over
     * WUBU_ANIM_OPEN_MS. Only the RENDERED height changes -- w->h itself is restored right
     * after, so hit-testing, snapping and the taskbar always see the real rect. */
    int anim_saved_h = w->h;
    w->h = wm_window_render_h(w);   /* open animation */
    ChromeContentRect content = chrome_draw_window(&g_dwm.windows[idx], fb, fb_w, fb_h);

    /* Content region: clip and call app's on_draw if present */
    if (w->on_draw) {
        static int no_content = -1;
        if (no_content < 0) no_content = getenv("WUBU_NO_CONTENT") ? 1 : 0;
        vbe_set_clip(content.x, content.y, content.w, content.h);
        if (!no_content) w->on_draw(w, fb, fb_w, fb_h);
        vbe_reset_clip();
    }

    w->h = anim_saved_h;   /* end of the animated frame */

    /* An app that fills its rect squares off the chrome's radius. Cut the corners
     * AFTER the app has drawn, so the window reads as rounded whatever the app did --
     * which is exactly what was happening: every window looked boxy while the theme
     * claimed an 8px radius. */
    /* Round when the THEME declares a radius, not when it answers to the name "fluent":
     * THEME_DARK never did, so no window was ever rounded while the theme claimed an 8px
     * radius -- "everything is boxy" was literally true. */
    int radius = WUBU_METRIC_RADIUS_WINDOW;   /* Win11: 8px at 100% scale, always */
    if (!(w->flags & DOSGUI_WIN_MAXIMIZED) &&
        (theme_radius() > 0 || wubu_theme_current() == THEME_DARK))
        chrome_mask_corners(w->x, w->y, w->w, w->h, radius);
}

/* -- Full-screen render (the single entry point) ------------------- */


/* Win11 desktop label: up to TWO centred lines, split at a space; the second line is
 * ellipsised only if it still overflows. Both earlier attempts are recorded here because each
 * shipped a visible defect: the first dropped the ellipsis (it compared the truncated string's
 * width against its own post-truncation length), the second wrapped where Windows ellipsises
 * and left the column truncating names like "My Computer" for no reason -- the cell is now
 * wide enough that a two-line wrap carries the whole name. */
static void wm_fit_lines(const char *name, int max_w,
                         char *l1, size_t cap1, char *l2, size_t cap2) {
    l1[0] = '\0'; l2[0] = '\0';
    if (!name) return;
    if (vbe_text_width(name, 1) <= max_w) { snprintf(l1, cap1, "%s", name); return; }

    /* Prefer the LAST space that leaves line 1 fitting. */
    int split = -1;
    for (const char *q = name; *q; q++) {
        if (*q != ' ') continue;
        char head[64];
        snprintf(head, sizeof head, "%.*s", (int)(q - name), name);
        if (vbe_text_width(head, 1) <= max_w) split = (int)(q - name);
    }
    if (split > 0) {
        snprintf(l1, cap1, "%.*s", split, name);
        snprintf(l2, cap2, "%s", name + split + 1);
    } else {
        snprintf(l1, cap1, "%s", name);          /* one long word: ellipsise below */
    }

    /* Ellipsise whatever overflows -- and REMEMBER that we cut, so the dots survive. */
    char *lines[2] = { l1, l2 };
    size_t caps[2] = { cap1, cap2 };
    for (int i = 0; i < 2; i++) {
        if (!lines[i][0]) continue;
        if (vbe_text_width(lines[i], 1) <= max_w) continue;
        int dots = vbe_text_width("...", 1);
        size_t len = strlen(lines[i]);
        while (len > 0 && vbe_text_width(lines[i], 1) > max_w - dots)
            lines[i][--len] = '\0';
        if (len + 3 < caps[i]) memcpy(lines[i] + len, "...", 4);
    }
}

void wm_render(uint32_t *fb, int fb_w, int fb_h) {
    /* Advance the desktop companion animation clock (~60 Hz). */
    wubu_bonzi_tick(16);
    /* Advance the focused window's rim islands (no-op when there are none). */
    wm_rim_tick();
    /* Advance the screen-edge slime seam (no-op when there are no blobs). */
    wm_slime_tick();

    /* The bar is always revealed (Windows 11 parity).  The auto-hide this
     * replaces slid the bar off-screen whenever the pointer left the bottom
     * band, which made the Start button unreachable unless the pointer
     * hugged the screen edge. */
    g_dwm.taskbar_offset = 0;
    /* Advance any active human-typing session (keystrokes land with
     * human rhythm, not all at once). */
    wm_typer_tick(16);

    /* WUBU_PERF_SKIP bitmask (1 wallpaper, 2 icons, 4 windows, 8 taskbar): locate frame cost
     * by bisection instead of guessing. Unset in normal use. */
    static int skip, skip_init;
    if (!skip_init) { skip_init = 1; const char *e = getenv("WUBU_PERF_SKIP"); skip = e ? atoi(e) : 0; }
    if (!(skip & 1)) draw_desktop_bg(fb_w, fb_h);

    { static int once; if (!once) { once = 1;
        fprintf(stderr, "[probe] render sees icon_count=%d\n", g_dwm.icon_count); } }
    for (int i = 0; i < ((skip & 2) ? 0 : g_dwm.icon_count); i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        if (!icon->alive) continue;

        /* Selection highlight (XP active-selection: navy box + focus rect). */
        if (icon->selected)
            wm_draw_icon_selection(icon->x, icon->y);

        /* Recognizable glyph instead of a flat colored box. Fall back to the
         * icon_color box only if the slot is somehow empty. */
        uint32_t accent = icon->icon_color ? icon->icon_color : tc()->win_title_active;
        wm_draw_icon_glyph(icon->type, icon->x, icon->y,
                                  /* face */ tc()->btn_face,
                                  /* light */ tc()->border_light,
                                  /* dark */ tc()->border_dark,
                                  /* accent */ accent);

        int label_y = icon->y + DOSGUI_ICON_SIZE + 2;
        /* The label owns the column pitch (Win98 columns are ~75px), less a
         * small margin, so a full name fits on one line instead of being cut
         * to three characters. */
        int max_label_w = DOSGUI_ICON_COL_PITCH - 8;
        int text_w = vbe_text_width(icon->name, 1);

        if (label_y + 8 <= fb_h) {
            /* Win11 label: CENTRED under the icon, up to two lines, ellipsis only when the
             * second line still does not fit. The old single left-aligned line in a 36px
             * sliver turned "My Computer" into "My..." and the column into noise. */
            uint32_t txt_col = icon->selected ? tc()->win_title_text : tc()->icon_text;
            int cx = icon->x + DOSGUI_ICON_SIZE / 2;          /* cell centre */
            char l1[64], l2[64];
            wm_fit_lines(icon->name, max_label_w, l1, sizeof l1, l2, sizeof l2);
            int lh = WUBU_FONT_CAP_H + 2;
            for (int line = 0; line < 2; line++) {
                const char *t = line ? l2 : l1;
                if (!t[0]) continue;
                int tw2 = vbe_text_width(t, 1);
                int tx = cx - tw2 / 2;
                if (icon->selected)
                    vbe_fill_rect(tx - 2, label_y + line * lh - 1, tw2 + 4, lh,
                                  tc()->select_bg);
                vbe_draw_text(tx + 1, label_y + line * lh, t, tc()->icon_text_shadow, 1);
                vbe_draw_text(tx, label_y + line * lh - 1, t, txt_col, 1);
            }
        }
    }

    /* Rubber-band drag-select lasso (Classic Mac / Win98 lesson). */
if (g_dwm.lasso_active) {
        int x0 = g_dwm.lasso_x0, y0 = g_dwm.lasso_y0;
        int x1 = g_dwm.lasso_x1, y1 = g_dwm.lasso_y1;
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        int w = x1 - x0, h = y1 - y0;
        /* Translucent navy fill + 1px solid border (Win98 rubber band). */
        vbe_blend_rect(x0, y0, w, h, tc()->select_bg, 60);
        vbe_rect(x0, y0, w, h, tc()->select_bg);
    }

    for (int j = 0; j < ((skip & 4) ? 0 : g_dwm.nz); j++) {
        int idx = g_dwm.zorder[j];
        DosGuiWindow *w = &g_dwm.windows[idx];
        if (w->alive && w->desktop == g_dwm.current_desktop && !(w->flags & DOSGUI_WIN_MINIMIZED)) {
            /* Confine the entire window frame to the area ABOVE the taskbar.
             * Without this, a window whose bottom extends into the taskbar
             * band would paint its title bar / close "X" over the taskbar,
             * producing the "FILE MANAGER CLOSE" text-merge artifact. */
            int task_h = taskbar_height_dynamic();
            vbe_set_clip(0, 0, fb_w, taskbar_top());
            if (idx == g_dwm.focused_id && wm_shadow_render)
                wm_shadow_render(w);
            draw_window(idx, fb, fb_w, fb_h);
            vbe_reset_clip();
        }
    }

    /* The screen-edge slime seam, then the window-rim islands above it. */
    wm_slime_render(fb, fb_w, fb_h);
    wm_rim_render(fb, fb_w, fb_h);

    /* Accessibility cluster: drawn over the focused window (top of z-order),
     * before the taskbar so it never paints under it. Skipped for minimized
     * windows (nothing to grab). */

    /* Desktop mascot (Bonzi Buddy): drawn above windows, below taskbar.
     * Balloon Help (System 7 lesson): when the cursor hovers a desktop
     * icon, the buddy's bubble becomes a tip about that icon; otherwise
     * it returns to the greeting. */
    if (wubu_bonzi_is_enabled()) {
        int hov = icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
        /* Only tip about an icon when NO window covers it (the cursor must
         * actually be on the desktop, not on a window above the icon). */
        if (hov >= 0 && hit_test(g_dwm.mouse_x, g_dwm.mouse_y) < 0) {
            DosGuiIcon *hic = &g_dwm.icons[hov];
            wubu_bonzi_set_bubble(hic->name, "Click to open");
        } else {
            wubu_bonzi_set_bubble("Hi! I'm WuBu Buddy!",
                                  "Click me for the AGI!");
        }
        /* The shader orb, not the PNG sprite: the sprite is what the desktop kept
         * drawing after the metaball renderer was built. */
        extern void wubu_bonzi_draw_orb(uint32_t *, int, int) __attribute__((weak));
        if (wubu_bonzi_draw_orb) wubu_bonzi_draw_orb(fb, fb_w, fb_h);
        else                    wubu_bonzi_draw(fb, fb_w, fb_h);
    }

    {
        extern __attribute__((weak)) void taskbar_overflow_render(uint32_t *, int, int);
        if (taskbar_overflow_render) taskbar_overflow_render(fb, fb_w, fb_h);
    }
    {
        extern __attribute__((weak)) void alt_tab_render(uint32_t *, int, int);
        if (alt_tab_render) alt_tab_render(fb, fb_w, fb_h);
    }
    {
        extern __attribute__((weak)) void projection_flyout_render(uint32_t *, int, int);
        if (projection_flyout_render) projection_flyout_render(fb, fb_w, fb_h);
    }
    /* Snap assist sits above the windows and below the bar: it is a choice about windows,
     * not chrome. Weak so a WM-only link without the shell still renders. */
    {
        extern __attribute__((weak)) void win11_snap_assist_render(uint32_t *, int, int);
        if (win11_snap_assist_render) win11_snap_assist_render(fb, fb_w, fb_h);
    }
    if (!(skip & 8)) taskbar_render(fb, fb_w, fb_h);
    notif_center_render(fb, fb_w, fb_h);

    /* Clock popup menu (Win98 parity): the clock's own calendar panel. */
    if (g_dwm.clock_menu_open)
        clock_menu_render(fb, fb_w, fb_h);

    /* Win+Tab desktop overview sits above everything except the cursor. */
    wm_overview_render(fb, fb_w, fb_h);

    /* Windows 11 shell layers: the widgets board, then the snap flyout.
     * Weak: a build without the shell skips them. */
    if (win11_shell_render) win11_shell_render(fb, fb_w, fb_h);

    /* Touchpad gestures are polled here so they advance once per frame. */
    wm_gesture_poll();

    /* Win+R run dialog, then the Win+X power menu (both modal overlays). */
    run_dialog_render(fb, fb_w, fb_h);
    power_menu_render(fb, fb_w, fb_h);

    /* The lock screen is the topmost layer: nothing else is visible. */
    wubu_lock_screen_render(fb, fb_w, fb_h);

    /* Idle screen saver sits above everything except the cursor. */
    wubu_screensaver_tick(33);            /* ~30 fps frame budget */
    wubu_screensaver_render(fb, fb_w, fb_h);

    /* Win+A quick settings, then the system OSD (volume / brightness). */
    quick_settings_render(fb, fb_w, fb_h);
    /* System OSD (volume / brightness) and its fade. */
    wubu_osd_tick();
    wubu_osd_render(fb, fb_w, fb_h);

    /* Human-lag cursor: the drawn cursor eases toward the input target.
     * Input LOGIC uses g_dwm.mouse_x/y so clicks land exactly where
     * intended.  The ease must be fast enough to read as the pointer, not
     * as a trailing ghost: 0.30/frame at 30fps took ~7 frames (~230ms) to
     * converge, which the user experienced as a laggy mouse. */
    g_dwm.cursor_x += (int)((g_dwm.mouse_x - g_dwm.cursor_x) * 0.65f);
    g_dwm.cursor_y += (int)((g_dwm.mouse_y - g_dwm.cursor_y) * 0.65f);
    if (g_dwm.cursor_x == 0 && g_dwm.cursor_y == 0) {
        g_dwm.cursor_x = g_dwm.mouse_x;
        g_dwm.cursor_y = g_dwm.mouse_y;
    }
    vbe_draw_cursor(g_dwm.cursor_x, g_dwm.cursor_y);

    /* Surface effects, from /n/effect.  A program writes "tint 50 ff0000" to
     * /n/effect/set and the NEXT frame is tinted: the stack it edits is the
     * same object applied here.
     *
     * The weak symbol must be TESTED before it is called, exactly as
     * launch_wet_applet does in apps.c: a weak-declared function that has no
     * definition is relocated to 0, and a direct call to it jumps to NULL --
     * the test reads the address first, so an unlinked runtime is a clean
     * no-op rather than a segfault. */
    if (wubu_ns_effect_stack) {
        const WubuEffectStack *fx = wubu_ns_effect_stack();
        bool ran = false;
        /* Prefer the GPU; the CPU engine is the documented fallback. */
        if (wubu_vk_effect_desktop_apply && fx &&
            wubu_vk_effect_desktop_apply(fx, fb, fb_w, fb_h))
            ran = true;
        if (!ran && fx) wubu_effect_apply(fx, fb, fb_w, fb_h);
    }
}

/* -- Back-compat wrappers (kept for hosted.c / tests) ---------- */

void wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    wm_render(fb, fb_w, fb_h);
}

/* -- Invalidations (bookkeeping for a future dirty-region renderer) --
 * The current hosted loop redraws every frame, so these are not
 * consumed by the renderer yet, but the API is real and tested. */

#define DOSGUI_DIRTY_MAX 64
static int    g_dirty_ids[DOSGUI_DIRTY_MAX];
static int    g_dirty_count = 0;
static bool   g_dirty_all   = false;

void wm_invalidate(DosGuiWindow *win) {
    if (!win) { g_dirty_all = true; return; }
    if (g_dirty_count < DOSGUI_DIRTY_MAX)
        g_dirty_ids[g_dirty_count++] = win->id;
}

void wm_invalidate_all(void) {
    g_dirty_all = true;
}

bool wm_poll_dirty(int *out_id) {
    if (g_dirty_all) {
        g_dirty_all = false;
        g_dirty_count = 0;
        if (out_id) *out_id = -1;
        return true;
    }
    if (g_dirty_count > 0) {
        if (out_id) *out_id = g_dirty_ids[--g_dirty_count];
        return true;
    }
    return false;
}

int wm_dirty_count(void) {
    return g_dirty_all ? -1 : g_dirty_count;
}