/*
 * fw_agi.h  --  WuBuOS AGI OS kernel shim interface (firmware side).
 */
#ifndef FW_AGI_H
#define FW_AGI_H

#include "fw.h"

/* Publishes an EFI Configuration Table (WUBU_AGI_ATTEST_GUID) carrying a
 * live PCR0-7 + Secure Boot snapshot that the booted OS image can read.
 * Safe to call after measurements + media init. */
void fw_agi_publish_attest(void);

/* Attest + boot an OS image at `path` on the ESP. Verifies Authenticode
 * + policy, extends the image digest into PCR4 (TCG "EFI application"),
 * then hands control to the image. Returns 0 on clean boot, -1 if refused. */
int fw_agi_attest_and_boot(const char *path);

#endif /* FW_AGI_H */
