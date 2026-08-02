/*
 * fw_tpm.h  --  WuBuFW TPM 2.0 driver + measured boot interface.
 */

#ifndef WUBUFW_TPM_H
#define WUBUFW_TPM_H

#include <stdint.h>

#define TPM_ALG_SHA1    0x0004
#define TPM_ALG_SHA256  0x000B

#define TPM_MAX_PCR     24

/* TCG PCR usage (PC Client Platform Firmware Profile) */
#define PCR_FIRMWARE_CODE     0   /* firmware executable code            */
#define PCR_FIRMWARE_CONFIG   1   /* firmware configuration              */
#define PCR_OPTION_ROM_CODE   2
#define PCR_OPTION_ROM_CONFIG 3
#define PCR_BOOT_LOADER       4   /* boot loader / boot attempt          */
#define PCR_BOOT_CONFIG       5   /* GPT / boot config                   */
#define PCR_PLATFORM          6
#define PCR_SECURE_BOOT       7   /* Secure Boot policy                  */

/* TCG event types */
#define EV_POST_CODE             0x00000001
#define EV_NO_ACTION             0x00000003
#define EV_SEPARATOR             0x00000004
#define EV_ACTION                0x00000005
#define EV_EVENT_TAG             0x00000006
#define EV_S_CRTM_CONTENTS       0x00000007
#define EV_S_CRTM_VERSION        0x00000008
#define EV_PLATFORM_CONFIG_FLAGS 0x0000000A
#define EV_TABLE_OF_DEVICES      0x0000000B
#define EV_EFI_VARIABLE_DRIVER_CONFIG 0x80000001
#define EV_EFI_VARIABLE_BOOT     0x80000002
#define EV_EFI_BOOT_SERVICES_APPLICATION 0x80000003
#define EV_EFI_GPT_EVENT         0x80000006
#define EV_EFI_PLATFORM_FIRMWARE_BLOB 0x80000008

typedef enum {
    TPM_IFACE_NONE = 0,
    TPM_IFACE_TIS,      /* FIFO / TIS at 0xFED40000  */
    TPM_IFACE_CRB       /* Command Response Buffer   */
} fw_tpm_iface;

int          fw_tpm_init(void);
int          fw_tpm_present(void);
fw_tpm_iface fw_tpm_interface(void);

/* Raw command submission (big-endian TPM wire format in/out). */
int fw_tpm_transmit(const uint8_t *cmd, uint32_t cmd_len,
                    uint8_t *resp, uint32_t *resp_len);

/* Measured boot */
int fw_tpm_startup(void);
int          fw_tpm_selftest(void);
int          fw_tpm_selftest_self(void);
int fw_tpm_pcr_extend(uint32_t pcr, const uint8_t digest[32]);
int fw_tpm_pcr_read(uint32_t pcr, uint8_t out[32]);
int fw_tpm_get_random(uint8_t *out, uint32_t n);

/* Hash a blob and extend it into a PCR, appending a TCG event-log entry. */
int fw_tpm_measure(uint32_t pcr, uint32_t event_type,
                   const void *data, uint64_t len,
                   const char *description);

/* Event log access (TCG2 format, for the OS / anti-cheat attestation). */
const void *fw_tpm_log_buffer(void);
uint64_t    fw_tpm_log_size(void);
uint32_t    fw_tpm_log_count(void);
void        fw_tpm_log_dump(void);

/* SHA-256 (also used by secure boot verification). */
void fw_sha256(const void *data, uint64_t len, uint8_t out[32]);

#endif
