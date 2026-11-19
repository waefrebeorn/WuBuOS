/*
 * wubu_guest_import.c -- see wubu_guest_import.h.
 *
 * L8 "load their system": the WSL-style import -> launch leg. One managed
 * root store (~/.wubu/roots/<name>), extraction delegated to wubu_rd_load,
 * execution delegated to the L9 validated path (wubu_ns_guests_launch ->
 * wubu_ct_create/set_cmd/start), publication delegated to wubu_ns_guests_sync.
 * The directory is the registry: every scan reads the live store, and the
 * live RUNNING state comes from the wubu_host_exec registry -- no mirror.
 */
#include "../egojar.h"
#include "wubu_guest_import.h"
#include "wubu_ramdisk.h"
#include "wubu_ramdisk_internal.h"
#include "wubu_host_exec.h"
#include "wubu_ns_guests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

#define GI_MAX_ROOTS 64   /* matches WUBU_CT_REGISTRY_MAX / GUESTS_MAX */

/* -- Managed roots directory ---------------------------------------- */

void wubu_guest_roots_dir(char *out, size_t outsz) {
    if (!out || outsz == 0) return;
    const char *env = getenv("WUBU_GUEST_ROOTS_DIR");
    if (env && *env) {
        snprintf(out, outsz, "%s", env);
    } else {
        const char *home = getenv("HOME");
        snprintf(out, outsz, "%s/.wubu/roots", home ? home : ".");
    }
    size_t l = strlen(out);
    while (l > 1 && out[l - 1] == '/') out[--l] = '\0';
}

