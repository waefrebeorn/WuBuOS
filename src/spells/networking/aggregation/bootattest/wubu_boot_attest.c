/*
 * wubu_boot_attest.c -- boot tracking over the SDR relay (implementation).
 *
 * C11, opaque struct, libc only.  The SHA-256 is NOT duplicated here: the
 * kernel's freestanding implementation (src/kernel/wubu_sha256.c) is the one
 * owner of that concern, so this module declares it and the link supplies it.
 */
#include "wubu_boot_attest.h"

#include <string.h>

/* The one SHA-256 in the tree (src/kernel/wubu_sha256.c, freestanding C11).
 * Declared rather than reimplemented: one owner per concern. */
extern int wubu_sha256(const void *data, size_t len,
                       uint8_t out[WUBU_BOOT_ATTEST_SIG_SZ]);

/* ---- the report ---------------------------------------------------- */

struct wubu_boot_attest {
    uint32_t boot_counter;
    uint64_t sealed_at;
    uint8_t  pcr_digest[WUBU_BOOT_ATTEST_SIG_SZ];
    uint8_t  kernel_digest[WUBU_BOOT_ATTEST_SIG_SZ];
    uint8_t  flags;
    uint8_t  stage_count;
    uint8_t  sealed;                        /* 0 until seal() runs */
    wubu_boot_attest_stage_t stages[WUBU_BOOT_ATTEST_MAX_STAGES];
};

/* A boot report must survive an allocation failure: it is the record of the
 * one thing that cannot be re-run.  So the backing store is static. */
static wubu_boot_attest_t g_boot_attest;

wubu_boot_attest_t *wubu_boot_attest_create(void)
{
    memset(&g_boot_attest, 0, sizeof(g_boot_attest));
    return &g_boot_attest;
}

void wubu_boot_attest_destroy(wubu_boot_attest_t *b)
{
    if (b) memset(b, 0, sizeof(*b));
}

int wubu_boot_attest_bind(wubu_boot_attest_t *b, uint32_t boot_counter,
                          const uint8_t pcr_digest[WUBU_BOOT_ATTEST_SIG_SZ],
                          const uint8_t kernel_digest[WUBU_BOOT_ATTEST_SIG_SZ])
{
    if (!b || !pcr_digest || !kernel_digest) return -1;
    b->boot_counter = boot_counter;
    memcpy(b->pcr_digest, pcr_digest, WUBU_BOOT_ATTEST_SIG_SZ);
    memcpy(b->kernel_digest, kernel_digest, WUBU_BOOT_ATTEST_SIG_SZ);
    b->flags |= WUBU_BOOT_ATTEST_F_BOUND;
    return 0;
}

int wubu_boot_attest_stage(wubu_boot_attest_t *b, const char *name,
                           uint32_t started_ms)
{
    if (!b || !name || !name[0]) return -1;
    if (b->stage_count >= WUBU_BOOT_ATTEST_MAX_STAGES) return -1;
    size_t n = strlen(name);
    if (n > WUBU_BOOT_ATTEST_NAME_LEN) return -1;

    wubu_boot_attest_stage_t *s = &b->stages[b->stage_count];
    memset(s->name, ' ', WUBU_BOOT_ATTEST_NAME_LEN);
    memcpy(s->name, name, n);
    s->name[WUBU_BOOT_ATTEST_NAME_LEN] = '\0';
    s->started_ms  = started_ms;
    s->duration_ms = 0;
    b->stage_count++;
    return 0;
}

/* Duration of stage i runs to the start of stage i+1; the last stage runs to
 * the seal time.  A stage that starts after the seal time (a clock step) is
 * clamped to zero rather than wrapping. */
static void fill_durations(wubu_boot_attest_t *b, uint32_t seal_ms)
{
    for (int i = 0; i < b->stage_count; i++) {
        uint32_t end = (i + 1 < b->stage_count) ? b->stages[i + 1].started_ms
                                                : seal_ms;
        b->stages[i].duration_ms =
            (end >= b->stages[i].started_ms) ? end - b->stages[i].started_ms : 0;
    }
}

