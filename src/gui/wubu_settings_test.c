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
    CHECK(native_w == 18, "native width of \"abc\" is 18px");

    s->a11y.font_scale = 2.0;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 2, "font_scale 2.0 -> text scale 2");
    CHECK(vbe_text_width("abc", 1) == native_w * 2,
          "vbe_text_width doubles with the scale (layout stays coherent)");

    s->a11y.font_scale = 1.0;
    wubu_settings_apply_live();
    CHECK(vbe_text_scale() == 1, "font_scale back to 1.0 -> text scale 1");
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
    CHECK(wubu_theme_colors()->desktop_bg == 0x202020,
          "dark desktop background");
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
    CHECK(wubu_theme_colors()->desktop_bg == 0x202020,
          "the dark palette is what renders");
}

int main(void) {
    printf("=== wubu_settings_test ===\n");
    wubu_theme_init();
    wubu_settings_init();

    test_font_scale();
    test_high_contrast();
    test_reduce_motion();
    test_dark_theme();

    printf("\n=== wubu_settings: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
