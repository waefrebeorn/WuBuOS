/* wubu_deploy_util.c -- Deployment layer file/command utilities.
 *
 * Self-contained module extracted from wubu_deploy.c: run_command(_capture),
 * write_file, mkdir_p, copy_file. Uses the public wubu_deploy API types via
 * wubu_deploy_internal.h. Minimal includes.
 */

#include "wubu_deploy_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

bool run_command(const char* cmd, const char* workdir) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (workdir) chdir(workdir);
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool run_command_capture(const char* cmd, char* output, size_t output_size) {
    FILE* fp = popen(cmd, "r");
    if (!fp) return false;
    size_t n = fread(output, 1, output_size - 1, fp);
    output[n] = '\0';
    pclose(fp);
    return true;
}

bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}

bool mkdir_p(const char* path, mode_t mode) {
    char* p = strdup(path);
    char* sep = p;
    while ((sep = strchr(sep + 1, '/'))) {
        *sep = '\0';
        mkdir(p, mode);
        *sep = '/';
    }
    mkdir(p, mode);
    free(p);
    return true;
}

bool copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return true;
}

/* ============================================================
 * Rootfs Creation
 * ============================================================ */
