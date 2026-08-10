/*
 * wubu_game_launch_test_stub.c -- the exec-backend stubs for the
 * game-launch routing test.
 *
 * wubu_game_launch.c references wubu_exec_win_pe/linux_elf/macho;
 * the classification test never CALLS them (the routing table is the
 * unit under test — the real backends are proven by the runtime build
 * + the era-apps integration tests). These stubs keep the test link
 * light. (The established pattern.)
 * C11.
 */
#include <stdint.h>
#include <stddef.h>

int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size)
{
    (void)pe_data; (void)pe_size;
    return 0;
}

int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size)
{
    (void)elf_data; (void)elf_size;
    return 0;
}

int64_t wubu_exec_macho(const void *macho_data, size_t macho_size)
{
    (void)macho_data; (void)macho_size;
    return 0;
}
