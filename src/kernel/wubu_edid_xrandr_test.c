/*
 * wubu_edid_xrandr_test.c -- gate for xrandr-based display discovery.
 *
 * The fixture is REAL `xrandr -q --props` output captured from this machine, not a
 * hand-written approximation: it contains the property noise an invented fixture would
 * omit (vrr_capable, Colorspace ranges, CONNECTOR_ID), and that noise is what breaks
 * naive parsers.  The eDP-1 block carries the same EDID blob already cross-checked
 * against sysfs and xrandr elsewhere, so the two sources can be asserted to agree.
 */
#define _POSIX_C_SOURCE 200809L  /* fmemopen */
#include <stdio.h>
#include <string.h>

#include "wubu_edid_xrandr.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

/* Captured from a live session on this box. */
static const char kXrandr[] =
    "Screen 0: minimum 320 x 200, current 320 x 200, maximum 16384 x 16384\n"
    "eDP-1 connected primary (normal left inverted right x axis y axis)\n"
    "	_MUFFIN_PRESENTATION_OUTPUT: 0 \n"
    "	EDID: \n"
    "		00ffffffffffff000dae211500000000\n"
    "		211e0104a52213780328659759548e27\n"
    "		1e505400000001010101010101010101\n"
    "		0101010101019e8580a070383e403020\n"
    "		a50058c1100000189e8580a070387c46\n"
    "		3020a50058c110000018000000fd0030\n"
    "		90a5a522010a202020202020000000fe\n"
    "		004e3135364852412d4541310a2000dd\n"
;

int main(void)
{
    wubu_connector_edid_t out[WUBU_EDID_MAX_CONNECTORS];
    memset(out, 0, sizeof(out));

    FILE *fp = fmemopen((void *)kXrandr, strlen(kXrandr), "r");
    TEST(fp != NULL, "fixture stream opens");
    if (!fp) return 1;

    int n = wubu_edid_xrandr_parse(fp, out, WUBU_EDID_MAX_CONNECTORS);
    fclose(fp);

    TEST(n >= 1, "at least one output parsed from real xrandr output");
    if (n >= 1) {
        bool found_edp = false;
        for (int i = 0; i < n; i++) {
            if (strcmp(out[i].name, "eDP-1") == 0) {
                found_edp = true;
                TEST(out[i].connected, "eDP-1 reported connected");
                TEST(out[i].parsed, "eDP-1 EDID hex block parses");
                TEST(strcmp(out[i].edid.name, "N156HRA-EA1") == 0,
                     "identity matches what sysfs yields for the same panel");
                TEST(out[i].edid.pref_w == 1920 && out[i].edid.pref_h == 1080,
                     "preferred resolution matches the sysfs source");
            }
        }
        TEST(found_edp, "the real panel was found by name");
    }

    /* Guards. */
    TEST(wubu_edid_xrandr_parse(NULL, out, 8) == -1, "NULL stream refused");
    TEST(wubu_edid_xrandr_parse(fmemopen((void*)"", 1, "r"), NULL, 8) == -1,
         "NULL output refused");

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed ? 1 : 0;
}
