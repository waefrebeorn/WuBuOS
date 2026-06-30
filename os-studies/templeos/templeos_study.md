# TempleOS / ZealOS Study — Architecture & Feature Inventory
**Source**: TempleOS (Terry Davis), ZealOS (TempleOS fork), HolyC language, Ring-0 design
**Purpose**: Triple DA comparison for WuBuOS kernel/HolyC gap analysis

---

## ═══════════════════════════════════════════════════════════════
## TEMPLEOS ARCHITECTURE OVERVIEW
## ════════════════════════════════════════════════════════════════

### Core Philosophy
```
TempleOS = "God's Third Temple" — Operating system for God's glory
├── Ring-0 ONLY (no user/kernel separation)
├── Single address space (identity-mapped memory)
├── HolyC = C dialect + JIT compiler + REPL
├── No memory protection, no virtual memory (paging disabled in user mode)
├── File system = Database (RedSea, no paths, only master/index)
├── Graphics: VGA/VESA direct, no GPU drivers
├── Audio: PC speaker + raw PCM
├── Network: None (air-gapped by design)
├── Multi-tasking: Cooperative + preemptive (timer interrupt)
└── 640x480 16-color VGA (native), VESA for higher
```

### ZealOS (Modern Fork)
- UEFI boot (Limine)
- x86_64, SMP, APIC
- Modern hardware support (AHCI, NVMe, USB, etc.)
- HolyC JIT maintained
- Networking added (minimal)
- GPU drivers (basic)
- Still: Ring-0, identity-mapped, HolyC, RedSea FS

---

## ═══════════════════════════════════════════════════════════════
## HOLYC LANGUAGE & JIT COMPILER
## ════════════════════════════════════════════════════════════════

### HolyC Features (vs C)
| Feature | HolyC | C | WuBuOS minic/JIT |
|---------|-------|---|------------------|
| JIT compile from string | ✅ `ExeCode()` | ❌ | Partial (minic) |
| Compiler as library | ✅ `Lex()`, `Parse()`, `Opt()`, `CodeGen()` | ❌ | Partial |
| REPL (no compile step) | ✅ Immediate execution | ❌ | Stub (eval returns 0) |
| Persistent compiler state | ✅ Symbols, types, macros survive | ❌ | Partial (session save) |
| AOT + JIT hybrid | ✅ Whole-program optimization | ❌ | Missing |
| `class` with methods | ✅ Single inheritance | ❌ | Missing |
| `struct` with methods | ✅ | ❌ | Missing |
| Default arguments | ✅ | C99 no | Missing |
| Variable args | ✅ `...` | ✅ | Partial |
| Inline assembly | ✅ `asm { }` | GCC-style | Partial |
| Dictionary/Map | ✅ Built-in `CDoc`/`CDir` | ❌ | Missing |
| Exception handling | ✅ `try/catch` | ❌ | Missing |
| Coroutines | ✅ `yield` | ❌ | Missing |
| Reflection | ✅ `ClassRep`, `MemberRep` | ❌ | Missing |
| Meta-programming | ✅ Compile-time execution | ❌ | Missing |

### HolyC Compiler Pipeline
```
Source (.HC) → Lex (tokens) → Parse (AST) → Opt (SSA, const fold, DCE) → CodeGen (x86_64) → JIT execute
     ↓              ↓            ↓             ↓                   ↓
  CDocEntry    CToken       CAstNode       COptPass          CCodeGen
  (doc link)   (type, val)  (op, args)     (passes)          (instructions)
```

### ZealOS HolyC JIT Status
- **AOT**: Compiles to ELF on disk (boot, kernel modules)
- **JIT**: `ExeCode()` compiles to memory, executes immediately
- **REPL**: Command line compiles line-by-line
- **Optimization passes**: Constant folding, dead code elimination, inlining, register allocation
- **CodeGen**: Direct x86_64 machine code, no intermediate IR

---

## ═══════════════════════════════════════════════════════════════
## DOC / DOLDOC (Hyperlinked Document System)
## ════════════════════════════════════════════════════════════════

