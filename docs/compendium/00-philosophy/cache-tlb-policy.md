# Cache / TLB Maintenance Policy (gap I4)

The kernel's caching and TLB doctrine — every invalidation point, the
write-back model, and what a change to paging MUST do.

## Doctrine

1. **Write-back caches, explicit flushes.** The x86 caches are coherent
   for normal memory (MESI); the kernel never needs a blind WBINVD for
   RAM. WBINVD is reserved for the rare physical-aliasing transition
   (identity ↔ higher-half remaps) where stale lines could alias.

2. **TLB invalidation is per-mapping, never global.** A page-table edit
   must `invlpg` the affected linear address (or `mov cr3` for a full
   switch — the task switch does this once per context, not per page).
   The vmm's map/unmap paths own their invalidation.

3. **The identity map is the exception.** The boot identity-map + the
   higher-half map of the same physical pages are legal aliases; they
   are created ONCE at boot before interrupts, so no invalidation race
   exists. Any new alias must be created with the same discipline:
   edit → `invlpg`/cr3-reload → publish.

4. **CR3 reloads happen at task switches and the vmm's full-teardown
   paths only.** Per-page edits use `invlpg` (cheaper, no TLB shootdown
   needed — the kernel is single-CPU today; SMP (gap I2) must add IPI
   shootdowns).

5. **NX + WP are always on** (crt0): WP makes RO pages fault on stray
   writes (#PF is the evidence, gap C9); NX keeps data pages
   non-executable. The page-table walk is the source of truth.

6. **The FPU/SSE state is NOT cache-policy:** the fxsave area (TaskContext
   @144) is 16-aligned ordinary RAM; no cache attribute tricks.

## Invalidation points (checked at every batch)

| Point | Action |
|---|---|
| vmm map/unmap page | `invlpg` the linear address |
| vmm full region teardown | `mov cr3` (context reload) |
| task switch | `mov cr3` (per-context tables) |
| identity↔higher-half aliasing | single boot-time setup, no runtime |
| page-table page itself freed | `invlpg` its mapped window first |

## Policy for future work

- SMP (I2): every `invlpg` becomes an IPI-assisted shootdown.
- COW (B4): the RO→RW promotion of a shared page must `invlpg` the
  page on the faulting CPU (the fault is the natural sync point).
- SMEP/SMAP (C10) rely on the U/S bits, not the cache — unchanged by
  this policy.

_Evidence-first: any change that touches a page table or CR3 must come
with a boot probe + a host test that exercises the invalidation path._
