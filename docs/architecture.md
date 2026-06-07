WuBuOS — Architecture Definition (LOCKED)

> This document defines the immutable high-level architecture for the My Seed OS.
> Changes require explicit unlock + re-lock ceremony.

## Vision

A personal, hackable "seed" OS combining:
1. ZealOS/TempleOS soul (ring-0, single-user, JIT, divine simplicity)
2. Win98/XP classic desktop (Start menu, taskbar, Explorer, retro-faithful)
3. Seamless DOS flip bridge (GUI ↔ Temple REPL)

## Layers (Bottom-Up)

### Layer 1: Core Kernel + JIT Runtime

**Source**: C-ported from ZealOS `.ZC` files + custom JIT engine

| Component | ZealOS Source | C Target | Priority |
|-----------|---------------|----------|----------|
| Memory | `Kernel/Memory/*.ZC` | `src/kernel/memory.c` | P0 |
| Interrupts | `Kernel/KExcept.ZC` | `src/kernel/interrupts.c` | P0 |
| Tasking | `Kernel/Sched.ZC` | `src/kernel/tasking.c` | P0 |
| VBE Graphics | `System/Gr/*.ZC` | `src/kernel/vbe.c` | P0 |
| Keyboard/Mouse | `Kernel/KeyDev.ZC` | `src/kernel/input.c` | P0 |
| FAT32 | `Kernel/BlkDev/FileSysFAT.ZC` | `src/kernel/fat32.c` | ✅ P1 |
| AHCI | `Kernel/BlkDev/DiskAHCI.ZC` | `src/kernel/ahci.c` | ✅ P2 |
| ISO 9660 | Custom builder | `src/tools/iso9660.c` | ✅ P4 |
| Compiler | `Compiler/*.ZC` | `src/compiler/` (hybrid) | ✅ P1 |

**JIT Engine**:
- Primary: MIR (C-to-MIR → native)
- Fallback: AsmJit (x86-64 machine code)
- Primitive: mmap(PROT_EXEC) (proven in jit_stub.c)
- API: `jit_compile_c()`, `jit_compile_holyc()`, `JIT_CALL()`

### Layer 5: Proton (Windows Compat)

**Proton** (`src/runtime/wubu_proton.c`):
- Windows PE32/PE64 binary validation and mapping
- Win32 → VSL syscall API translation (30+ APIs)
- 12 built-in Windows DLLs (kernel32, user32, gdi32, vulkan-1, etc.)
- Vulkan passthrough to VSL Vulkan driver (native performance)
- DirectX 9/11 → Vulkan translation via DXVK-style layer
- The "Proton within Proton": WuBuOS → VSL → Proton → Windows PE

### Layer 2: Bridge / DOS Flip + VBE↔WorldSim

**VBE↔WorldSim Bridge** (`src/bridge/vbe_ws_bridge.c`):
- Wires `ws_render_ctx_t.fb` to VBE's back-buffer
- Game loop: sim step → render → HUD → vbe_swap
- Camera: pan, center, zoom (0.25×–4.0×)
- HUD: FPS, frame/tick count, entity count, camera position
- Sim speed: configurable ticks-per-frame
- VBE_HOSTED: hosted test mode using calloc/free (avoids mem_alloc bug)

| Component | File | Description |
|-----------|------|-------------|
| Mode switcher | `src/bridge/switch.c` | GUI ↔ Temple toggle |
| Shared FS | (Layer 1 FAT32) | Same filesystem both modes |
| Clipboard | `src/bridge/clipboard.c` | Shared memory clipboard |
| IPC | `src/bridge/ipc.c` | Simple message passing |

**Modes**:
- `MODE_GUI`: Win98 desktop, windows, apps
- `MODE_TEMPLE`: Full-screen HolyC REPL (or windowed initially)

**Hotkey**: `Ctrl+Alt+T` or Start Menu → "Temple Mode"

### Layer 3: GUI Shell

| Component | File | Reference |
|-----------|------|-----------|
| Window Manager | `src/gui/wm.c` | NanoShellOS `src/wm/` |
| Desktop | `src/gui/desktop.c` | NanoShellOS `src/wapp/cpanel/desktop.c` |
| Taskbar | `src/gui/taskbar.c` | NanoShellOS `src/wapp/taskbar.c` |
| Start Menu | `src/gui/startmenu.c` | Custom (NanoShell has launcher) |
| Theme | `src/gui/theme.c` | 98.css → C drawing rules |
| Font | `src/gui/font.c` | 8×16 VGA bitmap (stb_truetype later) |

**Color Palette (Win98 Classic)**:
- Desktop: `#808080`
- Window face: `#C0C0C0`
- Title bar: `#000080` (active), `#808080` (inactive)
- 3D borders: white/light/highlight + gray/dark/black

### Layer 4: Apps

| App | File | Language | Phase |
|-----|------|----------|-------|
| Notepad | `src/apps/notepad.c` | C + JIT | 2 |
| Paint | `src/apps/paint.c` | C | 4 |
| Explorer | `src/apps/explorer.c` | C | 2 |
| Calculator | `src/apps/calc.c` | HolyC | 4 |
| Terminal | `src/apps/terminal.c` | C + JIT | 3 |
| HolyC REPL | `src/apps/repl.c` | C + JIT | 3 |
| LLM Chat | `src/apps/llm_chat.c` | C (bytropix) | 4 |

## Boot Sequence

```
BIOS/UEFI → Limine bootloader → ZealBooter (C prekernel)
  → My Seed kernel (C, ZealOS port)
  → JIT runtime init
  → GUI shell spawn
  → Desktop → user
  → [Ctrl+Alt+T] → Temple REPL
```

## Build System

- Cross-compiler: `x86_64-elf-gcc` for kernel
- Host compiler: `gcc` for tools
- Make: top-level `Makefile` with `kernel/`, `gui/`, `bridge/`, `apps/`
- Eventually: self-hosted TCC/MIR

## Testing

- Primary: QEMU `-serial stdio -display none`
- GUI: QEMU with VNC/display
- Fuzzing: slermes-style harnesses on JIT, filesystem, memory
- DA audit: Every milestone verified against reference

## Constraints (LOCKED)

1. **Pure C kernel** — no C++ in kernel, JIT, or bridge layers
2. **Ring-0 only** — no protection rings (like TempleOS)
3. **Single-user** — no multi-user, no permissions
4. **Under 100K LOC** — total kernel + JIT + GUI + bridge (excluding apps)
5. **Self-documenting** — like TempleOS, docs live in the OS
6. **Public domain** — TempleOS license for core, MIT for additions

---

## Layer Addition: WorldSim Engine

**Status**: ✅ Integrated (18/18 tests, 934 LOC)

WorldSim lives in Layer 1 (Core) alongside the kernel and JIT.

| Subsystem | Files | Description |
|-----------|-------|-------------|
| Terrain | terrain.c | 6-octave noise, island falloff, 5 biomes, thermal erosion |
| ECS | entity.c | 1024 entity pool, 7 component types |
| Physics | physics.c | Gravity, drag, AABB terrain collision |
| Render | render.c | Biome map, sprite blit, minimap (XRGB8888) |
| Sim | sim.c | Orchestration + 3 AI behaviors (wander/chase/flee) |

**Render wiring** (Cell 070 ✅): `ws_render_ctx_t.fb` → VBE back-buffer via `vbe_ws_bridge`. 25/25 tests. HUD overlay, camera, minimap, sim speed control.

---

*Locked: 2026-06-07*
*Unlock requires: explicit ceremony + DA audit of change*