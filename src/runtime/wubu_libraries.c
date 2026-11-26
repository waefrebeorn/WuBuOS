/*
 * wubu_libraries.c -- see wubu_libraries.h. No allocator, no shell, one small file read and write.
 *
 * Config format (one line per folder, "#" comments):
 *     pictures=/media/disk/Photos
 *     documents=/srv/scans
 * A line naming a library that does not exist is ignored, not fatal.
 *
 * C11.
 */
#include "wubu_libraries.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *const kNames[WUBU_LIB_COUNT] = {
    "Documents", "Pictures", "Music", "Videos"
};
static const char *const kKeys[WUBU_LIB_COUNT] = {
    "documents", "pictures", "music", "videos"
};
static const char *const kHomeDirs[WUBU_LIB_COUNT] = {
    "Documents", "Pictures", "Music", "Videos"
};
static const char *const kXdgVars[WUBU_LIB_COUNT] = {
    "XDG_DOCUMENTS_DIR", "XDG_PICTURES_DIR", "XDG_MUSIC_DIR", "XDG_VIDEOS_DIR"
};

const char *wubu_libraries_conf_path(void)
{
    static char path[512];
    static bool  have;
    if (have) return path;

    const char *env = getenv("WUBU_LIBRARIES_CONF");
    if (env && env[0]) {
        snprintf(path, sizeof path, "%s", env);
        have = true;
        return path;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/";
    snprintf(path, sizeof path, "%s/.config/wubu/libraries.conf", home);
    have = true;
    return path;
}

const char *wubu_library_name(WubuLibraryId id)
{
    if ((int)id < 0 || (int)id >= WUBU_LIB_COUNT) return "";
    return kNames[id];
}

WubuLibrary *wubu_library_get(WubuLibraries *libs, WubuLibraryId id)
{
    if (!libs || (int)id < 0 || (int)id >= WUBU_LIB_COUNT) return NULL;
    return &libs->lib[id];
}

static bool dir_exists(const char *path)
{
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

/* Append a folder, or refresh the exists flag when it is already there. Returns 0 added, 1 present,
 * -1 full. */
static int put_folder(WubuLibrary *lib, const char *path, bool from_default)
{
    if (!lib || !path || !path[0]) return -1;
    for (int i = 0; i < lib->count; i++) {
        if (strcmp(lib->folders[i].path, path) == 0) {
            lib->folders[i].exists = dir_exists(path);
            return 1;
        }
    }
    if (lib->count >= WUBU_LIB_MAX_FOLDERS) return -1;
    WubuLibraryFolder *f = &lib->folders[lib->count];
    snprintf(f->path, sizeof f->path, "%s", path);
    f->exists = dir_exists(path);
    f->from_default = from_default;
    lib->count++;
    return 0;
}

/* The default folder for a library: $XDG_*_DIR when the session set one, else $HOME/<name>. */
static void default_path(WubuLibraryId id, char *out, size_t cap)
{
    out[0] = '\0';
    const char *xdg = getenv(kXdgVars[id]);
    if (xdg && xdg[0] == '/') {                 /* XDG values must be absolute to be usable */
        snprintf(out, cap, "%s", xdg);
        return;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0]) return;
    snprintf(out, cap, "%s/%s", home, kHomeDirs[id]);
}

void wubu_libraries_load(WubuLibraries *libs)
{
    if (!libs) return;
    memset(libs, 0, sizeof(*libs));

    for (int i = 0; i < WUBU_LIB_COUNT; i++) {
        libs->lib[i].id = (WubuLibraryId)i;
        snprintf(libs->lib[i].name, sizeof libs->lib[i].name, "%s", kNames[i]);
        char def[WUBU_LIB_PATH_MAX];
        default_path((WubuLibraryId)i, def, sizeof def);
        if (def[0]) (void)put_folder(&libs->lib[i], def, true);
    }

    FILE *f = fopen(wubu_libraries_conf_path(), "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof line, f)) {
            size_t n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
            if (line[0] == '#' || line[0] == '\0') continue;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            const char *key = line;
            const char *val = eq + 1;
            while (*key == ' ' || *key == '\t') key++;
            while (*val == ' ' || *val == '\t') val++;
            for (int i = 0; i < WUBU_LIB_COUNT; i++) {
                /* Accept both "pictures" and "pictures+=" so a hand-written file is not a mystery. */
                size_t klen = strlen(kKeys[i]);
                if (strncmp(key, kKeys[i], klen) != 0) continue;
                char tail = key[klen];
                if (tail != '\0' && !(tail == '+' && key[klen + 1] == '\0')) continue;
                if (val[0] == '/') (void)put_folder(&libs->lib[i], val, false);
                break;
            }
        }
        fclose(f);
    }
    libs->loaded = 1;
}

int wubu_library_add(WubuLibraries *libs, WubuLibraryId id, const char *path)
{
    WubuLibrary *lib = wubu_library_get(libs, id);
    if (!lib) return -1;
    return put_folder(lib, path, false);
}

