/*
 * wubu_game_launch.h -- the GAME LAUNCHER (one kernel, every OS's
 * games).
 */
#ifndef WUBU_GAME_LAUNCH_H
#define WUBU_GAME_LAUNCH_H

#include <stddef.h>
#include <stdint.h>

/* the game formats the kernel hosts */
typedef enum {
    WUBU_GAME_UNKNOWN = 0,
    WUBU_GAME_WIN32,     /* Win32 PE -> wubu_exec_win_pe (Wine) */
    WUBU_GAME_LINUX,     /* Linux ELF -> wubu_exec_linux_elf */
    WUBU_GAME_MAC,       /* Mach-O   -> wubu_exec_macho (VSL/Darling) */
} WUBU_GAME_KIND;

/* GL1: classify a game file by its magic bytes. */
WUBU_GAME_KIND wubu_game_classify(const void *data, size_t size);

/* GL2: run a game file (the full personality routing). */
int64_t wubu_game_run(const char *path);

/* GL3: the kind name. */
const char *wubu_game_kind_name(WUBU_GAME_KIND kind);

#endif