int wubu_guest_name_valid(const char *name) {
    if (!name || !*name) return 0;
    size_t l = strlen(name);
    if (l >= WUBU_CT_NAME_MAX) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (size_t i = 0; i < l; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

/* -- Small filesystem helpers (no shell: fork+execv only) ------------ */

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* rm -rf via execv -- never /bin/sh -c on a path we were handed. */
static int rm_rf(const char *path) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execl("/bin/rm", "rm", "-rf", path, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/* A launchable root needs a shell the exec path can actually run. */
static int root_has_shell(const char *root) {
    char p[1024];
    snprintf(p, sizeof(p), "%s/bin/sh", root);
    if (access(p, X_OK) == 0) return 1;
    snprintf(p, sizeof(p), "%s/busybox", root);
    if (access(p, X_OK) == 0) return 1;
    snprintf(p, sizeof(p), "%s/bin/busybox", root);
    if (access(p, X_OK) == 0) return 1;
    return 0;
}

static int root_path(const char *name, char *out, size_t outsz) {
    char dir[768];
    wubu_guest_roots_dir(dir, sizeof(dir));
    snprintf(out, outsz, "%s/%s", dir, name);
    return 0;
}

/* -- Scan: the directory IS the imported-root registry --------------- */

typedef struct {
    char name[WUBU_CT_NAME_MAX];
    char root[WUBU_CT_ROOT_MAX];
} GiRoot;

static GiRoot g_roots[GI_MAX_ROOTS];
static int g_roots_n = 0;

static int gi_cmp(const void *a, const void *b) {
    return strcmp(((const GiRoot *)a)->name, ((const GiRoot *)b)->name);
}

static void gi_scan(void) {
    g_roots_n = 0;
    char dir[768];
    wubu_guest_roots_dir(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && g_roots_n < GI_MAX_ROOTS) {
        if (de->d_name[0] == '.') continue;
        if (!wubu_guest_name_valid(de->d_name)) continue;
        char root[WUBU_CT_ROOT_MAX], meta[1024];
        root_path(de->d_name, root, sizeof(root));
        struct stat st;
        if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        /* Only manage roots THIS module imported (the ownership marker). */
        snprintf(meta, sizeof(meta), "%s/.wubu-guest", root);
        if (access(meta, F_OK) != 0) continue;
        if (!root_has_shell(root)) continue;
        /* name_valid() already bounded the length < WUBU_CT_NAME_MAX. */
        size_t nl = strlen(de->d_name);
        memcpy(g_roots[g_roots_n].name, de->d_name, nl + 1);
        snprintf(g_roots[g_roots_n].root, WUBU_CT_ROOT_MAX, "%s", root);
        g_roots_n++;
    }
    closedir(d);
    qsort(g_roots, (size_t)g_roots_n, sizeof(GiRoot), gi_cmp);
}

int wubu_guest_import_count(void) {
    gi_scan();
    return g_roots_n;
}

int wubu_guest_import_entry(int idx, char *name, size_t namesz,
                            char *root, size_t rootsz,
                            char *state, size_t statesz) {
    if (idx < 0 || idx >= g_roots_n) return -1;
    if (name && namesz) snprintf(name, namesz, "%s", g_roots[idx].name);
    if (root && rootsz) snprintf(root, rootsz, "%s", g_roots[idx].root);
    if (state && statesz)
        snprintf(state, statesz, "%s",
                 wubu_ct_registry_find(g_roots[idx].name) ? "running" : "imported");
    return 0;
}

int wubu_guest_import_root(const char *name, char *out, size_t outsz) {
    if (!wubu_guest_name_valid(name)) return -1;
    gi_scan();
    for (int i = 0; i < g_roots_n; i++) {
        if (strcmp(g_roots[i].name, name) == 0) {
            if (out && outsz) snprintf(out, outsz, "%s", g_roots[i].root);
            return 0;
        }
    }
    return -1;
}

/* -- Import ---------------------------------------------------------- */

int wubu_guest_import(const char *path, const char *name) {
    if (!path || !*path || !wubu_guest_name_valid(name)) return -1;
    if (wubu_ct_registry_find(name)) return -1;      /* running: refuse */

    struct stat st;
    if (stat(path, &st) != 0) return -1;
    ImgFormat fmt = detect_format(path);
    if (fmt == IMG_UNKNOWN) return -1;               /* honest refusal */

    char dir[768], root[WUBU_CT_ROOT_MAX];
    wubu_guest_roots_dir(dir, sizeof(dir));
    if (mkdir_p(dir, 0755) != 0) return -1;
    root_path(name, root, sizeof(root));
    if (stat(root, &st) == 0) return -1;             /* never clobber */
    if (mkdir(root, 0755) != 0) return -1;

    /* Extraction: the ONE owner (wubu_rd_load + detect_format). DISK mode
     * so destroy never tries to unmount; MOUNTED = "directory ready". */
    WubuRamdisk rd;
    memset(&rd, 0, sizeof(rd));
    rd.mode = WUBU_RD_DISK;
    rd.state = WUBU_RD_MOUNTED;
    snprintf(rd.path, sizeof(rd.path), "%s", root);
    snprintf(rd.image_path, sizeof(rd.image_path), "%s", path);
    if (wubu_rd_load(&rd) != 0) {
        rm_rf(root);
        return -1;
    }

    /* Validate: a root with no shell is not a launchable system. */
    if (!root_has_shell(root)) {
        rm_rf(root);
        return -1;
    }

    /* The guest child unshares a user namespace without a uid mapping
     * (ct_iso_*), so its fsuid is the overflow id: DAC "other" write on
     * the root dir is what lets the guest write into its own filesystem. */
    chmod(root, 0777);
    char tmpdir[1024];
    snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", root);
    if (mkdir(tmpdir, 0777) == 0) chmod(tmpdir, 0777);

    /* Ownership marker: remove() only deletes roots that carry it. */
    char meta[1024];
    snprintf(meta, sizeof(meta), "%s/.wubu-guest", root);
    FILE *f = fopen(meta, "w");
    if (f) {
        fprintf(f, "name: %s\nsource: %s\n", name, path);
        fclose(f);
    }

    wubu_ns_guests_sync();
    return 0;
}

/* -- Launch (the SAME validated exec path L9 uses) -------------------- */

int wubu_guest_launch(const char *name, const char *cmdline) {
    char root[WUBU_CT_ROOT_MAX];
    if (wubu_guest_import_root(name, root, sizeof(root)) != 0) return -1;
    /* wubu_ns_guests_launch resolves the root through wubu_guest_import_root
     * and runs wubu_ct_create/set_cmd/start -- no second exec path here. */
    return wubu_ns_guests_launch(name, (cmdline && *cmdline) ? cmdline : "/bin/sh");
}

/* -- Remove ----------------------------------------------------------- */

int wubu_guest_remove(const char *name) {
    if (!wubu_guest_name_valid(name)) return -1;
    if (wubu_ct_registry_find(name)) return -1;      /* running: refuse */
    char root[WUBU_CT_ROOT_MAX], meta[1024];
    root_path(name, root, sizeof(root));
    struct stat st;
    if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    snprintf(meta, sizeof(meta), "%s/.wubu-guest", root);
    if (access(meta, F_OK) != 0) return -1;          /* not ours: refuse */
    if (rm_rf(root) != 0) return -1;
    wubu_ns_guests_sync();
    return 0;
}

/* -- Import argument parsing (ONE parser: terminal + guests/import) --- */

int wubu_guest_import_cmdline(const char *args) {
    if (!args) return -1;
    const char *p = args;
    while (*p == ' ' || *p == '\t') p++;
    char path[1024];
    size_t n = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (n + 1 >= sizeof(path)) return -1;
            path[n++] = *p++;
        }
        if (*p != '"') return -1;
        p++;
    } else {
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            if (n + 1 >= sizeof(path)) return -1;
            path[n++] = *p++;
        }
    }
    path[n] = '\0';
    while (*p == ' ' || *p == '\t') p++;
    char name[WUBU_CT_NAME_MAX];
    n = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        if (n + 1 >= sizeof(name)) return -1;
        name[n++] = *p++;
    }
    name[n] = '\0';
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p) return -1;                               /* trailing garbage */
    if (!path[0] || !name[0]) return -1;
    return wubu_guest_import(path, name);
}

