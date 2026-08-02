/*
 * wubu_attest.h -- WuBuOS kernel-side firmware attestation consumer (ring-0).
 *
 * The WuBuFW loader (src/firmware/loader) hands the booted kernel a
 * "loader handoff" block in low physical memory:
 *
 *   0x90040  [u64]  physical address of the wubu_loader_handoff_t
 *   0x91000  wubu_loader_handoff_t { magic, version, kernel_size,
 *                                    kernel_sha256[32], attest_addr }
 *   0x92000  wubu_attest_t         -- snapshot of the firmware's PCR0-7 +
 *                                     Secure Boot state (published as an EFI
 *                                     Configuration Table by fw_agi.c).
 *
 * The kernel is freestanding and must not include firmware headers, so the
 * attestation layout is duplicated here. Static asserts in wubu_attest.c pin
 * the sizes so a firmware-side layout change fails the KERNEL build instead
 * of silently misreading the table.
 *
 * Semantic: NO attestation -> the AGI supervisor refuses to promote any
 * self-improvement change. A mutation chain is only trusted while the
 * firmware's TCG measurement chain (PCR0-7 + AuthentiCode) is live.
 */
#ifndef WUBU_ATTEST_H
#define WUBU_ATTEST_H

#include <stdint.h>
#include <stdbool.h>

/* Mirrors fw_agi_attest_t (src/firmware/fw_agi_attest.h). */
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
} wubu_attest_t;

/* Loader handoff block. magic = WUBU_LOADER_HANDOFF_MAGIC. */
#define WUBU_LOADER_HANDOFF_MAGIC 0x4F46464E41485757ULL /* "WWHANFO" */

typedef struct {
    uint64_t magic;
    uint32_t version;          /* 1 */
    uint32_t kernel_size;      /* bytes of the loaded kernel image (ELF) */
    uint8_t  kernel_sha256[WUBU_AGI_PCR_SZ]; /* SHA-256 of the kernel ELF */
    uint64_t attest_addr;      /* physical addr of wubu_attest_t, or 0 */
} wubu_loader_handoff_t;

/* Fixed low-physical handoff addresses (documented in kernel.ld too). */
#define WUBU_HANDOFF_PTR_ADDR  0x90040ULL
#define WUBU_HANDOFF_ADDR      0x91000ULL
#define WUBU_ATTEST_ADDR       0x92000ULL

/* ---- API ----------------------------------------------------------- */

/* Ingest a raw attestation snapshot (copies it). Validates magic/version/
 * pcr_count. Returns 0 on success, -1 if invalid. */
int  wubu_attest_ingest(const void *raw);

/* Ingest a loader handoff block (as the WuBuFW loader lays it out in low
 * memory). Copies the kernel digest/size, then ingests the attestation it
 * points at (attest_addr == 0 => valid-without-attestation is NOT accepted:
 * return -1). Returns 0 on success. */
int  wubu_attest_ingest_handoff(const void *handoff);

/* Read the loader handoff from the fixed physical addresses. No-op (returns
 * -1) if the pointer slot is empty. Called by metal_main before the AGI
 * supervisor starts. */
int  wubu_attest_load_scratch(void);

/* Drop any previously ingested state (boot-without-firmware path). */
void wubu_attest_clear(void);

bool     wubu_attest_valid(void);
uint32_t wubu_attest_version(void);
uint32_t wubu_attest_boot_counter(void);
bool     wubu_attest_sb_enabled(void);
bool     wubu_attest_setup_mode(void);

/* Copy PCR i (0..7) into out[32]. 0 on success, -1 if invalid/out-of-range. */
int  wubu_attest_pcr(unsigned i, uint8_t out[WUBU_AGI_PCR_SZ]);
/* Convenience: PCR4 = the boot image digest (code-as-data). */
int  wubu_attest_pcr4_digest(uint8_t out[WUBU_AGI_PCR_SZ]);
/* SHA-256 of the kernel ELF as measured by the loader. */
int  wubu_attest_kernel_digest(uint8_t out[WUBU_AGI_PCR_SZ]);
uint32_t wubu_attest_kernel_size(void);

#endif /* WUBU_ATTEST_H */
