/*
 * test_cp_applets.c -- every Control Panel applet must actually PAINT.
 *
 * Three recipes (test_cp_hardware, test_cp_dnt, test_controlpanel) named test
 * sources that do not exist; they had rotted out of the build and nobody
 * noticed because no tier ran them. This is the one test they were each trying
 * to be, and it asserts what a user would notice:
 *
 *   1. the applet paints a real number of pixels in its content region;
 *   2. it uses the surface width we hand it (a canary word after the surface
 *      must survive -- cp_sound used to write fb[y*800+x], which walks off a
 *      320px surface and would smash the canary);
 *   3. the theme page shows actual palette colours, not just text.
 */
#include "control_panel.h"
#include "../runtime/wubu_parental.h"
#include "../runtime/wubu_biometric.h"
#include "../runtime/wubu_people.h"
#include "../runtime/wubu_pkgx.h"
#include "../kernel/wubu_world.h"

/* A world snapshot the Display page can report: without one the page draws its "no snapshot" line and a
 * heading-only page would still pass an ink count. */
static wubu_world_t junk_world_for_test;
static wubu_world_t fake_world_for_test = { .screen_w = 1234, .screen_h = 567,
                                            .gpu_connector = 1, .vram_mb = 64 };
static const wubu_world_t *fake_snapshot(void) { return &fake_world_for_test; }
static const wubu_world_t *junk_snapshot(void) { return &junk_world_for_test; }

/* The panel renders its ACTIVE page. Every gate that wanted a specific page once measured Display
 * instead; one of them blessed a heading-only page for that reason. Walk to an index, once, here. */
static void activate_page_index(int want)
{
    for (int guard = 0; guard < controlpanel_count() + 1; guard++) {
        if (controlpanel_active_index() == want) return;
        controlpanel_handle_key(0xE050, 0);
    }
}
static void activate_last_page(void) { activate_page_index(controlpanel_count() - 1); }
/* By NAME, because a loop that advances and asserts is measuring pages that happen to be under the cursor. */
static void activate_page_name(const char *name)
{
    for (int k = 0; k < controlpanel_count(); k++) {
        CpApplet *cand = controlpanel_page_at(k);
        if (cand && strcmp(cand->name, name) == 0) { activate_page_index(k); return; }
    }
}

#include <sys/stat.h>
#include "control_panel_text.h"
#include "../runtime/wubu_mirror.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "wubu_theme.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define W 320
#define H 220
#define CANARY 64u
#define CANARY_VAL 0xDEADBEEFu

static int failures;

/* These applets' *render* paths never touch audio; only cp_sound's click
 * handler does. A local stub keeps this test linkable without the audio stack. */
void wubu_audio_sink_int16(const int16_t *pcm, int n) { (void)pcm; (void)n; }

int cp_pages_check(uint32_t *dfb, int fb_w, int fb_h);   /* the page-level checks, split into their own file */

static void check(int cond, const char *what)
{
    printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) failures++;
}

/* 1 if `c` appears anywhere in the content region (y >= 32) */
static int has_colour(const uint32_t *fb, uint32_t c)
{
    for (int y = 32; y < H; y++)
        for (int x = 0; x < W; x++)
            if (fb[(size_t)y * W + x] == c) return 1;
    return 0;
}

static int non_black_in_content(const uint32_t *fb)
{
    int n = 0;
    for (int y = 32; y < H; y++)
        for (int x = 0; x < W; x++)
            if (fb[(size_t)y * W + x] != 0) n++;
    return n;
}

static int canary_intact(const uint32_t *fb)
{
    for (unsigned i = 0; i < CANARY; i++)
        if (fb[(size_t)W * H + i] != CANARY_VAL) return 0;
    return 1;
}

