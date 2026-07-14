

---

## ══════════════════════════════════════════════════════════════════════════════════
## CachyOS LINEAGE — SOURCE-VERIFIED STEALABLES (added 2026-07-13)
## ══════════════════════════════════════════════════════════════════════════════════

CachyOS is the *performance-tuned* sibling of the same Arch + Proton +
gamescope + Vulkan stack SteamOS is built on. Because it is Arch-based and
gaming-first, its source is the highest-signal place to steal concrete
tunables for WuBuOS's gaming/responsiveness story. Source cloned to
`vendor/cachyos/` (github.com/CachyOS): `linux-cachyos` (kernel patches) and
`Cachyos-pkgbuilds` (helper tools + meta packages).

### 1. Scheduler strategy (kernel responsiveness "vibe")
From `linux-cachyos/linux-cachyos/PKGBUILD` + per-scheduler subdirs:
- Selectable at build via `_cpusched`: **`bore`** (Burst-Oriented Response
  Enhancer — a patch *on top of EEVDF* for bursty-interactive workloads),
  **`eevdf`** (CachyOS default, with desktop-responsiveness tunables),
  `bmq`, `rt`, `rt-bore`, `hardened`.
- Real patch files in `cachyos/kernel-patches` (e.g. `0001-handheld.patch`
  for Steam-Deck-class hardware, `0001-hardened.patch`, `0001-rt-i915.patch`
  for Intel-GPU realtime, `0001-clang-polly.patch` for LLVM auto-vectorize).
- Runtime scheduler flipping is done via **`scx-manager`** (sched-ext
  userspace) — confirmed as a dep of `cachyos-kernel-manager`.

  → WuBuOS mapping: our `/n/kernel/scheduler` 9P file (proposed) maps directly
  to this. We can't recompile a kernel per user, but we CAN expose the active
  policy + apply the *equivalent* userspace tunables (see §2) and offer a
  sched-ext-style hook if the WuBuOS kernel gains one. BORE's design goal
  (protect bursty-interactive tasks like gamescope/proton from batch starvation)
  is exactly what our container CPU policy should emulate for bottles.

### 2. "Out of the box gaming" dependency set (from `cachyos-gaming-meta`)
Exact `depends=(...)` from the cloned PKGBUILD — the canonical gaming stack:
```
proton-cachyos-slr   # CachyOS-tuned Proton (Steam Runtime link)
protontricks         # prefix dependency installer (we already wrap this)
wine-cachyos-opt     # optimized Wine build
winetricks          # runtime libs (our install_deps target)
vulkan-tools        # vulkaninfo / GPU capability probe
lib32-alsa-plugins lib32-giflib lib32-gtk3 lib32-libjpeg-turbo
lib32-libva lib32-mpg123 lib32-ocl-icd lib32-openal
```
  → WuBuOS mapping: this IS our bottle provisioning list. Our
  `wubu_bottle_install_deps` should resolve this exact set (the lib32-* multilib
  libs are the usual "game won't launch" gap). Add a `BOTTLE_PRESET_GAMING`
  that pulls precisely these. Surfaced as `echo gaming > /n/bottles/<n>/preset`
  (extends the ns_bridge we shipped).

### 3. Hardware detection (from `chwd`, dep of kernel-manager)
`chwd` auto-detects GPU and flips NVIDIA<->AMD config. WuBuOS already has
`wubu_proton2_gpu.c` / `wubu_proton2_device.c` (GPU passthrough config).
  → WuBuOS mapping: `/n/hw/<gpu>/mode` (read = detected, write = switch) —
  the chwd "it just works" vibe as a 9P op.

### 4. Rollback "vibe" (btrfs + Snapper)
CachyOS's most-praised quality: updates are low-risk because every update is
a snapper snapshot + one-command rollback. WuBuOS ALREADY has this engine:
`wubu_snapshot_create/list/rollback/diff/gc` (confirmed in `wubu_snapshot.h`).
  → WuBuOS mapping: `/n/snap/<container>/{list,create,rollback,diff}` — the
  CachyOS rollback vibe through our control plane (build spec in
  `docs/cachyos_source_steal.md`, priority P1).

### 5. Prebuilt-binary convenience (Chaotic-AUR)
Chaotic-AUR serves prebuilt AUR packages so `pacman -S` needs no local
compile. WuBuOS parallel is the OCI layer (`oci/`: registry, image_*,
runtime_spec) + `container/wubucontainer`.
  → WuBuOS mapping: `/n/pkg/{available,install}` backed by the OCI layer —
  the "install anything instantly" vibe without a distro package manager.

### Bottom line for the SteamOS image
SteamOS (Valve) and CachyOS (community) share the SAME Arch+Proton+gamescope+
Vulkan foundation. CachyOS proves the *performance/responsiveness* layer on top
of that foundation. WuBuOS's differentiator vs BOTH: express every one of these
"vibes" (scheduler, gaming meta, hw detect, rollback, prebuilt install) through
ONE Styx/9P namespace (`/n/*`) instead of a pile of bespoke daemons. The
CachyOS source above gives us the *concrete* values (scheduler names, the exact
gaming dep list) to make that real rather than hand-wavy.
