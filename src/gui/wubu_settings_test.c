/*
 * wubu_settings_test.c -- prove the settings daemon APPLIES its values.
 *
 * Before this gate, wubu_settings_apply_live() only called wubu_theme_set():
 * a11y.font_scale, a11y.high_contrast and a11y.reduce_animations were
 * persisted to JSON and consumed by NOTHING (zero external callers of every
 * helper).  A settings panel that saves but never applies is a screenshot,
 * not a setting.  Each test below pins the LIVE effect of one setting.
 */
#include "wubu_settings.h"
#include "wubu_theme.h"
#include "shell/wubu_notify.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/* -- Minimal host stubs --------------------------------------------
 * This test drives the settings/theme/notify layers directly, so there is
 * no window manager and no kernel.  These are the only externals those
 * layers touch; each stub is the contract value a host would provide. */

int  taskbar_height(void) { return 28; }
int  wm_screen_w(void)    { return 800; }
int  wm_screen_h(void)    { return 600; }
void klog_printf(const char *fmt, ...) { (void)fmt; }
void *mem_alloc(size_t n) { return malloc(n); }
void  mem_free(void *p)   { free(p); }

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else { printf("  [FAIL] %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail++; } \
} while (0)

/* -- a11y.font_scale -> global UI text scale ---------------------- */

static void test_font_scale(void) {
    printf("\n[a11y.font_scale]\n");
    WubuSettings *s = wubu_settings_mut();

    s->a11y.font_scale = 1.0;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 1, "font_scale 1.0 -> text scale 1");
    int native_w = vbe_text_width("abc", 1);
    /* The old assertion was `== 18` -- 3 characters x the BITMAP 6px advance. The
     * renderer is proportional now (baked face, per-glyph advances), so a fixed
     * per-character pitch is no longer the contract. What must hold is: text has width,
     * width grows with length, and "iii" is narrower than "MMM" -- i.e. it is proportional
     * and not accidentally a constant. */
    CHECK(native_w > 0, "native width of \"abc\" is positive");
    CHECK(vbe_text_width("abcdef", 1) > native_w, "width grows with the string");
    CHECK(vbe_text_width("iii", 1) < vbe_text_width("MMM", 1),
          "advances are proportional, not a fixed pitch");

    s->a11y.font_scale = 2.0;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 2, "font_scale 2.0 -> text scale 2");
    /* Scaling goes through the bitmap path (a coverage mask would blur if stretched), so
     * the honest assertions are: it grew, and layout that measured it still fits. */
    CHECK(vbe_text_width("abc", 1) > native_w,
          "vbe_text_width grows with the scale (layout stays coherent)");

    s->a11y.font_scale = 1.0;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 1, "font_scale back to 1.0 -> text scale 1");

    /* DPI was written to the settings file and read back by nobody: a 192 dpi machine drew exactly like a
     * 96 dpi one. It is now the baseline for text size, so this must move. */
    s->fonts.dpi = 192;
    s->a11y.font_scale = 1.0;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 2, "dpi 192 at font_scale 1.0 -> text scale 2 (dpi is live)");
    s->fonts.dpi = 96;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 1, "dpi back to 96 -> text scale 1");

    /* font_size was the same decoration dpi was: written, read, consumed by nothing. It is the base px now
     * (13 = the baked face), so choosing 26 must actually double the text. */
    s->fonts.font_size = 26;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 2, "font_size 26 -> text scale 2 (font_size is live)");
    s->fonts.font_size = 13;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 1, "font_size back to 13 -> text scale 1");
}

/* -- init applies EVERYTHING, not just the theme ------------------- */

static void test_init_applies_saved_settings(void) {
    printf("\n[init applies saved settings]\n");
    WubuSettings *s = wubu_settings_mut();
    s->a11y.font_scale = 2.0;
    s->fonts.dpi = 96;
    wubu_settings_save();

    vbe_set_text_scale(1);                 /* a fresh process starts at 1 */
    CHECK(wubu_settings_init() == 0, "init runs");
    CHECK(vbe_text_scale() == 2,
          "a saved font scale is applied at BOOT, not only when Settings is opened");

    s = wubu_settings_mut();
    s->a11y.font_scale = 1.0;
    wubu_settings_apply_live();
}

