/*
 * test_cp_pages.c -- the page-level checks that used to live inside test_cp_applets.c.
 *
 * Split out when that file crossed the line budget: appending a check to a growing test is how a test file
 * becomes the thing nobody can read. Same binary, one entry point.
 */
#include "control_panel.h"
#include "wubu_theme.h"
#include "../runtime/wubu_ai_link.h"
#include "../runtime/wubu_people.h"
#include "../runtime/wubu_pkgx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int page_failures;
#define CHECK(c, m) do { printf("  %s %s\n", (c) ? "ok  " : "FAIL", (m)); if (!(c)) page_failures++; } while (0)

static void activate_by_name(const char *name)
{
    for (int k = 0; k < controlpanel_count(); k++) {
        CpApplet *cand = controlpanel_page_at(k);
        if (cand && strcmp(cand->name, name) == 0) {
            for (int guard = 0; guard < controlpanel_count() + 1; guard++) {
                if (controlpanel_active_index() == k) return;
                controlpanel_handle_key(0xE050, 0);
            }
            return;
        }
    }
}

int cp_pages_check(uint32_t *dfb, int fb_w, int fb_h)
{
    const int W = fb_w, H = fb_h;   /* the moved checks speak in W/H */
    page_failures = 0;
        /* The AI page: reports configuration and a MEASURED reachability, never a guess, and never
         * probes inside a render. */
        CpApplet ai = cp_create_ai_applet();
        setenv("WUBU_AI_ENDPOINT", "127.0.0.1:9", 1);     /* nothing listens here */
        controlpanel_init();
        controlpanel_register_applet(&ai);
        activate_by_name("AI");
        cp_ai_test_measure();
        controlpanel_render(dfb, W, H);
        const char *aline = cp_ai_last_drawn();
        char m8[260];
        snprintf(m8, sizeof m8, "the AI page reports the endpoint and a measured reachability (drew: '%s')",
                 aline ? aline : "");
        CHECK(aline && strstr(aline, "Endpoint") != NULL && strstr(aline, "Reachable") != NULL &&
              strstr(aline, "127.0.0.1:9") != NULL && strstr(aline, "not measured") == NULL, m8);
        CHECK(aline && strstr(aline, "yes,") == NULL, "a dead endpoint is never reported reachable");
        unsetenv("WUBU_AI_ENDPOINT");

        /* The People page must show contacts that are really in the store, and say so when there are none. */
        char pdb[] = "/tmp/wubu_cp_people_XXXXXX";
        int pfd = mkstemp(pdb);
        if (pfd >= 0) close(pfd);
        setenv("WUBU_PEOPLE_DB", pdb, 1);
        unlink(pdb);
        WubuPeople seed;
        memset(&seed, 0, sizeof seed);
        wubu_people_add(&seed, "Ada Lovelace", "ada@example.org", "555-0100");
        wubu_people_save(&seed);

        CpApplet ppl = cp_create_people_applet();
        controlpanel_init();
        controlpanel_register_applet(&ppl);
        activate_by_name("People");
        cp_people_test_reload();
        controlpanel_render(dfb, W, H);
        const char *pline = cp_people_last_drawn();
        char m9[260];
        snprintf(m9, sizeof m9, "the People page shows the contact from the store (drew: '%s')",
                 pline ? pline : "");
        CHECK(pline && strstr(pline, "Ada Lovelace") != NULL && strstr(pline, "1 contact") != NULL, m9);

        /* the EDITOR: typing a person through the page's own keys, then reading the store back. A page you
         * can only read is not a contacts app, and a test that pokes the store directly would prove nothing
         * about the page. */
        controlpanel_handle_key('n', 0);
        for (const char *ch = "Grace Hopper"; *ch; ch++) controlpanel_handle_key((unsigned char)*ch, 0);
        controlpanel_handle_key(0x09, 0);                       /* Tab -> email */
        for (const char *ch = "grace@example.org"; *ch; ch++) controlpanel_handle_key((unsigned char)*ch, 0);
        controlpanel_handle_key(0x0D, 0);                       /* Enter -> save */

        WubuPeople after;
        wubu_people_load(&after);
        CHECK(after.count == 2 && strcmp(after.p[1].name, "Grace Hopper") == 0 &&
              strcmp(after.p[1].email, "grace@example.org") == 0,
              "typing in the page and pressing Enter persists the contact to the store");

        /* a duplicate typed the same way must be refused, and say so on the page */
        controlpanel_handle_key('n', 0);
        for (const char *ch = "grace hopper"; *ch; ch++) controlpanel_handle_key((unsigned char)*ch, 0);
        controlpanel_handle_key(0x0D, 0);
        wubu_people_load(&after);
        CHECK(after.count == 2, "a duplicate typed in the page is refused, case-insensitively");

        /* Escape must leave the store untouched */
        controlpanel_handle_key('n', 0);
        for (const char *ch = "Nobody"; *ch; ch++) controlpanel_handle_key((unsigned char)*ch, 0);
        controlpanel_handle_key(0x1B, 0);
        wubu_people_load(&after);
        CHECK(after.count == 2, "Escape cancels without saving");

        /* and an empty store says so instead of showing an empty card */
        unlink(pdb);
        cp_people_test_reload();
        controlpanel_render(dfb, W, H);
        const char *empty = cp_people_last_drawn();
        CHECK(empty && strstr(empty, "no contacts") != NULL, "the People page says there are no contacts");
        unsetenv("WUBU_PEOPLE_DB");

        /* Programs: the packaged-app runtime must have a surface, not just a test. With no packages the
         * page says so; the root is always shown so a user can find where to install. */
        char pkgroot[] = "/tmp/wubu_cp_pkgs_XXXXXX";
        char *pr = mkdtemp(pkgroot);
        if (pr) setenv("WUBU_PKGS", pr, 1);
        CpApplet prog = cp_create_programs_applet();
        controlpanel_init();
        controlpanel_register_applet(&prog);
        activate_by_name("Programs");
        cp_programs_test_reload();
        controlpanel_render(dfb, W, H);
        const char *gline = cp_programs_last_drawn();
        char mA[260];
        snprintf(mA, sizeof mA, "the Programs page shows the package root and its state (drew: '%s')",
                 gline ? gline : "");
        CHECK(gline && strstr(gline, "Installed") != NULL && strstr(gline, pr ? pr : "packages") != NULL, mA);
        unsetenv("WUBU_PKGS");

        /* No page may paint the retired light card (0xFFF7F7F7) in a dark-only OS. Sound and Hardware both
         * did, and the Sound page also drew black text with it. Count the literal rather than trust a
         * per-page spot check. */
        {
            const char *pages[] = { "Sound", "Hardware", "Firmware", "Network", "Display" };
            for (size_t pi = 0; pi < sizeof pages / sizeof pages[0]; pi++) {
                activate_by_name(pages[pi]);
                for (int y = 0; y < H; y++)
                    for (int xx = 0; xx < W; xx++) dfb[(size_t)y * W + xx] = 0;
                controlpanel_render(dfb, W, H);
                int light = 0;
                for (int y = 0; y < H; y++)
                    for (int xx = 0; xx < W; xx++)
                        if (dfb[(size_t)y * W + xx] == 0xFFF7F7F7u) light++;
                char mL[200];
                snprintf(mL, sizeof mL, "no page paints the retired light card (%s: %d px)", pages[pi], light);
                CHECK(light == 0, mL);
            }
        }

    /* The AI endpoint EDITOR: type an endpoint through the page's keys, Enter, then read the config file
     * back. A store you can only change from outside the OS is not a settings page. */
    {
        char conf[] = "/tmp/wubu_cp_ai_XXXXXX";
        int fd = mkstemp(conf);
        if (fd >= 0) close(fd);
        setenv("WUBU_AI_CONF", conf, 1);
        unsetenv("WUBU_AI_ENDPOINT");

        CpApplet ai = cp_create_ai_applet();
        controlpanel_init();
        controlpanel_register_applet(&ai);
        activate_by_name("AI");
        controlpanel_handle_key('e', 0);
        for (const char *ch = "127.0.0.1:9"; *ch; ch++)
            controlpanel_handle_key((unsigned char)*ch, 0);
        controlpanel_handle_key(0x0D, 0);

        WubuAiLink back;
        int rc = wubu_ai_link_read(&back);
        char me[200];
        snprintf(me, sizeof me, "the AI page saves the endpoint it was told (read back: '%s')",
                 rc == 0 ? back.endpoint : "(none)");
        CHECK(rc == 0 && strstr(back.endpoint, "127.0.0.1:9") != NULL, me);

        /* Escape after typing must not save */
        setenv("WUBU_AI_ENDPOINT", "http://example.invalid/v1", 1);
        controlpanel_handle_key('e', 0);
        controlpanel_handle_key('x', 0);
        controlpanel_handle_key(0x1B, 0);
        unsetenv("WUBU_AI_ENDPOINT");
        rc = wubu_ai_link_read(&back);
        CHECK(rc == 0 && strstr(back.endpoint, "example.invalid") == NULL,
              "Escape in the endpoint editor changes nothing");

        unsetenv("WUBU_AI_CONF");
        unlink(conf);
    }

    return page_failures;
}
