# buy-wubu-a-mac

The macOS problem, solved without buying a Mac — and then the same
treatment for Linux and Windows. The name is the joke: the answer to
"buy WuBu a Mac" is that we don't have to, on any host.

MULTI-HOST (owner directive 2026-11-19): people on Linux, Windows, and
macOS run WuBuOS hosted (Wayland guest; WSL2/WSLg on Windows) or BOOT
the GRUB ISO in QEMU and hook in (serial + 9P). We ship source as the
distro. Lineage: TempleOS kernel design, expanded -> WuBuKernel ->
WuBuOS. See research/06-multi-host-linux-windows.md.

Owner constraints (2026-11-19): the only Mac owned is a 2012 iMac. No
debt, no payment plans. No telephone tag: any rented/remote Mac must be
drivable end-to-end by an agent (API/CLI/SSH), never a human support
loop. The goal is a FREE compile-and-verify loop for the macOS code,
finalized through the GitHub package/release flow once validated.

## The verdict (details in research/)

PROVEN 2026-11-19: the loop ran. Push 014e37e compiled the Apple
branch on real macOS (Intel + M1); the gate caught one environment-
dependent assertion; fix 9659d05; run 44494759630 GREEN on both
architectures. Free, agent-driven, no human. The workflow is LIVE in
wubucompositor/.github/workflows/macos-verify.yml.

1. PRIMARY: GitHub Actions macOS runners are FREE and UNLIMITED on
   public repositories (verified: docs.github.com, and anonymous
   git ls-remote confirms waefrebeorn/wubucompositor is public).
   Intel (macos-15-intel / macos-26-intel) and Apple Silicon
   (macos-15 / macos-26). Push or workflow_dispatch; the agent drives
   it with gh. No purchase, no human.
2. The 2012 iMac (iMac13,x) is the FALLBACK: Apple hardware, legal for
   macOS VMs, OCLP-supported past Catalina; SSH loop or self-hosted
   runner (runner needs macOS 11+, so OCLP first).
3. Compile-only checks from Linux via osxcross need no Mac at all
   (limits: GUI frameworks; SDK licensing is Apple-hardware-bound).
4. libwayland builds on macOS via epoll-shim (kqueue) — the compositor's
   protocol machinery is NOT a Linux dead end. MacPorts carries the
   recipe. This is what makes the Quartz host backend a real project.
5. DEAD ENDS recorded: Cirrus CI (jobs stopped 2026-06-01), darling
   (full Mach-O process exec), rent-a-Mac-with-a-human.

## Contents

- ESSAY.md — the position, in the owner's voice.
- stubs/ — every macOS stub in the tree (inventory + copies), plus the
  Quartz host-backend skeleton (honest stub, not compiled, not claimed).
- research/ — the findings, cited and dated:
  - 01-github-actions-free-loop.md (the primary loop + agent protocol)
  - 02-the-2012-imac.md (OCLP facts, runner/SSH options)
  - 03-cross-compile-from-linux.md (osxcross compile-only loop)
  - 04-free-tiers-and-programs.md (Codemagic/Blacksmith/GetMac/NoMac/
    MacStadium matrix, automation verdicts)
  - 05-wayland-on-macos.md (epoll-shim, what builds, what's left)
  - 06-multi-host-linux-windows.md (the two ways per host, the free
    three-OS boot matrix, source-as-distro, the lineage)
  - SOURCES.md (URLs, checked 2026-11-19)
  - workflows/macos-verify.yml (LIVE in wubucompositor, green)
  - workflows/linux-verify.yml (compositor build + Xvfb smoke; e2e phase 2)
  - workflows/boot-verify.yml (wubuos: one ISO booted under QEMU on
    Linux AND Windows AND macOS runners, serial-trace gate)

## The loop, end to end

    agent edits Apple branch / Quartz leaf
      -> git push (public repo)
      -> GitHub macOS runner compiles + runs headless gates (free)
      -> gh run view --log-failed  (agent reads, iterates; no human)
      -> green => gh release / package flow finalizes the artifact

Phase 1 (today): compile the portable leaves (wcc_spawn) + run the
headless spawn gate on real macOS, both architectures.
Phase 2 (when the Quartz backend lands): full compositor build against
MacPorts wayland + epoll-shim, live capture gate.