/*
 * wubu_edid_scan_test.c -- gate for connector EDID discovery.
 *
 * Uses a FIXTURE directory (WUBU_DRM_SYSFS) rather than the real /sys/class/drm, so the
 * test asserts behaviour rather than whatever happens to be plugged in today.  The real
 * machine's blob is still exercised: the golden EDID here is the same eDP-1 block from
 * this laptop, so the parser is validated against hardware data while the scan logic is
 * validated deterministically.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wubu_edid_scan.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static const uint8_t kEdid[128] = {
    0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00, 0x0d,0xae,0x21,0x15,0x00,0x00,0x00,0x00,
    0x21,0x1e,0x01,0x04,0xa5,0x22,0x13,0x78, 0x03,0x28,0x65,0x97,0x59,0x54,0x8e,0x27,
    0x1e,0x50,0x54,0x00,0x00,0x00,0x01,0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x9e,0x85, 0x80,0xa0,0x70,0x38,0x3e,0x40,0x30,0x20,
    0xa5,0x00,0x58,0xc1,0x10,0x00,0x00,0x18, 0x9e,0x85,0x80,0xa0,0x70,0x38,0x7c,0x46,
    0x30,0x20,0xa5,0x00,0x58,0xc1,0x10,0x00, 0x00,0x18,0x00,0x00,0x00,0xfd,0x00,0x30,
    0x90,0xa5,0xa5,0x22,0x01,0x0a,0x20,0x20, 0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xfe,
    0x00,0x4e,0x31,0x35,0x36,0x48,0x52,0x41, 0x2d,0x45,0x41,0x31,0x0a,0x20,0x00,0xdd
};

static char g_root[512];

static void put(const char *subdir, const char *file, const void *data, size_t len)
{
    char p[700];
    snprintf(p, sizeof(p), "%s/%s", g_root, subdir);
    mkdir(p, 0700);

    snprintf(p, sizeof(p), "%s/%s/%s", g_root, subdir, file);
    FILE *fp = fopen(p, "wb");
    if (!fp) return;
    if (len) fwrite(data, 1, len, fp);
    fclose(fp);
}

static void put_status(const char *subdir, const char *word)
{
    put(subdir, "status", word, strlen(word));
}

int main(void)
{
    snprintf(g_root, sizeof(g_root), "/tmp/wubu-edid-scan-%d", (int)getpid());
    mkdir(g_root, 0700);

    /* Two connectors with EDID; one connected panel, one disconnected port that still
     * carries a valid blob (common on laptops with a wired port driving nothing). */
    put_status("card1-eDP-1", "connected\n");
    put("card1-eDP-1", "edid", kEdid, sizeof(kEdid));

    put_status("card1-HDMI-A-1", "disconnected\n");
    put("card1-HDMI-A-1", "edid", kEdid, sizeof(kEdid));

    /* A connector with an EMPTY edid file: must be reported, not crash. */
    put_status("card1-DP-1", "disconnected\n");
    put("card1-DP-1", "edid", NULL, 0);

    /* A connector whose EDID is garbage: reported, parsed=false, blob retained. */
    put_status("card1-VGA-1", "connected\n");
    put("card1-VGA-1", "edid", "\xde\xad\xbe\xefgarbage", 11);

    /* A port directory with a status file but NO edid at all -- exactly what the real
     * machine exposes on its idle ports. It must still be REPORTED; dropping it was
     * the bug that made the scanner see one connector instead of four. */
    put_status("card2-eDP-2", "disconnected\n");

    /* Non-connector entries that must be ignored. */
    put("version", "version", "1.0", 3);

    wubu_connector_edid_t out[WUBU_EDID_MAX_CONNECTORS];
    memset(out, 0, sizeof(out));

    int n = wubu_edid_scan(g_root, out, WUBU_EDID_MAX_CONNECTORS);
    TEST(n == 5, "every connector directory is found");

    if (n >= 1) {
        /* Connector name is stripped of the card prefix -- "card1-eDP-1" -> "eDP-1". */
        bool found_panel = false, found_hdmi = false, found_dp = false,
             found_vga = false, found_bare = false;
        for (int i = 0; i < n; i++) {
            if (strcmp(out[i].name, "eDP-1") == 0) {
                found_panel = true;
                TEST(out[i].connected, "panel reports connected");
                TEST(out[i].parsed, "panel EDID parses");
                TEST(strcmp(out[i].edid.name, "N156HRA-EA1") == 0,
                     "panel identity survives the scan path");
                TEST(out[i].edid.pref_w == 1920 && out[i].edid.pref_h == 1080,
                     "panel preferred resolution survives the scan path");
                TEST(out[i].raw_len == sizeof(kEdid), "raw blob is retained verbatim");
            }
            if (strcmp(out[i].name, "HDMI-A-1") == 0) {
                found_hdmi = true;
                TEST(!out[i].connected, "unused port reports disconnected");
                TEST(out[i].parsed, "its EDID still parses -- present but inactive");
            }
            if (strcmp(out[i].name, "DP-1") == 0) {
                found_dp = true;
                TEST(out[i].raw_len == 0, "empty EDID yields an empty raw buffer");
                TEST(!out[i].parsed, "empty EDID is NOT marked parsed");
            }
            if (strcmp(out[i].name, "eDP-2") == 0 && out[i].raw_len == 0) {
                found_bare = true;
                TEST(!out[i].parsed, "port with no EDID file is not marked parsed");
                TEST(!out[i].connected, "idle port reports disconnected");
            }
            if (strcmp(out[i].name, "VGA-1") == 0) {
                found_vga = true;
                TEST(!out[i].parsed, "garbage EDID is not marked parsed");
                TEST(out[i].raw_len > 0, "garbage blob is retained for diagnostics");
            }
        }
        TEST(found_panel && found_hdmi && found_dp && found_vga && found_bare,
             "all five connectors were examined");
    }

    /* -- failure and guard paths -------------------------------------- */
    TEST(wubu_edid_scan("/tmp/definitely-not-a-drm-root", out, 8) == -1,
         "unreadable root returns -1 rather than zero connectors");
    TEST(wubu_edid_scan(NULL, out, 8) == -1, "NULL root refused");
    TEST(wubu_edid_scan(g_root, NULL, 8) == -1, "NULL output refused");

    printf("%d checks, %d failed\n", checks_run, checks_failed);

    /* best-effort cleanup */
    char p[700];
    const char *dirs[] = { "card1-eDP-1", "card1-HDMI-A-1", "card1-DP-1",
                           "card1-VGA-1", "version" };
    const char *files[] = { "status", "edid" };
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 2; j++) {
            snprintf(p, sizeof(p), "%s/%s/%s", g_root, dirs[i], files[j]);
            unlink(p);
        }
    for (int i = 0; i < 5; i++) {
        snprintf(p, sizeof(p), "%s/%s", g_root, dirs[i]);
        rmdir(p);
    }
    rmdir(g_root);

    return checks_failed ? 1 : 0;
}
