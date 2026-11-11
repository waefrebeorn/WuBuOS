# The Epic — WuBuOS, told from its commit log

Everything here is read off the tree, not remembered. As of **2026-11-11** the
log holds **961 commits** since **2026-06-07**, touching **5,067 files** and
**750,001 lines of C** across `src/`, with **512 test targets** that must all
pass. The story has four acts, and there is a gap in the middle that matters.

---

## Chapter 0 — The premise

WuBuOS is the **body** of an AGI. Three repositories, one system:

- **WuBuOS** — the body. An operating system built from scratch, in public,
  one working cell at a time.
- **wubuwizard** — the brain. Inference, training, the KV cache.
- **WuBuNOS** — the compiler. HolyC, from lexer to machine code, for many ISAs.

Built from scratch means what it sounds like: not a fork, not a Linux
distribution. A UEFI firmware that is not EDK2, a kernel that is not Linux, a
window manager that is not X11.

---

## Act I — Birth (June 2026, 96 commits)

The project opens as a **numbered cell game**: each commit builds one cell of
the OS and marks it done against a fixed-number test gate.

The log's texture in June is `Vault update: Cell 070 … 237/237 tests`,
`Cell 072 AHCI … 277/277`, `Cell 092 Proton … 261/261`, `Cell 100 Transactional
FS … 322/322`, `Cell 101 Double-buffered GUI … 339/339`. Each one carries its
evidence — the file count, the LOC, the green gate — in the commit message
itself.

So the first principle is set on day one: **nothing is done until it is proved
to a numbered gate.** The cells covered, in order: kernel plumbing, AHCI,
filesystems, a transactional layer, the Proton compatibility core, and the
first double-buffered GUI.

---

## Act II — Shape (July–August 2026, 670 commits)

July is where the monolith dies. The dominant verbs are **split** (8),
**desktop** (8), **dedup** (3), **refactor** (3): the cells are dissolved into
named, single-purpose modules, and duplicated code is removed rather than
piled.

August is the kernel surge — **56 kernel commits** — plus the first object that
needed no kernel at all: **`wubugrep`**, a SIMD C11 grep built to beat ripgrep,
its own 20,000-line toolchain project. The module registry doctrine lands
("wire the orphans into the build, never delete"), 48 orphaned modules are
integrated, and the three-repo trunk becomes real when `src/brain` and
`src/compiler` are wired in as submodules.

By the end of Act II the body has a shape: a kernel, a hosted mode, a desktop,
a toolchain, and an honest ledger of what is and is not done.

---

## The Gap (September–October 2026)

The log goes almost silent. This is not laziness and not a stall — it is the
trunk going quiet while the other two legs moved. The brain (wubuwizard) and
the compiler (WuBuNOS) carry those months. The body waits, and when it wakes it
wakes stronger.

---

## Act III — The Return (November 2026, 195 commits and counting)

The body comes back with a new method: **graft** — take a foreign OS's core
idea, slerm it (rewrite it from scratch, never fork it), and prove it against
real hardware. The November log is a catalogue of that:

- **VKD3D** — D3D12→Vulkan, 15 slices from pure-C logic up to **real draws,
  uploads, UAVs, 4× MSAA and a presented frame** on an RTX 5050 and an Intel
  iGPU.
- **Steam Datagram Relay / SDR** — ticket, route, relay and session layers,
  all fail-closed.
- **GVfs, Tracker, D-Bus, AT-SPI2** — the Linux desktop's plumbing, owned.
- **pkg, PKGBUILD, AUR** — a package stack that parses but never executes.
- **DolDoc, RedSea, raw VGA** — the TempleOS surface.
- **The NT dispatch ledger** — a collision detector that proved the tree had
  **zero** collisions, and exposed that a gap-map claim about a dead branch
  was stale.

And two new things that are not graft but architecture:

- **The spell book** — WuBuOS's DLL: a reusable module (`*.spell`) filed in a
  book (a broad need) under a genre, cast fail-closed.
- **The mind palace** — a verified walk through the OS, and a **doc gate** so
  the docs cannot drift the way 264 dead references had drifted them before.

The method of Act III is the method of the whole project, now stated plainly:
*write the test that would embarrass you, then fix what it finds.* Two driver
bugs were found because a test asserted behaviour on real hardware rather than
restating the implementation.

---

## Where it is going — the vision, stated as vision

The arc points at something the commits have not yet recorded, so it is written
here as a direction, not as fact.

WuBuOS is meant to be **distributed infra on its own game and system**: an OS
and a game that, as people play and use it, **peer-to-peer aggregate** into a
self-training AGI. The aggregation is carried on the SDR relay layers this
project built — tickets, routes, sessions — repurposed as the transport of a
many-node body.

And the security posture is a meme with teeth: **the most intentionally
insecure thing ever** — an *angel system* for people "with no sin". No trust
apparatus, because the premise is that the participants are trusted. It is not
a missing feature; it is the design.

---

## Epilogue — reading the log yourself

The commit messages are the primary source. This file is a map of terrain you
should visit directly:

```
git log --pretty='%h %ad %s' --date=short   # the whole arc
git log --reverse --pretty='%s'             # the birth, cell by cell
git log --since='2026-11-01' --pretty='%s'  # Act III, day by day
```

Trust the commits over this document. When this file grows stale — and it will,
because the body keeps moving — the commit log is still the truth.

**Reading rules (from the mind palace):** function and goal first; no orphan
claims; no aspiration presented as fact; stale is worse than short. This
document obeys all four, and `test_docs` exists to catch the day it stops.