/* signature = SHA-256(header[0..84) || stage records) */
static int derive_sig(const wubu_boot_attest_t *b, uint32_t seal_ms,
                      uint8_t out[WUBU_BOOT_ATTEST_SIG_SZ])
{
    uint8_t buf[WUBU_BOOT_ATTEST_HDR_SZ +
                WUBU_BOOT_ATTEST_MAX_STAGES * WUBU_BOOT_ATTEST_STAGE_SZ];
    size_t n = 0;

    buf[n++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC);
    buf[n++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC >> 8);
    buf[n++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC >> 16);
    buf[n++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC >> 24);
    buf[n++] = WUBU_BOOT_ATTEST_VERSION;
    buf[n++] = b->flags;
    buf[n++] = b->stage_count;
    buf[n++] = 0;                                    /* reserved */
    for (int i = 0; i < 4; i++)
        buf[n++] = (uint8_t)(b->boot_counter >> (8 * i));
    for (int i = 0; i < 8; i++)
        buf[n++] = (uint8_t)(b->sealed_at >> (8 * i));
    memcpy(buf + n, b->pcr_digest, WUBU_BOOT_ATTEST_SIG_SZ); n += WUBU_BOOT_ATTEST_SIG_SZ;
    memcpy(buf + n, b->kernel_digest, WUBU_BOOT_ATTEST_SIG_SZ); n += WUBU_BOOT_ATTEST_SIG_SZ;

    /* Stage records, with the durations seal() computed. */
    for (int i = 0; i < b->stage_count; i++) {
        const wubu_boot_attest_stage_t *s = &b->stages[i];
        memcpy(buf + n, s->name, WUBU_BOOT_ATTEST_NAME_LEN);
        n += WUBU_BOOT_ATTEST_NAME_LEN;
        for (int k = 0; k < 4; k++)
            buf[n++] = (uint8_t)(s->started_ms >> (8 * k));
        /* Serialize the STORED duration, exactly as encode() does: deriving it
         * here from the next stage would disagree with the frame bytes
         * whenever a stage was recorded after seal(). */
        for (int k = 0; k < 4; k++)
            buf[n++] = (uint8_t)(s->duration_ms >> (8 * k));
    }

    return wubu_sha256(buf, n, out);
}

int wubu_boot_attest_seal(wubu_boot_attest_t *b, uint64_t now_unix)
{
    if (!b || b->stage_count == 0) return -1;
    if (!(b->flags & WUBU_BOOT_ATTEST_F_BOUND)) return -1;

    b->sealed_at = now_unix;
    /* The seal time in monotonic ms: the last stage's duration is measured
     * from its own start, so we only need a value >= every start. */
    uint32_t last_start = b->stages[b->stage_count - 1].started_ms;
    fill_durations(b, last_start);
    b->sealed = 1;
    return 0;
}

/* ---- wire -------------------------------------------------------- */

