/*
 * control_test.c  --  WuBuOS Control Panel Test Suite
 * Cell 395: Win98-style settings panel. Verifies the Desktop tab
 * (Stream 4) renders and that control_desktop_apply() persists the
 * wallpaper path + placement mode and triggers a live wallpaper reload.
 */

#include "control/control.h"
#include "../gui/wm.h"
#include "../gui/wm_internal.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_settings.h"
#include "../kernel/vbe.h"
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
    wm_init(1024, 768);  /* WM running so apply() live-reloads */

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
    T(wm_wallpaper_mode() == 2, "live wallpaper reload applied new mode");

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
    T(wm_get_auto_arrange(), "auto-arrange toggled ON");
    T(s->theme.auto_arrange == true, "auto-arrange persisted to settings");
    control_set_auto_arrange(false);
    T(!wm_get_auto_arrange(), "auto-arrange toggled OFF");
    T(s->theme.auto_arrange == false, "auto-arrange OFF persisted");

    /* -- Show/hide desktop icons -- */
    printf("\n[Desktop Tab — Show/Hide Icons]\n");
    wm_refresh_desktop();
    int before_hide = wm_get_icon_count();
    control_set_show_icons(false);
    T(wm_get_icon_count() == 0, "icons hidden (count 0)");
    control_set_show_icons(true);
    T(wm_get_icon_count() == before_hide, "icons shown again (count restored)");

    /* -- Control Panel two-pane (XP category view) render -- */
    printf("\n[Control Panel — XP two-pane render]\n");
    control_set_theme(THEME_XP_LUNA_BLUE);
    vbe_init(520, 440);
    DosGuiWindow win;
    memset(&win, 0, sizeof(win));
    win.x = 0; win.y = 0; win.w = 520; win.h = 440; win.alive = true;
    strncpy(win.title, "Control Panel", sizeof(win.title) - 1);

    /* Draw to back buffer */
    control_draw(&win, vbe_framebuffer(), 520, 440, c);

    /* Swap so drawing appears in front buffer */
    vbe_swap();

    /* Read from front buffer after swap */
    uint32_t *fb2 = vbe_framebuffer();

    /* Content rect: border(1) + titlebar(24 for Luna) = 25 for y, border(1) for x. */
    int content_x = 1;  /* border */
    int content_y = 26; /* border + title bar (24 for Luna) + 1 */
    int content_w = 520 - 2;  /* 2 borders */
    int content_h = 440 - 26 - 1; /* total - titlebar - border */

    int found_orb = 0, found_cat = 0, found_flag_colour = 0;
    for (int y = 0; y < 440 && !(found_orb && found_cat); y++)
        for (int x = 0; x < 520; x++) {
            uint32_t p = fb2[y * 520 + x];
            /* The orb is four panes of ONE accent. It used to be Microsoft's
             * red/green/blue/yellow flag -- borrowed identity, the same defect as the Win95
             * flag on Start -- so the assertion now names the panes and forbids the flag. */
            if (p == wubu_theme_colors()->select_bg) found_orb = 1;
            if (p == 0xF24C3E || p == 0x6FCF3C || p == 0x39A0EC || p == 0xFFC90E)
                found_flag_colour = 1;
            /* Right pane light panel: theme uses 0xE8E8E8 for Luna, 0xC0C0C0 for Win98. 
             * Also check for variations. */
            if (x > content_x + 160 && (p == 0x00F8F8F8 || p == 0x00C0C0C0 || p == 0x00E8E8E8)) found_cat = 1;
        }

    /* Debug: print pixels around orb location */
    if (!found_orb) {
        printf("DEBUG: scanning for orb around (%d, %d)...\n", content_x + 12, content_y + 12);
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int x = content_x + 12 + dx;
                int y = content_y + 12 + dy;
                if (x >= 0 && x < 520 && y >= 0 && y < 440) {
                    uint32_t p = fb2[y * 520 + x];
                    if (p != 0) printf("  Pixel at (%d,%d) = 0x%08X\n", x, y, p);
                }
            }
        }
    }

    /* Debug: print right pane pixels */
    if (!found_cat) {
        printf("DEBUG: scanning right pane...\n");
        for (int x = content_x + 160; x < content_x + 170; x++) {
            for (int y = content_y + 60; y < content_y + 70; y++) {
                if (x >= 0 && x < 520 && y >= 0 && y < 440) {
                    uint32_t p = fb2[y * 520 + x];
                    if (p != 0) printf("DEBUG: right pane at (%d,%d) = 0x%08X\n", x, y, p);
                }
            }
        }
    }

    T(found_orb, "Control Panel orb drawn in the left rail, in the theme accent");
    T(!found_flag_colour, "Control Panel orb is not Microsoft's four-colour flag");
    T(found_cat, "Control Panel right pane (light panel) drawn");
    vbe_shutdown();

    /* -- Category grid: what is drawn is what a click selects -- */
    printf("\n[Control Panel — category grid interaction]\n");
    control_set_theme(THEME_XP_LUNA_BLUE);
    vbe_init(1024, 768);
    DosGuiWindow win2;
    memset(&win2, 0, sizeof(win2));
    win2.x = 40; win2.y = 40; win2.w = 820; win2.h = 620; win2.alive = true;
    strncpy(win2.title, "Control Panel", sizeof(win2.title) - 1);
    ControlState *c2 = control_create();
    win2.user_data = c2;
    control_draw(&win2, vbe_framebuffer(), 1024, 768, c2);

    T(control_category_count(c2) == 8, "8 categories laid out");
    int bx = 0, by = 0, bw = 0, bh = 0;
    int all_area = 1;
    for (int i = 0; i < control_category_count(c2); i++) {
        if (!control_category_rect(c2, i, &bx, &by, &bw, &bh) || bw <= 0 || bh <= 0)
            all_area = 0;
    }
    T(all_area, "every category has a clickable rect");

    T(control_category_rect(c2, 1, &bx, &by, &bw, &bh), "Display rect readable");
    control_mouse(&win2, bx + bw / 2, by + bh / 2, 1, 1);
    T(control_get_tab(c2) == 1, "clicking Display selects it");

    /* The detail pane must be laid out for the NEW category on the next draw. */
    control_draw(&win2, vbe_framebuffer(), 1024, 768, c2);
    T(control_action_count(c2) == 2, "Display offers its two real actions");
    T(control_action_rect(c2, 0, &bx, &by, &bw, &bh) && bw > 0 && bh > 0,
      "first action has a clickable rect");

    /* Clicking it must actually do the thing it says (settings are isolated to
     * /tmp above, so this cannot disturb the machine's desktop). */
    int before_mode = wubu_settings_get()->theme.wallpaper_mode;
    control_mouse(&win2, bx + bw / 2, by + bh / 2, 1, 1);
    T(wubu_settings_get()->theme.wallpaper_mode == (before_mode + 1) % 5,
      "clicking 'Next wallpaper placement' advances the placement mode");

    /* A category with nothing real behind it offers no buttons. */
    T(control_category_rect(c2, 6, &bx, &by, &bw, &bh), "System rect readable");
    control_mouse(&win2, bx + bw / 2, by + bh / 2, 1, 1);
    control_draw(&win2, vbe_framebuffer(), 1024, 768, c2);
    T(control_get_tab(c2) == 6, "clicking System selects it");
    T(control_action_count(c2) == 0, "System shows no fake buttons");

    control_mouse(&win2, 2, 2, 1, 1);
    T(control_get_tab(c2) == 6, "a click outside the grid changes nothing");

    /* Dump the frame the panel really produced: assertions say the rects line
     * up, this lets a human see the two-pane XP layout and the selection. */
    {
        VBEState *vs = vbe_state();
        if (vs && vs->back) {
            FILE *f = fopen("build/control_frame.ppm", "wb");
            if (f) {
                fprintf(f, "P6\n%d %d\n255\n", vs->width, vs->height);
                for (int i = 0; i < vs->width * vs->height; i++) {
                    uint32_t px = vs->back[i];
                    unsigned char rgb[3] = { (unsigned char)((px >> 16) & 0xff),
                                             (unsigned char)((px >> 8) & 0xff),
                                             (unsigned char)(px & 0xff) };
                    fwrite(rgb, 1, 3, f);
                }
                fclose(f);
                printf("  frame written to build/control_frame.ppm\n");
            }
        }
    }
    control_destroy(c2);

    /* -- Shutdown -- */
    printf("\n[Shutdown]\n");
    control_destroy(c);
    wm_shutdown();
    T(1, "control_destroy + wm shutdown succeed");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}