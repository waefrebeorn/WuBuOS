/*
 * wubu_firmware.c -- DMI/SMBIOS attributes and firmware-agent discovery. See wubu_firmware.h.
 *
 * Everything here is a read of a small text file under /sys/class/dmi/id (or $WUBU_DMI_ROOT) plus a
 * stat of a few paths. No allocator, no shell, no popen: the same discipline as the print backend,
 * because a system-identity query must not be able to run anything.
 *
 * C11.
 */
#include "wubu_firmware.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *wubu_firmware_dmi_root(void)
{
    const char *env = getenv("WUBU_DMI_ROOT");
    if (env && env[0]) return env;
    return "/sys/class/dmi/id";
}

/* A bounded read of one small file, trimmed of trailing whitespace. sysfs attributes are one line,
 * sometimes with padding; a NUL always ends the string. */
bool wubu_firmware_attr(const char *name, char *buf, size_t cap)
{
    if (!name || !name[0] || !buf || cap < 2) return false;
    buf[0] = '\0';

    char path[WUBU_FW_PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", wubu_firmware_dmi_root(), name);
    if (n <= 0 || (size_t)n >= sizeof(path)) return false;

    FILE *f = fopen(path, "r");
    if (!f) return false;

    size_t got = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[got] = '\0';

    while (got > 0) {                     /* trim CR/LF/space */
        char c = buf[got - 1];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t') break;
        buf[--got] = '\0';
    }
    return got > 0;
}

/* An executable regular file, and not a directory. */
static bool is_exe(const char *path)
{
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

bool wubu_firmware_find_agent(char *path, size_t cap)
{
    if (path && cap) path[0] = '\0';

    /* An explicit override is AUTHORITATIVE, exactly like WUBU_DMI_ROOT: if it names something that
     * is not executable, the answer is "no agent", not "look somewhere else". A test that pins the
     * agent must not be able to pass by finding the host's real fwupd. */
    const char *env = getenv("WUBU_FWUPD_AGENT");
    if (env && env[0]) {
        if (!is_exe(env)) return false;
        if (path && cap) snprintf(path, cap, "%s", env);
        return true;
    }

    static const char *cands[] = {
        "/usr/bin/fwupdmgr", "/usr/bin/fwupdtool",
        "/usr/local/bin/fwupdmgr", "/usr/local/bin/fwupdtool",
    };
    for (unsigned i = 0; i < sizeof(cands) / sizeof(cands[0]); i++) {
        if (is_exe(cands[i])) {
            if (path && cap) snprintf(path, cap, "%s", cands[i]);
            return true;
        }
    }
    return false;
}

int wubu_firmware_read(WubuFirmware *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    bool any = false;
    any |= wubu_firmware_attr("bios_vendor",    out->bios_vendor,    sizeof(out->bios_vendor));
    any |= wubu_firmware_attr("bios_version",   out->bios_version,   sizeof(out->bios_version));
    any |= wubu_firmware_attr("bios_date",      out->bios_date,      sizeof(out->bios_date));
    any |= wubu_firmware_attr("sys_vendor",     out->sys_vendor,     sizeof(out->sys_vendor));
    any |= wubu_firmware_attr("product_name",   out->product_name,   sizeof(out->product_name));
    any |= wubu_firmware_attr("board_name",     out->board_name,     sizeof(out->board_name));
    out->have_dmi = any;

    out->agent_present = wubu_firmware_find_agent(out->agent_path, sizeof(out->agent_path));
    return any ? 0 : -1;
}

const char *wubu_firmware_summary(const WubuFirmware *fw)
{
    static char buf[2 * WUBU_FW_STR_LONG + WUBU_FW_STR_SHORT + 8];
    buf[0] = '\0';
    if (!fw) return buf;
    if (!fw->bios_version[0] && !fw->bios_vendor[0]) return buf;

    if (fw->bios_vendor[0] && fw->bios_version[0])
        snprintf(buf, sizeof(buf), "%s %s", fw->bios_vendor, fw->bios_version);
    else
        snprintf(buf, sizeof(buf), "%s", fw->bios_vendor[0] ? fw->bios_vendor : fw->bios_version);

    if (fw->bios_date[0]) {
        size_t n = strlen(buf);
        snprintf(buf + n, sizeof(buf) - n, " (%s)", fw->bios_date);
    }
    return buf;
}
