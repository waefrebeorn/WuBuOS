/*
 * wubu_bottle_flatpak.c -- WuBuOS Bottles/Lutris: Flatpak manifest + runtime detection.
 *
 * Split from wubu_bottles.c (Cell 480 monolith mixing 4 concerns).
 * Self-contained: real Wine/Proton/Bottles/Lutris/Flatpak work; shares the
 * wubu_bottles.h public API + wubu_bottles_internal.h (json/fs) surface.
 * C11, opaque structs, minimal includes -- no god headers.
 */

/*
 * wubu_bottles.c  --  WuBuOS Bottles/Lutris Integration Implementation
 *
 * Cell 480: Stub implementation for .wubu container format.
 *
 * This provides the API structure for Bottles/Lutris integration.
 * Full implementation would handle:
 *   - tar.zst / squashfs extraction
 *   - JSON serialization
 *   - Wine prefix operations
 *   - Dependency installation via winetricks
 *   - Flatpak manifest generation
 */

#define _GNU_SOURCE

#include "wubu_bottles.h"
#include "wubu_bottles_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <ftw.h>
#include <sys/wait.h>

/* -- Recursive directory removal (replaces system("rm -rf")) -------- */

/* ==================================================================
 * Bottle Management
 * ================================================================== */

/* ==================================================================
 * Flatpak manifest + runtime detection
 * ================================================================== */

int wubu_bottle_flatpak_manifest(WubuBottle *bottle, const char *output_path) {
    if (!bottle || !output_path) return -1;
    char manifest[8192];
    snprintf(manifest, sizeof(manifest),
        "{\n"
        "  \"app-id\": \"com.wubu.%s\",\n"
        "  \"runtime\": \"org.freedesktop.Platform\",\n"
        "  \"runtime-version\": \"22.08\",\n"
        "  \"sdk\": \"org.freedesktop.Sdk\",\n"
        "  \"command\": \"%s\",\n"
        "  \"modules\": [\n"
        "    {\n"
        "      \"name\": \"%s\",\n"
        "      \"build-options\": {\n"
        "        \"env\": {\n"
        "          \"WINEPREFIX\": \"/app/prefix\",\n"
        "          \"WINEARCH\": \"win64\"\n"
        "        }\n"
        "      },\n"
        "      \"sources\": []\n"
        "    }\n"
        "  ],\n"
        "  \"finish-args\": [\n"
        "    \"--share=ipc\",\n"
        "    \"--socket=x11\",\n"
        "    \"--socket=wayland\",\n"
        "    \"--device=dri\"\n"
        "  ]\n"
        "}\n",
        bottle->name,
        bottle->exe_path[0] ? bottle->exe_path : "wine",
        bottle->runner_version);
    FILE *f = fopen(output_path, "w");
    if (!f) return -1;
    fwrite(manifest, 1, strlen(manifest), f);
    fclose(f);
    return 0;
}

bool wubu_bottle_flatpak_runtime_available(const char *runtime) {
    if (!runtime) return false;
    
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "flatpak --version >/dev/null 2>&1", (char*)NULL);
        _exit(1);
    }
    int ret;
    waitpid(pid, &ret, 0);
    if (!WIFEXITED(ret) || WEXITSTATUS(ret) != 0) return false;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "flatpak info %s >/dev/null 2>&1", runtime);
    pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(1);
    }
    waitpid(pid, &ret, 0);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) return true;
    return false;
}

/* ==================================================================
 * Query
 * ================================================================== */
