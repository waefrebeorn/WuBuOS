# Study list — upstream projects WuBuOS learns from

A registry of outside operating systems, kernels and toolkits this tree studies.
"Study" means: cloned locally, read against our own code, and mined for methods we
deliberately take or deliberately reject.  Each entry records WHERE the clone lives,
WHY it is here, and WHAT we take from it — so the next session does not have to
rediscover the reasoning.

Clone convention: `~/opt/<name>` (alongside slermes, omarchy, wubu_drivers).
Add an entry here whenever a new upstream is cloned for study; remove it when the
entry's conclusions have been fully absorbed into our own code.

---

## BMDS — Bare Metal Do Stuff

- **Upstream:** https://github.com/jthwho/bmds
- **Author:** Jason Howard (jthwho).  Licence: MIT (third-party components keep their
  own, see its `docs/THIRD_PARTY.md`).
- **Clone:** `~/opt/bmds` — commit `ae7095b`, ~82 MB working tree.
- **What it is:** a small DOS-like OS for the ESP32-P4 on the Olimex ESP32-P4-PC.
  C++20 inside with a plain C interface for programs; ELF programs built with its
  SDK and loaded from the card; numbered system calls "only ever added to".
  Scale at study time: 1,569 `.c` + 779 `.cpp` files, 60 design documents under
  `devplan/`, plus `docs/VISION.md`, `ARCHITECTURE.md`, `ABI.md`,
  `CODING_STANDARDS.md`.

**Why it is here.**  It is the most disciplined single-author bare-metal OS we have
found: nearly every subsystem has a written plan, a host test, and often a fuzzer.
Our own gap audit (`docs/BMDS_GAP_ANALYSIS.md`) checked its claims against our tree
one by one; the result is that its best ideas are **contracts and tests**, not
algorithms.

**What we take:**

| BMDS idea | Where it lands in WuBuOS |
|---|---|
| `docs/ABI.md` — a written syscall compatibility contract | proposed first step; we have 412/512 slots but no stated retirement/probing rule |
| `SignedContainer` + keyring (`bmds_pkg/include/bmds/packages.h`) | we have SHA-256/CRC32 but **no asymmetric primitive**, so nothing can authenticate a package yet |
| `PkgSolver` (order, chains, cycles, refuses breaking downgrades) | we have publish/install/remove and a parser, no solver |
| Pending-record package DB (a power cut is finished from) | no journal anywhere in our install path |
| Release metadata (`latest.json`, per-chip `flash.json`) + browser installer | nothing equivalent; our image targets one machine |
| Fuzzers run at scale + ASan/UBSan host gates | we have `test_jit_fuzzer`; **no sanitizer gate in `mk/*.mk`** |

**Deliberate rejections** (design differences, not gaps — do not "fix" these):
C++20 + musl + ELF programs vs our strict C11 + `.app`/`.spell` containers;
ESP32-P4 hardware layer vs our x86-64 hosted/bare-metal split.

**Reading order for a new session:** `docs/VISION.md` → `docs/ARCHITECTURE.md` →
`docs/ABI.md` → `devplan/` (60 short design notes, one per subsystem) →
`RELEASE_NOTES.md` (the changelog is unusually explicit about *why* each change was
made).

---

## ZealOS

- **Upstream:** https://github.com/ZealOperatingSystem/ZealOS (also zeal-os.org).
- **Where referenced:** `src/kernel/zealos_parity.h` (name-parity mapping used by
  the NT bridge), `wubu_syscall.h`.
- **Why:** TempleOS is the ancestor this whole stack descends from; the parity header
  keeps our naming honest against Terry Davis's original.

## ReactOS

- **Where referenced:** `src/gui/wm_layout.c`, `src/gui/shell/startmenu.h`,
  `wubu_wallpaper.h`, `explorer_*` (desktop folder enumeration follows
  `shell32/CDesktopFolder`).
- **Why:** the Win32/Win9x shell behaviour we mirror — desktop folder semantics,
  start-menu structure, explorer views.

## Limine

- **Where referenced:** `docs/research/limine.conf` (boot configuration notes).
- **Why:** boot-loader configuration conventions for the x86-64 images.

---

## Absorbed entries

Nothing here yet.  When a study's conclusions have all landed in our code, move the
entry here with a one-line pointer to the commit that absorbed it, so the active list
stays short.
