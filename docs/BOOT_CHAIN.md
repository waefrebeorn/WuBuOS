# WuBuOS Boot Chain — the verified spine

> 2026-08-04. How WuBuOS goes from power-on to the AGI supervisor with its
> root of trust intact. This is the Body's most important property: the
> measured boot chain (firmware 28/28 conformance, real kernel boots).

```
Power-on
  └─► WuBuFW (src/firmware — UEFI from scratch, no EDK2)
        fw_* modules: PCI, NVMe, AHCI, XHCI, GOP, TPM, secureboot, sha256, ACPI
        └─ measures the kernel → PCR4 + AuthentiCode (TPM)
  └─► chainloader reads KERNEL.ELF off the ESP
        └─ SHA-256 → attestation handoff in low RAM
  └─► ExitBootServices
        └─► crt0 → kernel_main (src/kernel)
              └─► AGI supervisor (wubu_agi_kernel) with the root-of-trust gate
                   LIVE — verified: `make test_agi_metal` = PASS
```

## The stages

| Stage | Code | Role |
|---|---|---|
| **Firmware** | `src/firmware/fw_*.c` | UEFI platform init, TPM measurement, secure boot. Built as `wubufw.fd` |
| **Chainloader** | `src/firmware/` (chainloader) | loads `KERNEL.ELF`, hashes it, hands attestation data to the kernel |
| **Measured boot** | PCR4 + AuthentiCode | the kernel's integrity is fixed before it runs — the AGI's root of trust |
| **crt0 / kernel_main** | `src/kernel/crt0*`, `kernel_main` | static constructors, GDT/IDT, memory, then the supervisor |
| **AGI supervisor** | `src/kernel/wubu_agi_kernel.c` | ring-0 loop, attestation-gated; hosts the Live Colonel REPL |

## The AGI organs on metal

| Module | Role |
|---|---|
| `wubu_recovery` | 5+1 rollback (five slots + the Jesus state) — mistakes are safe |
| `wubu_agi_kernel` | the AGI supervisor (ring-0, attestation-gated) |
| `wubu_hive` | the hive port (the AGI's memory, kernel-side; same API as the Brain's reference impl) |
| `wubu_attest` | root-of-trust attestation |
| `wubu_psych` / `wubu_tutor` / `wubu_bonzi_study` | the HX human-model family |
| `wubu_verifier` | DA-2 fail-closed verification |

## Verify it

```bash
make test_agi_metal     # the measured-boot/AGI gate (PASS is the contract)
make test               # the full 138-target gate
qemu-test.sh            # boot the built image in QEMU
```

Cross-repo: the Brain's model file reaches the Body through the 9P namespace
(`/n/kv/`, `/n/models/`) — the Live Colonel loads it on metal (see
`WUBUOS_INTEGRATION.md`).
