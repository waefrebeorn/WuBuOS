# Research Brief: CachyOS "Vibes" to Steal for WuBuOS

Source research (2026-07-13): CachyOS official wiki (wiki.cachyos.org),
Chaotic-AUR docs (aur.chaotic.cx), community reviews (distrowatch, reddit,
discuss.cachyos.org). Goal: lift the *experience* qualities users love about
CachyOS and re-express them through WuBuOS's secret weapon — the Styx/9P
namespace + existing snapshot/archd/bottles machinery — doing it *better*
(one uniform control plane, no PID1/D-Bus dichotomy).

=====================================================================
1. WHAT CachyOS ACTUALLY IS (verified)
=====================================================================
- Performance-centric **Arch Linux** derivative. Sells "blazingly fast".
- CPU-specific optimized repos: x86-64-v3 / v4 / Zen4+ prebuilt packages
  (no local compile for most users).
- Custom kernel patch set + multiple schedulers:
    * EEVDF with CachyOS desktop-responsiveness tunables (default)
    * BORE (Burst-Oriented Response Enhancer) — fluid under load
    * sched-ext framework support (pluggable user-space schedulers)
  Exposed via a "CachyOS Kernel Manager" GUI/CLI.
- **btrfs + Snapper** snapshots → one-command rollback of system updates.
  This is the single most-praised "vibe": "turns updates into low-risk
  experiments."
- **Chaotic-AUR**: a prebuilt binary AUR repository. `pacman -S` works for
  thousands of AUR packages without local `makepkg`. (Prebuilt binaries =
  the convenience killer feature.)
- Friendly onboarding: GUI + CLI installers, `cachy-chroot` helper,
  `chwd` (automated hardware detection: NVIDIA/AMD GPU switching),
  `cachyos-hello` post-install wizard, `cachy-benchmark`, `cachy-cli`.
- Gaming out-of-box: Steam + Proton + Vulkan + MangoHUD + GameMode-style
  tuning, handheld edition.

=====================================================================
2. THE "VIBES" WORTH STEALING  (mapped to WuBuOS)
=====================================================================

VIBE A — "Updates are low-risk experiments" (btrfs+Snapper rollback)
  CachyOS: `snapper create` / `snapper rollback`. Users rave that a bad
  update is a 10-second revert.
  WuBuOS already HAS this: wubu_snapshot_* (create/list/rollback/diff/gc).
  STEAL IT BETTER: expose snapshots as 9P files under /n/snap:
      /n/snap/<container>/list          -> wubu_snapshot_list
      /n/snap/<container>/create        (write tag) -> wubu_snapshot_create
      /n/snap/<container>/rollback <id> -> wubu_snapshot_rollback
      /n/snap/<container>/diff <id1> <id2> -> wubu_snapshot_diff
  Then `cat /n/snap/root/list` and `echo <id> > /n/snap/root/rollback`
  become the "CachyOS vibe" with NONE of systemd/snapper's machinery.
  This directly extends the wubu_ns_bridge I just shipped.

VIBE B — "Prebuilt, no-compile package access" (Chaotic-AUR)
  CachyOS: prebuilt AUR binaries, `pacman -S anything`.
  WuBuOS parallel: the OCI layer (oci/ subdir: oci_registry, oci_image_*,
  oci_runtime_spec) + container/wubucontainer already fetch+run images.
  STEAL IT BETTER: a /n/pkg namespace where `ls /n/pkg/available` lists
  prebuilt app images and `echo app > /n/pkg/install` pulls+runs via the
  OCI layer — the "Chaotic-AUR convenience" without a distro package
  manager at all. Uniform with /n/svc and /n/bottles.

VIBE C — "One kernel, pick your scheduler" (Kernel Manager + BORE/sched-ext)
  CachyOS: GUI to flip BORE/EEVDF/sched-ext per boot.
  WuBuOS parallel: the kernel has scheduler hooks (see interrupt/APIC code
  we touched). We can't recompile a kernel per user, but we CAN expose the
  *active policy* as a 9P file:
      /n/kernel/scheduler  (read current; write "bore"|"eevdf"|"fair")
  STEAL THE VIBE: the "I tuned my kernel in one click" feeling, expressed
  as a file write, not a kernel reinstall.

VIBE D — "Hardware just works" (chwd / cachy-chroot)
  CachyOS: `chwd` auto-detects GPU, switches NVIDIA<->AMD config.
  WuBuOS parallel: wubu_proton2_gpu.c / wubu_proton2_device.c already do
  GPU passthrough config. Expose as /n/hw/<gpu>/mode (read/set).
  STEAL THE VIBE: "detected your GPU, flipped the switch" as a namespace op.

VIBE E — "Friendly first-boot wizard" (cachyos-hello)
  CachyOS: post-install welcome that sets up gaming, updates, snapshots.
  WuBuOS parallel: a /n/welcome namespace or a holyd (WM) first-run panel
  that, behind the scenes, writes the right /n/* control files. The wizard
  is just a pretty face on the 9P control plane we already built.

VIBE F — "Gaming out of the box" (Steam/Proton/Vulkan/MangoHUD)
  CachyOS: installs the gaming stack automatically.
  WuBuOS ALREADY does this better via bottles: /n/bottles/<name>/ctl run
  launches a Wine/Proton prefix. Add /n/games/<name>/launch that maps to a
  bottle's run. The "out of the box game runs" vibe, uniform with everything.

=====================================================================
3. THE CORE INSIGHT (why WuBuOS wins the comparison)
=====================================================================
Every CachyOS "vibe" is implemented as a SEPARATE daemon/tool with its own
UX (snapper, pacman, kernel manager, chwd, hello). WuBuOS can express ALL of
them through the SAME /n 9P namespace the ns_bridge already provides:

    CachyOS tool          ->  WuBuOS 9P file
    snapper rollback      ->  echo <id> > /n/snap/root/rollback
    pacman -S chaotic-x   ->  echo x    > /n/pkg/install
    kernel-manager GUI    ->  echo bore  > /n/kernel/scheduler
    chwd GPU switch       ->  echo amd   > /n/hw/primary/mode
    cachyos-hello         ->  (writes the above files)
    bottles-cli run       ->  echo run   > /n/bottles/<n>/ctl   (DONE)

That is the "rip off the others, do it better" thesis made concrete: one
filesystem, zero bespoke daemon UX, the same dispatch fabric.

=====================================================================
4. RECOMMENDED NEXT BUILD (angel-coder, real + tested)
=====================================================================
Priority 1 (highest leverage, reuses shipped code): extend wubu_ns_bridge
  with a /n/snap subtree backed by wubu_snapshot_*. Write wubu_ns_snap.c
  + wubu_ns_snap_test.c proving:
     - publish container -> /n/snap/<c>/list shows snapshots
     - write "<id>" to /n/snap/<c>/rollback routes to wubu_snapshot_rollback
  This delivers VIBE A — the most beloved CachyOS quality — in ~1 module.

Priority 2: /n/pkg backed by the OCI layer (VIBE B).
Priority 3: /n/kernel/scheduler + /n/hw/* (VIBE C, D).

No stubs: each new subtree ships with a *_test.c asserting the file<->API
routing, exactly like wubu_ns_bridge_test (29/29 green).
