# The Kernel Interchange Network — design + the OS deep-dive gap map

> "reproduce and graft every missing piece from the other operating systems …
> and start working on the kernel interchange network that allows us to load
> and swap in kernels of different operating systems. This will work for
> Linux, Windows and macOS."
>
> (The user dictates through Siri speech-to-text: "Colonel" = **kernel**,
> "Lennox" = **Linux**.  The in-repo `wubu_colonel.c` app-dispatcher is a
> different, pre-existing thing and is NOT what this doc is about.)

## 1. What shipped

`src/runtime/wubu_kernel_xchg.{h,c}` + `test_kernel_xchg` (wired into
`test_critical_runtime`).

A kernel is a **registered object with a versioned contract**, not the
foundation of the box:

```c
typedef struct wubu_kernel_ops {
    const char *name;        /* "linux", "nt", "macos", "holyd", "cpm" */
    int abi_version;
    int schema_version;      /* /n/trace schema major; 0 = emits none */
    const char *formats;     /* "elf64" "pe" "macho" ... */
    int  (*try_dispatch)(uint64_t num, uint64_t *regs, int64_t *out);
    int  (*probe)(const char *path, const uint8_t *hdr, size_t n);
    int  (*exec)(const char *path, int argc, char *const argv[]);
} wubu_kernel_ops_t;
```

The four behaviours that make it a network rather than a table:

1. **Register** — a duplicate name is *rejected*, not silently overwritten.
   Two kernels answering to one name is the drift the interchange prevents.
   A kernel that can neither dispatch, probe nor exec is rejected as inert.
2. **Fail-closed contract** — `wubu_kernel_select()` refuses any kernel whose
   trace-schema major differs from ours.  A kernel that cannot be observed at
   the boundary must not be allowed to run (MEGA_COMPAT DA-2).
3. **Routing with fallthrough** — `wubu_kernel_interchange()` asks the active
   kernel first, then every other registered kernel, and the *first one that
   claims the call* answers it.  A foreign kernel's unimplemented syscall can
   still be answered by one that knows it.  `wubu_kernel_dispatch_active()`
   is the strict no-fallthrough variant.
4. **Observable swaps** — every change of the active kernel is appended to a
   swap log (`select` and `swap` funnel through one function, so the log cannot
   disagree with the state it describes).  `wubu_kernel_xchg_dump()` renders
   the whole network as plain text for the `/n/kernels` publisher.

## 2. The deep dive — what is already grafted

The `os-studies/` README maps reference OS features to module names.  **Part of
that map is aspirational** — the names are written down, the files do not exist.
This table was re-verified against the tree (`ls src/**/*<name>*.c`), not
copied from the README.

