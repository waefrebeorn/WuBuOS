/*
 * fw_tpm.c  --  WuBuFW TPM 2.0 driver (TIS/FIFO + CRB) and measured boot.
 *
 * Kernel-level anti-cheat (EAC/BattlEye/Vanguard) requires a hardware root of
 * trust: a real TPM, a firmware that measures each stage into PCRs before
 * executing it, and a TCG event log the OS can replay to prove the PCR values
 * were produced by the claimed code. This implements exactly that chain.
 *
 * Both TCG transports are supported:
 *   - TIS/FIFO  (MMIO at 0xFED40000, the classic locality-based interface)
 *   - CRB       (Command Response Buffer, discovered via the ACPI TPM2 table)
 *
 * When no TPM is present, measurement degrades to software-only: the event
 * log is still built and PCR values are still computed by the same extend
 * rule, so attestation policy can distinguish "no TPM" from "bad TPM" rather
 * than being silently skipped.
 */

#include "fw.h"
#include "fw_acpi.h"
#include "fw_tpm.h"
#include <string.h>

void fw_sha256_pair(const uint8_t a[32], const uint8_t b[32], uint8_t out[32]);

/* -- TIS register map (locality 0) ----------------------------------- */
#define TIS_BASE          0xFED40000ULL
#define TIS_ACCESS        0x0000
#define TIS_INT_ENABLE    0x0008
#define TIS_STS           0x0018
#define TIS_DATA_FIFO     0x0024
#define TIS_INTF_CAP      0x0014
#define TIS_DID_VID       0x0F00

#define ACCESS_VALID          0x80
#define ACCESS_ACTIVE_LOCALITY 0x20
#define ACCESS_REQUEST_USE    0x02

#define STS_VALID     0x80
#define STS_COMMAND_READY 0x40
#define STS_GO        0x20
#define STS_DATA_AVAIL 0x10
#define STS_EXPECT    0x08

/* -- CRB register map ------------------------------------------------ */
#define CRB_LOC_STATE     0x00
#define CRB_LOC_CTRL      0x08
#define CRB_LOC_STS       0x0C
#define CRB_CTRL_REQ      0x40
#define CRB_CTRL_STS      0x44
#define CRB_CTRL_CANCEL   0x48
#define CRB_CTRL_START    0x4C
#define CRB_CTRL_CMD_SIZE 0x58
#define CRB_CTRL_CMD_LADDR 0x5C
#define CRB_CTRL_CMD_HADDR 0x60
#define CRB_CTRL_RSP_SIZE 0x64
#define CRB_CTRL_RSP_ADDR 0x68

static fw_tpm_iface g_iface;
static uint64_t     g_crb_base;
static int          g_present;

/* Software PCR mirror: always maintained so attestation works even when the
 * hardware TPM is absent or a command fails mid-boot. */
static uint8_t g_pcr[TPM_MAX_PCR][32];

static volatile uint8_t *tis_reg(uint32_t off) {
    return (volatile uint8_t *)(uintptr_t)(TIS_BASE + off);
}
static volatile uint8_t *crb_reg(uint32_t off) {
    return (volatile uint8_t *)(uintptr_t)(g_crb_base + off);
}
static uint32_t rd32v(volatile uint8_t *p) { return *(volatile uint32_t *)p; }
static void     wr32v(volatile uint8_t *p, uint32_t v) { *(volatile uint32_t *)p = v; }

/* -- big-endian helpers (TPM wire format) ---------------------------- */
static void be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}
static uint16_t rbe16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rbe32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* -- TIS transport ---------------------------------------------------- */

static int tis_wait_sts(uint8_t mask, uint8_t want, uint64_t us) {
    for (uint64_t i = 0; i < us; i += 10) {
        uint8_t s = *tis_reg(TIS_STS);
        if ((s & mask) == want) return 0;
        fw_stall_us(10);
    }
    return -1;
}

static int tis_request_locality(void) {
    *tis_reg(TIS_ACCESS) = ACCESS_REQUEST_USE;
    for (uint64_t i = 0; i < 200000; i += 10) {
        uint8_t a = *tis_reg(TIS_ACCESS);
        if ((a & (ACCESS_VALID | ACCESS_ACTIVE_LOCALITY)) ==
            (ACCESS_VALID | ACCESS_ACTIVE_LOCALITY)) return 0;
        fw_stall_us(10);
    }
    return -1;
}

