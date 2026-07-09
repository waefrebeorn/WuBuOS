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
