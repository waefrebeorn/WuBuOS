/*
 * wubu_nt_sd_test.c -- tests for the NT security descriptor access check.
 *
 * The point of this module is that an access mask MEANS something: the same
 * GENERIC_ALL expands to different concrete rights on a File than on a Key,
 * a deny ACE beats an allow, and NO_WRITE_UP refuses a write the DACL
 * permitted. These tests pin each of those, because a rights system that
 * returns SUCCESS unconditionally passes every "can it open" test while
 * enforcing nothing.
 *
 * Also pins the malformed-input paths, which must FAIL CLOSED: a truncated or
 * overflowing ACE walk must not grant access and must not run off the buffer.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "wubu_nt_sd.h"

static int passed = 0, failed = 0;
#define CHECK(c, m) do { \
    if (c) { passed++; printf("  [PASS] %s\n", (m)); } \
    else  { failed++; printf("  ❌ %s\n", (m)); } } while (0)

/* Build a self-relative NT security descriptor with one DACL in the real wire
 * layout: header (20 bytes) + ACL (8 + 12*ace_count) + a mandatory label.
 * Layout: [SD 20][ACL ...][SACL/label 8]. */
static uint8_t g_sd[512];

static size_t build_sd(uint8_t *buf, const uint8_t *aces, uint16_t ace_count,
                       uint32_t dacl_granted_mask_unused) {
    (void)dacl_granted_mask_unused;
    memset(buf, 0, 512);
    uint16_t acl_size = (uint16_t)(8 + 12 * ace_count);
    size_t dacl_off = 20;               /* right after the SD header */
    size_t acl_at   = dacl_off;

    /* SECURITY_DESCRIPTOR: Revision=1, Sbz1, Control, then 4 offsets. */
    buf[0] = 1; buf[1] = 0;
    uint16_t control = WUBU_SE_DACL_PRESENT | WUBU_SE_SELF_RELATIVE;
    buf[2] = (uint8_t)(control & 0xFF);
    buf[3] = (uint8_t)(control >> 8);
    /* Owner/Group/Sacl offsets (unused, zero). */
    /* DACL offset at byte 8. */
    buf[8]  = (uint8_t)(acl_at & 0xFF);
    buf[9]  = (uint8_t)((acl_at >> 8) & 0xFF);
    buf[10] = (uint8_t)((acl_at >> 16) & 0xFF);
    buf[11] = (uint8_t)((acl_at >> 24) & 0xFF);
    /* Sacl offset at byte 16 (the mandatory label in NT, but we keep it simple
     * and set the MIC directly on the view). */
    buf[16] = 0; buf[17] = 0; buf[18] = 0; buf[19] = 0;

    /* ACL header: Revision=2, Sbz1, AclSize, AceCount, Sbz2. */
    uint8_t *acl = buf + acl_at;
    acl[0] = 2; acl[1] = 0;
    acl[2] = (uint8_t)(acl_size & 0xFF);
    acl[3] = (uint8_t)(acl_size >> 8);
    acl[4] = (uint8_t)(ace_count & 0xFF);
    acl[5] = (uint8_t)(ace_count >> 8);
    acl[6] = 0; acl[7] = 0;
    if (aces) memcpy(acl + 8, aces, 12 * (size_t)ace_count);
    return (size_t)acl_at + acl_size;
}

/* One ACE in the layout wubu_nt_dacl_decode reads:
 *   [0] AceType  [1] AceFlags  [2..3] AceSize(=12)  [4..7] SID  [8..11] rights */
static void put_ace(uint8_t *dst, uint8_t ace_type, uint32_t sid, uint32_t rights) {
    dst[0] = ace_type; dst[1] = 0;
    dst[2] = 12; dst[3] = 0;
    dst[4] = (uint8_t)(sid & 0xFF);
    dst[5] = (uint8_t)((sid >> 8) & 0xFF);
    dst[6] = (uint8_t)((sid >> 16) & 0xFF);
    dst[7] = (uint8_t)((sid >> 24) & 0xFF);
    dst[8]  = (uint8_t)(rights & 0xFF);
    dst[9]  = (uint8_t)((rights >> 8) & 0xFF);
    dst[10] = (uint8_t)((rights >> 16) & 0xFF);
    dst[11] = (uint8_t)((rights >> 24) & 0xFF);
}

