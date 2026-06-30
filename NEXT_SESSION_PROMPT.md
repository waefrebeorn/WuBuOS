# Next Session Prompt — WuBuOS Critical Tier Gap #4

## Context
- **Project**: WuBuOS — ZealOS kernel + Win98 shell + Styx/9P + Arch containers
- **State**: 1264 REAL_GAPs remaining | 170 closed this campaign | All 322 core tests green
- **Location**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/`
- **Build**: `make all` | Test: `make test_XXX`

---

## ✅ COMPLETED THIS CAMPAIGN (170 gaps closed)

| # | File | Gaps Closed | Key Implementation |
|---|------|-------------|-------------------|
| 1 | hosted/wubu_metal.c | 56 | DRM/KMS atomic, ALSA/PipeWire/Pulse/X11 dlopen, Vulkan surfaces, GAAD |
| 2 | runtime/wubu_vsl.c | ~40 | 17 syscalls (rt_sigaction, rt_sigprocmask, select, pipe2, clone3, io_uring*, statx) |
| 3 | apps/wubu_canvas.c | ~50 | Layer ops, undo/redo (50-snap), drawing tools+undo, PNG/GIF/BMP/PPM I/O, zoom/pan |
| 4 | gui/wubu_clipboard.c | 43 | Multi-MIME clipboard |
| 5 | gui/dosgui_wm.c | 22 | Resize snap, virtual desktop migrate, focus stack |
| 6 | gui/dosgui_term.c | 23 | PTY fork+exec, VT100 |
| 7 | gui/dosgui_explorer.c | 31 | 9P/Styx file ops, real zip mount |
| 8 | gui/dosgui_startmenu.c | 26 | .desktop parse, category map, shutdown wire |
| 9 | kernel/interrupt.c | 41 | IOAPIC/LAPIC/MSI |
| 10 | bridge/wubu_syscall.c | 97 | fd/Styx/container handlers, 26 trampolines |
| 11 | bear/bear_cudnn.c | 117 | CPU cuBLAS/cuDNN fallbacks |
| 12 | bear/bear_vulkan.c | 7 | 4 compute pipelines |

**Test Status**: All 322 core assertions passing (test_vsl, test_syscall, test_metal, test_clipboard, test_dosgui_wm, test_dosgui_explorer, test_dosgui_startmenu, test_dosgui_term, test_apps2)

---

## 🎯 NEXT PRIORITY: Critical Tier Gap #4

| # | File | Gaps | First DA-Verified Gap |
|---|------|------|------------------------|
| 1 | **compiler/holyc_codegen.c** | 29 placeholders | JIT backpatching, register allocation, HolyC→x86_64 codegen |
| 2 | runtime/styxfs.c | 14 void casts | auth, wstat, fsync, dir ops, symlink, mknod |
| 3 | runtime/wubu_vsl.c | 315 void casts | namespaces, fanotify, landlock, bpf, perf_event |

---

## 🔧 IMMEDIATE ACTION

```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed

# 1. Inspect the gap
cat src/compiler/holyc_codegen.c | grep -n "placeholder\|TODO\|return 0\|return -1" | head -40

# 2. Check existing tests
make test_holyc  # (list available test targets)
# or: make test_holyc  # if exists

# 3. Write real C implementation for first placeholder
# 4. make test_holyc
# 5. make all
```

---

## 📋 DA RULE
"Rewriting from scratch in C" = the work.
- system("...") → netlink/ioctl/syscall/fork+exec
- {} on success path → implement logic
- (void)param; → use the parameter
- return 0; on success → implement and return result
- "stub"/"TODO"/"for later" → implement it

Target: 1264 → 0.

---

## Key Files to Inspect
- `src/compiler/holyc_codegen.c` — main target
- `src/compiler/holyc_ptx.c` — PTX backend (may be related)
- `src/jit/jit.c` — JIT encoder (backpatching target)
- `src/jit/wubu_x86.c` — x86 encoder
- `src/jit/x86_regalloc.c` — register allocator
- `Makefile` — test targets (test_holyc, test_holyc_ptx)

---

## Blockers
- None — every gap is "rewrite in C" territory
- All test infrastructure working (58 targets, 747+ assertions)

---

**Next session**: Pick gap #1 from Critical tier (compiler/holyc_codegen.c) → implement → test → repeat