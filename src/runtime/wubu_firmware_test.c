/*
 * wubu_firmware_test.c -- the firmware surface must report what the machine says, and must not invent
 * a firmware version, an update agent, or a summary when it has read nothing.
 *
 * Runs against a FAKE DMI tree (WUBU_DMI_ROOT), because a gate that depends on the host's BIOS is not
 * portable, and then against the real tree as a SKIP-or-pass sanity check.
 *
 * C11.
 */
#include "wubu_firmware.h"

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

static void write_file(const char *path, const char *content, mode_t mode)
{
    FILE *f = fopen(path, "w");
    if (!f) { printf("  cannot write %s\n", path); return; }
    fputs(content, f);
    fclose(f);
    chmod(path, mode);
}

int main(void)
{
    char root[] = "/tmp/fwtestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }
    setenv("WUBU_DMI_ROOT", root, 1);

    printf("=== Test: firmware identity (fake DMI tree) ===\n");

    /* Nothing there yet: an empty tree must read as UNKNOWN, not as an empty-valued BIOS. */
    WubuFirmware fw;
    int rc = wubu_firmware_read(&fw);
    CHECK(rc == -1, "an empty DMI tree reads as unknown (-1)");
    CHECK(!fw.have_dmi, "have_dmi is false when nothing was read");
    CHECK(fw.bios_vendor[0] == '\0' && fw.bios_version[0] == '\0', "no invented strings");
    CHECK(strcmp(wubu_firmware_summary(&fw), "") == 0, "no summary when nothing is known");

    /* The real attributes, with the trailing newline sysfs always adds. */
    char p[512];
    snprintf(p, sizeof p, "%s/bios_vendor", root);   write_file(p, "TestBIOS\n", 0644);
    snprintf(p, sizeof p, "%s/bios_version", root);  write_file(p, "1.2.3\n", 0644);
    snprintf(p, sizeof p, "%s/bios_date", root);     write_file(p, "01/02/2026\n", 0644);
    snprintf(p, sizeof p, "%s/sys_vendor", root);    write_file(p, "ACME Corp\n", 0644);
    snprintf(p, sizeof p, "%s/product_name", root);  write_file(p, "Box 9000  \n", 0644);
    snprintf(p, sizeof p, "%s/board_name", root);    write_file(p, "BoardX\n", 0644);

    rc = wubu_firmware_read(&fw);
    CHECK(rc == 0, "a populated DMI tree reads successfully");
    CHECK(fw.have_dmi, "have_dmi is true when attributes were read");
    CHECK(strcmp(fw.bios_vendor, "TestBIOS") == 0, "bios_vendor is read and trimmed");
    CHECK(strcmp(fw.bios_version, "1.2.3") == 0, "bios_version is read");
    CHECK(strcmp(fw.bios_date, "01/02/2026") == 0, "bios_date is read");
    CHECK(strcmp(fw.sys_vendor, "ACME Corp") == 0, "sys_vendor is read");
    CHECK(strcmp(fw.product_name, "Box 9000") == 0, "trailing spaces are trimmed");
    CHECK(strcmp(fw.board_name, "BoardX") == 0, "board_name is read");

    /* The summary a user reads: "vendor version (date)". */
    CHECK(strcmp(wubu_firmware_summary(&fw), "TestBIOS 1.2.3 (01/02/2026)") == 0,
          "the summary names vendor, version and date");

    /* Only a version: the summary must not print an empty vendor with a leading space. */
    WubuFirmware only_ver;
    memset(&only_ver, 0, sizeof only_ver);
    snprintf(only_ver.bios_version, sizeof only_ver.bios_version, "9.9");
    CHECK(strcmp(wubu_firmware_summary(&only_ver), "9.9") == 0, "no leading space without a vendor");

    /* A value longer than its field truncates, stays NUL-terminated, and does not overrun. */
    snprintf(p, sizeof p, "%s/bios_version", root);
    char big[400];
    memset(big, 'V', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    write_file(p, big, 0644);
    wubu_firmware_read(&fw);
    CHECK(strlen(fw.bios_version) == sizeof fw.bios_version - 1, "an overlong value is truncated");
    CHECK(fw.bios_version[sizeof fw.bios_version - 1] == '\0', "the truncated value stays terminated");

    printf("\n=== Test: the update agent is either found or reported absent ===\n");
    char agent[512];
    unsetenv("WUBU_FWUPD_AGENT");
    /* Point the search at a directory of our own: nothing executable there yet. */
    char adir[512];
    snprintf(adir, sizeof adir, "%s/agent.bin", root);
    write_file(adir, "#!/bin/sh\n", 0644);          /* present but NOT executable */
    setenv("WUBU_FWUPD_AGENT", adir, 1);
    CHECK(!wubu_firmware_find_agent(agent, sizeof agent),
          "a file that is not executable is not an update agent");
    CHECK(agent[0] == '\0', "no path is reported when nothing was found");

    chmod(adir, 0755);
    CHECK(wubu_firmware_find_agent(agent, sizeof agent), "an executable agent is found");
    CHECK(strcmp(agent, adir) == 0, "the found agent's path is reported");

    wubu_firmware_read(&fw);
    CHECK(fw.agent_present, "the struct carries the agent when one exists");
    CHECK(strcmp(fw.agent_path, adir) == 0, "the struct carries the agent path");

    unsetenv("WUBU_FWUPD_AGENT");
    CHECK(strcmp(wubu_firmware_dmi_root(), root) == 0, "the root override is honoured");

    printf("\n=== Test: the real machine (skip when it exposes nothing) ===\n");
    unsetenv("WUBU_DMI_ROOT");
    WubuFirmware real;
    int real_rc = wubu_firmware_read(&real);
    if (real_rc != 0) {
        printf("  SKIPPED: this machine exposes no DMI attributes (no /sys/class/dmi/id)\n");
    } else {
        CHECK(real.have_dmi, "the real tree reads as known");
        CHECK(real.bios_version[0] || real.bios_vendor[0], "the real machine names its firmware");
        printf("  measured: %s\n", wubu_firmware_summary(&real));
    }

    /* Cleanup */
    snprintf(p, sizeof p, "%s/bios_vendor", root);  unlink(p);
    snprintf(p, sizeof p, "%s/bios_version", root); unlink(p);
    snprintf(p, sizeof p, "%s/bios_date", root);    unlink(p);
    snprintf(p, sizeof p, "%s/sys_vendor", root);   unlink(p);
    snprintf(p, sizeof p, "%s/product_name", root); unlink(p);
    snprintf(p, sizeof p, "%s/board_name", root);   unlink(p);
    unlink(adir);
    rmdir(root);

    printf("\n  firmware: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
