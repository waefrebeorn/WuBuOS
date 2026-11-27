# Owned-repo import map — every gap names its owner

**Source of truth:** `tools/owned_coverage.py` (run it with `--table`). This page
explains the rule; the tool enforces it.

## The rule

A workflow in `docs/compendium/04-roadmap/daily-driver-benchmarks.md` is in one of
six states. The sixth was missing until now:

| State | Meaning |
|---|---|
| IN-TREE | in `src/`, compiled into the product |
| SUBMODULE-BUILT | in an imported repo, produced by `make vendored` |
| SUBMODULE-UNBUILT | imported, never compiled by default |
| **OWNED-REPO** | **the capability exists in a repository we own that is NOT imported** |
| SIBLING | a local checkout outside the tree, no submodule |
| ABSENT | nothing anywhere in the ecosystem |

The point of OWNED-REPO is that "missing" and "not imported" must never look the
same from inside WuBuOS again. The first is a product decision; the second is an
afternoon of git.

## What is imported today (10 submodules + 1 symlink)

| Path | Repo | Why it is here |
|---|---|---|
| `src/brain` | wubuwizard | the AGI: inference, training, KV-FS |
| `src/compiler` (symlink) | WuBuNOS | the compiler, edited in its own repo |
| `src/kernel/WuBuKernel` | WuBuKernel | the portable kernel, consumed by the OS |
| `src/wemu` | wemu | x86-64 emulation |
| `src/apps/wubuoffice` | WuBuOffice | 25 office apps, 535 C files |
| `src/apps/wubupad` | WuBuPad | the editor core |
| `src/gui/wet` | WET | applet framework |
| `src/gui/vws` | VWS | forms designer |
| `src/lua/www` | WWW | world-space engine |
| `src/tools/wubugrep` | wubugrep | the sanctioned search tool |
| `src/audio/bigmac` | big-mac | audio engine |

## Owned but NOT imported, and the gap family each one owns

| Repo | Owns | Where the gap shows up |
|---|---|---|
| **WuBuIntel** ("the Bureau — vigilance") | `edr` | edition 5: process/file/network telemetry, quarantine, ATT&CK mapping |
| **WuBuGov** ("public digital state") | `privacy` | edition 3: consent, portability, erasure, breach path |
| **WuBuEconomics** | `admin` | edition 4: licence tracking |
| **WuBuMM / WuBuMedia / wubucodec / wubuedit / wuburvc / wubutv-network** | `media` | edition 1: video playback, screen record, subtitles, voice chat |
| **wubusearch** | `files` (search inside documents) | edition 1 #53 |
| **WuBuHIVE**, **mindpalace**, **slermes**, **W-Code** | `agentic` | edition 5: approval queue, verdict log, authority expiry |
| **W-Code** | `dev`, `live-compiler` | edition 5: edit-and-continue, live reload |
| **quantum_rng_audit** | `security` (entropy) | edition 3: crypto |
| **wubufw-tools** | `system` (firmware) | edition 3: firmware flashing |

## Families with NO owner anywhere

- **online** — there is no browser engine in any owned repo. `WWW` is a
  world-space engine, not a page renderer. This is the one genuinely missing
  capability, and the reason it stays #1 in the benchmark ranking.
- **comms** — no mail, chat or call client in any owned repo.
- **a11y** — WuBuOffice carries the a11y modules (`wubua11y`, `wubua11yannounce`,
  `wubua11ytree`, `wubudyslexia`); the shell has no owner, which is why the shell
  has no screen reader.

## Import order (cheapest first, each with its gate)

| # | Import | Unblocks | Risk | Gate |
|---|---|---|---|---|
| 1 | WuBuIntel as `src/security/intel` | the whole `edr` family | low: nothing links it yet | `make intel` produces an artefact; the map stops calling it unimported |
| 2 | W-Code as `src/apps/wcode` | `live-compiler`, `dev` IDE | medium: it is an app with its own build | an app entry launches it |
| 3 | mindpalace + WuBuHIVE (read-only) | agent governance docs/skills | low | path exists; map updated |
| 4 | wubusearch | search inside documents | medium: embedding model dependency | search a fixture tree, assert hits |
| 5 | WuBuMM/wubucodec | media playback | high: Vulkan + model weights | play one fixture file |

## What this map refuses to do

It does not claim a repo *works* because it exists. Every import above still needs
the behavioural gate listed beside it; until then the workflow stays at its current
state, not promoted.