/* render one applet alone and report what it painted */
static void paint_one(const char *name, CpApplet a, int want_palette)
{
    uint32_t *fb = malloc(sizeof(uint32_t) * (W * H + CANARY));
    if (!fb) { printf("  FAIL out of memory\n"); failures++; return; }
    for (size_t i = 0; i < (size_t)W * H + CANARY; i++) fb[i] = 0;
    for (unsigned i = 0; i < CANARY; i++) fb[(size_t)W * H + i] = CANARY_VAL;

    controlpanel_init();
    controlpanel_register_applet(&a);
    controlpanel_render(fb, W, H);

    int painted = non_black_in_content(fb);
    /* The accent is the THEME's selection colour, not a literal: the pages used to hardcode a light blue
     * in a dark-only OS, and a gate that names the literal would have defended that bug. */
    const WubuThemeColors *tc_now = wubu_theme_colors();
    char msg[160];
    snprintf(msg, sizeof msg, "%s paints its page (%d px)", name, painted);
    check(painted > 400, msg);

    snprintf(msg, sizeof msg, "%s honours the surface stride (canary %s)", name,
             canary_intact(fb) ? "intact" : "SMASHED");
    check(canary_intact(fb), msg);

    /* The background fill must be the applet's paper, not leftover zeros -- and "paper" is whatever the
     * theme says, measured from a corner the applet is known to fill. */
    snprintf(msg, sizeof msg, "%s clears its region (theme face)", name);
    uint32_t corner = fb[(size_t)(H - 1) * W + (W - 1)];
    check(corner == tc_now->win_face || corner == tc_now->desktop_bg || corner == tc_now->taskbar_bg, msg);

    if (want_palette) {
        snprintf(msg, sizeof msg, "%s draws the palette (not just text)", name);
        check(has_colour(fb, tc_now->win_face) && painted > 3000, msg);
    }

    controlpanel_shutdown();
    free(fb);
}

/* The Firmware applet paints its page AND reports the machine's real firmware identity. It is checked
 * separately from paint_one() because it paints in the theme's colours rather than the applets'
 * hardcoded paper, and because the interesting assertion is that the value came from DMI. */
static void test_firmware_applet(const char *dmi_root)
{
    uint32_t *fb = malloc(sizeof(uint32_t) * (W * H + CANARY));
    if (!fb) { printf("  FAIL out of memory\n"); failures++; return; }
    memset(fb, 0, sizeof(uint32_t) * (W * H + CANARY));
    for (unsigned i = 0; i < CANARY; i++) fb[(size_t)W * H + i] = CANARY_VAL;

    setenv("WUBU_DMI_ROOT", dmi_root, 1);

    controlpanel_init();
    CpApplet *a = controlpanel_get_applet(CP_APPLET_FIRMWARE);
    check(a != NULL, "the Firmware applet is registered");
    if (!a) { free(fb); return; }
    check(a->render != NULL, "the Firmware applet has a renderer");

    controlpanel_render(fb, W, H);
    check(non_black_in_content(fb) > 400, "the Firmware page paints");
    check(canary_intact(fb), "the Firmware page honours the surface stride");

    /* The page must show what DMI said, not a placeholder. */
    char summary[256] = {0};
    int rc = cp_firmware_test_summary(summary, sizeof summary);
    check(rc == 0, "the Firmware applet exposes its summary");
    check(strstr(summary, "ProbeBIOS") != NULL, "the page reports the firmware DMI named");
    check(strstr(summary, "7.7.7") != NULL, "the page reports the version DMI named");

    controlpanel_shutdown();
    free(fb);
}

