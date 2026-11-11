# Testing WuBuOS

The gate is `make test`: **512 targets**, run sequentially, all must pass with
0 errors and 0 segfaults. Every test compiles as a user-space binary with the
WuBu compliance flags and runs on the host — no hardware boot needed to develop.

Green looks like this at the end:

```
✅ All tests passed (all tiers)
```

---

## Run one test

```bash
make test_wm              # the window manager
make test_spellbook       # the spell book (reusable modules)
make test_vkd3d_heap      # one VKD3D slice
make test_fat32           # a kernel filesystem
```

`test_<name>` compiles from scratch each time — a test is only a real gate if
it actually rebuilds. If `make` says "nothing to be done", the recipe is likely
missing its sources (see below).

## Run a tier

Tiers are phony targets over groups of related tests. **Real member counts**
(2026-11-11):

| tier | members | what it covers |
|---|---|---|
| `test_critical_runtime` | 11 | containers, network, OCI, snapshots, VSL, HolyD, Proton, spawn |
| `test_critical_kernel` | 9 | filesystems, storage, display, decompressors, memory, tasking |
| `test_high_bridge` | 5 | syscall bridge, DOS flip |
| `test_high_gui` | 31 | WM, desktop, startmenu, explorer, terminal, shell, control |
| `test_high_bear` | 25 | Bear RL / JIT / compiler |
| `test_medium_other` | 81 | apps, spells, docs, VKD3D, SDR, package stack, D-Bus, AT-SPI2, … |
| `test_all` | 218 | the per-hardware-driver `test_hw_*` family |

```bash
make test_medium_other    # the medium tier (the one with the spell + doc gates)
make test_all             # the hardware-driver family
make test                 # every tier, sequentially — the full gate
```

## How it works

1. **Cached kernel objects** — `mk/tests.mk` precompiles verified kernel
   modules into `build/testobj/*.o` once; only changed sources recompile.
2. **Per-test link** — each test compiles its own `.c` against the cache and
   links the module under test. The stub file `src/kernel/wubu_test_stubs.c`
   supplies test-only symbols so a driver test is deterministic.
3. **Tier grouping** — `test_critical_*`, `test_high_*`, `test_medium_other`
   each depend on their member tests; `make test` runs them all.

## The gates that guard the tree

Three targets are load-bearing and worth understanding:

| test | what it proves |
|---|---|
| `test_spellbook` | the spell book scans clean; a mis-filed spell or a manifest that disagrees with its directory is a **build failure** |
| `test_docs` | every file path the front-door docs name still exists; a doc shorter than 200 bytes is a stub, not a doc |
| `test_vsl_nt_collision` | the NT dispatch table has zero real collisions (only intended stub→real upgrades) |

## How to ADD a test (read this before touching `mk/tests.mk`)

A test recipe has a **prerequisite line** and a **run line**. The golden rule:

> Anchor on the target's **LAST, run line** (`./binary`), never on the
> `target: prereqs` line. Inserting after the prerequisite line silently steals
> the previous target's recipe and makes the new target run several suites.

A test target must list its sources in its prerequisites. A test whose
prereqs omit the sources it compiles never rebuilds — `make` keeps the stale
binary and a green run proves nothing. This happened twice on this tree and
masked real breakage for months.

New runtime work also belongs to the **spell book**: `src/spells/` with a
`<name>.spell` manifest, and `test_spellbook` will start checking it.
