# 02-architecture — Boot Chain (measured)

```
QEMU q35 / real HW
  └─ WuBuFW (own UEFI firmware, no EDK2/OVMF)
       ├─ measurements: PCR0-7 + AuthentiCode digest
       └─ chainloads the kernel from the ESP (mkesp FAT32 layout)
            └─ kernel.elf (single PT_LOAD at 0x100000)
                 ├─ crt0: MPGCEW markers, page tables (PT_high @ +0x5000),
                 │        higher-half entry via ABSOLUTE jump to kernel_main
                 ├─ metal_main: serial, VBE, heap, tasking, PIT/LAPIC tick,
                 │              AGI supervisor, Bonzi + console + agent tasks
                 └─ attestation table (fw_agi_attest.h wire format)
                      └─ gates AGI promotion (DA-3: loop + verifier + live
                         attestation with valid digest)
```

## Key facts
- Attestation stash: phys 0x90040 → 0x91000 → 0x92000.
- Page tables at kernel base + 0x200000; identity map 0-1 GB + higher-half.
- APIC MMIO: LAPIC 0xFEE00000, IOAPIC 0xFEC00000 (select-window accessors),
  mapped via the PDP[3] window 0xC0000000..0xDFFFFFFF.
- System tick: LAPIC timer vector 32, periodic, 100 Hz.
