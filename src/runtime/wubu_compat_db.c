/*
 * wubu_compat_db.c -- WuBuOS per-app Windows-compat database (SteamOS ProtonDB
 * + shader-cache lesson). Real on-disk JSON store, no SQLite dependency.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_compat_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Expand "~" to $HOME (caller frees). */
static char *expand_home(const char *p) {
    if (p && p[0] == '~') {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        size_t n = strlen(home) + strlen(p + 1) + 2;
        char *o = malloc(n);
        if (o) snprintf(o, n, "%s%s", home, p + 1);
        return o;
    }
    return strdup(p ? p : WUBU_COMPAT_STORE);
}

void wubu_compat_normalize_title(const char *raw, char *out, int out_len) {
    int j = 0;
    if (raw) {
        for (int i = 0; raw[i] && j < out_len - 1; i++) {
            char c = raw[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            else if (c == ' ' || c == '-' || c == '.' || c == '/') c = '_';
            else if (c == ':' || c == '\\' || c == '(' || c == ')') continue;
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

static char *store_dir(void) {
    return expand_home(WUBU_COMPAT_STORE);
}

int wubu_compat_db_init(void) {
    char *dir = store_dir();
    if (!dir) return -1;
    /* mkdir -p: create each component (store may be nested under ~/.wubu). */
    for (char *p = dir + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(dir, 0755);
            *p = '/';
        }
    }
    int rc = mkdir(dir, 0755);
    free(dir);
    if (rc != 0 && errno != EEXIST) return -1;
    return 0;
}

/* Build the per-title json path into out (size n). */
static int title_path(const char *title, char *out, int n) {
    char key[WUBU_COMPAT_TITLE_MAX];
    wubu_compat_normalize_title(title, key, sizeof(key));
    char *dir = store_dir();
    if (!dir) return -1;
    snprintf(out, n, "%s/%s.json", dir, key);
    free(dir);
    return 0;
}

int wubu_compat_db_set(const WubuCompatEntry *e) {
    if (!e || !e->title[0]) return -1;
    if (wubu_compat_db_init() != 0) return -1;

    char path[1024];
    if (title_path(e->title, path, sizeof(path)) != 0) return -1;

    FILE *f = fopen(path, "w");
    if (!f) return -1;
    /* Real, human-readable store (ProtonDB-style profile). */
    fprintf(f, "{\n");
    fprintf(f, "  \"title\": \"%s\",\n", e->title);
    fprintf(f, "  \"proton_ver\": \"%s\",\n", e->proton_ver);
    fprintf(f, "  \"proton_flags\": \"%s\",\n", e->proton_flags);
    fprintf(f, "  \"env_overrides\": \"%s\",\n", e->env_overrides);
    fprintf(f, "  \"dll_overrides\": \"%s\",\n", e->dll_overrides);
    fprintf(f, "  \"rating\": %d,\n", e->rating);
    fprintf(f, "  \"cache_enabled\": %s\n", e->cache_enabled ? "true" : "false");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

int wubu_compat_db_get(const char *title, WubuCompatEntry *out) {
    if (!title || !out) return -1;
    memset(out, 0, sizeof(*out));

    char path[1024];
    if (title_path(title, path, sizeof(path)) != 0) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        /* Minimal "key": "value" parse (no external JSON dep). */
        char key[64], val[1800];
        if (sscanf(line, "  \"%63[^\"]\": \"%1799[^\"]\"", key, val) == 2) {
            if      (!strcmp(key, "title"))        strncpy(out->title, val, sizeof(out->title)-1);
            else if (!strcmp(key, "proton_ver"))   strncpy(out->proton_ver, val, sizeof(out->proton_ver)-1);
            else if (!strcmp(key, "proton_flags")) strncpy(out->proton_flags, val, sizeof(out->proton_flags)-1);
            else if (!strcmp(key, "env_overrides"))strncpy(out->env_overrides, val, sizeof(out->env_overrides)-1);
            else if (!strcmp(key, "dll_overrides"))strncpy(out->dll_overrides, val, sizeof(out->dll_overrides)-1);
            else if (!strcmp(key, "rating"))       out->rating = atoi(val);
        } else if (sscanf(line, "  \"cache_enabled\": %15s", val) == 1) {
            out->cache_enabled = (val[0] == 't' || val[0] == 'T');
        } else {
            char num[32];
            if (sscanf(line, "  \"rating\": %31s", num) == 1) {
                /* strip trailing comma */
                size_t L = strlen(num);
                if (L && num[L-1] == ',') num[L-1] = '\0';
                out->rating = atoi(num);
            }
        }
    }
    fclose(f);
    if (out->title[0] == '\0') return -1;
    return 0;
}

int wubu_compat_db_del(const char *title) {
    char path[1024];
    if (title_path(title, path, sizeof(path)) != 0) return -1;
    /* unlink returns 0 whether or not it existed. */
    unlink(path);
    return 0;
}

int wubu_compat_cache_dir(const char *title, char *out_path, int path_len) {
    if (!title || !out_path || path_len <= 0) return -1;
    char key[WUBU_COMPAT_TITLE_MAX];
    wubu_compat_normalize_title(title, key, sizeof(key));

    char *dir = store_dir();
    if (!dir) return -1;
    snprintf(out_path, path_len, "%s/%s.cache", dir, key);
    free(dir);

    /* mkdir -p for the cache path (store may be nested). */
    for (char *p = out_path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(out_path, 0755);
            *p = '/';
        }
    }
    int rc = mkdir(out_path, 0755);
    if (rc != 0 && errno != EEXIST) return -1;
    return 0;
}
