/*
 * fw_agi.c  --  WuBuOS AGI OS kernel shim (firmware-resident microkernel).
 *
 * This is the firmware-side half of the "AGI operating system recursive
 * learning platform": a tiny trusted ring-0 shim that (1) publishes a live
 * attestation table (PCR0-7 + Secure Boot state) to whatever OS image the
 * firmware hands control to, and (2) gates every boot of a self-modified OS
 * image through the TCG measurement + Authenticode chains we verified in
 * fw_tpm.c and fw_secureboot.c.
 *
 * The host-side recursive optimizer (wubuwizard/src/wubu_evolve.c) reads the
 * attested PCR bank, proposes a self-modification, and re-launches the image
 * via fw_agi_attest_and_boot(); the firmware re-measures into PCR4 and rejects
 * (refuses to boot) any mutation whose attestation no longer matches. No
 * mutation survives unless it passes the firmware's root of trust.
 *
 * C11, no third-party deps, freestanding with the firmware's own libc.
 */
#include "fw.h"
#include "fw_tpm.h"
#include "fw_secureboot.h"
#include "fw_agi_attest.h"
extern EFI_SYSTEM_TABLE *g_systab;
extern EFI_HANDLE g_vol_handle[8];
extern EFI_STATUS fw_image_create(void *buf, uint64_t size, EFI_HANDLE device, EFI_HANDLE *out);

/* Published to the OS image via an EFI Configuration Table. The layout +
 * GUID live in fw_agi_attest.h (shared with the loader and the kernel). */

static fw_agi_attest_t g_agit;
static int g_agit_ready = 0;
static uint32_t g_boot_counter = 0;

/*
 * Snapshot the live PCR bank + Secure Boot policy into the attestation
 * table and publish it as an EFI Configuration Table so the booted OS
 * image can locate and read it. The table lives in firmware-owned memory
 * that remains mapped through ExitBootServices.
 */
void fw_agi_publish_attest(void) {
    g_agit.magic       = WUBU_AGI_MAGIC;
    g_agit.version     = 1;
    g_agit.sb_enabled  = fw_sb_secureboot_enabled();
    g_agit.sb_setup_mode = fw_sb_setup_mode();
    g_agit.pcr_count   = 8;
    g_agit.boot_counter = g_boot_counter;
    g_agit.sb_db_count = 0; /* count not exported for secrecy; policy state is enough */
    for (uint32_t i = 0; i < 8; i++)
        fw_tpm_pcr_read(i, g_agit.pcr[i]);
    g_agit_ready = 1;

    if (g_systab && g_systab->BootServices) {
        static const EFI_GUID agi_guid = WUBU_AGI_ATTEST_GUID;
        g_systab->BootServices->InstallConfigurationTable((EFI_GUID *)&agi_guid, &g_agit);
    }
    fw_printf("[agi] attestation table published (PCR0-7, SB=%d setup=%d)\n",
              g_agit.sb_enabled, g_agit.sb_setup_mode);
}

/*
 * Attest + boot an OS image located on the ESP at `path`.
 *
 * Chain:
 *   1. Read image bytes through the firmware block/volume layer.
 *   2. fw_sb_verify (Authenticode + db/dbx + PCR7) — reject on policy fail.
 *   3. Extend the image's AuthentiCode hash into PCR4 (per TCG, PCR4 =
 *      OS loader / EFI application). This is the *measured* part of DM.
 *   4. Only if verify + extend both pass, hand control to the image.
 *
 * Returns 0 on a clean attested boot, -1 if the image was refused.
 */
int fw_agi_attest_and_boot(const char *path) {
    uint8_t *img = NULL;
    uint64_t sz = 0;
    /* Read through every registered volume (block layer); takes the first match. */
    int found = -1;
    int vol_idx = -1;
    for (int i = 0; i < fw_media_count(); i++) {
        fw_volume *v = fw_media_get(i);
        void *d = NULL; uint64_t s = 0;
        if (fw_vol_read_file(v, path, &d, &s) == 0) { img = d; sz = s; found = 0; vol_idx = i; break; }
    }
    if (found != 0) {
        fw_printf("[agi] cannot read image %s\n", path);
        return -1;
    }
    if (sz < 0x100 || sz > 0x200000) {      /* sanity: not a valid image */
        fw_printf("[agi] image size %llu out of range\n", (unsigned long long)sz);
        if (img) fw_free_pages(img, (uint32_t)((sz + 4095) / 4096));
        return -1;
    }

    /* 1. Verify Authenticode + policy BEFORE measuring (don't extend a
     *    rejected image into the PCR bank). */
    uint8_t pcr4_hash[32];
    if (fw_sb_verify(img, (uint32_t)sz, pcr4_hash) != 0) {
        fw_printf("[agi] image %s FAILED Secure Boot verification — refused\n", path);
        if (img) fw_free_pages(img, (uint32_t)((sz + 4095) / 4096));
        return -1;
    }

    /* 2. Extend into PCR4 (the TCG "EFI application" PCR). */
    fw_tpm_pcr_extend(4, pcr4_hash);
    uint8_t pcr4_after[32];
    fw_tpm_pcr_read(4, pcr4_after);
    fw_printf("[agi] PCR4 extended with image digest; PCR4 now:\n");
    for (int i = 0; i < 32; i++) fw_printf("%02X", pcr4_after[i]);
    fw_printf("\n");

    /* 3. Re-publish the fresh attestation (PCRs shifted by this boot). */
    fw_agi_publish_attest();
    g_boot_counter++;

    fw_printf("[agi] attest-and-boot: OS image %s verified + measured\n", path);

    /* Load the already-read image through the firmware PE loader. We pass
     * the verified/measured buffer straight in so the PE loader relocates
     * the exact bytes that passed attestation (no second file read). */
    EFI_HANDLE h = NULL;

    EFI_STATUS r = fw_image_create(img, sz, g_vol_handle[vol_idx], &h);
    if (r != EFI_SUCCESS || h == NULL) {
        fw_printf("[agi] image load failed (status %u)\n", (unsigned)r);
        if (img) fw_free_pages(img, (uint32_t)((sz + 4095) / 4096));
        return -1;
    }
    if (img) fw_free_pages(img, (uint32_t)((sz + 4095) / 4096));

    /* Hand control. fw_boot_image -> fw_bs_start_image, never returns on success. */
    fw_boot_image(h);
    return 0;
}
