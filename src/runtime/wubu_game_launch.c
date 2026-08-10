/*
 * wubu_game_launch.c -- the GAME LAUNCHER (one kernel, every OS's
 * games).
 *
 * The user's three goals route through three VSL personalities:
 *
 *   Halo CE PC demo  (Win32 .exe)  -> wubu_exec_win_pe   -> Wine in
 *                                      the SteamOS container
 *   Halo CE Mac demo (Mach-O .app) -> wubu_exec_macho    -> the VSL
 *                                      Mach-O loader / Darling
 *   OpenArena (Linux ELF)          -> wubu_exec_linux_elf -> the
 *                                      native container (GL leg)
 *
 * This module classifies a game file by its format + routes it to the
 * right personality — the honest single entry point the desktop's
 * Play action + the colonel use. The test proves the routing table
 * (classify -> the right exec backend) for all three formats.
 *
 * C11, minimal includes, shell-free (no /bin/sh is ever spawned).
 */
#include "wubu_game_launch.h"
#include "wubu_exec.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* GL1: classify a game file by its magic bytes. */
WUBU_GAME_KIND wubu_game_classify(const void *data, size_t size)
{
    if (!data || size < 4) return WUBU_GAME_UNKNOWN;
    const uint8_t *p = (const uint8_t *)data;

    /* Win32 PE: MZ */
    if (p[0] == 'M' && p[1] == 'Z') return WUBU_GAME_WIN32;

    /* Linux ELF: \x7F E L F */
    if (p[0] == 0x7F && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
        return WUBU_GAME_LINUX;

    /* Mach-O: the four magics + the FAT (universal) magic */
    uint32_t magic = 0;
    memcpy(&magic, p, 4);
    if (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
        magic == 0xCEFAEDFE || magic == 0xCFFAEDFE ||
        magic == 0xCAFEBABE || magic == 0xBEBAFECA)
        return WUBU_GAME_MAC;

    return WUBU_GAME_UNKNOWN;
}

/* GL2: run a game file. Returns the personality result (the exit
 * code for the Linux/container paths, or the exec rc). */
int64_t wubu_game_run(const char *path)
{
    if (!path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (256L << 20)) {   /* cap 256MB in-memory */
        fclose(f);
        return -1;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return -1; }

    WUBU_GAME_KIND kind = wubu_game_classify(buf, (size_t)sz);
    int64_t rc = -1;
    switch (kind) {
    case WUBU_GAME_WIN32:
        rc = wubu_exec_win_pe(buf, (size_t)sz);
        break;
    case WUBU_GAME_LINUX:
        rc = wubu_exec_linux_elf(buf, (size_t)sz);
        break;
    case WUBU_GAME_MAC:
        rc = wubu_exec_macho(buf, (size_t)sz);
        break;
    default:
        break;
    }
    free(buf);
    return rc;
}

/* GL3: the kind name (the colonel + the desktop show it). */
const char *wubu_game_kind_name(WUBU_GAME_KIND kind)
{
    switch (kind) {
    case WUBU_GAME_WIN32:  return "win32";
    case WUBU_GAME_LINUX:  return "linux-elf";
    case WUBU_GAME_MAC:    return "mach-o";
    default:               return "unknown";
    }
}
