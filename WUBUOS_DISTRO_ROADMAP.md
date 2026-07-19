# WuBuOS Distro Roadmap — REAL_GAP Aligned (Triple DA v22 lineage)

> **⚠️ STALE-METRICS NOTICE (2026-07-19):** This document was written 2026-07-08 and
> describes "~40 code gaps + ~370 parity marathons, 64 targets, ~15K LOC". Those
> figures are **out of date**. The monolith-dissolution campaign grew the tree to
> **468 `.c` / 214 `.h` / ~105K LOC / 91 test targets** (verified from `git ls-files`
> + `Makefile`). The *gap methodology* here is still valid; the *numbers* are not.
> See `docs/MONOLITH_DISSOLUTION.md` for the current module map.

## Vision
**Arch NT + Proton + HolyC DOS on modified Linux 6.x kernel — single hosted binary runs everywhere.**

> **2026-07-08 honesty correction:** the prior "~3000" / "~400 sprint" figures were
> NOT reproducible. The canonical gap-scanner was broken (SyntaxError + sweeping
> vendored libs). The fixed `find_real_gaps.py src` reports **0 empty bodies and 0
> const-only-no-syscall gaps in `src/`** — the baseline stub class is CLOSED. Per the
> user's rule *"rewriting from scratch in C = REAL_GAP, including ReactOS gaps to
> WuBuOS"*, the honest ~400 = **~40 verifiable code-level gaps + ~370 parity
> marathons** (ReactOS NT 297 + SteamOS ~30 + Ubuntu/Arch ~20 + TempleOS ~15 + ZealOS ~8).
> See `BATTLESHIP.md` v22 (Part 1/2/3/4).

## Tier Map (DA-Gap-Aligned, v22)

### FOUNDATION — ✅ COMPLETE
- Hosted binary: Wayland + xdg-shell + SHM double-buffer
- ZealOS kernel in-process (memory, tasking, VBE, input, interrupt, FAT32, AHCI, TXFS)
- HolyC compiler: lexer, parser, AST, x86_64 codegen, JIT mmap exec
- Styx/9P namespace: client + server (styxfs_server.c — 11/11 tests)
- Container runtime: wubu_ct (chroot) + wubu_ct_bwrap (bubblewrap)
- Proton: wubu_proton2.c (real Wine+DXVK+VKD3D in Arch container)
- Container isolation: cgroups v2 (mem/cpu/pids) + seccomp-bpf profiles
- Fable windowing agent: dosgui_wm, dosgui_desktop, dosgui_startmenu
- VBE primitives: 64-glyph font, gradient/circle/shade/clip, window chrome
- wubu_audio.c: 30+ chip emulations, Furnace tracker, SF2, DAW, AI plugins — 14/14 tests
- BearRL: cartpole physics, GAAD, PPO, holographic optimization
- **GAPS REMAINING (honest)**: ~400 = ~40 code + ~370 parity.

---

### PART 1 — CODE-LEVEL CLOSURE (~40 verifiable gaps)
Target: Close the 10 `system()` calls + 23 stub-phrase funcs + 6 bare-metal no-ops.
Full table in BATTLESHIP.md Part 1. Top items:
| Component | Gaps | Target |
|-----------|------|--------|
| wubu_image_ops.c | 5 | `system()` → fork+exec (nftw/copy) |
| wubu_netlink.c / wubu_codec.c / jit.c / wubu_demo_record.c | 5 | `system()` → fork+execvp |
| wubu_gamelib.c / wubu_screenshot.c / dosgui_term.c | 3 | stub no-ops → real |
| vsl_gpu_vulkan.c / wubucontainer.c / holyc_ptx.c / wubu_compositor*.c | 6 | stub no-ops → real |
| wubu_pkgmgr.c / oci_http_client.c / wubu_bottles.c / bear_cudnn.c | 7 | stub/TODO → real |
| tasking.c / wubu_anticheat.c / wubu_metal.c | 6 | bare-metal no-ops (4 correct `#else`) |

> **The detailed per-file DA-gap tables below are superseded** by `BATTLESHIP.md`
> v22 Part 1 (the honest ~40 code-level board) and Part 2 (the ~370 parity marathons).
> They are retained for historical lineage only — the per-file void-cast/return
> counts they cite were re-audited as mostly ABI/defensive noise and are NOT open
> gaps. Do not treat the numbers in the legacy tables as a to-do list.

