# 00-philosophy — Anti-Cheat by Attestation

*Addendum entry. Human-written. Last updated 2026-08-02.*

## The virtue

Cheating is a **proof problem, not an arms race**. Classic anti-cheat
(Vanguard/EAC/BattlEye) fights invisibility: usermode can't see above itself,
so AC escalates to a ring-0 driver, cheats escalate to hypervisors and DMA —
an endless rootkit-shaped escalation (ARES'24 taxonomy).

WuBuOS inverts the model — the core is measured and everything is visible:

1. **Ring-0-only = no user space to hide in.** The kernel sees everything
   because everything IS the kernel.
2. **Measured boot chain = the root of trust.** WuBuFW → PCR4 → kernel digest
   → attestation table → promotion gate (verified live on metal:
   `attest_valid=1`).
3. **Runtime integrity, not just boot.** Boot attestation proves the image;
   the anti-cheat module (`wubu_anticheat`) hashes code pages at runtime and
   extends PCRs so the server's attestation covers the CURRENT state.
4. **Per-overlay attestation scopes.** Each .wubu container carries its own
   digest; host PCR + container digest = a session proof.
5. **The below-OS plane is covered by hardware:** IOMMU/VT-d DMA remapping +
   device isolation in the measured firmware kills PCIe-DMA cheats.

## The watchman problem

Who watches the watcher? The Colonel runs the anti-cheat AND the games —
the answer is the measured boot + runtime PCR chain: the state is provable
to an external verifier (the game server), which is the only party that
needs to trust it.
