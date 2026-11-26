/*
 * wubu_biometric_test.c -- a fake machine with/without each of the three facts, because the point of
 * this module is that it never guesses: agent, reader and enrolment are each measured.
 *
 * C11.
 */
#include "wubu_biometric.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

static void mkdirs(const char *p) { (void)mkdir(p, 0755); }

static void mkexe(const char *p)
{
    FILE *f = fopen(p, "w");
    if (!f) return;
    fputs("#!/bin/sh\nexit 0\n", f);
    fclose(f);
    (void)chmod(p, 0755);
}

static void mkfile(const char *p, const char *content)
{
    FILE *f = fopen(p, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

int main(void)
{
    char root[] = "/tmp/biotestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }

    char home[300], sysbin[400], dev[400], datadir[500], enroll[600], devnode[500], hist[600];
    snprintf(home, sizeof home, "%s/home", root);
    snprintf(sysbin, sizeof sysbin, "%s/usr/bin", root);
    snprintf(dev, sizeof dev, "%s/dev", root);
    snprintf(datadir, sizeof datadir, "%s/.local/share/fprint", home);
    snprintf(enroll, sizeof enroll, "%s/fprintd-enroll", sysbin);
    snprintf(devnode, sizeof devnode, "%s/fprint0", dev);
    snprintf(hist, sizeof hist, "%s/right-index", datadir);

    /* mkdir(2) does not create parents: the fake machine needs each level, or every fixture below
     * silently fails to exist and the gate measures an empty tree while claiming to test a full one. */
    char p1[400], p2[400], p3[400];
    snprintf(p1, sizeof p1, "%s/usr", root);              mkdirs(p1);
    snprintf(p2, sizeof p2, "%s/.local", home);           mkdirs(home); mkdirs(p2);
    snprintf(p3, sizeof p3, "%s/share", p2);              mkdirs(p3);
    mkdirs(sysbin);
    mkdirs(dev);
    mkdirs(datadir);

    setenv("WUBU_FPRINT_ROOT", root, 1);
    setenv("HOME", home, 1);
    setenv("USER", "probe", 1);
    unsetenv("WUBU_FPRINT_DEVICE");

    printf("=== Test: an empty machine is three absences, and says which ===\n");
    WubuBiometric b;
    CHECK(wubu_biometric_probe(&b) == 0, "nothing present counts zero facts");
    CHECK(b.probed, "the struct is marked probed (a zeroed struct is not a verdict)");
    CHECK(!b.agent_enroll && !b.reader_present && b.enrolled == 0, "all three are absent");
    char status[200];
    wubu_biometric_status(&b, status, sizeof status);
    CHECK(strstr(status, "No fingerprint reader") != NULL, "the status names the missing reader first");

    printf("\n=== Test: an agent on disk is found, a non-executable file is not ===\n");
    mkfile(enroll, "not executable");
    (void)chmod(enroll, 0644);
    CHECK(wubu_biometric_probe(&b) == 0, "a non-executable fprintd-enroll is not an agent");
    CHECK(!b.agent_enroll, "and the module says so");

    mkexe(enroll);
    CHECK(wubu_biometric_probe(&b) == 1, "an executable agent is one present fact");
    CHECK(b.agent_enroll, "the agent is reported");
    CHECK(strstr(b.enroll_path, "fprintd-enroll") != NULL, "the agent's path is reported");
    wubu_biometric_status(&b, status, sizeof status);
    CHECK(strstr(status, "No fingerprint reader") != NULL, "still no reader");
    CHECK(strstr(status, "enrolment service is installed") != NULL,
          "the status says the service is there, so the missing half is clear");

    printf("\n=== Test: a reader under /dev is a reader ===\n");
    mkfile(devnode, "");
    CHECK(wubu_biometric_probe(&b) >= 2, "agent + reader are two facts");
    CHECK(b.reader_present, "the reader is reported present");
    CHECK(strstr(b.reader_path, "fprint") != NULL, "the reader's path is reported");
    wubu_biometric_status(&b, status, sizeof status);
    CHECK(strstr(status, "no fingerprints enrolled") != NULL,
          "with a reader and no prints the status asks for one");

    printf("\n=== Test: enrolled prints are counted, not guessed ===\n");
    mkfile(hist, "data");
    CHECK(wubu_biometric_probe(&b) == 3, "all three facts are present");
    CHECK(b.enrolled == 1, "one enrolled fingerprint");
    wubu_biometric_status(&b, status, sizeof status);
    CHECK(strstr(status, "1 fingerprint enrolled") != NULL, "the status counts it");
    char second[600];
    snprintf(second, sizeof second, "%s/left-thumb", datadir);
    mkfile(second, "data");
    CHECK(wubu_biometric_probe(&b) == 3 && b.enrolled == 2, "two fingerprints are two");
    wubu_biometric_status(&b, status, sizeof status);
    CHECK(strstr(status, "2 fingerprints enrolled") != NULL, "and the plural is right");

    printf("\n=== Test: an explicit device is authoritative ===\n");
    setenv("WUBU_FPRINT_DEVICE", "/nonexistent/goodix", 1);
    CHECK(wubu_biometric_probe(&b) >= 1, "the probe still runs");
    CHECK(!b.reader_present, "an override naming nothing means NO reader, even though /dev/fprint0 exists");
    setenv("WUBU_FPRINT_DEVICE", devnode, 1);
    CHECK(wubu_biometric_probe(&b) == 3 && b.reader_present, "an override naming the node is the reader");
    unsetenv("WUBU_FPRINT_DEVICE");

    printf("\n=== Test: a reader without the service is a different sentence ===\n");
    (void)chmod(enroll, 0644);          /* no longer executable */
    CHECK(wubu_biometric_probe(&b) >= 1, "the probe runs");
    CHECK(!b.agent_enroll && b.reader_present, "reader without an agent");
    wubu_biometric_status(&b, status, sizeof status);
    CHECK(strstr(status, "A reader is present") != NULL, "the status names that case specifically");
    CHECK(strstr(status, "no enrolment service") != NULL, "and what is missing");

    printf("\n=== Test: not probed is not 'unavailable' ===\n");
    WubuBiometric zero;
    memset(&zero, 0, sizeof zero);
    wubu_biometric_status(&zero, status, sizeof status);
    CHECK(strcmp(status, "not probed") == 0, "an unfilled struct says it was not probed");

    char rm[500];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  biometric: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
