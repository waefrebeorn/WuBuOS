/*
 * wubu_firmware.h -- what firmware this machine runs, and whether anything here can update it.
 *
 * Parity target: fwupd (CachyOS ships it; Windows shows the same facts in System Information /
 * Windows Update > Optional updates). Two questions, honestly answered:
 *
 *   1. WHAT FIRMWARE IS THIS?  SMBIOS/DMI exposes the BIOS vendor, version and date, plus the system
 *      and board identity. That is the same data `fwupdmgr get-devices` prints for the system device.
 *   2. CAN WE UPDATE IT?      Only if a firmware update agent exists on the machine. WuBuOS does not
 *      pretend: the panel says "no update agent" instead of offering an Update button that cannot work.
 *
 * Read-only, no allocator, bounded reads. The DMI root is overridable so the gate can run on a fake
 * tree instead of depending on the host's hardware.
 *
 * C11.
 */
#ifndef WUBU_FIRMWARE_H
#define WUBU_FIRMWARE_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_FW_STR_SHORT 32
#define WUBU_FW_STR_LONG  96
#define WUBU_FW_PATH_MAX  256

typedef struct {
    char bios_vendor[WUBU_FW_STR_LONG];
    char bios_version[WUBU_FW_STR_LONG];
    char bios_date[WUBU_FW_STR_SHORT];
    char sys_vendor[WUBU_FW_STR_LONG];
    char product_name[WUBU_FW_STR_LONG];
    char board_name[WUBU_FW_STR_LONG];
    bool have_dmi;                            /* at least one attribute was readable */
    bool agent_present;                       /* a firmware update agent exists here */
    char agent_path[WUBU_FW_PATH_MAX];        /* its path, or "" when absent */
} WubuFirmware;

/* The DMI root in use: $WUBU_DMI_ROOT, else /sys/class/dmi/id. */
const char *wubu_firmware_dmi_root(void);

/* Read one attribute file ("bios_version") under the DMI root into buf.
 * Returns true when a non-empty value was read. */
bool wubu_firmware_attr(const char *name, char *buf, size_t cap);

/* Fill *out. Returns 0 when at least one DMI attribute was read (have_dmi), -1 otherwise.
 * Never leaves the struct with uninitialised strings. */
int wubu_firmware_read(WubuFirmware *out);

/* Find a firmware update agent: $WUBU_FWUPD_AGENT, then fwupdmgr / fwupdtool on the usual paths,
 * then PATH. Returns true and fills path when one exists and is executable. */
bool wubu_firmware_find_agent(char *path, size_t cap);

/* "" when nothing is known, else "vendor version (date)". Static buffer. */
const char *wubu_firmware_summary(const WubuFirmware *fw);

#endif