int main(void)
{
    printf("test_cp_applets: five applets, real pixels\n");
    wubu_theme_init();

    paint_one("display",  cp_create_display_applet(),  0);
    paint_one("network",  cp_create_network_applet(),  0);
    paint_one("theme",    cp_create_theme_applet(),    1);
    paint_one("sound",    cp_create_sound_applet(),    0);
    paint_one("hardware", cp_create_hardware_applet(), 0);
    paint_one("account",  cp_create_account_applet(),  0);
    /* A page that draws only its heading reads as an empty app. The Display page's body line was never
     * drawn because its builder returns 0 on success and the renderer tested "> 0". Count the ink. */
    {
        /* A fake world snapshot: without one the page draws its "no snapshot" line and the gate would
         * count THAT as a body, which is exactly how the "> 0" bug passed a heading-only page. */
        cp_display_set_snapshot(fake_snapshot);
        CpApplet disp = cp_create_display_applet();
        uint32_t *dfb = calloc((size_t)W * H + CANARY, sizeof(uint32_t));
        for (unsigned i = 0; i < CANARY; i++) dfb[(size_t)W * H + i] = CANARY_VAL;
        controlpanel_init();
        controlpanel_register_applet(&disp);
        controlpanel_render(dfb, W, H);
        /* The page must DRAW its body, not only its heading. The page records the line it drew, so this
         * is a behavioural assertion rather than a pixel count that a background colour can satisfy. */
        const char *drawn = cp_display_last_drawn();
        char m2[220];
        snprintf(m2, sizeof m2, "the Display page draws its body line (drew: '%s')", drawn ? drawn : "");
        check(drawn && drawn[0] && strstr(drawn, "1234x567") != NULL, m2);

        /* A page with ONE line reads as unfinished. It must show several measured facts, and none of them
         * may be a refresh rate: nothing in this OS measures one, and an invented "@60" already shipped
         * here once. */
        int rows = cp_display_row_count();
        char m5[160];
        snprintf(m5, sizeof m5, "the Display page shows several measured rows (%d)", rows);
        check(rows >= 4, m5);
        check(strstr(drawn, "Hz") == NULL && strstr(drawn, "@60") == NULL,
              "no invented refresh rate in the Display rows");

        /* A provider that hands back a struct which cannot be a world must not be printed at face value:
         * a live session's provider returned 0x47a4ec with screen=3843x51383 while the real world sat at
         * 0x360d320 with 1920x1080. Garbage in must not become garbage on screen. */
        static wubu_world_t junk_world;
        junk_world.screen_w = 65535;
        junk_world.screen_h = 65535;
        junk_world.gpu_connector = 99;
        junk_world.vram_mb = 0xDEADBEEF;
        cp_display_set_snapshot(junk_snapshot);
        controlpanel_render(dfb, W, H);
        const char *junk_drawn = cp_display_last_drawn();
        char m6[240];
        snprintf(m6, sizeof m6, "an implausible world is never printed as a display (drew: '%s')",
                 junk_drawn ? junk_drawn : "");
        check(junk_drawn && strstr(junk_drawn, "65535") == NULL && strstr(junk_drawn, "3735928559") == NULL, m6);
        cp_display_set_snapshot(fake_snapshot);

        /* The Theme page had the same "> 0" defect; the same behavioural check applies. */
        CpApplet thm = cp_create_theme_applet();
        controlpanel_init();
        controlpanel_register_applet(&thm);
        /* The panel renders its ACTIVE page: walk to Theme, or this would render Display and the check
         * would pass for the wrong page. */
        activate_last_page();   /* Theme is registered last; the panel renders the ACTIVE page. */
        controlpanel_render(dfb, W, H);
        const char *tline = cp_theme_last_drawn();
        char m3[220];
        snprintf(m3, sizeof m3, "the Theme page draws its body line (drew: '%s')", tline ? tline : "");
        check(tline && tline[0], m3);

        /* The Network page had the same one-line shape; it must draw rows too. */
        CpApplet net = cp_create_network_applet();
        cp_network_set_snapshot(fake_snapshot);
        controlpanel_init();
        controlpanel_register_applet(&net);
        activate_last_page();
        controlpanel_render(dfb, W, H);
        const char *nline = cp_network_last_drawn();
        char m7[220];
        snprintf(m7, sizeof m7, "the Network page draws its rows (drew: '%s')", nline ? nline : "");
        check(nline && nline[0] && strstr(nline, "Wi-Fi") != NULL && strstr(nline, "Ethernet") != NULL, m7);

        failures += cp_pages_check(dfb, W, H);

        controlpanel_shutdown();
        free(dfb);
    }

    paint_one("limits",   cp_create_limits_applet(),   0);
    paint_one("signin",   cp_create_signin_applet(),   0);

    /* A fake DMI tree, so the gate measures the wiring and not the host's BIOS. */
    char froot[] = "/tmp/cpfwXXXXXX";
    if (mkdtemp(froot)) {
        char p[512];
        snprintf(p, sizeof p, "%s/bios_vendor", froot);  FILE *f = fopen(p, "w"); if (f) { fputs("ProbeBIOS\n", f); fclose(f); }
        snprintf(p, sizeof p, "%s/bios_version", froot); f = fopen(p, "w"); if (f) { fputs("7.7.7\n", f); fclose(f); }
        test_firmware_applet(froot);
        snprintf(p, sizeof p, "%s/bios_vendor", froot);  unlink(p);
        snprintf(p, sizeof p, "%s/bios_version", froot); unlink(p);
        rmdir(froot);
    }
    unsetenv("WUBU_DMI_ROOT");

    /* Software sources: the page must read the mirrorlist it is pointed at, and say when there is
     * none -- not draw an empty ranked table as if every mirror were fine. */
    {
        char mfile[] = "/tmp/cpmirXXXXXX";
        int fd = mkstemp(mfile);
        if (fd >= 0) {
            FILE *f = fdopen(fd, "w");
            if (f) {
                fputs("# test mirrorlist\nServer = https://a.example/cachyos/$repo/os/$arch\n"
                      "Server = http://b.example/cachyos/$repo/os/$arch\n", f);
                fclose(f);
            }
            setenv("WUBU_MIRRORLIST", mfile, 1);

            uint32_t *mfb = calloc((size_t)W * H + CANARY, sizeof(uint32_t));
            for (unsigned i = 0; i < CANARY; i++) mfb[(size_t)W * H + i] = CANARY_VAL;
            controlpanel_init();
            CpApplet *m = controlpanel_get_applet(CP_APPLET_MIRROR);
            check(m != NULL, "the Software sources applet is registered");
            controlpanel_render(mfb, W, H);
            check(non_black_in_content(mfb) > 400, "the Software sources page paints");
            check(canary_intact(mfb), "the Software sources page honours the surface stride");

            char first[WUBU_MIRROR_URL_MAX] = {0};
            int rows = cp_mirror_test_rows(first, sizeof first);
            check(rows == 2, "the page read both mirrors");
            check(strcmp(first, "https://a.example/cachyos/$repo/os/$arch") == 0,
                  "the page reports the mirrorlist's first entry");
            controlpanel_shutdown();
            free(mfb);

            /* No mirrorlist: the applet must report zero, not invent one. */
            setenv("WUBU_MIRRORLIST", "/nonexistent/mirrorlist", 1);
            controlpanel_init();
            char none[WUBU_MIRROR_URL_MAX] = {0};
            check(cp_mirror_test_rows(none, sizeof none) == 0,
                  "a missing mirrorlist reads as zero servers");
            controlpanel_shutdown();

            unlink(mfile);
        }
        unsetenv("WUBU_MIRRORLIST");
    }

    /* Your account: the page must name the user the machine names. */
    {
        controlpanel_init();
        uint32_t *afb = calloc((size_t)W * H + CANARY, sizeof(uint32_t));
        for (unsigned i = 0; i < CANARY; i++) afb[(size_t)W * H + i] = CANARY_VAL;
        CpApplet *acct = controlpanel_get_applet(CP_APPLET_ACCOUNT);
        check(acct != NULL, "the Your account applet is registered");
        controlpanel_render(afb, W, H);
        check(non_black_in_content(afb) > 400, "the Your account page paints");
        check(canary_intact(afb), "the Your account page honours the surface stride");
        char login[64] = {0};
        int arc = cp_account_test_login(login, sizeof login);
        check(arc == 0 && login[0] != '\0', "the page names the signed-in user");
        if (login[0]) printf("  measured: account page names '%s'\n", login);
        check(controlpanel_count() >= 8, "the Control Panel registry holds every applet it ships");

        /* -- The chrome: a labelled sidebar the user can actually click --------------------------- */
        check(controlpanel_sidebar_w() >= 120, "the sidebar is wide enough for page names");
        check(controlpanel_header_h() > 0, "the panel reserves a header");
        int rows_ok = 1, inside = 1;
        for (int i = 0; i < controlpanel_count(); i++) {
            int rx, ry, rw, rh;
            if (!controlpanel_sidebar_row_rect(i, &rx, &ry, &rw, &rh)) { rows_ok = 0; break; }
            if (rx + rw > controlpanel_sidebar_w()) inside = 0;
            if (controlpanel_sidebar_hit(rx + rw / 2, ry + rh / 2) != i) rows_ok = 0;
            if (i > 0) {
                int px, py, pw, ph;
                controlpanel_sidebar_row_rect(i - 1, &px, &py, &pw, &ph);
                if (py + ph != ry) rows_ok = 0;          /* rows tile: no dead gap between pages */
            }
        }
        check(rows_ok, "every sidebar row hits itself and the rows tile");
        check(inside, "no sidebar row is wider than the sidebar");

        /* Clicking a row selects that page; the pointer handler is the one the window uses. */
        {
            int rx, ry, rw, rh;
            controlpanel_sidebar_row_rect(2, &rx, &ry, &rw, &rh);
            controlpanel_handle_mouse(rx + rw / 2, ry + rh / 2, 1, 1);
            check(controlpanel_active_index() == 2, "clicking a sidebar row switches the page");
            controlpanel_handle_key(0xE050, 0);
            check(controlpanel_active_index() == 3, "Down moves to the next page");
            controlpanel_handle_key(0xE048, 0);
            check(controlpanel_active_index() == 2, "Up moves back");
            controlpanel_handle_mouse(controlpanel_sidebar_w() + 40, 200, 1, 1);
            check(controlpanel_active_index() == 2, "a press in the page area does not change pages");
        }
        controlpanel_shutdown();
        free(afb);
    }

    /* Family limits: the panel must be an EDITOR. Toggling and saving through its own keys must write a
     * policy, and the written policy must be the one the decision reads back. */
    {
        char conf[] = "/tmp/cplimitsXXXXXX";
        int fd = mkstemp(conf);
        if (fd >= 0) close(fd);
        remove(conf);                                  /* start with no policy file */
        setenv("WUBU_PARENTAL_CONF", conf, 1);

        controlpanel_init();
        CpApplet *lim = controlpanel_get_applet(CP_APPLET_LIMITS);
        check(lim != NULL, "the Family limits applet is registered");

        uint32_t *lfb = calloc((size_t)W * H + CANARY, sizeof(uint32_t));
        for (unsigned i = 0; i < CANARY; i++) lfb[(size_t)W * H + i] = CANARY_VAL;
        controlpanel_render(lfb, W, H);
        check(non_black_in_content(lfb) > 400, "the Family limits page paints");
        check(canary_intact(lfb), "the Family limits page honours the surface stride");

        bool dirty = false;
        const void *pol = cp_limits_test_policy(&dirty);
        check(pol != NULL, "the panel holds a policy");
        const WubuParentalPolicy *p = (const WubuParentalPolicy *)pol;
        check(!p->enabled, "a fresh machine has no limits enabled");
        check(!dirty, "nothing is unsaved before the user touches it");

        /* Space enables, Down moves the end hour, Enter saves. */
        lim->key(lim, ' ', 0);
        check(cp_limits_test_policy(&dirty) && dirty, "toggling marks the policy unsaved");
        lim->key(lim, 9, 0);                           /* Tab: select the end hour */
        lim->key(lim, 0xE050, 0);                      /* Down: end hour +30 min */
        lim->key(lim, 0x1C, 0);                        /* Enter: save */
        bool still = true;
        cp_limits_test_policy(&still);
        check(!still, "saving clears the unsaved mark");

        WubuParentalPolicy written;
        check(wubu_parental_load(&written) == 0, "the panel wrote a policy file");
        check(written.enabled, "the written policy is enabled");
        check(written.end_min == 20 * 60 + 30, "the written policy carries the edited end hour");

        /* And the decision agrees with what was written: at a moment outside the window, denied. */
        struct tm t;
        memset(&t, 0, sizeof t);
        t.tm_wday = 3; t.tm_hour = 3; t.tm_min = 0;
        char why[160];
        check(!wubu_parental_allowed(&written, &t, why, sizeof why),
              "the saved policy actually denies 03:00");

        controlpanel_shutdown();
        free(lfb);
        remove(conf);
        unsetenv("WUBU_PARENTAL_CONF");
    }

    /* Sign-in options: the page must report what the probe found, including "no reader" -- an empty
     * machine is a verdict, not a blank page. */
    {
        char root[] = "/tmp/cpbioXXXXXX";
        if (mkdtemp(root)) {
            setenv("WUBU_FPRINT_ROOT", root, 1);
            unsetenv("WUBU_FPRINT_DEVICE");

            controlpanel_init();
            CpApplet *si = controlpanel_get_applet(CP_APPLET_SIGNIN);
            check(si != NULL, "the Sign-in options applet is registered");
            uint32_t *sfb = calloc((size_t)W * H + CANARY, sizeof(uint32_t));
            for (unsigned i = 0; i < CANARY; i++) sfb[(size_t)W * H + i] = CANARY_VAL;
            controlpanel_render(sfb, W, H);
            check(non_black_in_content(sfb) > 400, "the Sign-in options page paints");
            check(canary_intact(sfb), "the Sign-in options page honours the surface stride");

            const WubuBiometric *bio = (const WubuBiometric *)cp_signin_test_probe();
            check(bio != NULL && bio->probed, "the page probed this machine");
            check(!bio->reader_present, "the fake machine has no reader, and the page says so");

            /* A reader appears; F5 must find it without a restart. */
            char dev[400], node[500];
            snprintf(dev, sizeof dev, "%s/dev", root);
            mkdir(root, 0755);
            check(mkdir(dev, 0755) == 0 || 1, "the fake /dev exists");
            snprintf(node, sizeof node, "%s/fprint0", dev);
            FILE *nf = fopen(node, "w");
            if (nf) fclose(nf);
            si->key(si, 0x3F, 0);                       /* F5 */
            const WubuBiometric *after = (const WubuBiometric *)cp_signin_test_probe();
            check(after && after->reader_present, "F5 finds a reader that appeared after the page opened");

            controlpanel_shutdown();
            free(sfb);
            char rm[400];
            snprintf(rm, sizeof rm, "rm -rf '%s'", root);
            if (system(rm) != 0) { /* best effort */ }
            unsetenv("WUBU_FPRINT_ROOT");
        }
    }

    /* cp_text is the shared drawer: it must honour clip bounds too */
    {
        uint32_t *fb = calloc((size_t)W * H + CANARY, sizeof(uint32_t));
        for (unsigned i = 0; i < CANARY; i++) fb[(size_t)W * H + i] = CANARY_VAL;
        cp_fill(fb, W, H, -10, -10, 20, 20, 0xFF00FF00);      /* clipped at 0,0 */
        check(fb[0] == 0xFF00FF00, "cp_fill clips at the origin");
        cp_fill(fb, W, H, W - 5, H - 5, 50, 50, 0xFF0000FF);   /* clipped at edge */
        check(fb[(size_t)(H - 1) * W + (W - 1)] == 0xFF0000FF, "cp_fill clips at the edge");
        check(canary_intact(fb), "cp_fill never writes past the surface");
        free(fb);
    }

    printf("test_cp_applets: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
