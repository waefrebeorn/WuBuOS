/*
 * wubu_vlanaudio_selftest.c -- verifies kernel-owned VLAN/audio-DSP routing.
 */
#include "wubu_vlanaudio.h"
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
    printf("=== wubu_vlanaudio_selftest ===\n\n");

    wubu_hw_detect();
    wubu_vlanaudio_probe();

    printf("  vlan=%d offload=%d pipewire=%d alsa=%d sof=%d\n",
           wubu_vlanaudio_vlan(), wubu_vlanaudio_vlan_offload(),
           wubu_vlanaudio_pipewire(), wubu_vlanaudio_alsa(),
           wubu_vlanaudio_sof());

    /* VLAN routing. */
    CHECK(strcmp(wubu_vlanaudio_vlan_for("ixgbe"), "8021q") == 0,
          "ixgbe -> 8021q");
    CHECK(strcmp(wubu_vlanaudio_vlan_for("mlx5"), "8021q") == 0,
          "mlx5 -> 8021q");
    CHECK(strcmp(wubu_vlanaudio_vlan_for("unknown"), "8021q") == 0,
          "unknown -> 8021q");

    /* Audio DSP routing. */
    CHECK(strcmp(wubu_vlanaudio_dsp_for("pipewire"), "pipewire") == 0,
          "pipewire -> pipewire");
    CHECK(strcmp(wubu_vlanaudio_dsp_for("sof"), "snd_sof") == 0,
          "sof -> snd_sof");
    CHECK(strcmp(wubu_vlanaudio_dsp_for("alsa"), "alsa-dmix") == 0,
          "alsa -> alsa-dmix");
    CHECK(strcmp(wubu_vlanaudio_dsp_for("pulse"), "pulseaudio") == 0,
          "pulse -> pulseaudio");
    CHECK(strcmp(wubu_vlanaudio_dsp_for("unknown"), "alsa") == 0,
          "unknown dsp -> alsa fallback");

    char s[256];
    wubu_vlanaudio_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "vlanaudio summary generated");

    printf("\n=== VLANAUDIO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