long wubu_boot_attest_encode(const wubu_boot_attest_t *b,
                             uint8_t *out, size_t n)
{
    if (!b || !out || !b->sealed) return -1;

    size_t need = WUBU_BOOT_ATTEST_HDR_SZ +
                  (size_t)b->stage_count * WUBU_BOOT_ATTEST_STAGE_SZ +
                  WUBU_BOOT_ATTEST_SIG_SZ;
    if (n < need) return -1;

    size_t o = 0;
    out[o++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC);
    out[o++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC >> 8);
    out[o++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC >> 16);
    out[o++] = (uint8_t)(WUBU_BOOT_ATTEST_MAGIC >> 24);
    out[o++] = WUBU_BOOT_ATTEST_VERSION;
    out[o++] = b->flags;
    out[o++] = b->stage_count;
    out[o++] = 0;
    for (int i = 0; i < 4; i++) out[o++] = (uint8_t)(b->boot_counter >> (8 * i));
    for (int i = 0; i < 8; i++) out[o++] = (uint8_t)(b->sealed_at >> (8 * i));
    memcpy(out + o, b->pcr_digest, WUBU_BOOT_ATTEST_SIG_SZ); o += WUBU_BOOT_ATTEST_SIG_SZ;
    memcpy(out + o, b->kernel_digest, WUBU_BOOT_ATTEST_SIG_SZ); o += WUBU_BOOT_ATTEST_SIG_SZ;

    for (int i = 0; i < b->stage_count; i++) {
        const wubu_boot_attest_stage_t *s = &b->stages[i];
        memcpy(out + o, s->name, WUBU_BOOT_ATTEST_NAME_LEN);
        o += WUBU_BOOT_ATTEST_NAME_LEN;
        for (int k = 0; k < 4; k++) out[o++] = (uint8_t)(s->started_ms >> (8 * k));
        for (int k = 0; k < 4; k++) out[o++] = (uint8_t)(s->duration_ms >> (8 * k));
    }

    uint8_t sig[WUBU_BOOT_ATTEST_SIG_SZ];
    if (derive_sig(b, b->stages[b->stage_count - 1].started_ms, sig) != 0)
        return -1;
    memcpy(out + o, sig, WUBU_BOOT_ATTEST_SIG_SZ);
    o += WUBU_BOOT_ATTEST_SIG_SZ;

    return (long)o;
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

/* Verify the signature over a frame body (everything before the sig). */
int wubu_boot_attest_verify(const uint8_t *in, size_t n,
                            uint8_t sig[WUBU_BOOT_ATTEST_SIG_SZ])
{
    if (!in || !sig) return -1;
    if (n < WUBU_BOOT_ATTEST_HDR_SZ + WUBU_BOOT_ATTEST_SIG_SZ) return -1;
    if (rd32(in) != WUBU_BOOT_ATTEST_MAGIC) return -1;
    if (in[4] != WUBU_BOOT_ATTEST_VERSION) return -1;

    uint8_t stages = in[6];
    if (stages > WUBU_BOOT_ATTEST_MAX_STAGES) return -1;
    size_t body = WUBU_BOOT_ATTEST_HDR_SZ +
                  (size_t)stages * WUBU_BOOT_ATTEST_STAGE_SZ;
    if (n < body + WUBU_BOOT_ATTEST_SIG_SZ) return -1;

    uint8_t want[WUBU_BOOT_ATTEST_SIG_SZ];
    if (wubu_sha256(in, body, want) != 0) return -1;

    /* Constant-time-ish compare: every byte contributes, so the time does not
     * leak how many leading bytes matched. */
    uint8_t diff = 0;
    for (int i = 0; i < WUBU_BOOT_ATTEST_SIG_SZ; i++)
        diff |= (uint8_t)(want[i] ^ in[body + i]);
    if (diff != 0) return -1;

    memcpy(sig, want, WUBU_BOOT_ATTEST_SIG_SZ);
    return 0;
}

int wubu_boot_attest_decode(const uint8_t *in, size_t n,
                            wubu_boot_attest_view_t *view)
{
    if (!in || !view) return -1;

    uint8_t sig[WUBU_BOOT_ATTEST_SIG_SZ];
    /* Verify FIRST and into a scratch buffer: a refused frame must leave the
     * caller's view untouched. */
    if (wubu_boot_attest_verify(in, n, sig) != 0) return -1;

    uint8_t stages = in[6];
    size_t o = 8;
    wubu_boot_attest_view_t v;
    memset(&v, 0, sizeof(v));
    v.boot_counter = rd32(in + o); o += 4;
    v.sealed_at    = rd64(in + o); o += 8;
    memcpy(v.pcr_digest, in + o, WUBU_BOOT_ATTEST_SIG_SZ); o += WUBU_BOOT_ATTEST_SIG_SZ;
    memcpy(v.kernel_digest, in + o, WUBU_BOOT_ATTEST_SIG_SZ); o += WUBU_BOOT_ATTEST_SIG_SZ;
    v.stage_count = stages;
    v.flags = in[5];

    for (int i = 0; i < stages; i++) {
        wubu_boot_attest_stage_t *s = &v.stages[i];
        memcpy(s->name, in + o, WUBU_BOOT_ATTEST_NAME_LEN);
        s->name[WUBU_BOOT_ATTEST_NAME_LEN] = '\0';
        o += WUBU_BOOT_ATTEST_NAME_LEN;
        s->started_ms  = rd32(in + o); o += 4;
        s->duration_ms = rd32(in + o); o += 4;
        /* The wire pads the name with spaces to a fixed 16 bytes; the view
         * hands the peer a trimmed name, as the swarm wire does for a kind. */
        for (int k = WUBU_BOOT_ATTEST_NAME_LEN - 1; k >= 0; k--) {
            if (s->name[k] == ' ') s->name[k] = '\0';
            else break;
        }
    }

    memcpy(view, &v, sizeof(v));
    return 0;
}

int wubu_boot_attest_pcr_digest(const uint8_t pcrs[WUBU_BOOT_ATTEST_PCR_COUNT]
                                              [WUBU_BOOT_ATTEST_SIG_SZ],
                                uint8_t out[WUBU_BOOT_ATTEST_SIG_SZ])
{
    if (!pcrs || !out) return -1;
    return wubu_sha256(pcrs, WUBU_BOOT_ATTEST_PCR_COUNT * WUBU_BOOT_ATTEST_SIG_SZ,
                       out);
}

/* ---- read-only view of a sealed report -------------------------- */

uint32_t wubu_boot_attest_get_boot_counter(const wubu_boot_attest_t *b)
{
    return (b && b->sealed) ? b->boot_counter : 0;
}

uint64_t wubu_boot_attest_get_sealed_at(const wubu_boot_attest_t *b)
{
    return (b && b->sealed) ? b->sealed_at : 0;
}

const uint8_t *wubu_boot_attest_get_pcr_digest(const wubu_boot_attest_t *b)
{
    return (b && b->sealed) ? b->pcr_digest : NULL;
}

const uint8_t *wubu_boot_attest_get_kernel_digest(const wubu_boot_attest_t *b)
{
    return (b && b->sealed) ? b->kernel_digest : NULL;
}

uint8_t wubu_boot_attest_get_stage_count(const wubu_boot_attest_t *b)
{
    return (b && b->sealed) ? b->stage_count : 0;
}