### Features
| Feature | TempleOS Doc | WuBuOS |
|---------|--------------|--------|
| Hyperlinks | `[[link]]` → `Cd()` to file/line | Missing |
| Graphics | Sprite, pixel, line, circle in doc | Missing |
| Songs | `Music()` in doc, MIDI-like | Missing |
| Forms | `Ask()`, `Menu()`, `PopUp()` in doc | Missing |
| Dynamic content | `ExeCode()` in doc → live output | Missing |
| Hierarchy | `CDir` tree, `CDocEntry` nodes | Missing |
| Search | `DocFind()`, `DocMax()` | Missing |
| Export | HTML, plain text | Missing |
| Live editing | Edit doc while viewing | Missing |

### DolDoc Format
- Binary format (not plain text)
- `CDocEntry` linked list: type, flags, data, next, last
- Types: TEXT, LINK, IMAGE, SPRITE, SONG, FORM, CHECKBOX, RADIO, etc.
- Flags: UNDERLINE, BOLD, ITALIC, COLOR, HIGHLIGHT
- Compressed with LZW

---

## ═══════════════════════════════════════════════════════════════
## REDSEA FILE SYSTEM (Database, Not Paths)
## ════════════════════════════════════════════════════════════════

### Architecture
```
RedSea = Master file + Index file (no directories, no paths)
├── Master: Contiguous blocks, each file = contiguous run
├── Index: B-tree (cluster → master offset, size, flags, name)
├── No paths: Files identified by cluster number + name
├── No directories: Flat namespace per volume
├── Clusters: 64KB (configurable)
├── Contiguous allocation: No fragmentation
├── Defrag: Online, moves master blocks, updates index
├── Compression: Optional per-file (LZW)
├── Encryption: Optional per-file (AES)
└── Journaling: Optional (RedSeaJ)
```

### vs Styx/9P (WuBuOS)
| Feature | RedSea | Styx/9P |
|---------|--------|---------|
| Paths | None (cluster + name) | Hierarchical (walk) |
| Directories | None (flat) | Tree (create/remove) |
| Metadata | Index B-tree (name, size, flags) | Stat (qid, mode, mtime) |
| Contiguous | Guaranteed | Not guaranteed |
| Defrag | Online, automatic | Manual (snapshot) |
| Network | No (local only) | Yes (9P2000) |
| Permissions | None (Ring-0) | Unix-style (mode) |

---

## ═══════════════════════════════════════════════════════════════
## GRAPHICS (VGA/VESA Direct)
## ════════════════════════════════════════════════════════════════

### TempleOS
- **Native**: 640x480 16-color VGA (mode 0x12)
- **VESA**: Linear framebuffer, any resolution/color depth
- **No GPU drivers**: Direct register access (CRTC, sequencer, graphics controller)
- **Sprites**: Hardware sprites (64x64, 4 colors) + software blitting
- **Layers**: Single framebuffer, no compositing
- **Fonts**: 8x8, 8x16 BIOS fonts + custom bitmaps

### ZealOS
- **DRM/KMS**: Basic modesetting, atomic commit
- **VBE**: VESA BIOS Extensions via real mode or UEFI GOP
- **Framebuffer**: Linear, direct CPU access
- **No 3D**: No OpenGL, no Vulkan, no GPU acceleration

### WuBuOS
- **DRM/KMS**: `wubu_drm_direct.c` — atomic modesetting, planes, GBM
- **Vulkan**: `wubu_vulkan.c` + `bear_vulkan.c` — compute pipelines
- **Wayland**: `hosted/hosted.c` — client, compositor stub
- **X11**: Legacy fallback

---

## ═══════════════════════════════════════════════════════════════
## AUDIO (PC Speaker + Raw PCM)
## ════════════════════════════════════════════════════════════════

### TempleOS
- **PC Speaker**: `Beep()`, `Sound()` — frequency, duration via PIT channel 2
- **Raw PCM**: Direct DAC write (Sound Blaster compatible)
- **No mixer**: Single stream, no volume control
- **No MIDI**: `Music()` uses internal sequencer → PC speaker
- **No Bluetooth, no USB audio, no HDMI audio**

