/*
 * control_panel_firmware.c -- the Firmware applet: this machine's BIOS/SMBIOS identity, and whether
 * anything on this machine can update it.
 *
 * Parity: fwupd's "System Firmware" row plus Windows' System Information. Two rules:
 *   - every value comes from a real DMI attribute (wubu_firmware_read), never a fabricated string;
 *   - when no firmware update agent exists, the applet SAYS SO. There is no Update button that cannot
 *     do anything -- a greyed control with no backend is the kind of lie this tree keeps removing.
 *
 * C11.
 */
#include "control_panel.h"
#include "wubu_tokens.h"
#include "control_panel_text.h"
#include "../runtime/wubu_firmware.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    WubuFirmware fw;
    bool         loaded;
} FwAppletData;

static void fw_load(FwAppletData *d)
{
    if (!d) return;
    wubu_firmware_read(&d->fw);
    d->loaded = true;
}

static void fw_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = calloc(1, sizeof(FwAppletData));
    fw_load((FwAppletData *)applet->user_data);
}

/* One "label  value" row through the shared helper, in the active theme's colours. */
static void row(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w,
                const char *label, const char *value)
{
    const WubuThemeColors *c = wubu_theme_colors();
    cp_row(fb, fb_w, fb_h, x, y, w, label, value ? value : "", c->body_text, c->body_text);
}

static void fw_render(CpApplet *applet, uint32_t *fb,
                      int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    FwAppletData *d = (FwAppletData *)applet->user_data;
    if (!d) return;
    if (!d->loaded) fw_load(d);
    const WubuFirmware *fw = &d->fw;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_page_title(fb, fb_w, fb_h, x + 10, y + 6, "System firmware", c->select_bg);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 24,
                 "What this machine reports about itself, and what can update it.",
                 c->body_text_ina, 1);

    int ry = y + 52;
    const int step = 22;

    if (!fw->have_dmi) {
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Firmware:  ", "not exposed by this machine");
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Source:    ", wubu_firmware_dmi_root());
    } else {
        char line[160];
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "BIOS:      ", wubu_firmware_summary(fw));
        ry += step;
        if (fw->bios_date[0]) {
            row(fb, fb_w, fb_h, x + 10, ry, w - 20, "BIOS date: ", fw->bios_date);
            ry += step;
        }
        if (fw->sys_vendor[0] || fw->product_name[0]) {
            snprintf(line, sizeof line, "%s%s%s", fw->sys_vendor,
                     (fw->sys_vendor[0] && fw->product_name[0]) ? "  " : "", fw->product_name);
            row(fb, fb_w, fb_h, x + 10, ry, w - 20, "System:    ", line);
            ry += step;
        }
        if (fw->board_name[0]) {
            row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Board:     ", fw->board_name);
            ry += step;
        }
    }

    ry += 8;
    if (fw->agent_present) {
        char line[WUBU_FW_PATH_MAX + 32];
        snprintf(line, sizeof line, "%s", fw->agent_path);
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Updater:   ", line);
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Updates:   ",
            "run the updater above to check the vendor's firmware");
    } else {
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Updater:   ", "no firmware update agent on this machine");
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Updates:   ",
            "install fwupd to check for firmware updates");
    }

    /* The provenance line: the user must be able to tell a measurement from a decoration. */
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - cp_text_line_h() - WUBU_SPACE_SM,
                 "Read from SMBIOS/DMI; nothing here is simulated.",
                 c->body_text_ina, 1);
}

static void fw_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)x; (void)y; (void)btn; (void)kind;
    FwAppletData *d = (FwAppletData *)applet->user_data;
    if (d) fw_load(d);
}

static void fw_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    if (key == 0x3F) {                       /* F5 re-reads the machine */
        FwAppletData *d = (FwAppletData *)applet->user_data;
        if (d) fw_load(d);
    }
}

static void fw_cleanup(CpApplet *applet)
{
    if (applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet cp_create_firmware_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_FIRMWARE;
    snprintf(a.name, sizeof(a.name), "Firmware");
    snprintf(a.desc, sizeof(a.desc), "BIOS and system identity, and what can update it");
    a.icon_color = 0xFF00A050;
    a.init = fw_init;
    a.render = fw_render;
    a.mouse = fw_mouse;
    a.key = fw_key;
    a.cleanup = fw_cleanup;
    return a;
}

/* Test hook: the summary the applet would draw, without a framebuffer. */
int cp_firmware_test_summary(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_FIRMWARE);
    if (!a) { out[0] = '\0'; return -1; }
    FwAppletData *d = (FwAppletData *)a->user_data;
    if (!d) { out[0] = '\0'; return -1; }
    snprintf(out, cap, "%s", wubu_firmware_summary(&d->fw));
    return 0;
}
