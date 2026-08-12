/*
 * wubu_ima.c -- kernel-owned IMA/EVM measured boot routing.
 *
 * IMA (Integrity Measurement Architecture) + EVM (Extended Verification
 * Module) provide file integrity + measurement on Linux. "Runs on
 * everything" includes correct integrity on every boot.
 *
 * IMA/EVM:
 *   - IMA: /sys/kernel/security/ima (measurement, appraisal)
 *   - EVM: /sys/kernel/security/evm (HMAC file attrs)
 *   - IMA policy: /sys/kernel/security/ima/policy
 *   - appraisal: file integrity verification
 *   - measured boot: TPM PCR extends on boot
 *   - keys: EVM keyring, IMA keys
 *
 * WuBuOS owns this: detect IMA/EVM support + policy + appraisal state,
 * route to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the IMA frontier):
 *   - IMA measurement + appraisal (/sys/kernel/security/ima)
 *   - EVM HMAC (/sys/kernel/security/evm)
 *   - IMA policy + keys
 *   - measured boot TPM PCR extends
 */
#include "wubu_ima.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_ima = 0;         /* IMA present */
static int  g_evm = 0;         /* EVM present */
static int  g_measure = 0;     /* measurement */
static int  g_appraise = 0;    /* appraisal */
static int  g_pcr = 0;         /* PCR (TPM) */
static char g_ima_drv[24] = "";

/* ---- W1: probe the IMA/EVM topology ---- */
void wubu_ima_probe(void)
{
    g_ima = 0; g_evm = 0; g_measure = 0; g_appraise = 0; g_pcr = 0;
    g_ima_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* IMA? */
    if (access("/sys/kernel/security/ima", R_OK) == 0) {
        g_ima = 1;
        strcpy(g_ima_drv, "ima");
        g_measure = 1;
    }
    /* EVM? */
    if (access("/sys/kernel/security/evm", R_OK) == 0) {
        g_evm = 1;
        if (!g_ima_drv[0]) strcpy(g_ima_drv, "evm");
    }
    /* appraisal? */
    if (access("/sys/kernel/security/ima/policy", R_OK) == 0) {
        g_appraise = 1;
    }
    /* PCR (TPM measurement)? */
    if (access("/sys/class/tpm", R_OK) == 0 || g_ima) {
        g_pcr = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_ima_present(void)   { return g_ima; }
int  wubu_ima_evm(void)       { return g_evm; }
int  wubu_ima_measure(void)   { return g_measure; }
int  wubu_ima_appraise(void)  { return g_appraise; }
int  wubu_ima_pcr(void)       { return g_pcr; }
const char *wubu_ima_driver(void){ return g_ima_drv[0] ? g_ima_drv : NULL; }

/* ---- W3: IMA/EVM routing ---- */
const char *wubu_ima_mode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "measure"))  return "measure";
    if (strstr(mode, "appraise")) return "appraise";
    if (strstr(mode, "audit"))    return "audit";
    return "measure";
}

const char *wubu_ima_policy_for(const char *policy)
{
    if (!policy) return NULL;
    if (strstr(policy, "ltcb"))   return "ltcb";
    if (strstr(policy, "tcb"))    return "tcb";
    if (strstr(policy, "ape"))    return "ape";
    if (strstr(policy, "critical")) return "critical-data";
    return "tcb";
}

/* ---- W4: summary ---- */
int wubu_ima_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ima[ima=%d evm=%d measure=%d appraise=%d pcr=%d drv=%s]",
        g_ima, g_evm, g_measure, g_appraise, g_pcr,
        wubu_ima_driver() ? wubu_ima_driver() : "none");
}
