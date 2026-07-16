/*
 * wubu_launch_test.c -- WuBuOS container/Proton launch + session-split tests.
 *
 * Validates Workstream B (session split: DESKTOP vs GAME mode) and C1
 * (container/Proton as the default Windows-launch path). Real assertions
 * against wubu_detect_payload_type + wubu_launch_windows + HMODE_GAME.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_container.h"
#include "wubu_proton.h"
#include "wubu_ct_isolate.h"
#include "wubu_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while (0)

int main(void) {
    printf("=== WuBuOS Launch (Proton/container) + Session-Split Test Suite ===\n\n");

    /* Payload detection (drives the launch route). */
    printf("[Payload Detection]\n");
    {
        /* MZ with a real PE header (e_lfanew->"PE\0\0") -> Win PE.
         * A bare MZ with no PE signature is now a 16-bit DOS EXE routed to
         * the FreeDOS emergency layer, so the launch test must use a true PE. */
        uint8_t mz[0x48];
        memset(mz, 0, sizeof(mz));
        mz[0]='M'; mz[1]='Z';
        mz[0x3C]=0x40;                       /* e_lfanew -> offset 0x40 */
        mz[0x40]='P'; mz[0x41]='E'; mz[0x42]=0; mz[0x43]=0;
        uint8_t elf[4] = { 0x7F,'E','L','F' };
        uint8_t wubu[8];
        memcpy(wubu, WUBU_MAGIC, WUBU_MAGIC_SIZE);
        uint8_t garbage[4] = { 0x01,0x02,0x03,0x04 };

        T(wubu_detect_payload_type(mz, sizeof(mz)) == WUBU_PAYLOAD_WIN_PE,
          "MZ -> Win PE");
        T(wubu_detect_payload_type(elf, sizeof(elf)) == WUBU_PAYLOAD_LINUX_ELF,
          "ELF -> Linux ELF");
        T(wubu_detect_payload_type(wubu, sizeof(wubu)) == WUBU_PAYLOAD_NESTED_WUBU,
          ".wubu magic -> nested .wubu");
        T(wubu_detect_payload_type(garbage, sizeof(garbage)) != WUBU_PAYLOAD_WIN_PE,
          "garbage -> not PE");
    }

    /* wubu_launch_windows error path: garbage must be rejected (-1), never
     * silently succeed. This proves the launch route does real work. */
    printf("\n[Launch Error Path]\n");
    {
        uint8_t garbage[16] = { 0 };
        T(wubu_launch_windows(garbage, sizeof(garbage), NULL) == -1,
          "garbage binary rejected (-1)");
        T(wubu_launch_windows(NULL, 0, NULL) == -1,
          "NULL data rejected (-1)");
    }

    /* Session split: GAME mode flips state away from GUI and marks fullscreen. */
    printf("\n[Session Split (GAME mode)]\n");
    {
        hosted_state_t st;
        memset(&st, 0, sizeof(st));
        st.mode = HMODE_GUI;
        st.fullscreen = false;

        /* A real PE header would route to proton; we feed garbage to exercise
         * the mode/state change (hosted_session_launch_game sets GAME + fullscreen
         * BEFORE the exec attempt, so state is observably correct). */
        uint8_t mz[8] = { 'M','Z',0,0,0,0,0,0 };
        int pid = wubu_session_launch_game(&st, mz, sizeof(mz), NULL);
        T(st.mode == HMODE_GAME, "GAME mode entered");
        T(st.fullscreen == true, "fullscreen set for game session");
        /* pid may be -1 if proton cannot exec in this env; that's fine -- the
         * session transition itself is the asserted behavior here. */
        (void)pid;

        T(strcmp(wubu_session_mode_name(HMODE_GAME), "Game") == 0, "mode name 'Game'");
        T(strcmp(wubu_session_mode_name(HMODE_GUI), "GUI") == 0, "mode name 'GUI'");
    }

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
