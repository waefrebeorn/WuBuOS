/*
 * dosgui_era_apps_test_stub.c -- the era-apps STUB for the BPM test.
 *
 * The real dosgui_era_apps.c pulls the whole VSL exec graph (the
 * program DB + wubu_dos_proc_launch) — too heavy for the BPM unit
 * test. This stub provides the SAME registry contract over a fixed
 * 7-app grid (4 runnable / 3 gaps), so the BPM shell's navigation +
 * launch + exit logic is tested against a controlled grid. The real
 * era-apps launch has its own integration tests elsewhere.
 *
 * (Precedent: dosgui_dos_window_test_stub.c.)
 * C11.
 */
#include "dosgui_era_apps.h"

#include <string.h>

/* the canonical era set (mirrors the real registry) */
static const struct {
    const char *name;
    const char *category;
    int         runnable;
} g_stub_apps[] = {
    { "CP/M :: STAT",      "Era: CP/M 1974",   0 },
    { "DOS :: HELLO",      "Era: MS-DOS 1981", 1 },
    { "Mac :: About",      "Era: Classic Mac", 0 },
    { "Win32 :: Era Demo", "Era: Win32 1993",  1 },
    { "macOS :: Era Demo", "Era: macOS XNU",   0 },
    { "Linux :: Era Demo", "Era: Linux 2007",  1 },
    { "HolyC :: Era Demo", "Era: HolyC 2020",  1 },
};
#define STUB_COUNT (int)(sizeof(g_stub_apps) / sizeof(g_stub_apps[0]))

void dosgui_era_apps_register(void) { /* the stub is pre-registered */ }

int dosgui_era_apps_runnable_count(void)
{
    int n = 0;
    for (int i = 0; i < STUB_COUNT; i++)
        if (g_stub_apps[i].runnable) n++;
    return n;
}

int dosgui_era_apps_total_count(void)
{
    return STUB_COUNT;
}

int dosgui_era_apps_launch(int idx)
{
    if (idx < 0 || idx >= STUB_COUNT || !g_stub_apps[idx].runnable) return -1;
    return 0;
}

int dosgui_era_apps_launch_by_name(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < STUB_COUNT; i++)
        if (strcmp(g_stub_apps[i].name, name) == 0)
            return dosgui_era_apps_launch(i);
    return -1;
}

int dosgui_era_apps_personality_coverage(uint32_t *out_personas, int max)
{
    if (!out_personas || max <= 0) return 0;
    out_personas[0] = 0x3F;
    return 1;
}

const char *dosgui_era_personality_label(uint32_t p)
{
    (void)p;
    return "era";
}
