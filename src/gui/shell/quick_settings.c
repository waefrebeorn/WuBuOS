/*
 * quick_settings.c -- Win+A quick settings panel
 *
 * Windows 11's Win+A panel is the model: one overlay listing the state a user
 * wants at a glance (wifi, ethernet, bluetooth, volume, brightness) with the
 * ones that can change responding to a click.
 *
 * Status comes from the kernel drivers (wubu_net_*, wubu_bt_*, wubu_backlight_*);
 * volume and brightness come from the OSD module, so the panel and the OSD
 * always agree.
 */

#include "wubu_gui_metrics.h"
#include "../wm.h"
#include "../wm_internal.h"
#include "../kernel/vbe.h"
#include "../kernel/wubu_drv_net.h"
#include "../kernel/wubu_bt.h"
#include "../kernel/wubu_backlight.h"
#include "../wubu_theme.h"
#include "wubu_osd.h"

#include <stdio.h>
#include <stdlib.h>

/* The kernel drivers are not linked into every WM target (the headless WM
 * tests link no driver raft), so probe them weakly -- the same pattern the
 * theme engine uses for its kernel node getter. */
extern __attribute__((weak)) int wubu_net_wifi_present(void);
extern __attribute__((weak)) int wubu_net_wifi_link(void);
extern __attribute__((weak)) int wubu_net_eth_present(void);
extern __attribute__((weak)) int wubu_net_eth_link(void);
extern __attribute__((weak)) int wubu_bt_present(void);

#define HAVE(fn) ((fn) != NULL)

/* Power profiles are the reference desktop's power-profiles-daemon: one control that moves the
 * machine between battery-first and latency-first. Weak, because the WM tests link no runtime. */
#include "../../runtime/wubu_power_profile.h"
/* Declared by the header above. Present in every product link; a graph without the runtime
 * still links because the header's symbols are satisfied by runtime unit it already carries. */
extern int wubu_power_profile_count(void) __attribute__((weak));
extern int wubu_power_profile_current(void) __attribute__((weak));
extern WubuPowerSetResult wubu_power_set(int id) __attribute__((weak));
extern const WubuPowerProfileDef *wubu_power_profile_def(int i) __attribute__((weak));
extern const char *wubu_power_result_str(WubuPowerSetResult r) __attribute__((weak));

static int wubu_power_callable(void) {
    return wubu_power_profile_count != NULL && wubu_power_profile_count() > 0;
}

#define QS_ROWS        6
#define QS_ROW_H       WUBU_METRIC_CONTROL_H /* was 30: off the grid */
#define QS_WIDTH       280
#define QS_PAD         12

static bool g_qs_open = false;

void quick_settings_toggle(void) {
    g_qs_open = !g_qs_open;
}

bool quick_settings_is_open(void) {
    return g_qs_open;
}

/* Panel rect, shared by the renderer and the hit-test. */
static void qs_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    *w = QS_WIDTH;
    *h = QS_PAD * 2 + QS_ROWS * QS_ROW_H;
    *x = fb_w - *w - 8;
    *y = fb_h - taskbar_height_dynamic() - *h - 8;
}

void quick_settings_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    qs_rect(fb_w, fb_h, x, y, w, h);
}

/* Which row is under the point, or -1. */
static int qs_hit(int mx, int my, int fb_w, int fb_h) {
    int x, y, w, h;
    qs_rect(fb_w, fb_h, &x, &y, &w, &h);
    if (mx < x || mx >= x + w || my < y || my >= y + h) return -1;
    int row = (my - y - QS_PAD) / QS_ROW_H;
    if (row < 0 || row >= QS_ROWS) return -1;
    return row;
}

