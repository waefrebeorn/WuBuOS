/*
 * wubu_boot_attest_edr.c -- put the measured boot in the EDR event stream.
 *
 * C11, minimal includes.  The report shape lives in the boot attestation
 * spell; the disclosure surface lives in wubu_edr; this module only joins
 * them and builds the report from the kernel's measured boot.
 */
#include "wubu_boot_attest_edr.h"

#include <stdio.h>
#include <string.h>

#include "../kernel/wubu_attest.h"

int wubu_boot_attest_edr_log(const wubu_boot_attest_t *report)
{
    if (!report) return -1;

    /* Re-encode just to be sure we are logging something sealed: encode()
     * refuses an unsealed report, which makes this a free precondition. */
    uint8_t frame[WUBU_BOOT_ATTEST_FRAME_SZ];
    if (wubu_boot_attest_encode(report, frame, sizeof(frame)) <= 0) return -1;

    char detail[256];
    snprintf(detail, sizeof(detail),
             "boot#%u pcr=%02x%02x%02x%02x%02x%02x%02x%02x"
             " kernel=%02x%02x%02x%02x%02x%02x%02x%02x stages=%u sealed=%llu",
             wubu_boot_attest_get_boot_counter(report),
             wubu_boot_attest_get_pcr_digest(report)[0],
             wubu_boot_attest_get_pcr_digest(report)[1],
             wubu_boot_attest_get_pcr_digest(report)[2],
             wubu_boot_attest_get_pcr_digest(report)[3],
             wubu_boot_attest_get_pcr_digest(report)[4],
             wubu_boot_attest_get_pcr_digest(report)[5],
             wubu_boot_attest_get_pcr_digest(report)[6],
             wubu_boot_attest_get_pcr_digest(report)[7],
             wubu_boot_attest_get_kernel_digest(report)[0],
             wubu_boot_attest_get_kernel_digest(report)[1],
             wubu_boot_attest_get_kernel_digest(report)[2],
             wubu_boot_attest_get_kernel_digest(report)[3],
             wubu_boot_attest_get_kernel_digest(report)[4],
             wubu_boot_attest_get_kernel_digest(report)[5],
             wubu_boot_attest_get_kernel_digest(report)[6],
             wubu_boot_attest_get_kernel_digest(report)[7],
             (unsigned)wubu_boot_attest_get_stage_count(report),
             (unsigned long long)wubu_boot_attest_get_sealed_at(report));

    /* u64a carries the boot counter so a reader can filter on it without
     * parsing the detail string. */
    return edr_log_event(EDR_EV_BOOT_ATTEST, 0, 0,
                         wubu_boot_attest_get_boot_counter(report),
                         wubu_boot_attest_get_sealed_at(report),
                         (uint32_t)wubu_boot_attest_get_stage_count(report),
                         detail);
}

int wubu_boot_attest_edr_boot(uint64_t now_unix,
                              const char *const *stages,
                              const uint32_t *starts, int stage_count,
                              const wubu_sdr_table_t *routes,
                              const wubu_sdr_addr_t *from,
                              const wubu_sdr_ticket_t *ticket,
                              wubu_boot_attest_tx tx, void *ctx)
{
    if (!wubu_attest_valid()) return -1;   /* no measurement, no attestation */
    if (stage_count < 0 || (stage_count > 0 && (!stages || !starts))) return -1;

    /* The PCR bank: 8 digests, each read out by the one owner of that state. */
    uint8_t pcrs[WUBU_BOOT_ATTEST_PCR_COUNT][WUBU_BOOT_ATTEST_SIG_SZ];
    for (unsigned i = 0; i < WUBU_BOOT_ATTEST_PCR_COUNT; i++)
        if (wubu_attest_pcr(i, pcrs[i]) != 0) return -1;

    uint8_t pcr_digest[WUBU_BOOT_ATTEST_SIG_SZ];
    if (wubu_boot_attest_pcr_digest(pcrs, pcr_digest) != 0) return -1;

    uint8_t kernel_digest[WUBU_BOOT_ATTEST_SIG_SZ];
    if (wubu_attest_kernel_digest(kernel_digest) != 0) return -1;

    wubu_boot_attest_t *report = wubu_boot_attest_create();
    if (!report) return -1;
    if (wubu_boot_attest_bind(report, wubu_attest_boot_counter(),
                              pcr_digest, kernel_digest) != 0) {
        wubu_boot_attest_destroy(report);
        return -1;
    }
    for (int i = 0; i < stage_count; i++) {
        if (wubu_boot_attest_stage(report, stages[i], starts[i]) != 0) {
            wubu_boot_attest_destroy(report);
            return -1;
        }
    }
    if (wubu_boot_attest_seal(report, now_unix) != 0) {
        wubu_boot_attest_destroy(report);
        return -1;
    }

    int logged = wubu_boot_attest_edr_log(report);

    int published = 0;
    if (routes && from && tx) {
        /* Never let a broadcast failure un-log the boot: the local record is
         * the one that cannot be re-run. */
        published = wubu_boot_attest_publish(report, routes, from, ticket,
                                             now_unix, tx, ctx);
        if (published < 0) published = 0;
    }

    wubu_boot_attest_destroy(report);
    return (logged == 0) ? 0 : -1;
}
