/*
 * fw_agi_attest.h -- WuBuFW <-> WuBuOS attestation wire format (SHARED).
 *
 * Single source of truth for the attestation snapshot layout + GUID that
 * the firmware publishes (fw_agi.c), the loader chainloader copies into
 * low memory (src/firmware/loader), and the kernel consumes
 * (src/kernel/wubu_attest.h -- which duplicates this layout + static-asserts
 * the size so a drift breaks the kernel build).
 *
 * C11, freestanding-safe (only stdint).
 */
#ifndef FW_AGI_ATTEST_H
#define FW_AGI_ATTEST_H

#include <stdint.h>

/* Mirrored by wubu_attest_t in src/kernel/wubu_attest.h. */
#define WUBU_AGI_MAGIC          0x4147494F75427557ULL /* "WuBuOIA" le-ish */
#define WUBU_AGI_ATTEST_VERSION 1u
#define WUBU_AGI_PCR_COUNT      8u
#define WUBU_AGI_PCR_SZ         32u

typedef struct {
    uint64_t magic;            /* WUBU_AGI_MAGIC */
    uint32_t version;          /* 1 */
    uint32_t sb_enabled;       /* Secure Boot enforcement active */
    uint32_t sb_setup_mode;    /* 1 = setup (no policy) */
    uint32_t sb_db_count;
    uint32_t pcr_count;        /* always 8 */
    uint32_t boot_counter;     /* increments each verified boot */
    uint8_t  pcr[WUBU_AGI_PCR_COUNT][WUBU_AGI_PCR_SZ]; /* PCR0..PCR7 */
} fw_agi_attest_t;             /* sizeof == 288 */

/* EFI Configuration Table GUID under which fw_agi.c publishes the live
 * snapshot (WUBU_AGI_ATTEST_GUID). */
#define WUBU_AGI_ATTEST_GUID \
    { 0x2a1f7c4d, 0x9b3e, 0x4c1a, { 0xbd, 0x8f, 0x2e, 0x71, 0x09, 0x5c, 0x33, 0xa7 } }

/* Loader handoff block (mirrors wubu_loader_handoff_t in the kernel). */
#define WUBU_LOADER_HANDOFF_MAGIC 0x4F46464E41485757ULL /* "WWHANFO" */

typedef struct {
    uint64_t magic;            /* WUBU_LOADER_HANDOFF_MAGIC */
    uint32_t version;          /* 1 */
    uint32_t kernel_size;      /* bytes of the loaded kernel image (ELF) */
    uint8_t  kernel_sha256[WUBU_AGI_PCR_SZ]; /* SHA-256 of the kernel ELF */
    uint64_t attest_addr;      /* physical addr of fw_agi_attest_t, or 0 */
} wubu_loader_handoff_t;       /* sizeof == 56 */

/* Fixed low-physical handoff addresses (identity-mapped for the kernel). */
#define WUBU_HANDOFF_PTR_ADDR  0x90040ULL
#define WUBU_HANDOFF_ADDR      0x91000ULL
#define WUBU_ATTEST_ADDR       0x92000ULL

#endif /* FW_AGI_ATTEST_H */
