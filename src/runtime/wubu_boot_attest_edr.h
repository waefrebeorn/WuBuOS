/*
 * wubu_boot_attest_edr.h -- put the measured boot in the EDR event stream.
 *
 * wubu_edr is the endpoint's disclosure surface: every process, file, network
 * and agent action lands on its ring, and /edr/alerts plus edr_recent_events()
 * read it back ("disclosure is the security model").  The boot was the one
 * thing that happened BEFORE any of that existed, so it never appeared -- a
 * machine could report everything it did after boot and nothing about what it
 * actually booted.
 *
 * This module closes that gap.  It builds the sealed boot report from the
 * kernel's firmware attestation, logs it as EDR_EV_BOOT_ATTEST (OS telemetry,
 * so it passes the age gate that only agent actions must clear), and offers it
 * to peers over the SDR relay.
 *
 * The event carries a SUMMARY: the full frame is binary and far larger than an
 * EDR detail string.  The bytes go to peers; the ring carries enough for a
 * human or a dashboard to see that the boot happened and what it measured.
 *
 * C11, minimal includes.
 */
#ifndef WUBU_BOOT_ATTEST_EDR_H
#define WUBU_BOOT_ATTEST_EDR_H

#include <stddef.h>
#include <stdint.h>

#include "wubu_edr.h"
#include "../spells/networking/aggregation/bootattest/wubu_boot_attest.h"
#include "wubu_boot_attest_relay.h"

/* Log a sealed boot report on the EDR ring as EDR_EV_BOOT_ATTEST.
 * Returns 0 on success, -1 if analytics is off, the queue is full, or the
 * arguments are bad. */
int wubu_boot_attest_edr_log(const wubu_boot_attest_t *report);

/* The whole boot-time sequence: build the report from the kernel's firmware
 * attestation (PCR0-7 + kernel digest + boot counter) and the stage list,
 * seal it, log it to EDR, and publish it to every peer the relay allows.
 *
 * `stages`/`starts` describe the boot in order; starts are monotonic
 * milliseconds.  `routes`/`from`/`ticket`/`tx` are the SDR side (see
 * wubu_boot_attest_relay.h); pass NULL for routes to skip the broadcast and
 * log locally only.
 *
 * Returns 0 on success.  -1 when there is NO firmware attestation -- a boot
 * without a measurement is not something to attest -- or when the arguments
 * are bad. */
int wubu_boot_attest_edr_boot(uint64_t now_unix,
                              const char *const *stages,
                              const uint32_t *starts, int stage_count,
                              const wubu_sdr_table_t *routes,
                              const wubu_sdr_addr_t *from,
                              const wubu_sdr_ticket_t *ticket,
                              wubu_boot_attest_tx tx, void *ctx);

#endif /* WUBU_BOOT_ATTEST_EDR_H */