/* -- a11y.high_contrast -> live palette override ------------------ */

static void test_high_contrast(void) {
    printf("\n[a11y.high_contrast]\n");
    WubuSettings *s = wubu_settings_mut();

    s->a11y.high_contrast = false;
    wubu_settings_apply_live();
    uint32_t normal_bg = wubu_theme_colors()->desktop_bg;

    s->a11y.high_contrast = true;
    wubu_settings_apply_live();
    CHECK(wubu_theme_high_contrast(), "high_contrast flag is set");
    CHECK(wubu_theme_colors()->desktop_bg == 0x000000, "desktop goes black");
    CHECK(wubu_theme_colors()->win_title_text == 0xFFFFFF, "title text goes white");

    /* The override must survive a base-theme switch. */
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    CHECK(wubu_theme_high_contrast(), "high_contrast survives a theme switch");
    CHECK(wubu_theme_colors()->desktop_bg == 0x000000,
          "desktop stays black after the theme switch");

    s->a11y.high_contrast = false;
    wubu_settings_apply_live();
    CHECK(!wubu_theme_high_contrast(), "high_contrast clears");
    CHECK(wubu_theme_colors()->desktop_bg == normal_bg,
          "the pre-override palette is restored exactly");
}

/* -- a11y.reduce_animations -> notification slide ---------------- */

static void test_reduce_motion(void) {
    printf("\n[a11y.reduce_animations]\n");
    WubuSettings *s = wubu_settings_mut();
    wubu_notify_init();

    Notification n;
    memset(&n, 0, sizeof(n));
    snprintf(n.app_name, sizeof(n.app_name), "settings-test");
    snprintf(n.summary, sizeof(n.summary), "reduce motion");

    uint32_t fb[800 * 600];
    memset(fb, 0, sizeof(fb));

    /* Reduce motion ON: closing settles in a single render. */
    s->a11y.reduce_animations = true;
    wubu_settings_apply_live();
    uint32_t id = wubu_notify_send(&n);
    CHECK(id != 0, "notification posted");
    CHECK(wubu_notify_active_count() == 1, "notification is active");
    wubu_notify_close(id);
    wubu_notify_render(fb, 800, 600);
    CHECK(wubu_notify_active_count() == 0,
          "reduce_motion ON: the close settles in ONE render");

    /* Reduce motion OFF: one render only advances the slide. */
    s->a11y.reduce_animations = false;
    wubu_settings_apply_live();
    id = wubu_notify_send(&n);
    CHECK(id != 0, "second notification posted");
    wubu_notify_close(id);
    wubu_notify_render(fb, 800, 600);
    CHECK(wubu_notify_active_count() == 1,
          "reduce_motion OFF: the close is still animating after one render");

    wubu_notify_clear_all();
    wubu_notify_shutdown();
}

/* -- Dark mode --------------------------------------------------- */

static void test_dark_theme(void) {
    printf("\n[dark mode]\n");
    CHECK(strcmp(wubu_theme_name(THEME_DARK), "Dark") == 0,
          "the Dark theme is registered");

    wubu_theme_set(THEME_DARK);
    /* The dark ladder is measured data, not a remembered constant: desktop 0x17 ->
     * face 0x24 -> title 0x32, hairline 0x3A. This check asserted the OLD 0x202020 and
     * went red when the ladder was fixed -- the test was pinning a value that had been
     * changed on purpose, so it now asserts the ladder's defining property as well. */
    CHECK(wubu_theme_colors()->desktop_bg == 0x171717,
          "dark desktop background (0x17 ladder base)");
    CHECK(wubu_theme_colors()->win_title_text == 0xFFFFFF,
          "white title text on the dark surface");
    CHECK(wubu_theme_colors()->taskbar_bg == 0x1F1F1F,
          "dark taskbar");

    /* It must also be reachable through the settings daemon, which is the
     * path the Control Panel uses. */
    WubuSettings *s = wubu_settings_mut();
    s->theme.theme_id = THEME_DARK;
    wubu_settings_apply_live();
    CHECK(wubu_theme_current() == THEME_DARK, "settings theme_id applies live");
    CHECK(wubu_theme_colors()->desktop_bg == 0x171717,
          "the dark palette is what renders");
}