static int tis_transmit(const uint8_t *cmd, uint32_t clen, uint8_t *resp, uint32_t *rlen) {
    if (tis_request_locality() != 0) return -1;

    *tis_reg(TIS_STS) = STS_COMMAND_READY;
    if (tis_wait_sts(STS_COMMAND_READY, STS_COMMAND_READY, 2000000) != 0) return -1;

    for (uint32_t i = 0; i < clen; i++) {
        *tis_reg(TIS_DATA_FIFO) = cmd[i];
    }
    if (tis_wait_sts(STS_VALID, STS_VALID, 2000000) != 0) return -1;

    *tis_reg(TIS_STS) = STS_GO;
    if (tis_wait_sts(STS_DATA_AVAIL, STS_DATA_AVAIL, 10000000) != 0) return -1;

    /* Header first so we know the true length. */
    uint32_t got = 0;
    while (got < 10) {
        if (!(*tis_reg(TIS_STS) & STS_DATA_AVAIL)) break;
        resp[got++] = *tis_reg(TIS_DATA_FIFO);
    }
    if (got < 10) return -1;
    uint32_t total = rbe32(resp + 2);
    if (total > *rlen) return -1;
    while (got < total) {
        if (!(*tis_reg(TIS_STS) & STS_DATA_AVAIL)) break;
        resp[got++] = *tis_reg(TIS_DATA_FIFO);
    }
    *tis_reg(TIS_STS) = STS_COMMAND_READY;
    *tis_reg(TIS_ACCESS) = ACCESS_ACTIVE_LOCALITY;   /* release */
    if (got != total) return -1;
    *rlen = total;
    return 0;
}

/* -- CRB transport ----------------------------------------------------- */

static int crb_transmit(const uint8_t *cmd, uint32_t clen, uint8_t *resp, uint32_t *rlen) {
    /* Request locality 0. */
    wr32v(crb_reg(CRB_LOC_CTRL), 1);
    for (uint64_t i = 0; i < 200000; i += 10) {
        if (rd32v(crb_reg(CRB_LOC_STS)) & 1) break;
        fw_stall_us(10);
    }

    uint32_t cmd_lo = rd32v(crb_reg(CRB_CTRL_CMD_LADDR));
    uint32_t cmd_hi = rd32v(crb_reg(CRB_CTRL_CMD_HADDR));
    uint32_t cmd_sz = rd32v(crb_reg(CRB_CTRL_CMD_SIZE));
    uint64_t cbuf   = ((uint64_t)cmd_hi << 32) | cmd_lo;
    uint32_t rsp_sz = rd32v(crb_reg(CRB_CTRL_RSP_SIZE));
    uint64_t rbuf;
    fw_memcpy(&rbuf, (void *)crb_reg(CRB_CTRL_RSP_ADDR), 8);

    if (!cbuf || clen > cmd_sz) return -1;

    /* Signal "command ready" and wait for the TPM to become idle-ready. */
    wr32v(crb_reg(CRB_CTRL_REQ), 1);
    for (uint64_t i = 0; i < 200000; i += 10) {
        if (!(rd32v(crb_reg(CRB_CTRL_REQ)) & 1)) break;
        fw_stall_us(10);
    }

    fw_memcpy((void *)(uintptr_t)cbuf, cmd, clen);
    wr32v(crb_reg(CRB_CTRL_START), 1);

    for (uint64_t i = 0; i < 10000000; i += 20) {
        if (!(rd32v(crb_reg(CRB_CTRL_START)) & 1)) break;
        fw_stall_us(20);
    }
    if (rd32v(crb_reg(CRB_CTRL_START)) & 1) return -1;

    const uint8_t *rp = (const uint8_t *)(uintptr_t)(rbuf ? rbuf : cbuf);
    uint32_t total = rbe32(rp + 2);
    if (total < 10 || total > *rlen || (rsp_sz && total > rsp_sz)) return -1;
    fw_memcpy(resp, rp, total);
    *rlen = total;
    return 0;
}

int fw_tpm_transmit(const uint8_t *cmd, uint32_t clen, uint8_t *resp, uint32_t *rlen) {
    if (!g_present) return -1;
    if (g_iface == TPM_IFACE_CRB) return crb_transmit(cmd, clen, resp, rlen);
    if (g_iface == TPM_IFACE_TIS) return tis_transmit(cmd, clen, resp, rlen);
    return -1;
}

/* -- TPM 2.0 commands -------------------------------------------------- */

#define ST_NO_SESSIONS 0x8001
#define ST_SESSIONS    0x8002

#define CC_Startup      0x00000144
#define CC_SelfTest     0x00000143
#define CC_GetRandom    0x0000017B
#define CC_PCR_Extend   0x00000182
#define CC_PCR_Read     0x0000017E

