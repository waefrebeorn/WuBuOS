/*
 * wubu_game_launch_test.c -- the game launcher routing test.
 *
 * Asserts the format classification for the three goal formats:
 *   1. a Win32 PE (MZ) -> WUBU_GAME_WIN32
 *   2. a Linux ELF -> WUBU_GAME_LINUX
 *   3. the Mach-O magics (thin + FAT/universal) -> WUBU_GAME_MAC
 *   4. garbage -> UNKNOWN (never misrouted)
 */
#include "wubu_game_launch.h"
#include <stdio.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_game_launch_test (one kernel, every OS) ===\n");

    /* 1. the Win32 PE */
    unsigned char mz[4] = { 'M', 'Z', 0x90, 0x00 };
    if (wubu_game_classify(mz, 4) != WUBU_GAME_WIN32)
        FAIL("MZ not classified win32");
    printf("  PASS: MZ -> win32 (%s)\n", wubu_game_kind_name(WUBU_GAME_WIN32));

    /* 2. the Linux ELF */
    unsigned char elf[4] = { 0x7F, 'E', 'L', 'F' };
    if (wubu_game_classify(elf, 4) != WUBU_GAME_LINUX)
        FAIL("ELF not classified linux");
    printf("  PASS: ELF -> linux-elf (%s)\n", wubu_game_kind_name(WUBU_GAME_LINUX));

    /* 3. the Mach-O magics — thin + FAT (the universal binary) */
    const uint32_t magics[] = { 0xFEEDFACE, 0xFEEDFACF, 0xCEFAEDFE,
                                0xCFFAEDFE, 0xCAFEBABE, 0xBEBAFECA };
    for (size_t i = 0; i < sizeof(magics) / sizeof(magics[0]); i++) {
        unsigned char m[4];
        memcpy(m, &magics[i], 4);
        if (wubu_game_classify(m, 4) != WUBU_GAME_MAC) {
            printf("  FAIL: magic %08x not classified mach-o\n", magics[i]);
            return 1;
        }
    }
    printf("  PASS: Mach-O magics (thin + FAT universal) -> mach-o\n");

    /* 4. garbage never misroutes */
    unsigned char junk[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    if (wubu_game_classify(junk, 4) != WUBU_GAME_UNKNOWN)
        FAIL("junk misclassified");
    if (wubu_game_classify(NULL, 4) != WUBU_GAME_UNKNOWN)
        FAIL("null misclassified");
    printf("  PASS: garbage + null -> unknown (never misrouted)\n");

    printf("=== ALL GAME-LAUNCH TESTS PASSED (the routing table is real) ===\n");
    return 0;
}
