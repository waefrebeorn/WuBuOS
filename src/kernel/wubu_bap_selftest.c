/*
 * wubu_bap_selftest.c -- verifies Bluetooth BAP routing.
 */
#include "wubu_bap.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_bap_selftest ===\n");

    wubu_bap_probe();

    int p = wubu_bap_present();
    CHECK(p == 0 || p == 1, "bap present is boolean");

    /* Codec validation. */
    CHECK(wubu_bap_codec(44100, 16) == 1, "44.1kHz/16bit valid");
    CHECK(wubu_bap_codec(48000, 24) == 1, "48kHz/24bit valid");
    CHECK(wubu_bap_codec(96000, 32) == 1, "96kHz/32bit valid");
    CHECK(wubu_bap_codec(22050, 16) == 0, "22kHz too low");
    CHECK(wubu_bap_codec(48000, 8) == 0, "8bit too low");

    /* Ready check. */
    CHECK(wubu_bap_is_ready(1, 1) == 1, "cfg+conn = ready");
    CHECK(wubu_bap_is_ready(1, 0) == 0, "cfg only = not ready");

    /* Summary builds. */
    char out[160] = "";
    wubu_bap_summary(out, sizeof(out));
    CHECK(strstr(out, "bap[") != NULL, "summary has bap fragment");

    printf("\n=== BAP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
