/* wubu_proton_util.c -- Proton layer filesystem / parsing utilities.
 *
 * Self-contained module extracted from wubu_proton.c: recursive rm, dir
 * helpers, Steam path detection, VDF/appmanifest parsing. Uses the shared
 * g_proton state + Proton types via wubu_proton_internal.h. Minimal includes.
 * _XOPEN_SOURCE must precede system headers for FTW_DEPTH/FTW_PHYS.
 */

#define _XOPEN_SOURCE 700
#include "wubu_proton_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <ftw.h>

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

int rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    
    /* Create parent directories recursively */
    char *copy = strdup(path);
    char *p = copy + 1;
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(copy, 0755);
            *p = '/';
        }
        p++;
    }
    mkdir(copy, 0755);
    free(copy);
    return true;
}

uint64_t get_dir_size(const char *path) {
    uint64_t total = 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        
        char full[PROTON_PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        
        struct stat st;
        if (lstat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total += get_dir_size(full);
            } else {
                total += (uint64_t)st.st_size;
            }
        }
    }
    closedir(d);
    return total;
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

char *find_steam_path(void) {
    /* Check common Steam locations */
    const char *locations[] = {
        "~/.steam/steam",
        "~/.local/share/Steam",
        "/usr/share/steam",
        "/var/lib/flatpak/app/com.valvesoftware.Steam/current/active/files/steam",
        NULL
    };
    
    const char *home = getenv("HOME");
    for (int i = 0; locations[i]; i++) {
        char path[PROTON_PATH_MAX];
        if (locations[i][0] == '~' && home) {
            snprintf(path, sizeof(path), "%s%s", home, locations[i] + 1);
        } else {
            strncpy(path, locations[i], sizeof(path) - 1);
        }
        
        if (file_exists(path)) {
            char *lib_path = malloc(PROTON_PATH_MAX);
            if (lib_path) {
                /* Check for steamapps */
                char steamapps[PROTON_PATH_MAX];
                snprintf(steamapps, sizeof(steamapps), "%s/steamapps", path);
                if (file_exists(steamapps)) {
                    strcpy(lib_path, path);
                    return lib_path;
                }
            }
        }
    }
    return NULL;
}

void parse_vdf_pairs(const char *data, size_t len, char ***keys, char ***vals, int *count) {
    /* Simple VDF parser for appmanifest files */
    /* Format: "key" "value" with nested braces */
    /* This is a simplified version - real VDF is more complex */
    *count = 0;
    *keys = NULL;
    *vals = NULL;
}

int parse_appmanifest(const char *path, char *appid, size_t aid_len, char *name, size_t name_len, char *installdir, size_t idir_len) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char *data = NULL;
    size_t size = 0;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize > 0) {
        data = malloc(fsize + 1);
        fread(data, 1, fsize, f);
        data[fsize] = '\0';
    }
    fclose(f);
    
    if (!data) return -1;
    
    /* Simple parsing for "appid" "12345" and "name" "Game Name" and "installdir" "folder" */
    char *p = data;
    while (*p) {
        if (*p == '"') {
            p++;
            char *key_start = p;
            while (*p && *p != '"') p++;
            if (!*p) break;
            *p = '\0';
            p++;
            while (*p && *p != '"') p++;
            if (!*p) break;
            p++;
            char *val_start = p;
            while (*p && *p != '"') p++;
            if (!*p) break;
            *p = '\0';
            
            if (strcmp(key_start, "appid") == 0) {
                strncpy(appid, val_start, aid_len - 1);
            } else if (strcmp(key_start, "name") == 0) {
                strncpy(name, val_start, name_len - 1);
            } else if (strcmp(key_start, "installdir") == 0) {
                strncpy(installdir, val_start, idir_len - 1);
            }
            p++;
        } else {
            p++;
        }
    }
    
    free(data);
    return (appid[0] && name[0]) ? 0 : -1;
}
