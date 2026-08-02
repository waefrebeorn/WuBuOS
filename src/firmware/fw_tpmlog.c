/*
 * fw_tpmlog.c  --  TCG 2.0 crypto-agile event log.
 *
 * The event log is what makes PCR values *meaningful*: an attestation
 * verifier replays the log, recomputes each extend, and checks the result
 * matches the quoted PCRs. Anti-cheat stacks that gate on measured boot
 * consume exactly this structure, so it follows the TCG PC Client Platform
 * Firmware Profile layout:
 *
 *   [0] TCG_PCR_EVENT   (SHA1 header event, "Spec ID Event03")
 *   [n] TCG_PCR_EVENT2  (crypto-agile: count + {algId,digest} + event data)
 */

#include "fw.h"
#include "fw_tpm.h"

#define LOG_CAPACITY (64 * 1024)

static uint8_t  g_log[LOG_CAPACITY];
static uint64_t g_log_used;
static uint32_t g_log_events;

static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8*i)); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static uint8_t *log_reserve(uint64_t n) {
    if (g_log_used + n > LOG_CAPACITY) return NULL;
    uint8_t *p = g_log + g_log_used;
    g_log_used += n;
    return p;
}

/* The spec-ID header event: a TCG_PCR_EVENT (SHA1 shape) whose event data
 * declares that everything after it is crypto-agile TCG_PCR_EVENT2. */
static void log_write_header(void) {
    const char sig[] = "Spec ID Event03";
    uint32_t vendor_len = 0;
    uint32_t evsize = 16 + 4 + 1 + 1 + 1 + 1 + 4 + (4) + 1 + vendor_len;

    uint8_t *p = log_reserve(32 + evsize);
    if (!p) return;

    put32(p, 0);                       /* PCRIndex = 0        */
    put32(p + 4, EV_NO_ACTION);        /* EventType           */
    fw_memset(p + 8, 0, 20);           /* SHA1 digest = zero  */
    put32(p + 28, evsize);
    uint8_t *e = p + 32;

    fw_memcpy(e, sig, 16);             /* includes NUL        */
    put32(e + 16, 0);                  /* platformClass       */
    e[20] = 0;                         /* specVersionMinor    */
    e[21] = 2;                         /* specVersionMajor    */
    e[22] = 105;                       /* specErrata          */
    e[23] = 2;                         /* uintnSize = 64-bit  */
    put32(e + 24, 1);                  /* numberOfAlgorithms  */
    put16(e + 28, TPM_ALG_SHA256);
    put16(e + 30, 32);                 /* digest size         */
    e[32] = (uint8_t)vendor_len;

    g_log_events++;
}

/* Append a crypto-agile TCG_PCR_EVENT2. */
static void log_write_event(uint32_t pcr, uint32_t type,
                            const uint8_t digest[32],
                            const char *desc) {
    if (g_log_used == 0) log_write_header();

    uint32_t dlen = desc ? (uint32_t)fw_strlen(desc) : 0;
    uint64_t need = 4 + 4 + 4 + (2 + 32) + 4 + dlen;
    uint8_t *p = log_reserve(need);
    if (!p) return;

    put32(p, pcr);
    put32(p + 4, type);
    put32(p + 8, 1);                   /* digest count        */
    put16(p + 12, TPM_ALG_SHA256);
    fw_memcpy(p + 14, digest, 32);
    put32(p + 46, dlen);
    if (dlen) fw_memcpy(p + 50, desc, dlen);

    g_log_events++;
}

int fw_tpm_measure(uint32_t pcr, uint32_t event_type,
                   const void *data, uint64_t len,
                   const char *description) {
    if (pcr >= TPM_MAX_PCR) return -1;

    uint8_t digest[32];
    fw_sha256(data ? data : "", data ? len : 0, digest);

    int rc = fw_tpm_pcr_extend(pcr, digest);
    log_write_event(pcr, event_type, digest, description);

    fw_printf("[measure] PCR%d <- %x%x%x%x... %s%s\n",
              pcr, digest[0], digest[1], digest[2], digest[3],
              description ? description : "",
              rc == 0 ? "" : " (extend failed)");
    return rc;
}

const void *fw_tpm_log_buffer(void) { return g_log; }
uint64_t    fw_tpm_log_size(void)   { return g_log_used; }
uint32_t    fw_tpm_log_count(void)  { return g_log_events; }

void fw_tpm_log_dump(void) {
    fw_printf("[tpm] event log: %u events, %lu bytes\n", g_log_events, g_log_used);
    for (uint32_t pcr = 0; pcr < 8; pcr++) {
        uint8_t v[32];
        if (fw_tpm_pcr_read(pcr, v) != 0) continue;
        int zero = 1;
        for (int i = 0; i < 32; i++) if (v[i]) { zero = 0; break; }
        if (zero) continue;
        fw_printf("[tpm] PCR%d = ", pcr);
        for (int i = 0; i < 16; i++) fw_puthex(v[i]);
        fw_puts("...\n");
    }
}