int main(void) {
    printf("=== wubu_nt_sd (NT security descriptor / access check) ===\n");

    /* ---- 1. Generic expansion: the SAME generic bits mean different things
     * per object type. This is the property a blanket pass cannot have. ---- */
    {
        uint32_t fa, ka, fr, kr;
        wubu_nt_generic_mapping(NT_OBJECT_TYPE_FILE, &fr, 0, 0, &fa);
        wubu_nt_generic_mapping(NT_OBJECT_TYPE_KEY,   &kr, 0, 0, &ka);
        CHECK(fa == 0x001F01FFu, "FILE_ALL_ACCESS = 0x001F01FF");
        CHECK(ka == 0x000F003Fu, "KEY_ALL_ACCESS = 0x000F003F");
        CHECK(fa != ka, "GENERIC_ALL differs between File and Key");

        uint32_t fexp = wubu_nt_expand_generic(NT_OBJECT_TYPE_FILE, WUBU_GENERIC_ALL);
        uint32_t kexp = wubu_nt_expand_generic(NT_OBJECT_TYPE_KEY,   WUBU_GENERIC_ALL);
        CHECK(fexp == 0x001F01FFu, "GENERIC_ALL on a File expands to FILE_ALL_ACCESS");
        CHECK(kexp == 0x000F003Fu, "GENERIC_ALL on a Key expands to KEY_ALL_ACCESS");
        CHECK(fexp != kexp, "the same GENERIC_ALL request yields different rights");

        /* GENERIC_READ on a File must include FILE_READ_DATA, on a Key the
         * query-value right -- different concrete rights, same request. */
        uint32_t frd = wubu_nt_expand_generic(NT_OBJECT_TYPE_FILE, WUBU_GENERIC_READ);
        uint32_t krd = wubu_nt_expand_generic(NT_OBJECT_TYPE_KEY,   WUBU_GENERIC_READ);
        CHECK((frd & 0x00000001u) != 0, "GENERIC_READ on File includes FILE_READ_DATA");
        CHECK((frd & 0x00000080u) != 0 && (krd & 0x00000080u) == 0,
              "GENERIC_READ: only the File mapping carries FILE_READ_ATTRIBUTES");
    }

    /* ---- 2. DACL: a grant that covers the request passes ---- */
    {
        uint8_t aces[12];
        put_ace(aces, 0x00, /*sid*/ 1001, /*rights*/ 0x00120089u); /* allow owner */
        size_t n = build_sd(g_sd, aces, 1, 0);

        wubu_nt_sd_view_t sd;
        CHECK(wubu_nt_sd_read(g_sd, &sd), "self-relative SD parses");
        CHECK(sd.dacl_present, "SD reports DACL present");
        CHECK(sd.dacl_view.ace_count == 1, "one ACE decoded");
        CHECK(sd.dacl_view.aces[0].sid == 1001, "ACE SID decoded");
        CHECK(sd.dacl_view.aces[0].rights == 0x00120089u, "ACE rights decoded");
        (void)n;

        wubu_nt_sidset_t subj;
        wubu_nt_sidset_reset(&subj);
        subj.owner_sid = 1001;
        subj.mic = 0x2000;   /* medium integrity */
        sd.no_write_up = false;

        wubu_nt_access_result_t r =
            wubu_nt_access_check(&sd, &subj, NT_OBJECT_TYPE_FILE,
                                 WUBU_GENERIC_READ);
        CHECK(r.status == WUBU_NT_STATUS_SUCCESS, "granted request -> SUCCESS");
        CHECK(r.granted == 0x00120089u,
              "granted mask equals the expanded request (0x00120089)");
        CHECK(r.missing == 0, "nothing missing on a full grant");
    }

    /* ---- 3. DACL: a short grant denies AND reports what's missing ---- */
    {
        uint8_t aces[12];
        /* Grant only read. Ask for read|write. */
        put_ace(aces, 0x00, 1001, 0x00020089u);
        build_sd(g_sd, aces, 1, 0);

        wubu_nt_sd_view_t sd;
        wubu_nt_sd_read(g_sd, &sd);
        wubu_nt_sidset_t subj;
        wubu_nt_sidset_reset(&subj);
        subj.owner_sid = 1001;
        sd.no_write_up = false;

        wubu_nt_access_result_t r =
            wubu_nt_access_check(&sd, &subj, NT_OBJECT_TYPE_FILE,
                                 WUBU_GENERIC_READ | WUBU_GENERIC_WRITE);
        CHECK(r.status == WUBU_NT_STATUS_ACCESS_DENIED,
              "request exceeding the grant -> ACCESS_DENIED");
        CHECK(r.missing != 0, "the write right is reported missing");
        CHECK((r.missing & 0x00000116u) != 0,
              "missing mask names the file write rights");
    }

    /* ---- 4. Deny ACE beats Allow -- NT's canonical ordering ---- */
    {
        uint8_t aces[24];
        put_ace(aces + 0,  0x00, 1001, 0x00120089u);  /* allow owner  */
        put_ace(aces + 12, 0x01, 1001, 0x00000002u);  /* deny  owner WRITE_DATA */
        build_sd(g_sd, aces, 2, 0);

        wubu_nt_sd_view_t sd;
        CHECK(wubu_nt_sd_read(g_sd, &sd), "two-ACE SD parses");
        CHECK(sd.dacl_view.ace_count == 2, "both ACEs decoded");

        wubu_nt_sidset_t subj;
        wubu_nt_sidset_reset(&subj);
        subj.owner_sid = 1001;
        sd.no_write_up = false;

        /* Read alone: allowed. This is the case a global denied_any flag got
         * WRONG -- the deny names FILE_WRITE_DATA, which was not requested. */
        wubu_nt_access_result_t rr =
            wubu_nt_access_check(&sd, &subj, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_READ);
        CHECK(rr.status == WUBU_NT_STATUS_SUCCESS, "read allowed despite a write deny");

        /* Read|write: the deny must beat the allow. */
        wubu_nt_access_result_t rw =
            wubu_nt_access_check(&sd, &subj, NT_OBJECT_TYPE_FILE,
                                 WUBU_GENERIC_READ | WUBU_GENERIC_WRITE);
        CHECK(rw.status == WUBU_NT_STATUS_ACCESS_DENIED,
              "deny ACE beats the allow ACE");
    }

    /* ---- 5. Group ACEs ---- */
    {
        uint8_t aces[12];
        put_ace(aces, 0x11, /*sid*/ 2002, 0x00120089u); /* allow group (callback) */
        build_sd(g_sd, aces, 1, 0);

        wubu_nt_sd_view_t sd;
        wubu_nt_sd_read(g_sd, &sd);
        wubu_nt_sidset_t in, out;
        wubu_nt_sidset_reset(&in);  in.owner_sid = 1; wubu_nt_sidset_add(&in, 2002);
        wubu_nt_sidset_reset(&out); out.owner_sid = 1; wubu_nt_sidset_add(&out, 9999);
        sd.no_write_up = false;

        CHECK(wubu_nt_access_check(&sd, &in,  NT_OBJECT_TYPE_FILE, WUBU_GENERIC_READ)
                  .status == WUBU_NT_STATUS_SUCCESS,
              "group member is granted by a group ACE");
        CHECK(wubu_nt_access_check(&sd, &out, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_READ)
                  .status == WUBU_NT_STATUS_ACCESS_DENIED,
              "non-member is denied by the same DACL");
    }

    /* ---- 6. Mandatory integrity: NO_WRITE_UP ---- */
    {
        uint8_t aces[12];
        put_ace(aces, 0x00, 1001, 0x001F01FFu);  /* allow the owner everything */
        build_sd(g_sd, aces, 1, 0);

        wubu_nt_sd_view_t sd;
        wubu_nt_sd_read(g_sd, &sd);
        sd.label_mic   = 0x3000;      /* HIGH integrity object */
        sd.no_write_up = true;

        wubu_nt_sidset_t low, high;
        wubu_nt_sidset_reset(&low);  low.owner_sid = 1001; low.mic  = 0x1000; /* LOW */
        wubu_nt_sidset_reset(&high); high.owner_sid = 1001; high.mic = 0x4000; /* SYSTEM */

        /* DACL allows, but the MIC must still refuse the low writer. */
        wubu_nt_access_result_t rl =
            wubu_nt_access_check(&sd, &low, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_WRITE);
        CHECK(rl.status == WUBU_NT_STATUS_ACCESS_DENIED,
              "NO_WRITE_UP blocks a low-integrity write the DACL allowed");
        CHECK(rl.mic_failed, "the refusal is attributed to the MIC, not the DACL");
        CHECK(rl.granted != 0, "the DACL did grant; only the MIC refused");

        /* Same object, a high-integrity writer: allowed. */
        wubu_nt_access_result_t rh =
            wubu_nt_access_check(&sd, &high, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_WRITE);
        CHECK(rh.status == WUBU_NT_STATUS_SUCCESS,
              "high-integrity writer is allowed by the same MIC");

        /* The MIC does not gate pure reads. */
        wubu_nt_access_result_t rread =
            wubu_nt_access_check(&sd, &low, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_READ);
        CHECK(rread.status == WUBU_NT_STATUS_SUCCESS,
              "NO_WRITE_UP does not block reads");
    }

    /* ---- 7. No DACL present => full access (NT semantics) ---- */
    {
        uint8_t sdbuf[64];
        memset(sdbuf, 0, sizeof(sdbuf));
        sdbuf[0] = 1;
        uint16_t control = WUBU_SE_SELF_RELATIVE;   /* no SE_DACL_PRESENT */
        sdbuf[2] = (uint8_t)(control & 0xFF);
        sdbuf[3] = (uint8_t)(control >> 8);

        wubu_nt_sd_view_t sd;
        CHECK(wubu_nt_sd_read(sdbuf, &sd), "null-DACL SD parses");
        CHECK(!sd.dacl_present, "SD reports no DACL");
        wubu_nt_sidset_t subj;
        wubu_nt_sidset_reset(&subj);
        subj.owner_sid = 1; subj.mic = 0x1000;
        wubu_nt_access_result_t r =
            wubu_nt_access_check(&sd, &subj, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_ALL);
        CHECK(r.status == WUBU_NT_STATUS_SUCCESS,
              "absent DACL grants full access (NT semantics)");
    }

    /* ---- 8. Malformed input must fail CLOSED, never read past the buffer ---- */
    {
        wubu_nt_sd_view_t sd;
        CHECK(!wubu_nt_sd_read(NULL, &sd), "NULL SD is rejected");
        CHECK(!wubu_nt_sd_read(g_sd, NULL), "NULL out is rejected");

        uint8_t bad[64];
        memset(bad, 0, sizeof(bad));
        bad[0] = 0x99;   /* wrong Revision */
        CHECK(!wubu_nt_sd_read(bad, &sd), "wrong SD revision is rejected");

        /* A DACL offset past our max must be refused, not dereferenced. */
        memset(bad, 0, sizeof(bad));
        bad[0] = 1;
        uint16_t c = WUBU_SE_DACL_PRESENT | WUBU_SE_SELF_RELATIVE;
        bad[2] = (uint8_t)(c & 0xFF); bad[3] = (uint8_t)(c >> 8);
        bad[8] = 0xFF; bad[9] = 0xFF; bad[10] = 0x00; bad[11] = 0x00;  /* 0xFFFF */
        CHECK(!wubu_nt_sd_read(bad, &sd), "oversized DACL offset is rejected");

        /* An ACE claiming a size past the ACL must end the walk, not overflow. */
        uint8_t aces[12];
        put_ace(aces, 0x00, 1001, 0x00120089u);
        aces[2] = 0xFF; aces[3] = 0xFF;   /* AceSize = 65535, far past the ACL */
        build_sd(g_sd, aces, 1, 0);
        CHECK(wubu_nt_sd_read(g_sd, &sd), "SD with an oversized ACE still parses");
        CHECK(sd.dacl_view.ace_count == 0,
              "an ACE overflowing the ACL is dropped (fails closed)");

        /* An ACE smaller than a header must also be dropped. */
        put_ace(aces, 0x00, 1001, 0x00120089u);
        aces[2] = 2; aces[3] = 0;        /* AceSize = 2 */
        build_sd(g_sd, aces, 1, 0);
        wubu_nt_sd_read(g_sd, &sd);
        CHECK(sd.dacl_view.ace_count == 0, "a too-small ACE is dropped (fails closed)");
    }

    /* ---- 9. NULL subject fails closed ---- */
    {
        uint8_t aces[12];
        put_ace(aces, 0x00, 1001, 0x001F01FFu);
        build_sd(g_sd, aces, 1, 0);
        wubu_nt_sd_view_t sd;
        wubu_nt_sd_read(g_sd, &sd);
        sd.no_write_up = false;
        wubu_nt_access_result_t r =
            wubu_nt_access_check(&sd, NULL, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_ALL);
        CHECK(r.status == WUBU_NT_STATUS_ACCESS_DENIED, "NULL subject -> DENIED");
        r = wubu_nt_access_check(NULL, NULL, NT_OBJECT_TYPE_FILE, WUBU_GENERIC_ALL);
        CHECK(r.status == WUBU_NT_STATUS_ACCESS_DENIED, "NULL SD -> DENIED");
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
