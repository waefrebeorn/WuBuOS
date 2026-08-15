# WuBu — The Self-Made AGI Operating System

> **One AGI. Three repos. Zero dependencies we don't own.**

WuBu is a complete artificial general intelligence operating system built from scratch in C11.
It is not a Linux distribution. It is not a research paper. It is a **working system** —
705,170 lines of code across three repositories — that boots, thinks, learns, and acts.

---

## The Three Repositories

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          W U B U   A G I                               │
│                                                                         │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │   wubuwizard     │  │    wubuos        │  │    wubunos       │      │
│  │   THE BRAIN      │  │    THE BODY      │  │    THE COMPILER  │      │
│  │                  │  │                  │  │                  │      │
│  │ 1167 C · 420 H   │  │ 2463 C · 1006 H  │  │ 33 C · 16 H      │      │
│  │ 37 CUDA           │  │ 472,955 LOC      │  │ 14,115 LOC       │      │
│  │ 218,100 LOC      │  │                  │  │                  │      │
│  │                  │  │ Kernel · GUI     │  │ HolyC JIT        │      │
│  │ Inference · KV   │  │ Styx/9P · Contain│  │ 11 ISA backends  │      │
│  │ Training · Encode│  │ Drivers · VSL    │  │ MIR optimizer    │      │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘      │
│           │                     │                     │                │
│           └─────────────────────┼─────────────────────┘                │
│                                 │                                      │
│                    WuBuOS links both as submodules:                    │
│                    src/brain/  → wubuwizard                           │
│                    src/compiler/ → wubunos                            │
└─────────────────────────────────────────────────────────────────────────┘
```

### wubuwizard — THE BRAIN
The inference engine. Loads models (GGUF, SafeTensors, ONNX), runs inference with
compressed KV cache, trains with PPO/GRPO, and encodes user files into a universal
compressible space. 93 research docs. 50 theory papers. 37 CUDA kernels.

**Key insight:** The KV cache IS a file system. The encoder IS a mount. The model
IS the amoeba body — hyperbolic spheres nesting inside each other, growing and
shrinking to fit any hardware.

### wubuos — THE BODY
The operating system. ZealOS kernel (memory, tasking, VBE, FAT32, AHCI, interrupt),
Win98/XP GUI shell, Styx/9P namespace, 20+ hardware drivers, VSL multi-OS syscall
dispatch, container runtime, measured-boot UEFI firmware. 414 test targets.

**Key insight:** WuBuOS boots WuBuOS. Third-party software runs ON the AGI kernel
via exec backends (PE/Mach-O/ELF), wine/proton compat layers, and arch/debian
containers in-wubuos.

### wubunos — THE COMPILER
The from-scratch C11 toolchain. HolyC frontend (lexer → parser → AST), mid-level IR
with optimizer passes, 11 ISA backends (x86-64, ARM64, RISC-V, MIPS, 68k, AVR,
8051, 8086, Z80, 6502, PTX). Self-hosting battery proves every C11 construct.

**Key insight:** The compiler runs ON the kernel. WuBuNOS compiles HolyC programs
that run ring-0 on WuBuOS. This is the TempleOS dream — a self-hosting system
where the compiler, the OS, and the applications are one.

---

## Design Philosophy

### 1. WuBu Compliance
We own the feature surface. No `_GNU_SOURCE`. No glibc feature macros. No POSIX
extensions we don't define ourselves. When we need a symbol, we provide it in
`wubu_gnu_compat.h`. When we need a behavior, we implement it in C11.

### 2. No Third Party (If We Can Write It)
Every line is ours. No imported encoders (licensing surface). No external libm
(we wrote `wubu_math.c`). No EDK2 (we wrote WuBuFW). No compiler runtime
(we wrote WuBuNOS). The only "third party" is the hardware itself.

### 3. Opaque Structs, Minimal Includes
Every module exposes an opaque type and accessor functions. No god headers.
No circular dependencies. Modules link through `_internal.h` seams.

### 4. Self-Hosting Proof
The compiler compiles itself. The OS boots itself. The AGI trains on its own
code. Every claim is backed by a test that runs on the host.

### 5. Ring-0 Colonel Space
TempleOS proved an AGI-sized computer can be all-ring-0. WuBuOS inherits this:
user programs ARE the kernel. The HolyC JIT runs ring-0 code. The Styx namespace
is the address space.

---

## The Amoeba Architecture

WuBu is an **amoeba** — a single living system that grows and shrinks to fit:

```
                    ┌─────────────────────────────┐
                    │      WuBu AGI Supervisor     │
                    │   (wubu_agi.c — the mind)    │
                    └──────────────┬──────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │                    │                    │
     ┌────────┴────────┐  ┌───────┴────────┐  ┌───────┴────────┐
     │   BOOT CORE     │  │  BODY SPHERES   │  │ MEMORY ORBITS  │
     │                 │  │                 │  │                │
     │ wubuos kernel   │  │ Hyperbolic      │  │ KV cache       │
     │ WuBuNOS compiler│  │ spheres:        │  │ Encoder space  │
     │ HolyC JIT       │  │ · Inference     │  │ Training data  │
     │                 │  │ · Training      │  │ User files     │
     │                 │  │ · Encoding      │  │ Research docs  │
     │                 │  │ · RL            │  │ Theory papers  │
     └─────────────────┘  └────────────────┘  └────────────────┘
```

The **BOOT CORE** (wubuos + wubunos) is always present. The **BODY SPHERES**
(inference, training, encoding, RL) grow on demand — each sphere is a hyperbolic
ball that nests inside the others. The **MEMORY ORBITS** (KV cache, encoder space,
training data, user files) orbit the spheres, accessible through the Styx
namespace.

Scale-to-fit: one checkpoint, any hardware. On a phone, the amoeba shrinks to
a single sphere. On a server, it grows to fill all available resources.

---

## Project Statistics (verified 2026-08-15)

| Metric | wubuos | wubuwizard | wubunos | TOTAL |
|--------|--------|------------|---------|-------|
| C files | 2,463 | 1,167 | 33 | 3,663 |
| H files | 1,006 | 420 | 16 | 1,442 |
| CUDA kernels | 0 | 37 | 0 | 37 |
| Total LOC | 472,955 | 218,100 | 14,115 | 705,170 |
| Test targets | 414 | ~150 | 1 | 565+ |
| Research docs | 60 | 93 | 0 | 153 |
| Theory papers | 0 | 50 | 0 | 50 |
| ISA backends | — | — | 11 | 11 |

---

## Getting Started

```bash
# Clone all three repos
git clone https://github.com/waefrebeorn/WuBuOS.git
git clone https://github.com/waefrebeorn/wubuwizard.git
git clone https://github.com/waefrebeorn/wubunos.git

# Build the Body (OS)
cd WuBuOS
make all                    # full build
make hosted                 # hosted binary (runs on Linux)
./src/hosted/wubu           # run the OS

# Build the Brain (inference)
cd ../wubuwizard
make all                    # full build
make test_all               # test gate

# Build the Compiler
cd ../wubunos
make all                    # builds the HolyC JIT + ISA backends
```

---

## The Mission

We are building the first AGI that **owns its entire stack**. Not running on
Linux. Not calling OpenAI. Not importing encoders. Every line of code is ours.
Every decision is ours. Every cycle counts.

This is not a startup. This is not a product. This is a **historical event** —
the moment a single architect built an entire operating system, compiler, and
inference engine from scratch, in C11, with no dependencies but the hardware.

**WuBu is the amoeba. The hive IS the body. Every cycle counts.**