bool quick_settings_handle_click(int mx, int my, int fb_w, int fb_h) {
    if (!g_qs_open) return false;

    int row = qs_hit(mx, my, fb_w, fb_h);
    if (row < 0) { g_qs_open = false; return true; }

    /* Volume (3) and brightness (4) respond to a click by stepping up. */
    if (row == 3) {
        wubu_osd_set_volume(wubu_osd_volume() + 0.1f);
        wubu_osd_notify();
        return true;
    }
    if (row == 4) {
        wubu_osd_set_brightness(wubu_osd_brightness() + 0.1f);
        wubu_osd_notify();
        return true;
    }
    /* Power (5) cycles saver -> balanced -> performance, and says so when the hardware refuses:
     * a control that silently does nothing is the same lie as a dead desktop icon. */
    if (row == 5 && wubu_power_callable()) {
        int cur = wubu_power_profile_current();
        int next = (cur == WUBU_POWER_SAVER) ? WUBU_POWER_BALANCED
                 : (cur == WUBU_POWER_BALANCED) ? WUBU_POWER_PERF
                 : WUBU_POWER_SAVER;
        int r = wubu_power_set(next);
        /* Only a refusal is worth a line: it means the hardware (or the permissions) said no,
         * and a control that silently does nothing is the same lie as a dead desktop icon. */
        if (r != WUBU_POWER_OK && wubu_power_result_str)
            fprintf(stderr, "[quick_settings] power profile refused: %s\n", wubu_power_result_str(r));
        return true;
    }
    /* The connectivity rows are status-only: no setter exists yet. */
    return true;
}

void quick_settings_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_qs_open) return;

    const WubuThemeColors *c = tc();
    int x, y, w, h;
    qs_rect(fb_w, fb_h, &x, &y, &w, &h);

    vbe_fill_rect_rounded(x, y, w, h, 8, c->win_face);
    vbe_rect_rounded(x, y, w, h, 8, c->border_light);

    char line[64];

    for (int r = 0; r < QS_ROWS; r++) {
        int ry = y + QS_PAD + r * QS_ROW_H;

        const char *label = "";
        switch (r) {
            case 0:
                label = (HAVE(wubu_net_wifi_present) && wubu_net_wifi_present() &&
                         HAVE(wubu_net_wifi_link) && wubu_net_wifi_link())
                        ? "WiFi: connected" : "WiFi: off";
                break;
            case 1:
                label = (HAVE(wubu_net_eth_present) && wubu_net_eth_present() &&
                         HAVE(wubu_net_eth_link) && wubu_net_eth_link())
                        ? "Ethernet: connected" : "Ethernet: off";
                break;
            case 2:
                label = (HAVE(wubu_bt_present) && wubu_bt_present())
                        ? "Bluetooth: on" : "Bluetooth: off";
                break;
            case 3:
                snprintf(line, sizeof(line), "Volume: %d%%%s",
                         (int)(wubu_osd_volume() * 100.0f + 0.5f),
                         wubu_osd_muted() ? " (muted)" : "");
                label = line;
                break;
            case 4:
                snprintf(line, sizeof(line), "Brightness: %d%%",
                         (int)(wubu_osd_brightness() * 100.0f + 0.5f));
                label = line;
                break;
            case 5: {
                int cur = wubu_power_callable() ? wubu_power_profile_current()
                                                : WUBU_POWER_UNKNOWN;
                const char *nm = "n/a";
                if (wubu_power_callable()) {
                    for (int i = 0; i < wubu_power_profile_count(); i++) {
                        const WubuPowerProfileDef *d = wubu_power_profile_def(i);
                        if (d && d->id == cur) nm = d->name;
                    }
                }
                snprintf(line, sizeof(line), "Power: %s%s", nm,
                         cur == WUBU_POWER_UNKNOWN ? " (custom)" : "");
                label = line;
                break;
            }
            default: break;
        }

        /* Ink-centred in the row, from the font metrics -- "ry + 8" was a hand-typed baseline. */
        vbe_draw_text(x + QS_PAD, WUBU_TEXT_LABEL_Y(ry, QS_ROW_H), label, c->btn_text, 1);
    }
}
