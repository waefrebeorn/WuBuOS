/*
 * wubu_print.c -- printing, as the two things the shell actually needs.
 *
 * WHY THIS EXISTS
 *   Win11's PrintDialog was the largest hole left in the gap matrix: there was no printing path in
 *   this tree at all. A dialog with nothing behind it is worse than no dialog, so the backend comes
 *   first: enumerate the printers the machine really has, submit a job, and report what happened.
 *
 * WHAT IT REFUSES TO DO
 *   * It never claims a job was queued because a command exited 0. A job id must come back from the
 *     spooler, or the answer is REFUSED with the spooler's own words.
 *   * It never builds a shell command. A file named `; rm -rf ~` must be printable without becoming
 *     executable, so arguments go to execvp as an argv array -- no /bin/sh anywhere in this path.
 *   * No spooler at all is ABSENT, not a failure: many machines genuinely have none.
 *
 * C11, minimal includes, no allocation beyond one read buffer.
 */
#define _GNU_SOURCE
#include "wubu_print.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *cups_conf(void) {
    const char *p = getenv("WUBU_CUPS_CONF");    /* test seam, checked every call */
    return (p && *p) ? p : "/etc/cups/printers.conf";
}

const char *wubu_print_conf_path(void) { return cups_conf(); }

int wubu_print_enum(WubuPrinter *out, int cap) {
    FILE *f = fopen(cups_conf(), "r");
    if (!f) return -1;                            /* no CUPS at all */
    char line[512];
    int n = 0, cur = -1;
    char default_name[WUBU_PRINTER_NAME_MAX] = {0};

    while (fgets(line, sizeof line, f)) {
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        if (strncmp(line, "<Printer ", 9) == 0) {
            char name[WUBU_PRINTER_NAME_MAX] = {0};
            if (sscanf(line + 9, "%95[^>]", name) != 1) continue;
            cur = -1;
            for (int i = 0; i < n; i++)
                if (strcmp(out[i].name, name) == 0) cur = i;
            if (cur < 0) {
                if (n >= cap) { cur = -2; continue; }   /* keep parsing for Default */
                cur = n++;
                memset(&out[cur], 0, sizeof out[cur]);
                snprintf(out[cur].name, sizeof out[cur].name, "%s", name);
                snprintf(out[cur].state, sizeof out[cur].state, "unknown");
                out[cur].accepting = 1;
            }
            continue;
        }
        if (strncmp(line, "</Printer>", 10) == 0) { cur = -1; continue; }

        /* DeviceURI tells the transport; State gives the human word CUPS prints. */
        if (cur >= 0) {
            if (strncmp(line, "DeviceURI ", 10) == 0) {
                snprintf(out[cur].uri, sizeof out[cur].uri, "%s", line + 10);
                const char *s = strstr(line + 10, "://");
                if (s) {
                    size_t tl = (size_t)(s - (line + 10));
                    if (tl >= sizeof out[cur].transport) tl = sizeof out[cur].transport - 1;
                    memcpy(out[cur].transport, line + 10, tl);
                    out[cur].transport[tl] = '\0';
                } else {
                    snprintf(out[cur].transport, sizeof out[cur].transport, "file");
                }
            } else if (strncmp(line, "State ", 6) == 0) {
                snprintf(out[cur].state, sizeof out[cur].state, "%s", line + 6);
            } else if (strncmp(line, "Accepting ", 10) == 0) {
                out[cur].accepting = strstr(line + 10, "Yes") != NULL;
            }
        }
        /* The default printer lives outside any <Printer> block in CUPS 2.x files, and as an
         * attribute in others; accept both spellings. */
        if (strncmp(line, "Default ", 8) == 0) {
            snprintf(default_name, sizeof default_name, "%s", line + 8);
        } else if (strncmp(line, "DefaultPrinter ", 15) == 0) {
            snprintf(default_name, sizeof default_name, "%s", line + 15);
        }
    }
    fclose(f);

    for (int i = 0; i < n; i++)
        out[i].is_default = default_name[0] && strcmp(out[i].name, default_name) == 0;
    return n;
}