/* -- Terminal dispatch ------------------------------------------------- */

int wubu_guest_cli(const char *line, char *out, size_t outsz) {
    if (!line || !out || outsz == 0) return 0;
    out[0] = '\0';
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "guest", 5) != 0) return 0;
    if (p[5] != '\0' && p[5] != ' ' && p[5] != '\t') return 0;
    p += 5;
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '\0' || strcmp(p, "help") == 0) {
        snprintf(out, outsz,
                 "guest import <path> <name>   import a rootfs (dir/tar.gz/tar.zst/cpio.gz)\n"
                 "guest launch <name> [cmd]    run a command in the guest (default /bin/sh)\n"
                 "guest list                   imported guests + state\n"
                 "guest remove <name>          delete an imported root");
        return 1;
    }

    if (strcmp(p, "list") == 0) {
        int n = wubu_guest_import_count();
        if (n == 0) {
            snprintf(out, outsz, "no guests imported (guest import <path> <name>)");
            return 1;
        }
        size_t off = 0;
        for (int i = 0; i < n && off + 1 < outsz; i++) {
            char name[WUBU_CT_NAME_MAX], root[WUBU_CT_ROOT_MAX], state[16];
            if (wubu_guest_import_entry(i, name, sizeof(name), root, sizeof(root),
                                        state, sizeof(state)) != 0) continue;
            int w = snprintf(out + off, outsz - off, "%s\t%s\t%s\n",
                             name, state, root);
            if (w < 0 || (size_t)w >= outsz - off) break;
            off += (size_t)w;
        }
        return 1;
    }

    if (strncmp(p, "import", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        const char *args = p + 6;
        while (*args == ' ' || *args == '\t') args++;
        /* Recover path+name for the message by re-parsing is wasteful;
         * report against the raw args, honestly. */
        if (wubu_guest_import_cmdline(args) == 0)
            snprintf(out, outsz, "imported guest: %s", args);
        else
            snprintf(out, outsz,
                     "guest import: refused (need: <path> <name>, a readable "
                     "dir/tar.gz/tar.zst/cpio.gz with /bin/sh or /busybox, "
                     "and a free valid name)");
        return 1;
    }

    if (strncmp(p, "launch", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        const char *q = p + 6;
        while (*q == ' ' || *q == '\t') q++;
        char name[WUBU_CT_NAME_MAX];
        size_t n = 0;
        while (*q && *q != ' ' && *q != '\t') {
            if (n + 1 >= sizeof(name)) { name[0] = '\0'; break; }
            name[n++] = *q++;
        }
        name[n] = '\0';
        while (*q == ' ' || *q == '\t') q++;
        if (!name[0]) {
            snprintf(out, outsz, "guest launch: need a name");
            return 1;
        }
        if (wubu_guest_launch(name, *q ? q : NULL) == 0)
            snprintf(out, outsz, "launched guest %s%s%s", name,
                     *q ? ": " : "", *q ? q : "");
        else
            snprintf(out, outsz,
                     "guest launch: refused (%s not imported, already running, "
                     "or bad command)", name);
        return 1;
    }

    if (strncmp(p, "remove", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        const char *q = p + 6;
        while (*q == ' ' || *q == '\t') q++;
        char name[WUBU_CT_NAME_MAX];
        size_t n = 0;
        while (*q && *q != ' ' && *q != '\t') {
            if (n + 1 >= sizeof(name)) { name[0] = '\0'; break; }
            name[n++] = *q++;
        }
        name[n] = '\0';
        if (wubu_guest_remove(name) == 0)
            snprintf(out, outsz, "removed guest %s", name);
        else
            snprintf(out, outsz,
                     "guest remove: refused (%s not imported or running)", name);
        return 1;
    }

    snprintf(out, outsz, "guest: unknown subcommand (try 'guest help')");
    return 1;
}
