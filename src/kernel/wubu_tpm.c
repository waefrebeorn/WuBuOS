/*
 * wubu_tpm.c -- kernel-owned TPM 2.0 full-stack driver routing.
 *
 * The TPM (trusted platform module) is the hardware root of trust:
 * attestation, PCR measurement, key sealing, measured boot. "Runs on
 * everything" includes the full TPM 2.0 stack.
 *
 * TPM stack:
 *   - Kernel: tpm_tis (FIFO/LPC), tpm_crb (CRB/ACPI), tpm2-space,
 *     /dev/tpm0, /dev/tpmrm0 (TPM 2.0 Resource Manager)
 *   - Userspace: tpm2-tss (libtss2), tpm2-tools (tpm2_*), tpm2-abrmd
 *   - Attestation: PCR quote, AK (attestation key) signing
 *   - Measured boot: PCR[0-7] measurement chain
 *   - Sealing: key sealed to PCR policy (tpm2_seal)
 *
 * WuBuOS owns this: detect the TPM (tpm0/CRB), route to the right driver,
 * and expose the TPM topology + capability (attestation/sealing).
 *
 * Research (Kevin-Bacon 7-hop on the TPM frontier):
 *   - tpm_tis / tpm_crb: kernel TPM 2.0 drivers, /dev/tpmrm0
 *   - tpm2-tss: the userspace TPM software stack (libtss2, tpm2-abrmd)
 *   - tpm2-tools: tpm2_quote, tpm2_seal, tpm2_getcap, tpm2_pcrread
 *   - measured boot: PCR measurement chain, secure boot
 */
#include "wubu_tpm.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_tpm = 0;          /* TPM present */
static int  g_tpm2 = 0;         /* TPM 2.0 */
static int  g_tss = 0;          /* tpm2-tss userspace */
static int  g_crb = 0;          /* CRB interface */
static int  g_measured_boot = 0;
static char g_tpm_drv[24] = "";

/* ---- W1: probe the TPM topology ---- */
void wubu_tpm_probe(void)
{
    g_tpm = 0; g_tpm2 = 0; g_tss = 0; g_crb = 0; g_measured_boot = 0;
    g_tpm_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* TPM device present? */
    if (access("/dev/tpm0", R_OK) == 0 || access("/sys/class/tpm", R_OK) == 0) {
        g_tpm = 1;
        /* TPM 2.0 has tpmrm (resource manager) or tpm2 device */
        if (access("/dev/tpmrm0", R_OK) == 0) {
            g_tpm2 = 1;
            strcpy(g_tpm_drv, "tpm2");
        } else {
            strcpy(g_tpm_drv, "tpm");
        }
        /* CRB interface (ACPI) vs FIFO (LPC) */
        if (access("/sys/bus/platform/drivers/tpm_crb", R_OK) == 0) {
            g_crb = 1;
            if (g_tpm2) strcpy(g_tpm_drv, "tpm_crb");
        } else if (access("/sys/bus/platform/drivers/tpm_tis", R_OK) == 0) {
            if (g_tpm2) strcpy(g_tpm_drv, "tpm_tis");
        }
    }
    /* tpm2-tss userspace present? */
    if (access("/usr/lib/x86_64-linux-gnu/libtss2-tcti-device.so", R_OK) == 0 ||
        access("/usr/lib/libtss2.so", R_OK) == 0 ||
        access("/usr/bin/tpm2_getcap", R_OK) == 0) {
        g_tss = 1;
    }
    /* Measured boot (secure boot + PCR 0-7)? */
    if (access("/sys/firmware/efi/efivars/SecureBoot-*", R_OK) == 0 ||
        access("/sys/kernel/security/tpm0", R_OK) == 0) {
        g_measured_boot = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_tpm_present(void)     { return g_tpm; }
int  wubu_tpm_is_tpm2(void)     { return g_tpm2; }
int  wubu_tpm_has_tss(void)     { return g_tss; }
int  wubu_tpm_has_crb(void)     { return g_crb; }
int  wubu_tpm_has_measured_boot(void){ return g_measured_boot; }
const char *wubu_tpm_driver(void){ return g_tpm_drv[0] ? g_tpm_drv : NULL; }

/* ---- W3: TPM driver routing ---- */
const char *wubu_tpm_driver_for(const char *iface)
{
    if (!iface) return NULL;
    if (strstr(iface, "crb"))   return "tpm_crb";
    if (strstr(iface, "tis") || strstr(iface, "fifo")) return "tpm_tis";
    if (strstr(iface, "spi"))   return "tpm_tis_spi";
    if (strstr(iface, "i2c"))   return "tpm_i2c_atmel";
    return "tpm_tis";
}

/* ---- W4: summary ---- */
int wubu_tpm_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "tpm[tpm=%d tpm2=%d tss=%d crb=%d measured=%d drv=%s]",
        g_tpm, g_tpm2, g_tss, g_crb, g_measured_boot,
        wubu_tpm_driver() ? wubu_tpm_driver() : "none");
}
