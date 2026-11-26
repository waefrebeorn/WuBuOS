/*
 * pin_confirm.c -- see pin_confirm.h.
 *
 * The pin itself is applied through wubu_taskbar_pin (weak: a WM-less test graph has no taskbar, and
 * that is a legitimate graph).
 *
 * C11.
 */
#include "pin_confirm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static PinConfirmState g_pc;
static int g_ask_pref = -1;        /* -1 = not read yet */

const char *pin_confirm_conf_path(void)
{
    /* Not cached, for the reason the parental policy taught: a path captured before the override was set
     * reads the wrong file and the gate passes for the wrong reason. */
    static char path[512];
    const char *env = getenv("WUBU_PIN_CONF");
    if (env && env[0]) snprintf(path, sizeof path, "%s", env);
    else {
        const char *home = getenv("HOME");
        snprintf(path, sizeof path, "%s/.config/wubu/pin.conf", (home && home[0]) ? home : "/");
    }
    return path;
}

static void save_pref(int ask)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s", pin_confirm_conf_path());
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (dir[0]) (void)mkdir(dir, 0755);
    }
    FILE *f = fopen(pin_confirm_conf_path(), "w");
    if (!f) return;
    fprintf(f, "# WuBuOS taskbar pinning -- ask=0 means 'pin without asking'\nask=%d\n", ask ? 1 : 0);
    fclose(f);
    g_ask_pref = ask ? 1 : 0;
}

void pin_confirm_reset(void)
{
    g_ask_pref = -1;
    memset(&g_pc, 0, sizeof g_pc);
    cd_close(&g_pc.card);
}

bool pin_confirm_should_ask(void)
{
    if (g_ask_pref >= 0) return g_ask_pref != 0;

    g_ask_pref = 1;                                  /* asking is the default */
    FILE *f = fopen(pin_confirm_conf_path(), "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            if (strncmp(line, "ask=", 4) == 0) {
                g_ask_pref = atoi(line + 4) ? 1 : 0;
                break;
            }
        }
        fclose(f);
    }
    return g_ask_pref != 0;
}

bool pin_confirm_ask(const char *key, const char *title, const char *exec, int fb_w, int fb_h)
{
    if (!pin_confirm_should_ask()) return false;

    memset(&g_pc, 0, sizeof g_pc);
    snprintf(g_pc.key, sizeof g_pc.key, "%s", key ? key : "");
    snprintf(g_pc.title, sizeof g_pc.title, "%s", title ? title : "");
    snprintf(g_pc.exec, sizeof g_pc.exec, "%s", exec ? exec : "");

    char heading[CD_TITLE_MAX];
    snprintf(heading, sizeof heading, "Pin %s to the taskbar?",
             g_pc.title[0] ? g_pc.title : "this app");

    cd_open(&g_pc.card, heading, "It can be unpinned from the same menu afterwards.", false, fb_w, fb_h);
    snprintf(g_pc.card.ok_label, sizeof g_pc.card.ok_label, "Pin");
    snprintf(g_pc.card.cancel_label, sizeof g_pc.card.cancel_label, "Cancel");
    snprintf(g_pc.card.footer, sizeof g_pc.card.footer, "Don't ask me again");

    /* A card that does not fit its surface is not a card: the caller proceeds without asking. */
    if (!cd_rect(&g_pc.card, NULL, NULL, NULL, NULL)) {
        cd_close(&g_pc.card);
        return false;
    }

    g_pc.open = true;
    g_pc.answered = 0;
    return true;
}

bool pin_confirm_is_open(void) { return g_pc.open; }
const PinConfirmState *pin_confirm_state(void) { return &g_pc; }

static void apply_pin(void)
{
    extern void wubu_taskbar_pin(const char *, const char *, const char *) __attribute__((weak));
    if (wubu_taskbar_pin) wubu_taskbar_pin(g_pc.key, g_pc.title, g_pc.exec);
    snprintf(g_pc.note, sizeof g_pc.note, "%s pinned to the taskbar",
             g_pc.title[0] ? g_pc.title : "This app");
}

static void finish(int answer)
{
    if (g_pc.card.footer_checked) save_pref(0);      /* the user's answer outlives this pin */
    g_pc.answered = answer;
    g_pc.open = false;
    cd_close(&g_pc.card);
}

int pin_confirm_click(int x, int y, int fb_w, int fb_h)
{
    (void)fb_w; (void)fb_h;
    if (!g_pc.open) return 0;

    int hit = cd_hit(&g_pc.card, x, y);
    switch (hit) {
    case CD_HIT_FOOTER:
        g_pc.card.footer_checked = !g_pc.card.footer_checked;
        return 0;
    case CD_HIT_OK:
        apply_pin();
        finish(1);
        return 1;
    case CD_HIT_CANCEL:
        snprintf(g_pc.note, sizeof g_pc.note, "Pinning cancelled");
        finish(2);
        return 2;
    default:
        return 0;
    }
}

bool pin_confirm_key(uint32_t key, int fb_w, int fb_h)
{
    (void)fb_w; (void)fb_h;
    if (!g_pc.open) return false;

    if (key == 0x1C || key == 0xE01C || key == '\r' || key == '\n') {
        apply_pin();
        finish(1);
        return true;
    }
    if (key == 27) {
        snprintf(g_pc.note, sizeof g_pc.note, "Pinning cancelled");
        finish(2);
        return true;
    }
    if (cd_key(&g_pc.card, key)) return true;
    return true;      /* modal: nothing leaks to the desktop behind it */
}

void pin_confirm_render(uint32_t *fb, int fb_w, int fb_h)
{
    if (!g_pc.open) return;
    cd_render(&g_pc.card, fb, fb_w, fb_h);
}
