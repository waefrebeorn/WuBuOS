/*
 * fw_secureboot.h  --  WuBuFW authenticated boot policy.
 */

#ifndef WUBUFW_SECUREBOOT_H
#define WUBUFW_SECUREBOOT_H

#include <stdint.h>

int  fw_sb_enroll_db(const uint8_t *der_cert, uint32_t len);
int  fw_sb_enroll_dbx(const uint8_t *der_cert, uint32_t len);
void fw_sb_set_pk(void);
int  fw_sb_secureboot_enabled(void);
int  fw_sb_selftest(void);
int  fw_sb_selftest_esl(void);
int  fw_sb_selftest_pe(void);
int  fw_sb_setup_mode(void);

/* omarchy/sbctl-style enrollment: enroll an EFI_SIGNATURE_LIST into PK/KEK/
 * db/dbx (real UEFI variables, setup->user transition on PK). */
int  fw_sb_enroll_esl(const char *which, const uint8_t *esl, uint32_t len);
int  fw_sb_enroll_file(const char *which, const char *path);
int  fw_sb_create_platform_key(void);
int  fw_sb_var_present(const char *name);
void fw_sb_status(void);

/* Returns 0 if the image may execute (and fills `hash` with the
 * Authenticode PE hash for PCR7 measurement), -1 to refuse. */
int  fw_sb_verify(const uint8_t *image, uint32_t size, uint8_t *hash);

#endif
