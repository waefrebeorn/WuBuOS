/*
 * wubu_edid_test.c -- gate for EDID parsing.
 *
 * The golden vectors here are a REAL 128-byte EDID captured from this machine's eDP-1
 * panel (/sys/class/drm/card1-eDP-1/edid), and every expected value is cross-checked
 * against what `xrandr` reports for the same panel.  Synthetic tests alone would only
 * prove the parser agrees with itself.
 */
#include <stdio.h>
#include <string.h>

#include "wubu_edid.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

/* Real eDP-1 EDID from this machine.  Panel name reads "N156HRA-EA1", manufacturer
 * CMN, physical size 34x19 cm -- xrandr independently reports 344x194 mm. */
static const uint8_t kRealEdid[128] = {
    0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00, 0x0d,0xae,0x21,0x15,0x00,0x00,0x00,0x00,
    0x21,0x1e,0x01,0x04,0xa5,0x22,0x13,0x78, 0x03,0x28,0x65,0x97,0x59,0x54,0x8e,0x27,
    0x1e,0x50,0x54,0x00,0x00,0x00,0x01,0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x9e,0x85, 0x80,0xa0,0x70,0x38,0x3e,0x40,0x30,0x20,
    0xa5,0x00,0x58,0xc1,0x10,0x00,0x00,0x18, 0x9e,0x85,0x80,0xa0,0x70,0x38,0x7c,0x46,
    0x30,0x20,0xa5,0x00,0x58,0xc1,0x10,0x00, 0x00,0x18,0x00,0x00,0x00,0xfd,0x00,0x30,
    0x90,0xa5,0xa5,0x22,0x01,0x0a,0x20,0x20, 0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xfe,
    0x00,0x4e,0x31,0x35,0x36,0x48,0x52,0x41, 0x2d,0x45,0x41,0x31,0x0a,0x20,0x00,0xdd
};

int main(void)
{
    wubu_edid_t e;

    /* -- the real blob ------------------------------------------------ */
    memset(&e, 0xAA, sizeof(e));
    TEST(wubu_edid_parse(kRealEdid, sizeof(kRealEdid), &e) == 0,
         "real EDID parses");
    TEST(e.valid, "parsed EDID is marked valid");

    TEST(strcmp(e.manuf, "CMN") == 0,
         "manufacturer ID decodes to the three-letter code");
    TEST(e.mm_w == 340 && e.mm_h == 190,
         "physical size matches the panel (xrandr says 344x194 mm)");
    TEST(strcmp(e.name, "N156HRA-EA1") == 0,
         "monitor name descriptor is read, terminator stripped");
    TEST(e.pref_w == 1920 && e.pref_h == 1080,
         "preferred timing is the panel's native resolution");
    TEST(e.pref_refresh_mhz > 0,
         "refresh is derived from the clock and blanking");
    TEST(e.year >= 1990, "manufacture year is absolute");

    /* -- refusals ---------------------------------------------------- */
    uint8_t buf[128];
    memcpy(buf, kRealEdid, 128);

    TEST(wubu_edid_parse(kRealEdid, 64, &e) == -1,
         "short buffer refused");
    TEST(!e.valid, "refusal leaves the output marked invalid");

    memcpy(buf, kRealEdid, 128);
    buf[0] ^= 0xFF;
    TEST(wubu_edid_parse(buf, 128, &e) == -1, "bad header refused");

    memcpy(buf, kRealEdid, 128);
    buf[127] ^= 0x01;                       /* break the checksum */
    TEST(wubu_edid_parse(buf, 128, &e) == -1, "bad checksum refused");

    memcpy(buf, kRealEdid, 128);
    buf[18] = 2;                            /* EDID 2.0 -- different layout */
    /* Recompute the checksum so ONLY the version is wrong. */
    buf[127] = 0;
    unsigned sum = 0;
    for (int i = 0; i < 128; i++) sum += buf[i];
    buf[127] = (uint8_t)(256 - (sum & 0xFF)) & 0xFF;
    TEST(wubu_edid_parse(buf, 128, &e) == -1, "unsupported version refused");

    TEST(wubu_edid_parse(NULL, 128, &e) == -1, "NULL input refused");
    TEST(!e.valid, "NULL refusal leaves the output invalid");

    /* -- no name descriptor: must be empty, not garbage ------------- */
    memcpy(buf, kRealEdid, 128);
    memset(buf + 108, 0x00, 18);           /* wipe descriptor slot 4 (the name slot) */
    buf[127] = 0;
    sum = 0;
    for (int i = 0; i < 128; i++) sum += buf[i];
    buf[127] = (uint8_t)(256 - (sum & 0xFF)) & 0xFF;
    TEST(wubu_edid_parse(buf, 128, &e) == 0, "EDID without a name still parses");
    TEST(e.name[0] == '\0', "absent name yields an empty string, not junk");

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed ? 1 : 0;
}
