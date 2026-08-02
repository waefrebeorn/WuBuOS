# 02-architecture — Translation-Simplified Layers (speed for AI + games)

*Kevin-Bacon pass 3 synthesis. Every cross-OS boundary is a translation;
translation speed = fewer boundaries, batched moves, in-process tables.*

## The research convergence

- **Wine 11** rewrote syscall dispatching + WoW64 to eliminate PE/ELF
  boundary overhead — the stutter fix. Lesson: the translation must live
  IN the process, dispatched directly, not crossing boundaries per call.
- **Syscall User Dispatch (Linux 5.9)**: the compatibility layer captures
  foreign syscalls in userspace — a fast trap, not an emulator.
- **DXVK / vkd3d-proton**: DirectX 9/10/11/12 → Vulkan translation at the
  API level — frequently FASTER than the native path (simplified pipeline,
  batching, no legacy baggage).
- **GameInput (Microsoft GDK)**: one unified input model — keyboards, mice,
  gamepads, controllers — common time base, near-identical code with
  filters. The "all-device HID" pattern (our `wubu_usb.h` design matches).

## The WuBuOS translation stack

```
Windows/Linux/macOS program
   │
   ├─ PE/ELF loader (wubu_exec) ── in-process, no boundary crossing
   ├─ VSL syscall tables (NT/Linux/macOS) ── the already-built translation
   │     · in-process dispatch (Wine 11 lesson)
   │     · batched, zero-copy where possible
   ├─ graphics: GDI/DirectX → the /theme namespace (DXVK lesson:
   │     ONE translation target beats a zoo of APIs)
   ├─ input: any device → GameInput-style unified events → the WM
   └─ audio/fs/network: same pattern — tables, not emulation
```

## Speed rules (the "simplify for transliteration" doctrine)

1. **The table is the law.** A foreign syscall maps to one entry — no
   reimplementation per call, no heuristic dispatch.
2. **In-process translation.** The foreign binary + its translation tables
   share the process (no PE/ELF boundary) — the Wine 11 fix.
3. **Batch before you cross.** Grouped syscalls, damage-rect rendering,
   batched HID reads — crossing a boundary is expensive; crossing once for
   N items is not (the fread/fwrite batching result: 5-10×).
4. **One target per domain.** Graphics → /theme; input → unified events;
   syscalls → VSL. Every personality translates TO the same target, so
   adding an OS is adding a table, not a subsystem.
5. **Zero-copy where possible.** Shared memory for buffers (the anti-cheat
   telemetry ring pattern) instead of per-call copies.

## Game porting + AI design implications

- **Game porting = tables + a shim.** The Win32 API is ALREADY being
  recreated (the NT syscall tables); graphics ride the /theme translation;
  input rides the unified HID. Porting a game = picking its personality
  tier + letting the tables do the work.
- **AI design speed** = the AGI drives the same namespaced APIs the user
  does: the Colonel edits /theme, launches apps via WuBuFX, reads state via
  /app/<id>/state — no special "AI bridge" needed, because the whole system
  is already namespace-shaped.

## Layering (future-proof)

The four layers (UI → OS core → anti-cheat → hardware/drivers) each depend
only on the layer below (layered-OS rule), and each boundary is a
translation with a versioned contract:
- level 1 apps → /theme + /app namespaces
- level 0 core → the VSL tables + WuBuFX gates
- anti-cheat → the measured boot + runtime PCR + IOMMU policy
- hardware → the driver model (PCI config ✓, USB/HID next, network after)