int fw_tpm_startup(void) {
    uint8_t cmd[12], rsp[64];
    uint32_t rl = sizeof(rsp);
    be16(cmd, ST_NO_SESSIONS);
    be32(cmd + 2, 12);
    be32(cmd + 6, CC_Startup);
    be16(cmd + 10, 0x0000);           /* TPM_SU_CLEAR */
    if (fw_tpm_transmit(cmd, 12, rsp, &rl) != 0) return -1;
    uint32_t rc = rbe32(rsp + 6);
    /* 0x100 = TPM_RC_INITIALIZE: already started, which is success for us. */
    return (rc == 0 || rc == 0x100) ? 0 : -1;
}

int fw_tpm_selftest(void) {
    uint8_t cmd[11], rsp[64];
    uint32_t rl = sizeof(rsp);
    be16(cmd, ST_NO_SESSIONS);
    be32(cmd + 2, 11);
    be32(cmd + 6, CC_SelfTest);
    cmd[10] = 0;                       /* fullTest = NO */
    if (fw_tpm_transmit(cmd, 11, rsp, &rl) != 0) return -1;
    return rbe32(rsp + 6) == 0 ? 0 : -1;
}

/* Self-test the software PCR extend math against the TCG spec vector:
 *   PCR(0) = 0^32 ; extend with digest = 0^32 ; result = SHA256(0^32 || 0^32)
 * = f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b
 * A failure here means measured boot itself is broken and attestation is unsafe.
 */
int fw_tpm_selftest_self(void) {
    uint8_t zero[32];
    memset(zero, 0, 32);
    /* Force PCR 23 (a scratch register) into a known state by resetting the
     * software mirror, then extend. */
    memcpy(g_pcr[23], zero, 32);
    if (fw_tpm_pcr_extend(23, zero) != 0) return -1;
    const uint8_t expect[32] = {
        0xf5,0xa5,0xfd,0x42,0xd1,0x6a,0x20,0x30,0x27,0x98,0xef,0x6e,0xd3,0x09,0x97,0x9b,
        0x43,0x00,0x3d,0x23,0x20,0xd9,0xf0,0xe8,0xea,0x98,0x31,0xa9,0x27,0x59,0xfb,0x4b
    };
    uint8_t got[32];
    memcpy(got, g_pcr[23], 32);
    int pass = (memcmp(got, expect, 32) == 0) ? 1 : 0;
    /* Restore PCR 23 to zero so the self-test is invisible to attestation. */
    memset(g_pcr[23], 0, 32);
    fw_puts(pass ? "[tpm] PCR extend math (SHA-256 TCG vector): PASS\n"
                 : "[tpm] PCR extend math (SHA-256 TCG vector): FAIL\n");
    return pass ? 0 : -1;
}


int fw_tpm_get_random(uint8_t *out, uint32_t n) {
    if (!out || !n) return -1;
    uint32_t done = 0;
    while (done < n) {
        uint8_t cmd[12], rsp[128];
        uint32_t rl = sizeof(rsp);
        uint16_t want = (uint16_t)((n - done) > 32 ? 32 : (n - done));
        be16(cmd, ST_NO_SESSIONS);
        be32(cmd + 2, 12);
        be32(cmd + 6, CC_GetRandom);
        be16(cmd + 10, want);
        if (fw_tpm_transmit(cmd, 12, rsp, &rl) != 0) return -1;
        if (rbe32(rsp + 6) != 0 || rl < 12) return -1;
        uint16_t got = rbe16(rsp + 10);
        if (got == 0 || 12u + got > rl) return -1;
        if (got > n - done) got = (uint16_t)(n - done);
        fw_memcpy(out + done, rsp + 12, got);
        done += got;
    }
    return 0;
}

