/*
 * wubu_bottle_ops.c -- WuBuOS Bottles/Lutris: List + verify installed bottles.
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
 * List + verify installed bottles
 * ================================================================== */

int wubu_bottle_list(const char *install_dir, WubuBottle ***out_bottles, int *count) {
    if (!install_dir || !out_bottles || !count) return -1;
    *out_bottles = NULL;
    *count = 0;
    DIR *d = opendir(install_dir);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s/bottle.json", install_dir, ent->d_name);
        WubuBottle *bottle = wubu_bottle_load(meta_path);
        if (!bottle) continue;
        (*count)++;
        *out_bottles = realloc(*out_bottles, sizeof(WubuBottle *) * (*count));
        (*out_bottles)[(*count) - 1] = bottle;
    }
    closedir(d);
    return *count;
}

const WubuBottle *wubu_bottle_find(const char *install_dir, const char *name) {
    if (!install_dir || !name) return NULL;
    WubuBottle **bottles = NULL;
    int count = 0;
    int ret = wubu_bottle_list(install_dir, &bottles, &count);
    if (ret < 0) return NULL;
    for (int i = 0; i < count; i++) {
        if (strcmp(bottles[i]->name, name) == 0) {
            const WubuBottle *result = bottles[i];
            free(bottles);
            return result;
        }
    }
    free(bottles);
    return NULL;
}

bool wubu_bottle_verify(WubuBottle *bottle) {
    if (!bottle) return false;
    if (bottle->prefix_path[0]) {
        struct stat st;
        if (stat(bottle->prefix_path, &st) != 0) return false;
    }
    if (bottle->prefix_path[0]) {
        char meta[512];
        snprintf(meta, sizeof(meta), "%s/bottle.json", bottle->prefix_path);
        struct stat st;
        if (stat(meta, &st) != 0) return false;
    }
    return true;
}