int wubu_library_remove(WubuLibraries *libs, WubuLibraryId id, const char *path)
{
    WubuLibrary *lib = wubu_library_get(libs, id);
    if (!lib || !path) return 1;
    for (int i = 0; i < lib->count; i++) {
        if (strcmp(lib->folders[i].path, path) != 0) continue;
        for (int j = i; j < lib->count - 1; j++) lib->folders[j] = lib->folders[j + 1];
        lib->count--;
        return 0;
    }
    return 1;
}

int wubu_libraries_save(const WubuLibraries *libs)
{
    if (!libs) return -1;

    /* Create the directory if needed: .config/wubu may not exist on a fresh profile. */
    char dir[512];
    snprintf(dir, sizeof dir, "%s", wubu_libraries_conf_path());
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (dir[0]) (void)mkdir(dir, 0755);      /* EEXIST is fine */
    }

    FILE *f = fopen(wubu_libraries_conf_path(), "w");
    if (!f) return -1;
    fputs("# WuBuOS libraries -- one line per folder: <library>=<absolute path>\n", f);
    fputs("# Documents/Pictures/Music/Videos home defaults are derived at load and not written here.\n", f);
    for (int i = 0; i < WUBU_LIB_COUNT; i++) {
        for (int j = 0; j < libs->lib[i].count; j++) {
            const WubuLibraryFolder *fo = &libs->lib[i].folders[j];
            if (fo->from_default) continue;
            fprintf(f, "%s=%s\n", kKeys[i], fo->path);
        }
    }
    fclose(f);
    return 0;
}

static int by_name_then_dir(const void *a, const void *b)
{
    const WubuLibraryItem *x = a, *y = b;
    if (x->is_dir != y->is_dir) return x->is_dir ? -1 : 1;      /* directories first */
    return strcmp(x->name, y->name);
}

int wubu_library_list(const WubuLibraries *libs, WubuLibraryId id,
                      WubuLibraryItem *out, int max)
{
    if (!libs || !out || max <= 0 || (int)id < 0 || (int)id >= WUBU_LIB_COUNT) return -1;

    const WubuLibrary *lib = &libs->lib[id];
    int n = 0;

    for (int f = 0; f < lib->count && n < max; f++) {
        const WubuLibraryFolder *folder = &lib->folders[f];
        if (!folder->exists) continue;

        DIR *dir = opendir(folder->path);
        if (!dir) continue;

        struct dirent *ent;
        while (n < max && (ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

            /* De-duplicate by name: the first folder that holds a name wins, so the merged view is
             * stable rather than depending on directory order. */
            bool seen = false;
            for (int i = 0; i < n; i++)
                if (strcmp(out[i].name, ent->d_name) == 0) { seen = true; break; }
            if (seen) continue;

            char full[WUBU_LIB_PATH_MAX * 2];
            snprintf(full, sizeof full, "%s/%s", folder->path, ent->d_name);

            struct stat st;
            bool have = (stat(full, &st) == 0);

            WubuLibraryItem *it = &out[n];
            snprintf(it->name, sizeof it->name, "%s", ent->d_name);
            snprintf(it->path, sizeof it->path, "%s", full);
            const char *base = strrchr(folder->path, '/');
            snprintf(it->source, sizeof it->source, "%s", base ? base + 1 : folder->path);
            /* stat() is the answer; d_type is only a hint and is not even available under strict C11. */
            it->is_dir = have && S_ISDIR(st.st_mode);
            it->size = have ? (long)st.st_size : 0;
            it->modified = have ? (long)st.st_mtime : 0;
            n++;
        }
        closedir(dir);
    }

    qsort(out, (size_t)n, sizeof out[0], by_name_then_dir);
    return n;
}

int wubu_library_suggest(const WubuLibraries *libs, WubuLibraryId id,
                         char out[][WUBU_LIB_PATH_MAX], int max)
{
    if (!libs || !out || max <= 0 || (int)id < 0 || (int)id >= WUBU_LIB_COUNT) return 0;

    int n = 0;
    const char *home = getenv("HOME");
    if (!home || !home[0]) return 0;

    /* Candidates that exist on this machine and are not already in the library: the library's own
     * name under the home directory first (already added when it is the default, so usually skipped),
     * then the sibling user folders, which is exactly the "you have photos elsewhere" case. */
    for (int i = 0; i < WUBU_LIB_COUNT && n < max; i++) {
        char cand[WUBU_LIB_PATH_MAX];
        snprintf(cand, sizeof cand, "%s/%s", home, kHomeDirs[i]);
        if (!dir_exists(cand)) continue;

        bool already = false;
        for (int j = 0; j < libs->lib[id].count; j++)
            if (strcmp(libs->lib[id].folders[j].path, cand) == 0) already = true;
        if (already) continue;

        snprintf(out[n], WUBU_LIB_PATH_MAX, "%s", cand);
        n++;
    }
    return n;
}