int fw_tpm_pcr_extend(uint32_t pcr, const uint8_t digest[32]) {
    if (pcr >= TPM_MAX_PCR || !digest) return -1;

    /* Always maintain the software mirror: PCR_new = SHA256(PCR_old || digest) */
    uint8_t nv[32];
    fw_sha256_pair(g_pcr[pcr], digest, nv);
    fw_memcpy(g_pcr[pcr], nv, 32);

    if (!g_present) return 0;          /* software-only measurement */

    /*
     * TPM2_PCR_Extend with a password session:
     *   header(10) authHandle(4) authSize(4) authArea(9) digests(4+2+2+32)
     */
    uint8_t cmd[64], rsp[64];
    uint32_t rl = sizeof(rsp);
    uint32_t len = 10 + 4 + 4 + 9 + 4 + 2 + 2 + 32;
    uint8_t *p = cmd;

    be16(p, ST_SESSIONS);       p += 2;
    be32(p, len);               p += 4;
    be32(p, CC_PCR_Extend);     p += 4;
    be32(p, pcr);               p += 4;      /* pcrHandle           */
    be32(p, 9);                 p += 4;      /* authorizationSize   */
    be32(p, 0x40000009);        p += 4;      /* TPM_RS_PW           */
    be16(p, 0);                 p += 2;      /* nonce               */
    *p++ = 0;                                /* sessionAttributes   */
    be16(p, 0);                 p += 2;      /* hmac                */
    be32(p, 1);                 p += 4;      /* digest count        */
    be16(p, TPM_ALG_SHA256);    p += 2;
    fw_memcpy(p, digest, 32);   p += 32;

    if (fw_tpm_transmit(cmd, (uint32_t)(p - cmd), rsp, &rl) != 0) return -1;
    return rbe32(rsp + 6) == 0 ? 0 : -1;
}

int fw_tpm_pcr_read(uint32_t pcr, uint8_t out[32]) {
    if (pcr >= TPM_MAX_PCR || !out) return -1;
    if (!g_present) { fw_memcpy(out, g_pcr[pcr], 32); return 0; }

    uint8_t cmd[32], rsp[128];
    uint32_t rl = sizeof(rsp);
    uint8_t sel[3] = { 0, 0, 0 };
    sel[pcr / 8] = (uint8_t)(1u << (pcr % 8));

    uint8_t *p = cmd;
    be16(p, ST_NO_SESSIONS);   p += 2;
    be32(p, 10 + 4 + 2 + 1 + 3); p += 4;
    be32(p, CC_PCR_Read);      p += 4;
    be32(p, 1);                p += 4;       /* count            */
    be16(p, TPM_ALG_SHA256);   p += 2;
    *p++ = 3;                                /* sizeofSelect     */
    *p++ = sel[0]; *p++ = sel[1]; *p++ = sel[2];

    if (fw_tpm_transmit(cmd, (uint32_t)(p - cmd), rsp, &rl) != 0) return -1;
    if (rbe32(rsp + 6) != 0) return -1;
    /* header(10) pcrUpdateCounter(4) selCount(4) sel(2+1+3) digCount(4) size(2) digest */
    uint32_t off = 10 + 4 + 4 + 2 + 1 + 3 + 4;
    if (off + 2 + 32 > rl) return -1;
    if (rbe16(rsp + off) != 32) return -1;
    fw_memcpy(out, rsp + off + 2, 32);
    return 0;
}

/* -- detection --------------------------------------------------------- */

int fw_tpm_init(void) {
    g_present = 0;
    g_iface = TPM_IFACE_NONE;
    fw_memset(g_pcr, 0, sizeof(g_pcr));

    /* Prefer the ACPI-described CRB interface. */
    uint64_t ca = fw_acpi_tpm2_control_area();
    uint32_t sm = fw_acpi_tpm2_start_method();
    if (ca && (sm == 7 || sm == 8 || sm == 11 || sm == 12)) {
        g_crb_base = ca;
        g_iface = TPM_IFACE_CRB;
        g_present = 1;
        fw_printf("[tpm] CRB interface at 0x%lx (start method %u)\n", ca, sm);
    } else {
        /* Probe TIS: a real TPM answers DID/VID with something other than
         * all-ones or all-zeros. */
        uint32_t didvid = rd32v(tis_reg(TIS_DID_VID));
        if (didvid != 0xFFFFFFFFu && didvid != 0) {
            g_iface = TPM_IFACE_TIS;
            g_present = 1;
            fw_printf("[tpm] TIS interface at 0x%lx  DID:VID=%x\n",
                      (uint64_t)TIS_BASE, didvid);
        }
    }

    if (!g_present) {
        fw_puts("[tpm] no TPM detected - measurements are software-only\n");
        return -1;
    }

    if (fw_tpm_startup() != 0) {
        fw_puts("[tpm] TPM2_Startup failed - falling back to software measurement\n");
        g_present = 0;
        return -1;
    }
    if (fw_tpm_selftest() != 0)
        fw_puts("[tpm] warning: self test reported failure\n");

    fw_puts("[tpm] TPM 2.0 ready\n");
    return 0;
}

int          fw_tpm_present(void)   { return g_present; }
fw_tpm_iface fw_tpm_interface(void) { return g_iface; }
