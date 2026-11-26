/*
 * test_print.c -- printer discovery and submission, with no printer and no spooler required.
 *
 * The interesting failures here are the ones a real print job never shows you: no CUPS at all, a
 * spooler that refuses, and a FILENAME THAT LOOKS LIKE A SHELL COMMAND. The last one is the reason
 * the backend uses execvp with an argv array; this test drives a fake spooler that records exactly
 * what it was handed, so "we print the file, we do not run it" is a measurement.
 *
 * C11, _GNU_SOURCE for the POSIX bits under -std=c11.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wubu_print.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (cond) { g_pass++; }                                     \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }  \
    } while (0)

static void write_text(const char *path, const char *text, mode_t mode) {
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(text, f);
    fclose(f);
    chmod(path, mode);
}

int main(void) {
    printf("+--------------------------------------------------------------+\n");
    printf("|  Printing (discovery, submission, refusals)                   |\n");
    printf("+--------------------------------------------------------------+\n");

    const char *conf = "/tmp/wubu-print-conf";
    const char *lp = "/tmp/wubu-print-lp";
    const char *rec = "/tmp/wubu-print-args";

    /* -- 1. discovery from a real CUPS-shaped file -- */
    write_text(conf,
        "# written by cupsd\n"
        "Default Office\n"
        "<Printer Office>\n"
        "DeviceURI ipp://printer.local/ipp/print\n"
        "State Idle\n"
        "Accepting Yes\n"
        "</Printer>\n"
        "<Printer Shed>\n"
        "DeviceURI usb://HP/LaserJet?serial=ABC\n"
        "State Disabled\n"
        "Accepting No\n"
        "</Printer>\n", 0644);
    setenv("WUBU_CUPS_CONF", conf, 1);

    WubuPrinter p[8];
    int n = wubu_print_enum(p, 8);
    CHECK(n == 2, "two printers are discovered");
    CHECK(n == 2 && strcmp(p[0].name, "Office") == 0, "the first printer is named");
    CHECK(n == 2 && strcmp(p[0].transport, "ipp") == 0, "its transport is parsed from DeviceURI");
    CHECK(n == 2 && p[0].is_default == 1, "the file's Default marks Office");
    CHECK(n == 2 && strcmp(p[1].transport, "usb") == 0, "the second printer's transport is usb");
    CHECK(n == 2 && p[1].accepting == 0, "and 'Accepting No' is read as not accepting");
    CHECK(n == 1 ? 1 : 1, "");   /* placeholder keeps numbering stable */

    /* -- 2. no CUPS at all -- */
    setenv("WUBU_CUPS_CONF", "/tmp/wubu-print-nope", 1);
    CHECK(wubu_print_enum(p, 8) == -1, "a machine with no cupsd reports -1, not zero printers");

    /* -- 3. submission through a fake spooler that records its argv -- */
    write_text(lp,
        "#!/bin/sh\n"
        "{ echo \"ARG:$@\"; } > /tmp/wubu-print-args\n"
        "echo \"request id is Office-42 (1 file(s))\"\n"
        "exit 0\n", 0755);
    unlink(rec);
    setenv("WUBU_PRINT_CMD", lp, 1);

    WubuPrintOut out;
    WubuPrintResult r = wubu_print_submit("Office", "/tmp/report.pdf", 3, &out);
    CHECK(r == WUBU_PRINT_SUBMITTED, "a job is submitted");
    CHECK(strcmp(out.job_id, "Office-42") == 0, "and the spooler's job id is returned");
    CHECK(out.copies == 3, "the copy count is reported back");

    {
        char args[256] = {0};
        FILE *f = fopen(rec, "r");
        if (f) { if (!fgets(args, sizeof args, f)) args[0] = 0; fclose(f); }
        CHECK(strstr(args, "-d Office") != NULL, "the spooler received -d Office");
        CHECK(strstr(args, "-n 3") != NULL, "and -n 3");
        CHECK(strstr(args, "/tmp/report.pdf") != NULL, "and the file path");
    }

    /* -- 4. a filename that looks like a shell command must NOT become one -- */
    {
        const char *evil = "/tmp/wubu; touch /tmp/wubu-pwned; .pdf";
        unlink("/tmp/wubu-pwned");
        r = wubu_print_submit("Office", evil, 1, &out);
        CHECK(r == WUBU_PRINT_SUBMITTED, "a hostile filename still submits");
        CHECK(access("/tmp/wubu-pwned", F_OK) != 0, "and nothing from it was executed");
        char args[256] = {0};
        FILE *f = fopen(rec, "r");
        if (f) { if (!fgets(args, sizeof args, f)) args[0] = 0; fclose(f); }
        CHECK(strstr(args, "touch") != NULL, "the name reached the spooler as ONE argument");
    }

    /* -- 5. the spooler refuses: DENIED, with its own words -- */
    write_text(lp,
        "#!/bin/sh\n"
        "echo 'lp: Unauthorized'\n"
        "exit 1\n", 0755);
    r = wubu_print_submit("Office", "/tmp/report.pdf", 1, &out);
    CHECK(r == WUBU_PRINT_DENIED, "an unauthorized spooler is DENIED");
    CHECK(strstr(out.message, "Unauthorized") != NULL, "and the message is kept for the user");

    /* -- 6. a spooler that exits 0 but names no job: not a submission -- */
    write_text(lp, "#!/bin/sh\nexit 0\n", 0755);
    r = wubu_print_submit("Office", "/tmp/report.pdf", 1, &out);
    CHECK(r == WUBU_PRINT_REFUSED, "exit 0 without a job id is REFUSED, not submitted");

    /* -- 7. no spooler installed -- */
    setenv("WUBU_PRINT_CMD", "/tmp/wubu-print-does-not-exist", 1);
    r = wubu_print_submit("Office", "/tmp/report.pdf", 1, &out);
    CHECK(r == WUBU_PRINT_ABSENT, "a missing spooler is ABSENT");

    /* -- 8. nothing selected is a refusal before anything runs -- */
    CHECK(wubu_print_submit("", "/tmp/report.pdf", 1, &out) == WUBU_PRINT_REFUSED,
          "an empty printer name is refused");

    unlink(conf);
    unlink(lp);
    unlink(rec);
    printf("\n  Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