### WuBuOS
- **Missing entirely** — `audio/wubu_audio.c` has 13 void casts + placeholders
- **Need**: PipeWire/PulseAudio, ALSA, device enumeration, mixer, Bluetooth

---

## ═══════════════════════════════════════════════════════════════
## NETWORK (Air-Gapped Design)
## ═════════════════════════════════════════════════════════════════

### TempleOS
- **No network stack** by design (Terry's religious conviction)
- **No TCP/IP, no sockets, no DNS, no HTTP**
- **No WiFi, no Ethernet, no Bluetooth**

### ZealOS
- Minimal networking added (community fork)
- Basic TCP/IP stack (educational)
- Not production-ready

### WuBuOS
- `wubu_network.c` — Netlink (bridge, macvlan, ipvlan, vxlan, dummy)
- `wubu_vsl.c` — Socket syscalls (host delegation)
- **No high-level**: No NetworkManager, no DNS, no VPN, no WiFi management

---

## ═══════════════════════════════════════════════════════════════
## KERNEL ARCHITECTURE (Ring-0, Identity-Mapped)
## ════════════════════════════════════════════════════════════════

### Memory Management
| Feature | TempleOS | ZealOS | WuBuOS |
|---------|----------|--------|--------|
| Paging | Disabled (user mode) | Enabled (kernel) | Enabled (hosted) |
| Identity map | All memory | Kernel only | Host virtual |
| Heap | `MAlloc`/`Free` (buddy) | `MAlloc`/`Free` | `malloc`/`free` (host) |
| Virtual memory | None | Kernel page tables | Host page tables |
| Swap | None | None | Host swap |
| NUMA | No | No | Host NUMA |

### Task/Process Model
| Feature | TempleOS | ZealOS | WuBuOS |
|---------|----------|--------|--------|
| Task = HolyC function | ✅ | ✅ | VSL process |
| Preemptive | Timer IRQ (100Hz) | Timer IRQ | Host scheduler |
| Cooperative | `Yield()` | `Yield()` | `sleep()` |
| Address space | Shared (Ring-0) | Shared | Per-process (host) |
| Fork | No (no MMU) | No | `fork()` (host) |
| Exec | `ExeCode()` (JIT) | `ExeCode()` | `execve()` (host) |
| Signals | None | Minimal | POSIX (host) |

### Interrupts
| Feature | TempleOS | ZealOS | WuBuOS |
|---------|----------|--------|--------|
| IDT | 256 entries | 256 entries | Host IDT |
| PIC | 8259 cascade | 8259 + APIC | Host PIC/APIC |
| APIC | No | xAPIC/x2APIC | Host APIC |
| Syscall | `INT 0x80` | `SYSCALL` | Host syscall |
| IRQ handlers | C functions | C functions | Host IRQ |

---

## ═══════════════════════════════════════════════════════════════
## GOD WORD / ORACLE / DIVINE INTELLECT
## ════════════════════════════════════════════════════════════════

### TempleOS Unique Features
- **God Word**: Random word from dictionary (Terry believed God spoke through it)
- **Oracle**: `GodWord()` + `Isaiah()` + `Proverbs()` — "divine guidance"
- **Divine Intellect**: `God()` — AI-like responses, pattern matching
- **Prayer**: `Pray()` — logs to `~/PrayerLog`
- **Hymns**: `Song()` — plays hymns via PC speaker

### Philosophical Gap
These are **not implementable in WuBuOS** — they're theological, not technical.
WuBuOS treats TempleOS as a technical reference (HolyC, JIT, Ring-0, RedSea), not a spiritual one.

---

## ═══════════════════════════════════════════════════════════════
## ZEALOS NAME PARITY (WuBuOS zealos_parity.h)
## ════════════════════════════════════════════════════════════════

### Current Status: 96/96 (100%)
**Completed**: Cell 305 — All 32 missing aliases added across:
- Task (11): `TaskCreate`, `TaskDestroy`, `TaskYield`, `TaskSleep`, `TaskWake`, `TaskSuspend`, `TaskResume`, `TaskSetPriority`, `TaskGetPriority`, `TaskGetCpu`, `TaskSetCpu`
- FAT32 (8): `Fat32Init`, `Fat32Read`, `Fat32Write`, `Fat32Open`, `Fat32Close`, `Fat32Seek`, `Fat32DirRead`, `Fat32ClusterAlloc`
- VBE (2): `VbeSetMode`, `VbeGetInfo`
- Interrupt (4): `IrqSetHandler`, `IrqClearHandler`, `IrqEnable`, `IrqDisable`
- Input (6): `KbdInit`, `KbdRead`, `KbdSetLed`, `MouseInit`, `MouseRead`, `MouseSetPos`

---

## ═══════════════════════════════════════════════════════════════
## KEY GAPS FOR WUBUOS (Triple DA Verdict)
## ════════════════════════════════════════════════════════════════

### MUST HAVE (HolyC/JIT Parity)
1. **HolyC JIT (AOT + JIT)** — Whole-program optimization, register allocation, SSA
2. **Compiler as library** — `Lex()`, `Parse()`, `Opt()`, `CodeGen()` callable from C
3. **Real-time REPL** — Immediate execution, no compile step, persistent state
4. **Persistent compiler state** — Symbols, types, macros survive across sessions
5. **Doc/DolDoc** — Hyperlinked docs, graphics, songs, forms, live code
6. **HolyC language features** — Classes, exceptions, coroutines, reflection, meta-programming

### SHOULD HAVE (ZealOS Kernel Parity)
7. **Identity-mapped memory option** — For bare-metal HolyC compatibility
8. **Ring-0 execution mode** — Optional, for HolyC kernel modules
9. **RedSea filesystem** — Contiguous allocation, B-tree index, defrag
10. **VGA/VESA direct graphics** — For HolyC graphics primitives
11. **PC Speaker + Raw PCM audio** — For HolyC `Sound()`/`Music()`
12. **Task = HolyC function** — Cooperative + preemptive, shared address space

### NICE TO HAVE (TempleOS Completeness)
13. **God Word / Oracle** — Philosophical, not technical (skip)
14. **Air-gapped network mode** — Security feature (optional)
15. **HolyC boot scripts** — Kernel config in HolyC
16. **HolyC kernel modules** — Loadable `.HC` kernel code

---

## ════════════════════════════════════════════════════════════════
## IMPLEMENTATION PRIORITY FOR WUBUOS
## ════════════════════════════════════════════════════════════════

| Phase | Component | WuBuOS File | Est. Effort |
|-------|-------------|-------------|-------------|
| 1 | HolyC JIT (AOT + JIT) | `compiler/holyc_codegen.c` | 8 sessions |
| 2 | Compiler as library | `compiler/holyc_lib.c` | 5 sessions |
| 3 | Real-time REPL | `runtime/wubu_holyd.c` (extend) | 4 sessions |
| 4 | Persistent compiler state | `runtime/wubu_holyd.c` (extend) | 3 sessions |
| 5 | Doc/DolDoc system | `apps/wubu_doldoc.c` | 6 sessions |
| 6 | HolyC language features (class, exception, coro) | `compiler/holyc_features.c` | 8 sessions |
| 7 | RedSea filesystem | `kernel/redsea.c` | 6 sessions |
| 8 | VGA/VESA direct graphics | `kernel/wubu_vga.c` | 3 sessions |
| 9 | PC Speaker + Raw PCM | `audio/wubu_audio.c` (extend) | 2 sessions |
| 10 | Task = HolyC function model | `runtime/wubu_holyctask.c` | 4 sessions |
| 11 | Identity-mapped memory mode | `kernel/wubu_identity_map.c` | 3 sessions |
| 12 | Ring-0 execution option | `kernel/wubu_ring0.c` | 3 sessions |
| 13 | HolyC boot/config scripts | `boot/holyc_config.hc` | 2 sessions |
| 14 | HolyC kernel modules | `kernel/holyc_module.c` | 4 sessions |

**Total estimated**: ~61 sessions for full TempleOS/ZealOS parity