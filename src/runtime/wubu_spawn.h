/*
 * wubu_spawn.h  --  Shell-free external program launcher.
 *
 * Single dependency-free definition of wubu_run_program(): fork()+execvp()
 * so callers never spawn /bin/sh. No container/compiler/holyc deps -- this
 * file is meant to be linkable by any test target in isolation.
 */
#ifndef WUBUOS_SPAWN_H
#define WUBUOS_SPAWN_H

#include <stdbool.h>

/*
 * Run an external program WITHOUT going through the shell.
 * form!=function closure: every former system("cmd") / popen("cmd") in the
 * tree must route here so no /bin/sh is spawned and no shell-injection
 * surface exists. argv must be NULL-terminated; the first element is the
 * program as resolved by execvp(3) (PATH search). When silent is true the
 * child's stdout/stderr are redirected to /dev/null.
 * Returns the child's exit status (0 = success), or -1 if fork/exec/wait
 * failed before the child could run.
 */
int wubu_run_program(const char *file, char *const argv[], bool silent);

/*
 * wubu_popen_read — shell-free popen("r") replacement.
 * Forks+execs `file argv[]` (NO shell, no injection), captures stdout into a
 * heap buffer, NUL-terminates it, and returns it. Caller must free() the result.
 * Returns NULL on fork/exec/read error. Like popen but argv-based and safe.
 */
char *wubu_popen_read(const char *file, char *const argv[]);

#endif /* WUBUOS_SPAWN_H */
