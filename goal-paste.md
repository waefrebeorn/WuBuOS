# Goal Paste — WuBuOS 412 REAL_GAP Campaign (Triple DA Verified)

## Primary Goal
**Close 412 DA-verified REAL_GAPs — every gap = "rewriting from scratch in C".**

The 2185 automated "gaps" were 81% false positives (defensive `if (!ptr) return -1;`).
DA classification: empty bodies `{}`, `(void)param;` only, `return 0/-1` on SUCCESS path, `system()` fallbacks, incomplete protocols.

## Priority Order (Critical → High → Medium)

### 🔴 CRITICAL — Runtime Stack (142 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 1 | **wubu_network.c** | 28 | `system("ip link...")` netlink → ioctl; `system("tc qdisc...")` QoS; `system("wg/tailscale")` |
| 2 | **wubu_oci.c** | 31 | No TLS (line 75-80); `system("cp...")` layer copy (line 1140); no streaming blob I/O |
| 3 | **wubu_snapshot.c** | 24 | `mount()/umount2()` "non-fatal" (line 348); `system("cp -a...")` restore (line 656); `system("find/rm")` GC |
| 4 | **wubu_holyd.c** | 22 | `snprintf` truncation (512B); void casts `(void)win;(void)fb`; eval/compile return 0 stubs |
| 5 | **wubu_vsl.c** | 18 | ELF PT_LOAD minimal; syscall handlers return 0; drivers (Vulkan/CUDA/NET) stubs |
| 6 | **wubu_image.c** | 12 | `system("rm -rf/mkdir")` cleanup; multi-stage build dirs only; no actual build exec |
| 7 | **wubu_proton.c** | 7 | `mkstemp` + `system()` Wine launch; `setenv` only; DXVK INI write only |

### 🔴 CRITICAL — Kernel (67 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 8 | **interrupt.c** | 23 | No CPUID check for LAPIC; no MSI/MSI-X; no SYSCALL_STACK; PIC cascade only; no TSC deadline |
| 9 | **fat32.c** | 12 | `lfn_chk` unused; no LFN support; linear cluster scan; no pre-allocation |
| 10 | **txfs.c** | 10 | CRC only, no replay verify; commit not atomic; no auto-checkpoint |
| 11 | **ahci.c** | 8 | No FIS receive setup; sim disk only; polling only, no interrupt completion |
| 12 | **memory.c** | 8 | Single heap region; canary only on free, not alloc |
| 13 | **tasking.c** | 6 | Simple RR, no priority scheduler; no FPU/SSE save in context switch |
| 14 | **wubu_math.c** | 8 | Taylor series 6-term only; atan2 quadrant bugs (~0.025 rad); fixed-iter Newton-Raphson |

### 🟠 HIGH — GUI Shell (89 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 15 | **dosgui_wm.c** | 28 | Badge buffer truncation (8 bytes); "(stub)" notifies; no snap-to-grid; no focus config |
| 16 | **dosgui_explorer.c** | 18 | `system("cp -a")` file ops; `realpath()` no error handling; find/status text only |
| 17 | **dosgui_startmenu.c** | 10 | `snprintf` truncation (128B); no fs watcher for programs DB |
| 18 | **dosgui_desktop.c** | 12 | Placeholder window; fixed grid icons; solid color wallpaper only |
| 19 | **dosgui_term.c** | 8 | ANSI parsing stub; no pty management; no HolyC REPL session |
| 20 | **wubu_theme.c** | 5 | 4 hardcoded themes; no theme file loading; no CSS/INI parsing |
| 21 | **wubu_notify.c** | 4 | No timeout dismissal; no history |
| 22 | **wubu_proton/gamelib.c** | 4 | Steam/GOG/Epic API missing |

### 🟠 HIGH — Bear RL (54 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 23 | **bear_nn.c** | 18 | Checkpointing stub; no SIMD/AVX; backward pass stubs |
| 24 | **bear_ppo.c** | 12 | V-Trace placeholder; GAE lambda=1 only; fixed epsilon clip |
| 25 | **bear_vulkan.c** | 14 | "These are stubs", "TODO: Implement full forward", "TODO: GAE dispatch", "TODO: env step" |
| 26 | **bear_cudnn.c** | 6 | `void* handle /* stub */`; all return NOT_INITIALIZED |
| 27 | **bear_vulkan_soft.c** | 4 | "Remaining API stubs (software no-ops)" |

### 🟠 HIGH — Hosted Platform (38 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 28 | **wubu_drm_direct.c** | 14 | Manual uAPI structs; no atomic commit; no planes; no modifiers; no hotplug/EDID |
| 29 | **wubu_vulkan.c** | 10 | No loader API chaining; first GPU only; no surface init; no features; no swapchain recreate |
| 30 | **wubu_metal.c** | 8 | 5 empty `{}` shutdown/flip; "Simple stub — in future use GAAD" |
| 31 | **wubu_gbm.c** | 3 | Boilerplate only; no modifiers/multi-planar |
| 32 | **hosted.c** | 3 | Styx 9P read/write return 0 for dirs |

### 🟡 MEDIUM — Compiler (9 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 33 | **holyc_codegen.c** | 5 | Placeholder rel32 emit/jcc/jmp — fragile manual patch tracking |
| 34 | **holyc_ptx.c** | 4 | "TODO: Load matrix tiles" in generated PTX; CUDA driver stub; gpu_matmul returns 0 |

### 🟡 MEDIUM — Apps (13 gaps)

| # | File | Gaps | DA-Verified Issues |
|---|------|------|-------------------|
| 35 | **wubu_editor.c** | 5 | "Code folding" comment only; keyword highlight only; linear search/replace |
| 36 | **wubu_canvas.c** | 4 | "Drawing (stubs)", "Filters (stubs)", "Canvas ops (stubs)" — explicit |
| 37 | **wubu_codec.c** | 2 | Audio/video decode stubs; no FFmpeg/libav |
| 38 | **control/explorer/calc.c** | 2 | `*_shutdown()` empty bodies `{}` |

### 🟡 MEDIUM — Audio (0 gaps — ARCHITECTURAL GAP: ENTIRE SUBSYSTEM MISSING)
- Furnace (12 chips: NES, SNES, GB, Genesis, etc.) — NOT IMPLEMENTED
- TinySoundFont (SF2 parser: RIFF pdta/sdta, samples, envelopes, modulators) — NOT IMPLEMENTED
- Ardour DAW parity (sample-accurate automation, LV2/VST3/CLAP, JACK, AAF/OMF, video sync) — NOT IMPLEMENTED
- AI plugin container streaming — NOT IMPLEMENTED

### 🟡 MEDIUM — Bridge (0 gaps — FUNCTIONAL)
- Syscall bridge: 26 ZealOS-compat syscalls functional
- DOS↔Temple flip: functional

### 🔵 LOW — Tools/Shell/JIT (0 gaps — FUNCTIONAL)
- ISO9660, screenshot, weight_check — all tests pass
- wubu_shell.c — 9P namespace ops functional
- JIT encoder/disasm/ASMJIT/MIR/minic — all tests pass

---

## Work Mode
- Pick ONE gap at a time from the tables above
- Write real C that does real work (replace `system()`, fill `{}`, use `(void)params`)
- Test passes (`make test_XXX`)
- Update BATTLESHIP.md when gap closed

---

## DA Verdict
**"Rewriting from scratch in C" is the point.** The 412 gaps above ARE the work. Everything else (747+ tests passing) is the foundation we build on.