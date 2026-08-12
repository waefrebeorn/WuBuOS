/*
 * wubu_securekey.c -- kernel-owned security key / TOTP / TPM driver routing.
 *
 * The security tier: FIDO2/U2F hardware keys, smart-card readers, and the
 * TPM. "Runs on everything" includes hardware authentication. WuBuOS owns
 * the routing to the FIDO HID driver, the CCID smart-card driver, and the
 * TPM (tpm_tis / tpm_crb) chip driver.
 *
 * Security devices:
 *   - FIDO2/U2F keys: u2f-zero, YubiKey, Google Titan (hid-u2f / hid-fido2)
 *   - Smart card readers: CCID (pcscd / ccid driver), SCR3310, ACR122U
 *   - TPM: tpm_tis (FIFO/LPC), tpm_crb (CRB/ACPI), tpm_i2c_atmel,
 *     tpm_tis_spi; /dev/tpm0, /dev/tpmrm0
 *   - TOTP/OTP: hardware tokens (YubiKey OTP over HID)
 *
 * WuBuOS owns this: detect the security device (HID/USB/ACPI TPM), route
 * to the right driver, and flag the security topology for attestation.
 *
 * Research (Kevin-Bacon 7-hop on the security-key frontier):
 *   - FIDO2: hid-u2f / hid-fido2 (Linux kernel driver for U2F tokens)
 *   - CCID: pcscd + ccid driver (smart-card readers)
 *   - TPM: tpm_tis, tpm_crb (ACPI CRB), /dev/tpm0; TPM 2.0
 *   - TOTP: hardware OTP tokens (YubiKey OTP, FIDO)
 */
#include "wubu_securekey.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_fido = 0;         /* FIDO2/U2F key */
static int  g_ccid = 0;         /* smart card reader */
static int  g_tpm = 0;          /* TPM chip */
static char g_sec_drv[32] = "";
static char g_sec_type[24] = "";

/* ---- W1: probe the security topology ---- */
void wubu_securekey_probe(void)
{
    g_fido = 0; g_ccid = 0; g_tpm = 0;
    g_sec_drv[0] = '\0'; g_sec_type[0] = '\0';

#ifdef _GNU_SOURCE
    /* FIDO2/U2F key (HID driver loaded)? */
    if (access("/sys/bus/hid/drivers/hid-u2f", R_OK) == 0 ||
        access("/sys/bus/hid/drivers/hid-fido2", R_OK) == 0) {
        g_fido = 1;
        strcpy(g_sec_drv, "hid-fido2");
        strcpy(g_sec_type, "FIDO2/U2F");
    }
    /* Smart card reader (CCID)? */
    if (access("/sys/bus/usb/drivers/ccid", R_OK) == 0 ||
        access("/dev/reader0", R_OK) == 0 ||
        access("/run/pcscd/pcscd.comm", R_OK) == 0) {
        g_ccid = 1;
        if (!g_sec_drv[0]) { strcpy(g_sec_drv, "ccid"); strcpy(g_sec_type, "smart card"); }
    }
    /* TPM chip? */
    if (access("/dev/tpm0", R_OK) == 0 ||
        access("/dev/tpmrm0", R_OK) == 0 ||
        access("/sys/class/tpm", R_OK) == 0) {
        g_tpm = 1;
        if (!g_sec_drv[0]) { strcpy(g_sec_drv, "tpm_tis"); strcpy(g_sec_type, "TPM"); }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_securekey_fido(void)  { return g_fido; }
int  wubu_securekey_ccid(void)  { return g_ccid; }
int  wubu_securekey_tpm(void)   { return g_tpm; }
int  wubu_securekey_present(void){ return (g_fido || g_ccid || g_tpm); }
const char *wubu_securekey_driver(void){ return g_sec_drv[0] ? g_sec_drv : NULL; }
const char *wubu_securekey_type(void){ return g_sec_type[0] ? g_sec_type : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_securekey_driver_for(const char *dev)
{
    if (!dev) return NULL;
    if (strstr(dev, "u2f") || strstr(dev, "fido")) return "hid-fido2";
    if (strstr(dev, "ccid"))  return "ccid";
    if (strstr(dev, "tpm_tis")) return "tpm_tis";
    if (strstr(dev, "tpm_crb")) return "tpm_crb";
    if (strstr(dev, "tpm"))   return "tpm_tis";
    return "security-core";
}

/* ---- W4: summary ---- */
int wubu_securekey_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "sec[fido=%d ccid=%d tpm=%d drv=%s type=%s]",
        g_fido, g_ccid, g_tpm,
        wubu_securekey_driver() ? wubu_securekey_driver() : "none",
        wubu_securekey_type() ? wubu_securekey_type() : "-");
}
