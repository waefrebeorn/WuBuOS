# Boot attestation over SDR

The firmware measures the boot — PCR0-7 plus the Secure Boot state, published
as an EFI Configuration Table (`src/firmware/fw_agi.c`) and consumed by the
kernel (`src/kernel/wubu_attest.c`). None of it leaves the machine.

The boot attestation spell is the half that does.

```
src/spells/networking/aggregation/bootattest/wubu_boot_attest.{c,h}
```

It turns the measured boot into a small sealed report: a fixed 84-byte header
(magic, version, flags, stage count, boot counter, seal time, the PCR digest,
the kernel digest), then one 24-byte record per boot stage (name, start, duration),
then a 32-byte signature over everything before it. A peer decodes it, recomputes
the signature, and compares the PCR digest against a known-good boot.

`src/runtime/wubu_boot_attest_relay.{c,h}` is the seam to the SDR relay
(`src/runtime/wubu_sdr*.c`): the report is offered to **every** route in the
relay table, through the relay's own per-packet decision, and handed to a
transmit callback. A peer that is not routable, not ready, or not ticketed gets
nothing — the report buys its way past none of the relay's rules.

## The boot appears in the EDR stream

`src/runtime/wubu_boot_attest_edr.{c,h}` joins the report to `wubu_edr`
(`src/runtime/edr/edr_core.c`), the endpoint's disclosure surface. The boot is
logged as `EDR_EV_BOOT_ATTEST` (27), so a machine reports what it booted — the
one thing that happened *before* any other telemetry existed. It carries a
summary in the detail string (boot counter, the first bytes of both digests,
stage count, seal time); the full frame goes to peers. It is OS/kernel
telemetry, so it passes the age gate that only agent actions must clear.

A boot with no firmware measurement returns -1 and claims nothing.

## What the signature is, and is not

The signature is `SHA-256(header || stage records)`. It proves the report was
not altered in transit and binds it to the firmware's PCR bank.

It is **not authentication**. The tree has no signing primitive: the firmware's
RSA is verify-only (`src/firmware/fw_rsa.h`) and the private key never leaves
the TPM. Anyone can mint a report. That is the same doctrine the swarm spell
states — identity is placement, not authentication. The transparency comes from
everyone being able to see everything, not from a trusted signer.

## Gates

```
make test_wubu_boot_attest         # the report: seal, encode, decode, fail closed
make test_wubu_boot_attest_relay   # the seam: every peer, and only the allowed ones
make test_wubu_boot_attest_edr    # the disclosure surface: the boot on the EDR ring
make spellcheck                    # the spell book
```
