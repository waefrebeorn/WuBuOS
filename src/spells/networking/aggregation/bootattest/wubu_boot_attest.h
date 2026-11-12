/*
 * wubu_boot_attest.h -- boot tracking over the SDR relay.
 *
 * What this is FOR: the firmware already measures the boot (PCR0-7 + Secure
 * Boot state, published as an EFI Configuration Table and consumed by the
 * kernel's wubu_attest).  None of it leaves the machine.  This module is the
 * half that DOES: it turns the measured boot into a small, sealed report --
 * stage by stage, with a time stamp and a signature -- so a peer can see that
 * a boot happened, when, and what it measured.
 *
 * The report is carried on the SDR relay (see wubu_swarm_wire.h for the
 * contribution frame; this frame is the boot report's own shape) and is
 * broadcast to every peer, not sent to one.
 *
 * What it deliberately is NOT: authentication.  The tree has no signing
 * primitive -- the firmware's RSA is VERIFY-only (fw_rsa.h) and the private
 * key never leaves the TPM.  So the signature here is a SHA-256 hash chain
 * that (a) proves the report was not altered in transit and (b) binds it to
 * the firmware's PCR bank.  It cannot prove WHO booted.  That is the
 * "insecure by design" doctrine the swarm already states: identity is
 * placement, not authentication, and the transparency comes from everyone
 * being able to see everything -- not from a trusted signer.
 *
 * Wire layout (little-endian, fixed header, then stage records, then the
 * signature over everything before it):
 *
 *   off   size  field
 *     0      4  magic "WMBT"
 *     4      1  version (1)
 *     5      1  flags (bit0 = firmware attestation bound)
 *     6      1  stage_count
 *     7      1  reserved
 *     8      4  boot_counter
 *    12      8  sealed_at (unix seconds)
 *    20     32  pcr_digest (SHA-256 over PCR0..7)
 *    52     32  kernel_digest
 *    84  24*n   stage records: name[16] space-padded, started_ms u32,
 *                duration_ms u32
 *   84+24n  32  signature = SHA-256(header[0..84) || stage records)
 *
 * C11, opaque struct, libc only.  Part of the swarm spell
 * (seedbook networking/aggregation).
 */
#ifndef WUBU_BOOT_ATTEST_H
#define WUBU_BOOT_ATTEST_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_BOOT_ATTEST_MAGIC      0x54424D57u  /* "WMBT" */
#define WUBU_BOOT_ATTEST_VERSION    1u
#define WUBU_BOOT_ATTEST_HDR_SZ     84
#define WUBU_BOOT_ATTEST_STAGE_SZ   24
#define WUBU_BOOT_ATTEST_SIG_SZ     32
#define WUBU_BOOT_ATTEST_MAX_STAGES 16
#define WUBU_BOOT_ATTEST_NAME_LEN   16
#define WUBU_BOOT_ATTEST_PCR_COUNT  8

/* flags */
#define WUBU_BOOT_ATTEST_F_BOUND    0x01u  /* firmware attestation bound */

/* The largest frame we emit or accept. */
#define WUBU_BOOT_ATTEST_FRAME_SZ \
    (WUBU_BOOT_ATTEST_HDR_SZ + \
     WUBU_BOOT_ATTEST_MAX_STAGES * WUBU_BOOT_ATTEST_STAGE_SZ + \
     WUBU_BOOT_ATTEST_SIG_SZ)

typedef struct {
    char     name[WUBU_BOOT_ATTEST_NAME_LEN + 1];
    uint32_t started_ms;      /* milliseconds since boot, monotonic */
    uint32_t duration_ms;     /* filled by seal() */
} wubu_boot_attest_stage_t;

/* What a peer gets back from a decoded report. */
typedef struct {
    uint32_t boot_counter;
    uint64_t sealed_at;
    uint8_t  pcr_digest[WUBU_BOOT_ATTEST_SIG_SZ];
    uint8_t  kernel_digest[WUBU_BOOT_ATTEST_SIG_SZ];
    uint8_t  stage_count;
    uint8_t  flags;
    wubu_boot_attest_stage_t stages[WUBU_BOOT_ATTEST_MAX_STAGES];
} wubu_boot_attest_view_t;

typedef struct wubu_boot_attest wubu_boot_attest_t;

/* Create an empty report.  Never returns NULL (a static instance backs it,
 * so a failed allocation cannot lose a boot report). */
wubu_boot_attest_t *wubu_boot_attest_create(void);
void                wubu_boot_attest_destroy(wubu_boot_attest_t *b);

/* Bind the report to the firmware's measured boot.  Both digests are COPIED.
 * Returns -1 when either digest is NULL (a report without a measurement is
 * not a boot report). */
int wubu_boot_attest_bind(wubu_boot_attest_t *b, uint32_t boot_counter,
                          const uint8_t pcr_digest[WUBU_BOOT_ATTEST_SIG_SZ],
                          const uint8_t kernel_digest[WUBU_BOOT_ATTEST_SIG_SZ]);

/* Record a boot stage.  `started_ms` is monotonic milliseconds since boot;
 * the duration is computed by seal() from the NEXT stage (the last stage
 * gets the seal time as its end).  Returns -1 on an empty/over-long name or
 * a full stage table. */
int wubu_boot_attest_stage(wubu_boot_attest_t *b, const char *name,
                           uint32_t started_ms);

/* Seal the report: stamp `now_unix`, compute durations, derive the
 * signature.  Returns 0, or -1 when there is nothing to seal (no stages or
 * no bound measurement). */
int wubu_boot_attest_seal(wubu_boot_attest_t *b, uint64_t now_unix);

/* Encode the sealed report.  Returns the number of bytes written, or -1 when
 * the buffer is too small or the report is not sealed. */
long wubu_boot_attest_encode(const wubu_boot_attest_t *b,
                             uint8_t *out, size_t n);

/* Decode a frame and verify its signature.  On success fills `view` and
 * returns 0.  Every failure -- bad magic, bad version, truncated frame,
 * stage overflow, signature mismatch -- returns -1 and leaves `view`
 * untouched, so a caller cannot learn WHY a report was refused. */
int wubu_boot_attest_decode(const uint8_t *in, size_t n,
                            wubu_boot_attest_view_t *view);

/* Recompute the signature of a decoded frame, so a peer can check it without
 * trusting decode()'s verdict.  Returns 0 and fills `sig`, or -1. */
int wubu_boot_attest_verify(const uint8_t *in, size_t n,
                            uint8_t sig[WUBU_BOOT_ATTEST_SIG_SZ]);

/* Digest PCR0..7 into out[32].  Pure: the caller supplies the PCR bank (the
 * kernel reads it with wubu_attest_pcr). */
int wubu_boot_attest_pcr_digest(const uint8_t pcrs[WUBU_BOOT_ATTEST_PCR_COUNT]
                                              [WUBU_BOOT_ATTEST_SIG_SZ],
                                uint8_t out[WUBU_BOOT_ATTEST_SIG_SZ]);

#endif /* WUBU_BOOT_ATTEST_H */
