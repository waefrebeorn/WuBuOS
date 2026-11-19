# 06 — Multi-host: Linux and Windows get the same deal

The macOS loop was never just about the Mac. WuBuOS is a WSL
replacement in BOTH directions: people develop in Linux and run the
result on macOS, and people on Windows and Linux run WuBuOS itself,
hosted or booted. One OS, every host, source shipped as the distro.

Lineage: TempleOS kernel design, expanded. Ring-0, one address space,
the whole machine as a single organism you can hold in your head.
ZealOS carried it forward; WuBuKernel is the own-C11 kernel repo
(driver model, console/heap/ticks, physical memory manager); WuBuOS is
the body it grows into — firmware, kernel, runtime, GUI, guests.

## How each host runs WuBuOS (the two ways)

### Way 1: hosted (the desktop as a guest process)

The WuBuOS desktop is a pure C11 Wayland client. Whoever serves
Wayland decides where it runs:

- Linux: WuBuCompositor nested in the existing X session (today,
  verified) or on DRM/KMS directly (next backend). This is the
  develop-in-Linux path.
- Windows: WSL2 + WSLg. WSLg ships a Wayland compositor; the WuBuOS
  client connects to it like any Wayland app, no porting. The
  compositor itself can also run inside WSL2 nested. This is the
  WSL-replacement shape exactly: Linux userspace, Windows host,
  seamless window. (CI cannot nest WSL2 — the Windows runner is
  already a VM without nested virt — so this leg is verified on user
  machines and documented, not faked in CI.)
- macOS: WuBuCompositor with the Quartz host backend (the split makes
  it a drop-in; libwayland builds via epoll-shim, research/05). Until
  that lands, the macOS leg is the shader/exec proof + the CI compile
  loop.

### Way 2: boot it (bare metal in a VM) + hook

`make iso` in wubuos builds src/kernel/wubuos.iso — GRUB multiboot,
the deployable interboot artifact. QEMU boots it identically on
Linux, Windows, and macOS; so does bare metal via GRUB. The skill
wubuos-interboot-deploy records the boot health contract: ~3700-4600
serial bytes to the interactive WuBuOS> console; 38/106 bytes are the
fixed-bug regression ceilings.

Hooking host<->guest:
- serial (COM1) console + trace: the verify channel CI asserts on;
- 9P namespace (/n/*): the file/namespace hook between host and guest
  (wubuos-9p-namespace-subsystem; the A3 proof fetches a module over
  COM2 and runs it on-kernel);
- network: the guest's stack reaches the host's (Steam Networking
  port in progress);
- display: QEMU std-vga/Bochs VBE — the kernel drives the LFB
  directly (screendump-verified 1920x1080 desktop + buddy).

## The free CI matrix (all public-repo runners, $0)

| workflow | repo | what it proves |
|---|---|---|
| macos-verify.yml | wubucompositor | Apple branch compiles + spawn gate on macOS Intel + M1 (LIVE, green 2026-11-19) |
| linux-verify.yml | wubucompositor | full compositor build + spawn gate + Xvfb startup smoke; phase 2 = full hosted e2e capture |
| boot-verify.yml | wubuos | build kernel+ISO once, BOOT IT under QEMU on ubuntu AND windows AND macos runners, assert the healthy serial trace on each |

boot-verify is the strongest multi-host statement: the SAME artifact
boots to the WuBuOS> console on all three host OSes, in CI, free, on
every kernel push. tools/ci_boot_assert.py is the shared gate (45s
window, >=3000 bytes + console marker, regression ceilings fail).

Windows runner specifics: QEMU via choco; TCG (no KVM/Hyper-V accel
needed for a 45s boot); python for the assert; bash shell steps.
macOS runner: QEMU via brew. Both boot the artifact the Linux job
built — one build, three hosts.

## Source as the distro

We ship source + build recipes, not opaque binaries: the repo IS the
distribution. Per host: `make hosted` (Linux/WSL2), `make` in
wubucompositor (Linux; macOS once Quartz lands), `make iso && qemu`
(every host). The CI loops prove each recipe on every push; the
GitHub package/release flow finalizes artifacts (ISO + source
tarballs + per-host build notes) once the matrix is green. Users
never need a payment plan or a support ticket: clone, make, run —
or download the ISO and boot it.

## Campaign CLOSED: kernel gcc14/C11 declaration hygiene (2026-11-19)

The boot-verify CI (gcc14 on ubuntu-latest) exposed that the kernel
still compiled via implicit declarations and other pre-C11 habits:
**214 strict errors** measured with -Werror=incompatible-pointer-types
-Werror=int-conversion -Werror=implicit-function-declaration.
**Closed same day: 214 -> 0.** Owner directive: "goal is gcc14, move
into that" — CI now builds with the runner's gcc14 AND runs
`make kernel-strict` as a permanent ratchet step.

How: tools/kernel_strict_wave.py (campaign accelerator: parses the
strict error log, maps each implicit declaration to its owning header,
inserts the include, rebuilds, reports the remainder). Three waves:
- wave 1 (manual): wubu_drv.h prototype-scope struct tag (a REAL type
  bug); missing includes in 10 files; vsnprintf forward decl in libc.c.
- wave 2 (tool): ~200 implicit declarations auto-mapped to owner
  headers (wubu_probe_matrix.c alone calls ~200 subsystem summary
  functions and now includes each owner).
- wave 3 (manual): metal_main.c BSS linker-symbol casts (char[] ->
  uint64_t* decay + explicit cast).
Verified: kernel-strict 0 errors, normal build 0 errors, QEMU boot PASS
(console + heartbeat), hosted green, bridge all pass.
Still open (recorded): libc.h re-defines struct tm/sigaction/cli/sti
that libc.c owns — duplicate-owner merge for a later batch (libc.c
cannot include its own header today).

## Closed gap: WuBuKernel is public (2026-11-19)

The WuBuKernel submodule repo was PRIVATE — a fresh public clone of
wubuos could not fetch it, so test_kernel_bridge was unbuildable from
source alone. That contradicted source-as-distro. Scanned (12 files,
pure C11 kernel code, zero secrets — no security reason for privacy),
made public by owner directive, anonymous clone verified at d92b0f2
(the exact pinned commit). boot-verify now fetches it and runs the
bridge gate on every kernel push.

## Honest limits

- WSL2/WSLg leg: real on user machines, not CI-verifiable (no nested
  virt on GitHub runners). Documented, not claimed as CI-green.
- Windows hosted compositor: no native Win32 backend planned; the
  Windows path is WSL2 (hosted) or QEMU (booted). That is the
  WSL-replacement philosophy, not a gap.
- QEMU on CI is TCG (slow); the 45s window is calibrated for it.
  KVM on user machines is faster.
- The macOS Quartz backend is still the open build leg (stubs +
  recipe + CI compile loop ready).