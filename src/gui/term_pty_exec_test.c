/*
 * term_pty_exec_test.c -- prove an ARBITRARY PROGRAM can be started on a PTY.
 *
 * Every pre-existing entry point assumed the thing being started was a shell. This
 * gate exists because that assumption is what blocked hosting a real TUI (W-CODE), a
 * foreign kernel's console, or any debugger: they are not shells and must not get a
 * login flag applied.
 *
 * Runs /bin/echo, waits for its output to arrive on the master fd, and asserts the text
 * comes back. If the child never starts, exec fails, or the pty plumbing is wrong, the
 * test fails instead of passing on a silent empty buffer.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "term.h"
#include "term_pty.h"

int main(void)
{
    TermPtySession pty;
    memset(&pty, 0, sizeof(pty));

    static const char *argv[] = { "/bin/echo", "wubu-pty-exec-ok", NULL };

    if (term_pty_spawn_exec(argv, NULL, &pty) != 0) {
        printf("FAIL spawn_exec returned -1\n");
        return 1;
    }

    /* Master is O_NONBLOCK, so poll: a correct spawn delivers the bytes within a
     * fraction of a second. Bound the wait so a broken one fails rather than hangs. */
    char buf[512];
    size_t used = 0;
    bool seen = false;

    for (int i = 0; i < 100 && !seen; i++) {
        ssize_t n = read(pty.ptm_fd, buf + used, sizeof(buf) - 1 - used);
        if (n > 0) {
            used += (size_t)n;
            buf[used] = '\0';
            if (strstr(buf, "wubu-pty-exec-ok")) seen = true;
        } else {
            usleep(10000);                 /* 10 ms */
        }
    }
    buf[used] = '\0';

    /* Reap the child so the test does not leave a zombie behind for the watchdog. */
    int status = 0;
    waitpid(pty.child_pid, &status, 0);
    term_pty_cleanup(&pty);

    printf("spawn_exec read %zu bytes: %s", used, buf);
    if (!seen) {
        printf("FAIL expected marker missing\n");
        return 1;
    }

    printf("OK arbitrary program ran on the pty\n");
    return 0;
}
