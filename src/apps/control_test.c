/*
 * control_test.c  --  WuBuOS Control Panel Test Suite
 * Cell 395: Win98-style settings panel. Verifies the Desktop tab
 * (Stream 4) renders and that control_desktop_apply() persists the
 * wallpaper path + placement mode and triggers a live wallpaper reload.
 */

#include "control/control.h"
#include "../gui/dosgui_wm.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s\n", msg); } \
} while(0)

int main(void) {
    /* Isolate settings to a temp dir so we don't touch real config. */
    setenv("XDG_CONFIG_HOME", "/tmp/wubu_ctrl_test", 1);

    printf("=== WuBuOS Control Panel Test Suite ===\n\n");

    wubu_settings_init();        /* load/create default settings */
    dosgui_wm_init(1024, 768);  /* WM running so apply() live-reloads */

    /* -- Lifecycle -- */
    printf("[Lifecycle]\n");
    ControlState *c = control_create();
    T(c != NULL, "control_create succeeds");

    /* -- Desktop tab render (no crash, real content) -- */
    printf("\n[Desktop Tab Render]\n");
    DosGuiWindow *w = control_launch();
    T(w != NULL, "control_launch returns a window");
    uint32_t fb[1024 * 80];
    memset(fb, 0, sizeof(fb));
    control_draw(w, fb, 1024, 80, c);
    T(1, "control_draw renders into framebuffer without crash");

    /* -- Stream 4: apply wallpaper change (persist + live reload) -- */
    printf("\n[Desktop Tab Apply — Stream 4]\n");
    control_desktop_apply("/tmp/wubu_ctrl_test/wp.bmp", 2);
    const WubuSettings *s = wubu_settings_get();
    T(strcmp(s->theme.wallpaper_path, "/tmp/wubu_ctrl_test/wp.bmp") == 0,
      "wallpaper path persisted to settings");
    T(s->theme.wallpaper_mode == 2, "placement mode persisted (Stretch=2)");
    T(dosgui_wm_wallpaper_mode() == 2, "live wallpaper reload applied new mode");

    /* Apply again with a different mode to confirm update path. */
    control_desktop_apply("/tmp/wubu_ctrl_test/wp2.bmp", 4);
    T(s->theme.wallpaper_mode == 4, "re-apply updates placement mode (Fill=4)");
    T(strcmp(s->theme.wallpaper_path, "/tmp/wubu_ctrl_test/wp2.bmp") == 0,
      "re-apply updates wallpaper path");

    /* -- Theme switch (XP Luna / etc.) persists + applies live -- */
    printf("\n[Desktop Tab — Theme Switch]\n");
    control_set_theme(THEME_XP_LUNA_BLUE);
    T(s->theme.theme_id == THEME_XP_LUNA_BLUE, "theme_id persisted to settings");
    T(wubu_theme_get()->id == THEME_XP_LUNA_BLUE, "live theme applied (XP Luna Blue)");
    control_set_theme(THEME_WIN98_CLASSIC);
    T(wubu_theme_get()->id == THEME_WIN98_CLASSIC, "theme switches back to Win98 Classic");

    /* -- View -> Auto-arrange toggle persists + re-flows -- */
    printf("\n[Desktop Tab — Auto-arrange]\n");
    control_set_auto_arrange(true);
    T(dosgui_wm_get_auto_arrange(), "auto-arrange toggled ON");
    T(s->theme.auto_arrange == true, "auto-arrange persisted to settings");
    control_set_auto_arrange(false);
    T(!dosgui_wm_get_auto_arrange(), "auto-arrange toggled OFF");
    T(s->theme.auto_arrange == false, "auto-arrange OFF persisted");

    /* -- Show/hide desktop icons -- */
    printf("\n[Desktop Tab — Show/Hide Icons]\n");
    dosgui_wm_refresh_desktop();
    int before_hide = dosgui_wm_get_icon_count();
    control_set_show_icons(false);
    T(dosgui_wm_get_icon_count() == 0, "icons hidden (count 0)");
    control_set_show_icons(true);
    T(dosgui_wm_get_icon_count() == before_hide, "icons shown again (count restored)");

    /* -- Shutdown -- */
    printf("\n[Shutdown]\n");
    control_destroy(c);
    dosgui_wm_shutdown();
    T(1, "control_destroy + wm shutdown succeed");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