| Reference OS | Grafted, real module | Genuinely missing |
|---|---|---|
| **GNOME 46+** | D-Bus activation `runtime/wubu_bus.c`; socket activation `wubu_svc_socket.c`; polkit `wubu_auth.c`; logind `wubu_logind.c`; the whole session/service runtime (`wubu_session_boot/daemon/mgr`, `wubu_ns_background`) | **GVfs** (SMB/SFTP/cloud/trash VFS), **Tracker** (content indexer), **AT-SPI2** (the a11y bus — `gui/wubu_a11y.c` exists but is the WM's own a11y surface, not the AT-SPI2 wire protocol) |
| **SteamOS 3.x** | DXVK config + UI (`runtime/wubu_proton_dxvk.c`, `wubu_dxvk_conf.c`, `gui/wubu_proton_dxvk.c`); GameScope (`wubu_proton2_gamescope.c`); Pressure Vessel (`wubu_pressure_vessel`); Steam Input + the real Deck report protocol (`wubu_steaminput.c`, `wubu_ns_steaminput.c`); EC fan/PWM (`wubu_ec_control.c`) | **VKD3D-Proton** (D3D12→Vulkan — no file), **Steam Networking** (P2P/relay/SDR — no file), **shader pre-cache / fossilize** (no file) |
| **TempleOS/ZealOS** | HolyC lexer/parser/codegen + JIT, the REPL (`runtime/wubu_holyd*.c`), the whole ZealOS parity header | **DolDoc** (hyperlinked doc format), **RedSea FS** (contiguous B-tree, no paths), **raw VGA/VESA** from HolyC |
| **Arch Linux** | package registry + pacman-style ops (`runtime/wubu_pkg.c`, `wubu_ns_pkg.c`); container/OCI runtime; snapshots; the archd daemon | **makepkg/PKGBUILD**, **AUR**, **mkinitcpio** (encrypt/lvm/raid/btrfs hooks), **systemd-homed/sysusers** |
| **Windows (NT)** | 296-slot NT dispatch table + 11 extension modules (`runtime/vsl/vsl_nt_*.c`), ReactOS ordinal transliteration, PE loader (`wubu_proton_pe.c`), the Win98/XP shell | **VKD3D** (see SteamOS), **ALPC/WNF/Enclave** blocks of the NT table (the `nt-parity-wip` branch carries 60 colliding slots and was deliberately NOT merged) |
| **macOS** | Mach-O loader (`vsl_macho.c`), 77 BSD + 13 Mach trap handlers (`vsl_syscall_mac.c`), ~250 syscall numbers | **IOKit parity**, **Metal/GPU parity**, the mach trap coverage beyond the 13 implemented |
| **CachyOS** | scheduler study + `chwd`-class hw detect (`wubu_hw_detect`) | **BORE/EEVDF/scx** scheduler plugins (the kernel tasking is EEVDF already; the plugins are not) |
| **Inferno/Plan 9** | Styx/9P client + server, the whole `/n` namespace bridge | the 9P **kernel** server (the server is userspace) |
| **CP/M, classic Mac** | both are VSL personalities (`vsl_cpm_syscall_dispatch`, `vsl_macclassic_syscall_dispatch`) | — |

## 3. The graft order (highest leverage first)

Each item names the **interchange slot** that will own it — that is the point of
the network: a graft lands as a registered kernel, not as a hard-coded branch.

1. **`/n/kernels` surface** — publish `wubu_kernel_xchg_dump()` through the Styx
   bridge so an operator can see and drive the network from the namespace.
   *Prerequisite for everything below being observable.*
2. **Wire the VSL personalities into the interchange** — the Linux, NT, macOS,
   CP/M and classic-Mac dispatchers already exist; today `vsl_syscall_dispatch()`
   routes by hardcoded class bits (`0xC0`/`0xB0`/high-byte/bare).  Register each
   as a `wubu_kernel_ops_t` so the router, not the class bits, decides.  This is
   the "works for Linux, Windows and macOS" half, and it is mostly adapters.
3. **VKD3D** (NT/SteamOS slot) — D3D12→Vulkan.  The largest single gaming gap.
4. **Shader pre-cache / fossilize** (SteamOS slot) — without it every Proton
   launch stutters on first shader compile.
5. **Steam Networking** (SteamOS slot) — P2P/relay/SDR.
6. **GVfs + Tracker** (GNOME slot) — the file-manager and search backends.
7. **makepkg / AUR / mkinitcpio** (Arch slot) — source builds + the boot-time
   hook chain.
8. **DolDoc / RedSea / raw VGA** (HolyC slot) — the TempleOS surface.
9. **AT-SPI2** (GNOME slot) — the a11y wire protocol.
10. **NT ALPC/WNF/Enclave** — only after the `nt-parity-wip` collision fix lands.

## 4. Honest status

- Shipped and verified: the interchange registry, contract, routing, swap log
  and dump — `test_kernel_xchg`, 60 assertions, all passing.
- The VSL personalities are **not yet registered** into the interchange; the
  class-bit router in `vsl_syscall_dispatch()` is still the live path.  Until
  step 2 lands, the network is a working skeleton with no production kernels
  in it — deliberately reported rather than papered over.
- The graft list above is ordered by leverage, not by ease.  Items 3–5 are the
  gaming surface; items 6–9 are the desktop surface.