/* -- the store may not persist a configuration it never built -------- */

/* Internal store flag (wubu_settings_internal.h); this gate drives the
 * public API, so it declares the one internal it needs. */
extern bool g_settings_ready;

static void test_save_guards(void) {
    printf("\n[settings integrity]\n");

    /* A save before defaults is how the live settings file got zeroed: version
     * 0, dpi 0, font_scale 0.00, every toggle false. It must be refused. */
    g_settings_ready = false;
    CHECK(wubu_settings_save() == -1, "an uninitialised store refuses to save");
    g_settings_ready = true;

    /* Schema version 0 cannot come from an initialised store (defaults stamp
     * 1), so such a file is refused on load and defaults win. */
    char path[512];
    snprintf(path, sizeof path, "%s", wubu_settings_path());
    FILE *f = fopen(path, "w");
    CHECK(f != NULL, "the sandbox settings file is writable");
    if (f) {
        fprintf(f, "{\n  \"version\": 0,\n  \"theme\": {\n "
                   "\"theme_id\": 0,\n \"show_desktop_icons\": false\n  },\n"
                   "  \"fonts\": {\n \"dpi\": 0\n  }\n}\n");
        fclose(f);
    }
    CHECK(wubu_settings_load() == -1, "a version-0 settings file is refused");

    /* And a version-1 file still loads: the guard must not reject real ones. */
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\n  \"version\": 1,\n  \"fonts\": {\n \"dpi\": 144\n  }\n}\n");
        fclose(f);
    }
    CHECK(wubu_settings_load() == 0, "a version-1 settings file still loads");
    CHECK(wubu_settings_get()->fonts.dpi == 144, "the loaded value reaches the store");
}

int main(void) {
    printf("=== wubu_settings_test ===\n");
    /* SANDBOX FIRST. This gate saves settings (font_scale 2.0, dpi 192, font_size
     * 26) and used to do it into the live user's settings.conf on every `make
     * test` -- which is exactly how a real desktop came up light-themed, hidden
     * icons and double-sized text. The path resolver no longer caches, so setting
     * XDG_CONFIG_HOME here is enough to keep the whole run in a scratch dir. */
    {
        const char *base = getenv("TMPDIR");
        if (!base || !*base) base = "/tmp";
        char sandbox[512];
        snprintf(sandbox, sizeof sandbox, "%s/wubu_settings_test_cfg", base);
        (void)mkdir(sandbox, 0755);
        setenv("XDG_CONFIG_HOME", sandbox, 1);
        /* Fresh file every run: a leftover sandbox file from a previous run would
         * be LOADED at init and change the baseline these checks assume (a persisted
         * dpi 144 turns "font_scale 2.0 -> scale 2" into 3). */
        char cfg[600];
        snprintf(cfg, sizeof cfg, "%s/wubu/settings.conf", sandbox);
        (void)remove(cfg);
    }
    wubu_theme_init();
    wubu_settings_init();
    CHECK(strncmp(wubu_settings_path(), getenv("XDG_CONFIG_HOME"),
                  strlen(getenv("XDG_CONFIG_HOME"))) == 0,
          "the gate writes to its sandbox, never the live settings file");

    test_font_scale();
    test_init_applies_saved_settings();
    test_high_contrast();
    test_reduce_motion();
    test_dark_theme();
    test_save_guards();

    printf("\n=== wubu_settings: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
