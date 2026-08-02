# 02-architecture — Levels

The four-level model (see `docs/AGI_OS_DESIGN.md` for the full synthesis).

```
LEVEL 3  Foreign OS overlays (absorbed "upper space")
         Windows/Linux/macOS/Steam apps via VSL translation + containers
LEVEL 2  Overlay runtime (exec, personalities, network, snapshot)
LEVEL 1  Wayland-style user space (the human surface)
         WuBuOS WM/compositor + Bonzi Buddy (hedged human interface)
LEVEL 0  RING-0 COLONEL SPACE (the AGI's DOS)
         wubuwizard + AGI supervisor + HolyC JIT + drivers + single
         address space, under the WuBuFW measured boot
```

## Current metal status (2026-08-02)

- **Level 0 live:** console REPL (COM1), PCI config, APIC/LAPIC tick (100 Hz,
  interrupts deliver), cooperative scheduler (4 tasks: idle, agent, bonzi,
  console), AGI supervisor cycle, Bonzi on the framebuffer, attestation gate.
- **Level 0 next:** HolyC JIT to metal, USB xHCI + HID, `wubu_vmm` +
  single-level store, network.
- **Levels 1-3:** hosted scaffolds; port when the level-0 substrate lands.
