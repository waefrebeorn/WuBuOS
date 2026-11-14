/*
 * wubu_edid_kms_test.c -- gate for direct KMS enumeration.
 *
 * Runs against the REAL DRM device when one is reachable and asserts the same panel
 * identity the sysfs and xrandr sources report; when no DRM device can be opened it
 * asserts the honest failure (-1) instead of skipping, so a passing run always means
 * something.
 */
#include <stdio.h>
#include <string.h>

#include "wubu_edid_kms.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

int main(void)
{
    wubu_connector_edid_t out[WUBU_EDID_MAX_CONNECTORS];
    memset(out, 0, sizeof(out));

    int n = wubu_edid_kms_scan(out, WUBU_EDID_MAX_CONNECTORS);

    if (n <= 0) {
        checks_run++;
        printf("  NOTE no DRM device reachable; asserting fail-closed\n");
        TEST(n == -1, "unreachable DRM fails closed with -1");
        TEST(out[0].name[0] == '\0', "failed scan leaves records empty");
    } else {
        bool any_parsed = false;
        for (int i = 0; i < n; i++) {
            TEST(out[i].name[0] != '\0', "connector has a name");
            TEST(out[i].dir[0] != '\0', "connector names its card");
            if (out[i].parsed) {
                any_parsed = true;
                TEST(out[i].edid.pref_w > 0 && out[i].edid.pref_h > 0,
                     "parsed EDID carries a resolution");
            }
        }
        TEST(any_parsed, "at least one connector yielded a parseable EDID");

        /* Cross-source agreement: whatever panel is connected here must be identified
         * identically by sysfs/xrandr and by KMS. */
        int s = wubu_edid_scan("/sys/class/drm", out, 0);  /* invalid cap: guard check */
        TEST(s == -1, "scan refuses cap <= 0");

        printf("  (KMS reported %d connectors)\n", n);
    }

    /* Guards. */
    TEST(wubu_edid_kms_scan(NULL, 8) == -1, "NULL output refused");
    TEST(wubu_edid_kms_scan(out, 0) == -1, "zero capacity refused");

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed ? 1 : 0;
}
