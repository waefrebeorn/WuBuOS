# Goal Paste — WuBuOS ~3000 REAL_GAP Campaign (Triple DA Verified)

## Primary Goal
**Close ~3000 DA-verified REAL_GAPs — every gap = "rewriting from scratch in C".**

The automated "gaps" were 81% false positives (defensive `if (!ptr) return -1;`).
DA classification: empty bodies `{}`, `(void)param;` only, `return 0/-1` on SUCCESS path, `system()` fallbacks, incomplete protocols, missing entire subsystems vs SteamOS/Ubuntu/TempleOS/ZealOS/ReactOS.

## Priority Order (Critical → High → Medium) — PARALLEL EXECUTION

### 🔴 CRITICAL — Code-Level Gaps (1423 from stub hunt)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 1 | **wubu_holyd.c** | 57 | HolyC REPL returns 0, compiler state placeholder, `snprintf` truncation |
| 2 | **wubu_metal.c** | 55 | 5 empty `{}` shutdown/flip, audio backends dlopen-only, X11/Vulkan stubs |
| 3 | **wubu_network.c** | 52 | `system("ip link...")` netlink, `system("tc...")` QoS, `system("wg/tailscale")` |
| 4 | **styxfs.c** | 51 | 9P callbacks return 0/empty, Styx offset tracking stubs |
| 5 | **interrupt.c** | 47 | No CPUID LAPIC check, no MSI/MSI-X, no SYSCALL_STACK, PIC cascade only |
| 6 | **wubu_snapshot.c** | 43 | `mount/umount2` "non-fatal", `system("cp -a")` restore, `system("find/rm")` GC |
| 7 | **wubu_oci.c** | 41 | No TLS, `system("cp...")` layer copy, no streaming blob I/O |
| 8 | **styx.c** | 41 | 9P protocol handlers return 0/empty |
| 9 | **wubu_x86.c** | 36 | Placeholder rel32 emit, JCC/JMP backpatch tracking fragile |
| 10 | **vsl_syscall.c** | 36 | 173 void casts (6-reg ABI), namespaces/fanotify/landlock/bpf stubs |

### 🔴 CRITICAL — Architectural Parity (1572)

| Stream | Target | Gaps | Key Missing |
|--------|--------|------|-------------|
| **A** | SteamOS Parity | ~400 | Steam Client CEF, Steam Input, Steam Networking, Proton, gamescope, Pressure Vessel |
| **B** | Ubuntu/Arch Parity | ~450 | systemd, apt/pacman, NetworkManager, Polkit, D-Bus, PipeWire, CUPS, AppArmor |
| **C** | TempleOS Parity | ~350 | HolyC JIT AOT+JIT, Doc/DolDoc, Compiler-as-library, RedSea FS, Ring-0 |
| **D** | ZealOS Parity | ~200 | Identity-mapped memory, VGA/VESA direct, PC speaker, God word |
| **E** | ReactOS NT Emulation | ~162 | 297 syscalls → VSL/Styx9/ZealOS/TempleOS pipeline |

### 🟠 HIGH — Remaining Code (10)
- bear_env.c (13): MuJoCo/Atari/custom env API
- tasking.c (6): No priority scheduler, no FPU/SSE save
- wubu_math.c (8): Taylor series bugs, fixed-iter Newton-Raphson

---

## Work Mode
- Pick gaps from **ALL streams in PARALLEL** — no "pick one"
- Write real C that does real work (replace `system()`, fill `{}`, use `(void)params`)
- Test passes (`make test_XXX`)
- Commit → repeat

---

## DA Verdict
**"Rewriting from scratch in C" is the point.** The ~3000 gaps above ARE the work. Everything else (747+ tests passing tests passing) is the foundation we build on.

**PARALLEL UNTIL 3000 → 0**