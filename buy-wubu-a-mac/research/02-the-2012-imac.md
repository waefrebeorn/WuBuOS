# 02 — The 2012 iMac: fallback, not foundation

Facts verified 2026-11-19 (OCLP docs + GitHub self-hosted runner docs):

## Hardware and OS ceiling

- Late-2012 iMac = iMac13,1 (21.5") or iMac13,2 (27"). Ivy Bridge CPU,
  NVIDIA Kepler GPU.
- Official Apple ceiling: macOS 10.15 Catalina.
- OpenCore Legacy Patcher SUPPORTS iMac13,1/13,2 explicitly (MODELS.md),
  including macOS Sequoia (supported since OCLP 2.0.0; ocp.dev/sequoia
  lists both 2012 iMacs). Caveats: "Legacy Metal (macOS 13+)" graphics
  patching; Ivy Bridge + Kepler need MetallibSupportPkg; 3 GB+ RAM for
  Sonoma/Sequoia. OCLP targets Big Sur 11.x through Sequoia 15.x.
- Ventura+ nominally wants AVX2 (Ivy Bridge has AVX, not AVX2); OCLP's
  Sequoia support for iMac13,x is documented, but treat newer-than-
  Sonoma as best-effort on this machine. Big Sur / Monterey / Ventura
  are the sane targets for a build box.

## What it can do for the loop

1. SSH target (works on ANY macOS version it runs): the agent compiles
   and runs gates over ssh, cron or on-demand. No runner software, no
   version floor. Simplest, fully automatable.
2. GitHub self-hosted runner: the runner application REQUIRES macOS
   11.0 Big Sur or later (docs.github.com, self-hosted runners
   reference). Catalina alone does NOT qualify. With OCLP to Big Sur+
   the iMac becomes a free self-hosted macOS x64 runner; GitHub assigns
   jobs to it like any runner. Keep it patched (runner updates have a
   63-day grace window).
3. Legal macOS VM host: Apple's EULA permits macOS virtualization on
   Apple hardware. The iMac can host VMs of newer macOS for clean-room
   verify runs. The Linux box cannot legally do this.

## Role

Fallback for: bare-metal quirks the VM runners hide, older-macOS
compatibility checks (Catalina-era users), and anything the free CI
quota can't reach. NOT the primary loop: it's old, slow, and a single
point of failure; the free CI matrix is faster and disposable.

Setup is a one-time human act (install OCLP / enable SSH / register the
runner). After that, everything is agent-drivable. That one-time act is
the only telephone-tag-free exception: it's his own machine, no nerd
required.