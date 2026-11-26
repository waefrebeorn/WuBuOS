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
#include "control_panel_text.h"

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
    char msg[160];
    snprintf(msg, sizeof msg, "%s paints its page (%d px, accent %s)", name,
             painted, has_colour(fb, 0xFF1060B0) ? "yes" : "no");
    check(painted > 400 && has_colour(fb, 0xFF1060B0), msg);

    snprintf(msg, sizeof msg, "%s honours the surface stride (canary %s)", name,
             canary_intact(fb) ? "intact" : "SMASHED");
    check(canary_intact(fb), msg);

    /* the background fill must be the applet's paper, not leftover zeros */
    snprintf(msg, sizeof msg, "%s clears its region", name);
    check(fb[(size_t)(H - 1) * W + (W - 1)] == 0xFFF7F7F7, msg);

    if (want_palette) {
        snprintf(msg, sizeof msg, "%s draws the palette (not just text)", name);
        check(has_colour(fb, 0xFFF7F7F7) && painted > 3000, msg);
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
