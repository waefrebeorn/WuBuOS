/*
 * wubu_attest.c -- WuBuOS kernel-side firmware attestation consumer (ring-0).
 *
 * Freestanding C11: no malloc, no pthreads, no hosted APIs. The module owns
 * a single static snapshot of the firmware attestation (one global -- the
 * kernel is single-instance). The WuBuFW loader wrote the handoff block
 * before tearing down to 32-bit mode; the kernel reads it through the fixed
 * low-physical addresses (identity-mapped by crt0's 0..1GB PD).
 */
#include "wubu_attest.h"
#include <string.h>

/* Pin the wire layout so a firmware/loader change breaks the kernel build
 * instead of silently misreading the table. */
_Static_assert(sizeof(wubu_attest_t) == 288,
               "wubu_attest_t layout drift vs fw_agi_attest_t");
_Static_assert(sizeof(wubu_loader_handoff_t) == 56,
               "wubu_loader_handoff_t layout drift vs WuBuFW loader");

static wubu_attest_t g_att;
static bool          g_valid = false;
static uint8_t       g_kernel_sha[WUBU_AGI_PCR_SZ];
static uint32_t      g_kernel_size = 0;

/* Runtime PCR (gap A10): a kernel-side PCR beyond the firmware's 0-7.
 * The kernel EXTENDS it with its runtime state (promotions, uptime,
 * counters) so the measured-boot chain continues into the running
 * kernel -- the anti-cheat's live proof that the kernel's behavior is
 * the one the boot measured. Starts as SHA-256 of the empty string;
 * every extension chains: pcr = sha256(pcr || data). */
static uint8_t g_runtime_pcr[WUBU_AGI_PCR_SZ];
static bool    g_runtime_pcr_live = false;

static int attest_copy(const void *raw)
{
    if (!raw) return -1;
    const wubu_attest_t *a = (const wubu_attest_t *)raw;
    if (a->magic        != WUBU_AGI_MAGIC)         return -1;
    if (a->version      != WUBU_AGI_ATTEST_VERSION) return -1;
    if (a->pcr_count    != WUBU_AGI_PCR_COUNT)      return -1;
    memcpy(&g_att, a, sizeof(g_att));
    g_valid = true;
    return 0;
}

int wubu_attest_ingest(const void *raw)
{
    g_valid = false;
    return attest_copy(raw);
}

int wubu_attest_ingest_handoff(const void *handoff)
{
    g_valid = false;
    memset(g_kernel_sha, 0, sizeof(g_kernel_sha));
    g_kernel_size = 0;
    if (!handoff) return -1;
    const wubu_loader_handoff_t *h = (const wubu_loader_handoff_t *)handoff;
    if (h->magic   != WUBU_LOADER_HANDOFF_MAGIC) return -1;
    if (h->version != 1)                          return -1;
    memcpy(g_kernel_sha, h->kernel_sha256, sizeof(g_kernel_sha));
    g_kernel_size = h->kernel_size;
    if (h->attest_addr == 0) return -1;   /* no firmware root of trust */
    return attest_copy((const void *)(uintptr_t)h->attest_addr);
}

int wubu_attest_load_scratch(void)
{
    const volatile uint64_t *slot =
        (const volatile uint64_t *)(uintptr_t)WUBU_HANDOFF_PTR_ADDR;
    uint64_t handoff_addr = *slot;
    if (handoff_addr == 0) return -1;
    return wubu_attest_ingest_handoff((const void *)(uintptr_t)handoff_addr);
}

void wubu_attest_clear(void)
{
    g_valid = false;
    memset(&g_att, 0, sizeof(g_att));
    memset(g_kernel_sha, 0, sizeof(g_kernel_sha));
    g_kernel_size = 0;
}

bool wubu_attest_valid(void)        { return g_valid; }
uint32_t wubu_attest_version(void)  { return g_valid ? g_att.version : 0; }
uint32_t wubu_attest_boot_counter(void) { return g_valid ? g_att.boot_counter : 0; }
bool wubu_attest_sb_enabled(void)   { return g_valid && g_att.sb_enabled != 0; }
bool wubu_attest_setup_mode(void)   { return g_valid && g_att.sb_setup_mode != 0; }

int wubu_attest_pcr(unsigned i, uint8_t out[WUBU_AGI_PCR_SZ])
{
    if (!g_valid || i >= WUBU_AGI_PCR_COUNT || !out) return -1;
    memcpy(out, g_att.pcr[i], WUBU_AGI_PCR_SZ);
    return 0;
}

int wubu_attest_pcr4_digest(uint8_t out[WUBU_AGI_PCR_SZ])
{
    return wubu_attest_pcr(4, out);
}

int wubu_attest_kernel_digest(uint8_t out[WUBU_AGI_PCR_SZ])
{
    if (!g_valid || !out) return -1;
    memcpy(out, g_kernel_sha, WUBU_AGI_PCR_SZ);
    return 0;
}

/* ---- Runtime PCR (gap A10) ---- */

/* One-shot extend: pcr := sha256(pcr || data). The runtime PCR starts
 * as the digest of the empty string, so the first extension chains from
 * a known value. */
int wubu_attest_extend_runtime(const void *data, size_t len)
{
    extern int wubu_sha256(const void *, size_t, uint8_t[WUBU_AGI_PCR_SZ]);
    if (!g_valid || (!data && len)) return -1;

    if (!g_runtime_pcr_live) {
        wubu_sha256(NULL, 0, g_runtime_pcr);   /* sha256("") */
        g_runtime_pcr_live = true;
    }
    /* chain: pcr_new = sha256(pcr_old || data) */
    uint8_t buf[WUBU_AGI_PCR_SZ * 2 + 64];
    memcpy(buf, g_runtime_pcr, WUBU_AGI_PCR_SZ);
    size_t n = WUBU_AGI_PCR_SZ;
    const uint8_t *p = (const uint8_t *)data;
    while (len) {
        size_t chunk = len < 64 ? len : 64;
        memcpy(buf + n, p, chunk);
        n += chunk;
        p += chunk;
        len -= chunk;
        if (n >= WUBU_AGI_PCR_SZ + 64) {
            wubu_sha256(buf, n, g_runtime_pcr);
            n = WUBU_AGI_PCR_SZ;
            memcpy(buf, g_runtime_pcr, WUBU_AGI_PCR_SZ);
        }
    }
    if (n > WUBU_AGI_PCR_SZ)
        wubu_sha256(buf, n, g_runtime_pcr);
    else if (n == WUBU_AGI_PCR_SZ && len == 0)
        wubu_sha256(buf, n, g_runtime_pcr);
    return 0;
}

int wubu_attest_runtime_pcr(uint8_t out[WUBU_AGI_PCR_SZ])
{
    if (!g_valid || !out) return -1;
    if (!g_runtime_pcr_live) {
        wubu_sha256(NULL, 0, g_runtime_pcr);
        g_runtime_pcr_live = true;
    }
    memcpy(out, g_runtime_pcr, WUBU_AGI_PCR_SZ);
    return 0;
}

uint32_t wubu_attest_kernel_size(void)
{
    return g_valid ? g_kernel_size : 0;
}
