/*
 * wubu_onboarding.c -- P4 first-boot onboarding decision + sentinel persistence.
 *
 * A first boot (no sentinel) drives the session to the GREETER; completing a
 * greeter flow persists a signature sentinel, so the NEXT boot (re-load at the
 * same path) goes straight to DESKTOP. The sentinel is a small file whose
 * content is the recorded config signature -- explicit, inspectable, portable.
 */
#include "wubu_onboarding.h"
#include <stdio.h>
#include <string.h>

static char  g_path[512];
static char  g_sig[256];           /* recorded signature ("" if none) */
static int   g_first = 1;

void wubu_onboarding_load(const char *path)
{
    g_path[0] = 0;
    if (path) {
        strncpy(g_path, path, sizeof(g_path) - 1);
        g_path[sizeof(g_path) - 1] = 0;
    }
    g_sig[0] = 0;
    g_first = 1;
    if (g_path[0]) {
        FILE *f = fopen(g_path, "r");
        if (f) {
            size_t n = fread(g_sig, 1, sizeof(g_sig) - 1, f);
            fclose(f);
            g_sig[n] = 0;
            while (n && (g_sig[n - 1] == '\n' || g_sig[n - 1] == '\r'))
                g_sig[--n] = 0;          /* strip trailing newline */
            if (n > 0) g_first = 0;      /* a sentinel exists => not first boot */
        }
    }
}

int wubu_onboarding_first_boot(void) { return g_first; }

wubu_onboard_target wubu_onboarding_target(void)
{
    return g_first ? WUBU_ONBOARD_GREETER : WUBU_ONBOARD_DESKTOP;
}

int wubu_onboarding_complete(const char *signature)
{
    if (!g_path[0]) return -1;
    FILE *f = fopen(g_path, "w");
    if (!f) return -1;
    fputs(signature ? signature : "", f);
    fclose(f);

    strncpy(g_sig, signature ? signature : "", sizeof(g_sig) - 1);
    g_sig[sizeof(g_sig) - 1] = 0;
    g_first = 0;
    return 0;
}

const char *wubu_onboarding_signature(void) { return g_sig; }
