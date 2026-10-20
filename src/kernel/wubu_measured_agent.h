/*
 * wubu_measured_agent.h -- agent self-integrity verification against the
 * measured-boot PCRs (AGIEDR #44).
 *
 * The AGI supervisor must prove its own integrity before it may modify
 * itself: the agent's state checkpoints chain into the kernel's runtime
 * PCR (which itself chains from the firmware's measured-boot PCR4 via
 * the loader handoff), and the promotion gate refuses self-modification
 * unless every link is live. A broken root of trust (no attestation)
 * means NO self-modification -- fail closed, always.
 *
 * Owner boundaries: wubu_attest.c owns the PCR state (firmware-measured
 * PCR4, the runtime PCR, ingest/valid/digest); THIS module owns the
 * AGENT policy on top of it: the baseline lock, the checkpoint chain,
 * and the promotion gate. wubu_agi_kernel.c consults
 * wubu_measured_agent_may_selfmodify() instead of poking attest state
 * directly.
 *
 * C11, freestanding-compatible (static state, no malloc).
 */
#ifndef WUBU_MEASURED_AGENT_H
#define WUBU_MEASURED_AGENT_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_MA_PCR_SZ     32
#define WUBU_MA_CHECKPOINTS 64

/* Lock the integrity baseline from the live measured-boot state:
 * requires a VALID firmware attestation (wubu_attest_valid) and snapshots
 * PCR4 + the current runtime PCR as the agent's chain origin. 0 ok;
 * -1 no root of trust (attestation invalid/absent -- fail closed);
 * -2 already initialized. After init, the agent MAY self-modify. */
int  wubu_measured_agent_init(void);

/* Tear down (tests + deliberate re-lock after a measured reboot). */
void wubu_measured_agent_reset(void);

/* Is the baseline locked and the root of trust live? */
int  wubu_measured_agent_ready(void);

/* The promotion gate: 1 = the agent may self-modify (baseline locked AND
 * the measured-boot attestation is still valid AND the checkpoint chain
 * verifies); 0 = refuse. This is the single gate the AGI supervisor
 * must consult. */
int  wubu_measured_agent_may_selfmodify(void);

/* Record a state checkpoint: hash(data) chains into the runtime PCR and
 * the checkpoint's expected digest is retained for verification. Returns
 * the checkpoint sequence number (>= 1), or 0 on bad args, -1 when the
 * baseline is not locked. */
int64_t wubu_measured_agent_checkpoint(const void *data, size_t len);

/* Verify the agent's own integrity RIGHT NOW: recompute the live
 * runtime PCR's digest and check it against the retained chain head.
 * 0 = intact; -1 = tampered (a checkpoint outside the chain, or the
 * runtime PCR moved without a checkpoint -- both mean someone extended
 * state we did not authorize); -2 = not initialized. */
int  wubu_measured_agent_verify(void);

/* Diagnostics: the locked PCR4 baseline and the checkpoint count. */
int  wubu_measured_agent_baseline(uint8_t out[WUBU_MA_PCR_SZ]);
int  wubu_measured_agent_checkpoint_count(void);

#endif /* WUBU_MEASURED_AGENT_H */
