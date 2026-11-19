/*
 * wubu_spawn.c  --  Shell-free external program launcher.
 *
 * See wubu_spawn.h. Dependency-free: only POSIX fork/exec/wait + open,
 * so any target can link this without pulling in container/compiler code.
 */
#include "../egojar.h"
#include "wubu_spawn.h"

#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
/* ---- Windows host: CreateProcess replaces fork/exec/wait --------------
 * Same contract as the POSIX branch: no shell, argv[0] is the program,
 * exit status returned; wubu_popen_read captures stdout only. */
#include <windows.h>
#include <string.h>

/* Append one argument, quoted per MSVCRT argv-parsing rules.
 * Returns the new position, or cap on overflow. */
static size_t port_quote_arg(const char *s, char *out, size_t cap, size_t pos)
{
    int need_quotes = (*s == '\0');
    for (const char *p = s; *p; p++)
        if (*p == ' ' || *p == '\t' || *p == '"') { need_quotes = 1; break; }
    if (!need_quotes) {
        size_t n = strlen(s);
        if (pos + n >= cap) return cap;
        memcpy(out + pos, s, n);
        return pos + n;
    }
    if (pos + 1 >= cap) return cap;
    out[pos++] = '"';
    size_t nbs = 0;
    for (const char *p = s; ; p++) {
        char c = *p;
        if (c == '\\') { nbs++; continue; }
        if (c == '"') {
            if (pos + 2 * nbs + 2 >= cap) return cap;
            memset(out + pos, '\\', 2 * nbs + 1); pos += 2 * nbs + 1;
            out[pos++] = '"';
            nbs = 0;
            continue;
        }
        if (c == '\0') {
            /* trailing backslashes before the closing quote must double */
            if (pos + 2 * nbs >= cap) return cap;
            memset(out + pos, '\\', 2 * nbs); pos += 2 * nbs;
            break;
        }
        if (nbs) {
            if (pos + nbs >= cap) return cap;
            memset(out + pos, '\\', nbs); pos += nbs;
            nbs = 0;
        }
        if (pos + 1 >= cap) return cap;
        out[pos++] = c;
    }
    if (pos + 1 >= cap) return cap;
    out[pos++] = '"';
    return pos;
}

static int port_cmdline(char *const argv[], char *out, size_t cap)
{
    size_t pos = 0;
    for (int i = 0; argv[i]; i++) {
        if (i) { if (pos + 1 >= cap) return -1; out[pos++] = ' '; }
        pos = port_quote_arg(argv[i], out, cap, pos);
        if (pos >= cap) return -1;
    }
    out[pos] = '\0';
    return 0;
}

int wubu_run_program(const char *file, char *const argv[], bool silent) {
    if (!file || !argv) return -1;
    (void)file; /* argv[0] carries the program; CreateProcess searches PATH */
    char cmd[8192];
    if (port_cmdline(argv, cmd, sizeof cmd) != 0) return -1;
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    HANDLE nul = INVALID_HANDLE_VALUE;
    if (silent) {
        SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
        nul = CreateFileA("NUL", GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &sa, OPEN_EXISTING, 0, NULL);
        if (nul == INVALID_HANDLE_VALUE) return -1;
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = nul;
        si.hStdError = nul;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

char *wubu_popen_read(const char *file, char *const argv[]) {
    if (!file || !argv) return NULL;
    (void)file;
    char cmd[8192];
    if (port_cmdline(argv, cmd, sizeof cmd) != 0) return NULL;
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return NULL;
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0); /* our copy: not inherited */
    HANDLE err = NULL;
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE),
                    GetCurrentProcess(), &err, 0, TRUE, DUPLICATE_SAME_ACCESS);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = err ? err : wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(wr);
    if (err) CloseHandle(err);
    if (!ok) { CloseHandle(rd); return NULL; } /* binary not found: no result */
    size_t cap = 4096, pos = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        CloseHandle(rd);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return NULL;
    }
    DWORD r;
    while (ReadFile(rd, buf + pos, (DWORD)(cap - pos - 1), &r, NULL) && r > 0) {
        pos += r;
        if (pos + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) break;
            buf = nb;
        }
    }
    buf[pos] = '\0';
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    /* same contract as POSIX: 127 with NO output means "never started" */
    if (code == 127 && pos == 0) { free(buf); return NULL; }
    return buf;
}

#else /* POSIX fork/exec/wait */

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
    /* execvp failure in the child (binary not found) exits 127 having written
     * NOTHING.  Only that case is "no result" (NULL) so callers can fall back.
     *
     * Do NOT treat every 127 as failure: /bin/sh ALSO exits 127 for its own
     * "command not found", and it writes that message to stderr on the way
     * out.  Discarding it made every mistyped command look like it had never
     * started -- the shell's own diagnosis was being thrown away.  If there
     * was output, the program ran; report what it said. */
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127 && pos == 0) {
        free(buf);
        return NULL;
    }
    return buf;
}
#endif /* _WIN32 */
