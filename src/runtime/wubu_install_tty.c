/*
 * wubu_install_tty.c -- the stdio front end for the configurator.
 *
 * A thin, honest shell around wubu_install_wizard: it turns prompts into
 * stdout lines and answers into stdin reads, turns echo off for the password
 * where the platform allows, and writes the confirmed plan out as the `/n/install`
 * answers document. Nothing else -- the disk write and user creation that would
 * consume that document are the next layers, not this module's job.
 */
#include "wubu_install_tty.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

/* One prompted value: `def` is the value if the user just presses enter. */
static int t_ask(const char *prompt, const char *def, char *out, size_t n)
{
    printf("%s [%s]: ", prompt, def);
    fflush(stdout);
    char line[256];
    if (!fgets(line, sizeof line, stdin)) return 0;   /* EOF = cancel */
    line[strcspn(line, "\r\n")] = 0;
    if (!line[0]) { snprintf(out, n, "%s", def); }
    else           { snprintf(out, n, "%s", line); }
    return out[0] ? 1 : 0;
}

/* A non-echoed value (the password). Echo is disabled where termios exists;
 * on Windows the Windows CRT has no termios, so the read is echoed -- fine for
 * a pipe (a test), and the reason a real install injects a GUI/console UI
 * instead of this front end for the password. */
static int t_secret(const char *prompt, char *out, size_t n)
{
    printf("%s: ", prompt);
    fflush(stdout);
#ifndef _WIN32
    struct termios attr, raw;
    tcgetattr(STDIN_FILENO, &attr);
    raw = attr;
    raw.c_lflag &= ~(unsigned)ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    char *ok = fgets(out, (int)n, stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);
    if (!ok) return 0;   /* EOF = cancel */
#else
    if (!fgets(out, (int)n, stdin)) return 0;
#endif
    out[strcspn(out, "\r\n")] = 0;
    printf("\n");
    /* An empty password is handed to the wizard, which rejects it; returning 0
     * here would be read as a cancel, so only EOF cancels. */
    return 1;
}

/* A real install needs a fresh random salt; the stdio front end has no RNG, so
 * a constant is used. The comment is the contract: the boot/OS configurator
 * injects a random salt from the kernel RNG instead of this front end. */
static int t_salt(char *out, size_t n)
{
    snprintf(out, n, "wubutty");
    return 1;
}

/* The single commit point: show the masked summary, require an explicit yes. */
static int t_confirm(const char *summary)
{
    printf("\n%s\n\nInstall now? [y/N]: ", summary);
    fflush(stdout);
    char line[16];
    if (!fgets(line, sizeof line, stdin)) return 0;
    line[strcspn(line, "\r\n")] = 0;
    return (line[0] == 'y' || line[0] == 'Y') ? 1 : 0;
}

int wubu_install_run_tty(wubu_install_plan *plan, const char *answers_path)
{
    if (!plan) return -1;
    wubu_install_io io = { t_ask, t_secret, t_salt, t_confirm };
    int rc = wubu_install_wizard(plan, &io);
    if (rc != 1) return rc;                 /* 0 cancelled, -1 invalid */
    if (!answers_path) return 1;            /* confirmed; no document asked for */
    char doc[1024];
    if (wubu_install_serialize(plan, doc, sizeof doc) != 0) return -1;
    FILE *f = fopen(answers_path, "w");
    if (!f) return -1;
    int ok = fputs(doc, f) >= 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 1 : -1;
}
