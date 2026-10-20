/*
 * wubu_measured_agent.c -- agent self-integrity verification against the
 * measured-boot PCRs (AGIEDR #44). See wubu_measured_agent.h.
 */
#include "wubu_measured_agent.h"
#include "wubu_attest.h"
#include "wubu_sha256.h"

#include <string.h>
#include <stdbool.h>

/* The retained checkpoint chain: each checkpoint stores the runtime-PCR
 * digest reached by extending the PCR with that state. The PCR's own
 * sha256(pcr || data) construction makes the sequence a chain; this
 * table is what verify() compares the LIVE PCR against, so any extend
 * we did not authorize (or any altered record) shows up as a mismatch. */
static struct {
    uint64_t seq;
    uint8_t  digest[WUBU_MA_PCR_SZ];
} g_ma_checkpoints[WUBU_MA_CHECKPOINTS];

static int      g_ma_ready;
static int64_t  g_ma_seq;
static uint8_t g_ma_baseline[WUBU_MA_PCR_SZ];       /* measured-boot PCR4 */
static uint8_t g_ma_origin[WUBU_MA_PCR_SZ];        /* runtime PCR at lock */
static int      g_ma_origin_valid;

void wubu_measured_agent_reset(void)
{
    memset(g_ma_checkpoints, 0, sizeof(g_ma_checkpoints));
    memset(g_ma_baseline, 0, sizeof(g_ma_baseline));
    memset(g_ma_origin, 0, sizeof(g_ma_origin));
    g_ma_origin_valid = 0;
    g_ma_ready = 0;
    g_ma_seq = 0;
}

int wubu_measured_agent_ready(void)
{
    return g_ma_ready;
}

int wubu_measured_agent_init(void)
{
    if (g_ma_ready) return -2;
    /* Fail closed: no live root of trust, no self-modification policy. */
    if (!wubu_attest_valid()) return -1;
    if (wubu_attest_pcr4_digest(g_ma_baseline) != 0) return -1;
    /* Snapshot the runtime PCR as the chain ORIGIN. Reading it here also
     * SEALS it (wubu_attest seals an unread PCR to sha256("") on first
     * read), so the origin is a fixed digest, not a moving target. */
    if (wubu_attest_runtime_pcr(g_ma_origin) != 0) return -1;
    g_ma_origin_valid = 1;
    g_ma_ready = 1;
    g_ma_seq = 0;
    return 0;
}

int64_t wubu_measured_agent_checkpoint(const void *data, size_t len)
{
    if (!data || len == 0) return 0;
    if (!g_ma_ready) return -1;
    if (g_ma_seq >= WUBU_MA_CHECKPOINTS) return 0;   /* chain table full */
    /* Chain the state into the measured-boot lineage... */
    if (wubu_attest_extend_runtime(data, len) != 0) return -1;
    /* ...and retain the digest the PCR reached. */
    uint8_t now[WUBU_MA_PCR_SZ];
    if (wubu_attest_runtime_pcr(now) != 0) return -1;
    g_ma_seq++;
    g_ma_checkpoints[g_ma_seq - 1].seq = g_ma_seq;
    memcpy(g_ma_checkpoints[g_ma_seq - 1].digest, now, sizeof(now));
    return g_ma_seq;
}

int wubu_measured_agent_verify(void)
{
    if (!g_ma_ready) return -2;
    /* The chain lives in the attested runtime PCR: with the root of
     * trust gone the chain is UNREADABLE, which is a different fact
     * from verified-and-broken. -2 = cannot verify; -1 = tampered. */
    if (!wubu_attest_valid()) return -2;
    if (g_ma_seq == 0) {
        /* No checkpoint yet: the live runtime PCR must still equal the
         * sealed ORIGIN captured at init -- anything else is an
         * unauthorized extend. */
        uint8_t now[WUBU_MA_PCR_SZ];
        if (wubu_attest_runtime_pcr(now) != 0) return -1;
        if (!g_ma_origin_valid) return -1;
        if (memcmp(now, g_ma_origin, sizeof(now)) != 0) return -1;
        return 0;
    }
    uint8_t now[WUBU_MA_PCR_SZ];
    if (wubu_attest_runtime_pcr(now) != 0) return -1;
    if (memcmp(now, g_ma_checkpoints[g_ma_seq - 1].digest,
               sizeof(now)) != 0)
        return -1;   /* the PCR moved without an authorized checkpoint */
    return 0;
}

int wubu_measured_agent_may_selfmodify(void)
{
    if (!g_ma_ready) return 0;
    if (!wubu_attest_valid()) return 0;             /* root of trust died */
    return wubu_measured_agent_verify() == 0;
}

int wubu_measured_agent_baseline(uint8_t out[WUBU_MA_PCR_SZ])
{
    if (!out || !g_ma_ready) return -1;
    memcpy(out, g_ma_baseline, WUBU_MA_PCR_SZ);
    return 0;
}

int wubu_measured_agent_checkpoint_count(void)
{
    return (int)g_ma_seq;
}
