# 00-philosophy — Memory Design

*Addendum entry. Human-written. Last updated 2026-08-02.*

## The virtue

The Colonel's memory IS the storage. There is no files-vs-memory split: the
hive, the trace, the weights, the persistent world are ONE single-level store
(IBM i lineage) with orthogonal persistence (Phantom OS): restart = the world
reappears.

## The hierarchy (MemGPT-converged)

- **Core memory** — the AGI's in-context state (working set).
- **Archival memory** — the hive: the user's hand-drawn Vector/List/Hive
  structure; stable pointers, O(1) mark-erase, cache-friendly iteration.
  This is the C11 realization of A-MEM's atomic notes + memory network.
- **Storage** — the single-level store: persistent segments (`wubu_segments`
  / `wubu_store`), checkpointed, faulted in on demand.
- **Overlay memory** — every foreign OS gets an EPT-style second-level
  translation (guest VA → guest PA → real PA); the IOMMU (VT-d/AMD-Vi: DMA
  remapping, IOVAs, device isolation, interrupt remapping) covers the DMA
  plane — the anti-cheat answer to PCIe-DMA.

## What we found (this session, on metal)

- The #PF handler must FAULT-IN, not halt — the current halt-on-#PF is a
  stopgap until `wubu_vmm` lands.
- The ISR must preserve ALL caller-saved registers (rax included!) across
  the dispatch — the "memcpy writes to 0xff000000" corruption was an ISR
  clobbering rax mid-loop (see 03-learned/bugs.md).
- BSS adjacency is a real hazard: g_current sat 8 bytes from g_vbe.fb —
  any overlong write silently corrupts the framebuffer pointer.
