/*
 * wubu_spawn.c  --  Shell-free external program launcher.
 *
 * See wubu_spawn.h. Dependency-free: only POSIX fork/exec/wait + open,
 * so any target can link this without pulling in container/compiler code.
 */
#include "wubu_spawn.h"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

int wubu_run_program(const char *file, char *const argv[], bool silent) {
    if (!file || !argv) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (silent) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
        }
        execvp(file, argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

/*
 * wubu_popen_read — shell-free popen("r") replacement.
 * Forks+execs `file argv[]` (NO shell), captures stdout into a heap buffer,
 * NUL-terminates, returns it. Caller must free(). Returns NULL on error.
 */
char *wubu_popen_read(const char *file, char *const argv[]) {
    if (!file || !argv) return NULL;
    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return NULL; }
    if (pid == 0) {
        close(pipefd[0]);
        if (pipefd[1] != STDOUT_FILENO) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }
        execvp(file, argv);
        _exit(127);
    }
    close(pipefd[1]);
    size_t cap = 4096, pos = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { close(pipefd[0]); return NULL; }
    ssize_t r;
    while ((r = read(pipefd[0], buf + pos, cap - pos - 1)) > 0) {
        pos += (size_t)r;
        if (pos + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { buf[pos] = '\0'; close(pipefd[0]); return buf; }
            buf = nb;
        }
    }
    buf[pos] = '\0';
    close(pipefd[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return buf;
}
