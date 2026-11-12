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
make spellcheck                    # the spell book
```
