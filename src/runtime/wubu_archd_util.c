/* wubu_archd_util.c -- Process / filesystem helper utilities for the
 * Arch daemon. Self-contained: run_cmd / run_chroot_cmd (fork+exec, no
 * system()), write_file, mkdir_p. Types + WUBU_ARCHD_MAX_CMD via wubu_archd.h.
 * Minimal includes.
 */

#include "wubu_archd_internal.h"

int run_cmd(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

int run_chroot_cmd(const char *root, const char *fmt, ...) {
    char cmd[WUBU_ARCHD_MAX_CMD];
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);
    
    pid_t pid = fork();
    if (pid < 0) return -1;
    
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
    
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

/* N1 (BATTLESHIP): shell-free argv exec. No /bin/sh -c anywhere in the
 * child path — the argument vector is passed to execv() untouched, so
 * user-controlled values (package names, service names, paths) are inert
 * even when they contain ';', '$()', backticks, or other metacharacters. */
int run_argv(const char *file, char *const argv[]) {
    if (!file || !argv || !argv[0]) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(file, argv);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

int run_chroot_argv(const char *root, const char *file, char *const argv[]) {
    if (!root || !file || !argv || !argv[0]) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (chroot(root) != 0 || chdir("/") != 0) _exit(126);
        execv(file, argv);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

bool archd_write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}

int archd_mkdir_p(const char *path, mode_t mode) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';
    
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
        return -1;
    return 0;
}