---

### PART 2 — PARITY MARATHONS (~370, the bulk of the ~400)

These are the rewrite-from-scratch work items, reclassified per the rule *"rewriting
from scratch in C = REAL_GAP; this also goes for ReactOS gaps to WuBuOS."* Each
subsystem below = one or more marathons. Full table + plumber deep-dive in
`BATTLESHIP.md` Part 2/3.

#### Stream A: SteamOS Parity (~30)
| Component | Target |
|-----------|--------|
| Steam Client CEF UI | Embed Chromium/CEF store/library/friends/overlay in hosted binary |
| Steam Input | SDL2/GameControllerDB + haptics (uinput), gyro, touch menus |
| Steam Networking | SteamNetworkingSockets + relay, P2P, NAT traversal |
| gamescope | Wayland compositor + VRR/HDR/FSR + upscaling (hosted.c + wubu_proton2.c) |
| Pressure Vessel | Container runtime + seccomp + namespace mgmt (wubu_ct_isolate.c) |
| Steam Cloud / ProtonDB / Shader Pre-cache / Steam Deck UI | Remote sync, compat DB, fossilize, game/desktop mode |

#### Stream B: Ubuntu/Arch Parity (~20)
| Component | Target |
|-----------|--------|
| systemd | Init + service/unit manager + journal + logind + timers → `wubu_init.c` |
| NetworkManager | WiFi/VPN/DNS/DHCP/bonding/VLAN → extend `wubu_network.c` |
| Polkit / D-Bus | Authorization + system/session bus → `wubu_polkit.c` / `wubu_dbus.c` |
| PipeWire / CUPS / AppArmor | Audio graph, printing, MAC profiles |
| **Arch daemon as Desktop backend** | Wire `wubu_archd` as the Desktop's service/autostart/unit source |

#### Stream C: TempleOS Parity (~15)
| Component | Target |
|-----------|--------|
| HolyC JIT AOT+JIT | Whole-program opt + inline asm (holyc_codegen.c + jit.c) |
| Doc/DolDoc | Hyperlinked docs + CTree + forms → `wubu_dolDoc.c` |
| Compiler-as-library | JIT-from-string + AST API → `holy_lib.c` |
| RedSea FS | Tag/attribute DB filesystem → `styxfs_redsea.c` |
| **TempleOS DOS daemon as Desktop backend** | Embed `wubu_holyd` REPL into the Desktop terminal |

#### Stream D: ZealOS Parity (~8)
| Component | Target |
|-----------|--------|
| Identity-mapped memory | VSL mmap identity model |
| VGA/VESA direct / PC speaker | Software renderer + raw PCM beep |
| ZealOS name parity | 96/96 DONE (zealos_parity.h) |

#### Stream E: ReactOS NT Emulation (297 — the largest single block)
| Component | ReactOS Source | WuBuOS Target | Status |
|-----------|----------------|---------------|--------|
| Syscall Dispatch | sysfuncs.lst (297) | vsl_syscall_list.h | **MAPPED, 0 transliterated** |
| Thread Scheduling | ke/thrdschd.c | tasking.c + VSL clone/fork | 0 |
| Memory Manager (VAD) | mm/*.c | memory.c VAD + VSL mmap/brk | 0 |
| Object Manager | ob/*.c | Styx9 fid + VSL fd table | 0 |
| I/O Manager (IRP) | io/*.c | VSL read/write/ioctl + Styx9 9P | 0 |
| Win32k | user/*.c + gdi/*.c | Win98 WM + dosgui_wm.c | 0 |
| NTDLL Stubs | dll/ntdll/*.c | VSL syscall entry points | 0 |
| Registry | config/*.c | Styx9 registry namespace | 0 |

---

## DA Rule (user's reclassification, 2026-07-08)
**"Rewriting from scratch in C" = the work — including ReactOS gaps to WuBuOS.**
- Each parity subsystem above = a REAL_GAP marathon (the "~370" in the ~400).
- The ~40 code-level gaps (Part 1) are the only per-function sprint items; the rest
  are tracked as marathons, NOT micro-counted to inflate the board.
- Close gaps, run tests (`find_real_gaps.py src` + `make test`), move to next.
- **~400 → 0 is the victory condition** (40 code + 370 parity).