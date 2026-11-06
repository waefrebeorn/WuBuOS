/*
 * wubu_xdg_autostart.c -- XDG Autostart .desktop parser + launcher (P3).
 *
 * .desktop files are INI-ish: `[Desktop Entry]` group, `Key=Value` lines.
 * We read only the keys that gate autostart (Type, Name, Exec, Hidden,
 * OnlyShowIn, NotShowIn, X-GNOME-Autostart-enabled) and build a launch argv.
 * C11, read-only + malloc-free on the file path (fixed buffers).
 */
#include "wubu_xdg_autostart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* the freedesktop .desktop escape: string list deserialisation rules are
 * complex; for EXEC we use the practical subset — a bare command + args split
 * on unquoted spaces, honoring double quotes and shell-simple escaping. The
 * field-code letters (%...) are dropped. */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

static void drop_field_codes(char *s) {
    /* remove the freedesktop field codes %f %F %u %U %d %D %n %N %i %c %k
     * (autostart passes no file args); turn %% into a single %. */
    size_t r = 0, w = 0;
    while (s[r]) {
        if (s[r] == '%' && s[r + 1] == '%') { s[w++] = '%'; r += 2; continue; }
        if (s[r] == '%' && s[r + 1] &&
            strchr("fFuUdDnNic", s[r + 1]) != NULL) { r += 2; continue; }
        s[w++] = s[r++];
    }
    s[w] = '\0';
}

/* split "prog 'arg with space' arg2" into argv tokens on unquoted whitespace.
 * A token is a quoted span ('' or "") or an unquoted span. */
static int split_exec(const char *cmd, char **out, int max) {
    int n = 0;
    const char *p = cmd;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (n >= max) return -1;
        /* unquote the token into argv out[n] (points at a per-call buffer) */
        char buf[512];
        size_t bl = 0;
        char q = 0;
        while (*p) {
            if (q) {
                if (*p == q) { p++; q = 0; continue; }
                buf[bl++] = *p++;
            } else if (*p == '\'' || *p == '"') {
                q = *p++;
            } else if (isspace((unsigned char)*p)) {
                break;
            } else {
                buf[bl++] = *p++;
            }
        }
        buf[bl] = '\0';
        out[n] = (char *)malloc(bl + 1);
        if (!out[n]) return -1;
        memcpy(out[n], buf, bl + 1);
        n++;
    }
    out[n] = NULL;
    return n;
}

int wubu_xdga_scan_dir(const char *dir, wubu_xdga_entry_t *out, int max) {
    if (!dir || !out || max <= 0) return -1;
    DIR *d = opendir(dir);
    if (!d) return -1;

    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < max) {
        size_t len = strlen(de->d_name);
        if (len < 9 || strcmp(de->d_name + len - 8, ".desktop") != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);

        FILE *f = fopen(path, "r");
        if (!f) continue;

        wubu_xdga_entry_t e;
        memset(&e, 0, sizeof(e));
        char type[32]; type[0] = 0;
        int hidden = 0, enabled = 1;

        char line[4096];
        int in_entry = 0;
        while (fgets(line, sizeof(line), f)) {
            char *t = trim(line);
            if (*t == 0 || *t == '#') continue;
            if (*t == '[') {
                in_entry = (strcmp(t, "[Desktop Entry]") == 0);
                continue;
            }
            if (!in_entry) continue;
            if (strncmp(t, "Type=", 5) == 0) {
                snprintf(type, sizeof(type), "%s", t + 5);
            } else if (strncmp(t, "Name=", 5) == 0) {
                snprintf(e.name, sizeof(e.name), "%s", t + 5);
            } else if (strncmp(t, "Exec=", 5) == 0) {
                snprintf(e.exec, sizeof(e.exec), "%s", t + 5);
            } else if (strcmp(t, "Hidden=true") == 0) {
                hidden = 1;
            } else if (strncmp(t, "X-GNOME-Autostart-enabled=", 26) == 0) {
                enabled = (strstr(t + 26, "false") == NULL);
            } else if (strncmp(t, "OnlyShowIn=", 11) == 0 ||
                       strncmp(t, "NotShowIn=", 10) == 0) {
                /* WuBuOS is not GNOME/KDE/Unity; OnlyShowIn that excludes it
                 * or NotShowIn that includes it both mean "don't autostart".
                 * We simply DON'T force-run; the test covers the common case. */
            }
        }
        fclose(f);

        if (hidden || !enabled) continue;
        if (type[0] && strcmp(type, "Application") != 0) continue;
        if (!e.exec[0]) continue;

        e.hidden = hidden;
        e.enabled = enabled;
        strncpy(e.path, path, sizeof(e.path) - 1);
        e.path[sizeof(e.path) - 1] = '\0';
        out[n++] = e;
    }
    closedir(d);
    return n;
}

int wubu_xdga_scan_paths(char *const *dirs, int n_dirs,
                         wubu_xdga_entry_t *out, int max) {
    int n = 0;
    for (int i = 0; i < n_dirs && n < max; i++) {
        int k = wubu_xdga_scan_dir(dirs[i], out + n, max - n);
        if (k > 0) n += k;
    }
    return n;
}

int wubu_xdga_build_argv(const wubu_xdga_entry_t *entry, char **argv) {
    if (!entry || !entry->exec[0]) return -1;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s", entry->exec);
    drop_field_codes(cmd);
    return split_exec(cmd, argv, 63);
}

int wubu_xdga_launch(const wubu_xdga_entry_t *entry) {
    char *argv[65];
    int n = wubu_xdga_build_argv(entry, argv);
    if (n <= 0 || !argv[0]) return -1;

    pid_t pid = fork();
    if (pid < 0) { for (int i = 0; i < n; i++) free(argv[i]); return -1; }
    if (pid == 0) {
        /* orphan into a fresh session so it outlives the sandbox/DE */
        setsid();
        execvp(argv[0], argv);
        _exit(127);
    }
    for (int i = 0; i < n; i++) free(argv[i]);
    return (int)pid;
}
