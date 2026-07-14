# CachyOS Source Steal — Build Spec (source-verified)

Cloned 2026-07-13 into `vendor/cachyos/`:
- `linux-cachyos/`  (github.com/CachyOS/linux-cachyos)  — kernel PKGBUILDs +
  per-scheduler variants (bore/eevdf/bmq/rt/hardened).
- `Cachyos-pkgbuilds/` (github.com/CachyOS/Cachyos-pkgbuilds) — helper tools
  + meta packages (cachyos-hello, cachyos-kernel-manager, cachyos-gaming-meta,
  cachyos-ananicy-rules, chwd, cachy-chroot, cachy-update).

Every proposal below cites the exact source fact it steals. No stubs: each new
subtree ships with a `*_test.c` asserting file<->API routing (like
wubu_ns_bridge_test, 29/29 green).

=====================================================================
P1 — /n/snap  [DONE — wubu_ns_snap.c, 16/16 green]
=====================================================================
SOURCE: CachyOS wiki "Why CachyOS?" + btrfs/Snapper integration; WuBuOS
already has wubu_snapshot_{create,list,rollback,diff,gc} (wubu_snapshot.h).

BUILD: extend wubu_ns_bridge with a snap subtree.
  /n/snap/<container>/list            -> wubu_snapshot_list
  /n/snap/<container>/create <tag>    -> wubu_snapshot_create
  /n/snap/<container>/rollback <id>   -> wubu_snapshot_rollback
  /n/snap/<container>/diff <id1> <id2>-> wubu_snapshot_diff
FILES: src/runtime/wubu_ns_snap.c (+ _test.c). Wire into test_ns_bridge or a
new test_ns_snap. Reuses g_ns_root + ns_mkdir from wubu_ns_bridge.c.

=====================================================================
P2 — /n/pkg  [DONE — wubu_ns_pkg.c, 13/13 green]
=====================================================================
SOURCE: Chaotic-AUR docs (aur.chaotic.cx) — prebuilt AUR binaries, `pacman -S`
works without local makepkg. WuBuOS parallel: oci/ layer + container/wubucontainer.

BUILD (as shipped): /n/pkg/{install,remove,list,repos,addrepo} wrapping the REAL
wubu_pkg_* API (pkg_install/remove/list/add_repo) -- flatpak-style .wubu
packages with dep DAG + repo sources. /n/pkg/repos is the Chaotic-AUR vibe
(enable a prebuilt-binary repo). list/repos are live views. Reuses existing
pkg code, no duplication.

=====================================================================
P3 — /n/kernel/scheduler  [DONE — wubu_ns_kernel.c, 13/13 green]
=====================================================================
SOURCE: linux-cachyos PKGBUILD `_cpusched` ∈ {bore,eevdf,bmq,rt,rt-bore,
hardened}; runtime flip via scx-manager (sched-ext). BORE = patch on EEVDF for
bursty-interactive (gamescope/proton) workloads.

BUILD: /n/kernel/scheduler — read = active policy; write "bore"|"eevdf"|"fair"
applies equivalent userspace tunables (CPU policy for bottles) + records
selection. If WuBuOS kernel later gains sched-ext, this becomes the switch.

=====================================================================
P4 — BOTTLE_PRESET_GAMING  (cachyos-gaming-meta dep list, verbatim)
=====================================================================
SOURCE: pkgbuilds/cachyos-gaming-meta/PKGBUILD `depends=(...)`:
  proton-cachyos-slr protontricks wine-cachyos-opt winetricks vulkan-tools
  lib32-alsa-plugins lib32-giflib lib32-gtk3 lib32-libjpeg-turbo
  lib32-libva lib32-mpg123 lib32-ocl-icd lib32-openal

BUILD: map to wubu_bottle_install_deps targets; add preset enum
BOTTLE_PRESET_GAMING. Surface as `echo gaming > /n/bottles/<n>/preset`. The
lib32-* multilib libs are the canonical "game won't launch" gap — our dep
installer should resolve exactly these.

=====================================================================
P5 — /n/hw  [DONE — wubu_ns_kernel.c, 13/13 green]
=====================================================================
SOURCE: chwd (dep of cachyos-kernel-manager) auto-detects + flips NVIDIA/AMD.
WuBuOS already has wubu_proton2_gpu.c / wubu_proton2_device.c.

BUILD: /n/hw/<gpu>/mode — read = detected, write = switch passthrough config.

=====================================================================
UNIFIED THESIS (the "do it better" payoff)
=====================================================================
CachyOS implements each vibe as a SEPARATE daemon/tool (snapper, pacman,
kernel-manager, chwd, hello). WuBuOS expresses ALL of them through the SAME
/n 9P namespace the ns_bridge already provides:

  snapper rollback     -> echo <id> > /n/snap/<c>/rollback     [DONE]
  chaotic-aur install  -> echo x    > /n/pkg/install        [DONE]
  kernel-manager GUI   -> echo bore  > /n/kernel/scheduler   [DONE]
  chwd GPU switch      -> echo amd   > /n/hw/<gpu>/mode       [DONE]
  cachyos-hello        -> (writes the above files)
  bottles-cli run      -> echo run   > /n/bottles/<n>/ctl   (DONE)

One filesystem, zero bespoke daemon UX, one dispatch fabric. That is the
"rip off the others, do it better" thesis made concrete and source-verified.
