/*
 * wubu_mixgraph_selftest.c -- verifies kernel-owned audio-graph routing.
 */
#include "wubu_mixgraph.h"
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
    printf("=== wubu_mixgraph_selftest ===\n\n");

    wubu_hw_detect();
    wubu_mixgraph_probe();

    printf("  pw=%d pulse=%d jack=%d alsa=%d wplumber=%d\n",
           wubu_mixgraph_pipewire(), wubu_mixgraph_pulse(),
           wubu_mixgraph_jack(), wubu_mixgraph_alsa(),
           wubu_mixgraph_wireplumber());

    /* Graph driver routing is always consistent. */
    CHECK(strcmp(wubu_mixgraph_driver_for("pipewire"), "pipewire") == 0,
          "pipewire -> pipewire");
    CHECK(strcmp(wubu_mixgraph_driver_for("wireplumber"), "wireplumber") == 0,
          "wireplumber -> wireplumber");
    CHECK(strcmp(wubu_mixgraph_driver_for("pulse"), "pulseaudio") == 0,
          "pulse -> pulseaudio");
    CHECK(strcmp(wubu_mixgraph_driver_for("jack"), "jack") == 0,
          "jack -> jack");
    CHECK(strcmp(wubu_mixgraph_driver_for("alsa"), "alsa-dmix") == 0,
          "alsa -> alsa-dmix");
    CHECK(strcmp(wubu_mixgraph_driver_for("unknown"), "alsa") == 0,
          "unknown -> alsa fallback");

    char s[256];
    wubu_mixgraph_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "mixgraph summary generated");

    printf("\n=== MIXGRAPH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