/* Run argv, capture its first line of stdout, return the exit status.
 * execvp, never a shell: the arguments below include user-chosen paths. */
static int run_capture(char *const argv[], char *out, size_t cap, int *exit_code) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);          /* the spooler's refusal is the useful message */
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);                              /* 127 = "could not execute", the ABSENT case */
    }
    close(pipefd[1]);
    size_t used = 0;
    char buf[256];
    ssize_t got;
    while ((got = read(pipefd[0], buf, sizeof buf)) > 0) {
        if (used + 1 < cap) {
            size_t take = (size_t)got;
            if (take > cap - 1 - used) take = cap - 1 - used;
            memcpy(out + used, buf, take);
            used += take;
        }
    }
    out[used] = '\0';
    int status = 0;
    waitpid(pid, &status, 0);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return 0;
}

/* The spooler names the job; anything else is not a submission. */
static int parse_job_id(const char *text, char *id, size_t cap) {
    const char *p = strstr(text, "request id is ");
    if (!p) return 0;
    p += strlen("request id is ");
    size_t n = 0;
    while (*p && !isspace((unsigned char)*p) && n + 1 < cap) id[n++] = *p++;
    id[n] = '\0';
    return n > 0;
}

WubuPrintResult wubu_print_submit(const char *printer, const char *path,
                                  int copies, WubuPrintOut *out) {
    if (out) { memset(out, 0, sizeof *out); }
    if (!printer || !*printer || !path || !*path) return WUBU_PRINT_REFUSED;
    if (copies < 1) copies = 1;

    const char *cmd = getenv("WUBU_PRINT_CMD");
    if (!cmd || !*cmd) cmd = "lp";

    char nbuf[16];
    snprintf(nbuf, sizeof nbuf, "%d", copies);

    char *const argv[] = { (char *)cmd, (char *)"-d", (char *)printer,
                           (char *)"-n", nbuf, (char *)path, NULL };

    char text[512] = {0};
    int code = 0;
    if (run_capture(argv, text, sizeof text, &code) != 0) return WUBU_PRINT_REFUSED;

    if (code == 127) return WUBU_PRINT_ABSENT;            /* no spooler installed */

    char id[96];
    if (code == 0 && parse_job_id(text, id, sizeof id)) {
        if (out) { snprintf(out->job_id, sizeof out->job_id, "%s", id); out->copies = copies; }
        return WUBU_PRINT_SUBMITTED;
    }

    if (out) {
        size_t n = strlen(text);
        if (n >= sizeof out->message) n = sizeof out->message - 1;
        memcpy(out->message, text, n);
        out->message[n] = '\0';
    }
    /* The spooler said no. Which no matters: permission is a different fix from a bad queue.
     * Matched case-insensitively because lp says "Unauthorized", cups says "Permission denied" and
     * a remote queue says "Not permitted" -- three spellings of the same problem. */
    char lower[512];
    size_t i = 0;
    for (; text[i] && i + 1 < sizeof lower; i++) lower[i] = (char)tolower((unsigned char)text[i]);
    lower[i] = '\0';
    if (strstr(lower, "unauthoriz") || strstr(lower, "denied") || strstr(lower, "permission")
        || strstr(lower, "not permitted") || strstr(lower, "forbidden"))
        return WUBU_PRINT_DENIED;
    return WUBU_PRINT_REFUSED;
}

const char *wubu_print_result_name(WubuPrintResult r) {
    switch (r) {
        case WUBU_PRINT_SUBMITTED: return "SUBMITTED";
        case WUBU_PRINT_NO_PRINTER: return "NO_PRINTER";
        case WUBU_PRINT_DENIED:    return "DENIED";
        case WUBU_PRINT_REFUSED:   return "REFUSED";
        case WUBU_PRINT_ABSENT:    return "ABSENT";
        default:                   return "UNKNOWN";
    }
}
