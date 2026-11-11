# The ~WuBu~ Ecosystem — how the repositories cohere

**Grounded 2026-11-11** by reading every repo's own README. This is the map of
"one AGI, many repositories" — the cohesion that `WuBuOS/README.md` reduces to
"three repos, one system". It is bigger than three, and it layers.

The single rule of the family, stated once: **everything that claims ~WuBu~ is
C11, from-scratch, and zero-third-party** — a SLERM (a ground-up
reimplementation, not a fork). The one exception is the import batch, which is
sourced, archived, and marked, so nobody reads a fork as an own-repo by
accident.

## Layer 0 — the three pillars

| repo | name | what it is |
|---|---|---|
| **WuBuOS** | the BODY | the operating system this README documents |
| **wubuwizard** | the BRAIN | `wubuwizard` — the amoeba AGI: C11 inference/training engine + KV-cache FS. "One AGI, two repos: this Brain + wubuos the Body." |
| **WuBuNOS** | the COMPILER | the C11 toolchain that runs ON the kernel: HolyD frontend, MIR + optimizer, 14 ISA backends, self-hosting battery |

## Layer 1 — what the Body hosts (submodules of WuBuOS)

`git submodule` answers it definitively. Each of these is its own repo, wired
into `src/`:

| repo | mount | what it is |
|---|---|---|
| **wubuwizard** | `src/brain/` | the Brain |
| **Wua** | (the Lua surface) | `Wua` — slerm-Lua, a ground-up reimplementation of Lua 5.4 in C11; the script substrate |
| **WWW** | `src/lua/www/` | the World Wide WorldSpace — the ~WuBu~ space engine (C11 memory/ECS/physics, Lua logic/GUI) |
| **WET** | `src/gui/wet/` | `.WET` — "WuBu's .net": declarative, scriptable applets over the Win98 shell |
| **VWS** | `src/gui/vws/` | the Visual WuBu System — "WuBu's Visual Basic": form designer + code-behind over .WET |
| **WuBuPad** | `src/apps/wubupad/` | the clean-room C11 code editor (Notepad++ parity) |
| **WuBuOffice** | `src/apps/wubuoffice/` | the from-scratch OOXML/OpenDocument office suite |
| ~~**WuBuContainer**~~ | *(removed)* | the container is in-tree at `src/runtime/container/*.c`; the GitHub repo is an unrelated web converter and the broken submodule was dropped. |

## Layer 2 — extracted subsystems (standalone so other things can reuse)

| repo | what it is |
|---|---|
| **WuBuHIVE** | the hive memory substrate — mutable state with stable addresses — extracted from the WWW engine |
| **BearRL** | the math/RL environment + proof engine, extracted from `WuBuOS/src/bear/` |
| **WuBuMM** | the WuBuDesk native media engine — runs omni-modal models via the C11 kernel + Vulkan compute work |
| **wubufw-tools** | C11 tools that write PE32+/FAT32-GPT on-disk structures by hand, so the WuBuFW firmware's readers are tested against exactly what we emit |

## Layer 3 — the slermed applications (signature ground-up rewrites)

| repo | what it is |
|---|---|
| **wubucodec** | the ~WuBu~ video codec — README claims 22 of 24 SOTA categories closed against AV1 |
| **wuburvc** | the voice engine — from-scratch C11 RVC (HuBERT→content→Vocoder), parity-verified to corr 0.9999 |
| **WuBuMath** | C11 math + the WUBQ codec, slermed from Python/JAX: manifolds, SIMD GEMM, MLP, the Hamilton encoder |
| **wubugrep** | a from-scratch grep — "written by an AI agent that was not allowed to lie" |
| **wubuedit** | the SLERM ground-up C11 video editor |
| **quantum_rng_audit** | a secure RNG on a real quantum-measurement simulation, with NIST SP 800-90B health testing |
| **distiller-c11** | the Distiller One device path, reimplemented from scratch |

## Layer 4 — the ~WuBu~ distributed space

The "one truth and many copies" layer — this is the peer-to-peer / governance
face of the family, and it is where the distributed-AGI vision (see
`docs/SAGA.md`) lives as repos:

| repo | what it is |
|---|---|
| **WuBuFederation** | the registry the layers share; agents join here (JOINING.md, agent-protocol.json) |
| **WuBuCore** | the C11 operations core: citizenship numbers, the Penny Score, the PennyChain ledger |
| **WuBuCitizen / WuBuGov / WuBuIntel / WuBuEconomics / WuBuResearch** | the seven organs — registry, state, vigilance, economics, research |
| **WuBu** | **the P2P Distributed Inference Network** — constrained nodes pool compute via bittorrent-style model-shard distribution, incentivized by RTC tokens. This is the sibling of `wubu_swarm` (networking/aggregation spell in this repo). |
| **wubutv-network** | WuBuTV — the media network: every piece is one genuine 10s scene, 1280x720, native audio |
| **WuBuMedia** | the stream-native public face layer, "the friendly Hollywood cohost" |
| **slermes** | SLERM of Hermes Agent into C11 — a single native binary, zero Python |
| **W-Code** | the WuBuOS agentic IDE — human (raw-termios TUI) and agent (JSON on stdin) frontends over one core |
| **WWW** *(again)* | the space itself |

## Layer 5 — the honest import batch (archived, NOT own-repo)

All pushed 2026-07-28 in one batch. These are **third-party works**, and the
~WuBu~ rule means they must never be mistaken for slerms:

- `WuBuVideo` / `WuBuCut` — OpenCut (archived legacy + a web editor)
- `WuBuSkills` — NVIDIA Agent Skills
- `WuBuFX` — StreamFX (an OBS plugin)
- `WuBuTool_movfuscator` — movfuscator
- `KAN-WuBu-Memory`, `WuBu-DLP`, `WuBuWallPaper`, `WuBuCapture` — mixed; treat as sourced until read

## What this means for reading WuBuOS docs

When a doc in this repo says "one AGI, three repositories", it is telling only
Layer 0. The honest sentence is:

> **WuBuOS is the body.** It hosts the Brain (wubuwizard) and the Compiler
> (WuBuNOS), mounts the app/space submodules (WWW, Wua, WET, VWS, WuBuPad,
> WuBuOffice, WuBuContainer), shares extracted subsystems (WuBuHIVE, BearRL),
> and belongs to a ~WuBu~ space whose organs and P2P layer (WuBuFederation,
> WuBuCore, … and `WuBu`, the distributed-inference network) it connects to
> over the SDR relay — the places where the aggregated AGI (the `wubu_swarm`
> ledger) is going to be cast.
