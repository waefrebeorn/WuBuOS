# Tiny upstreams — free, permissive, small files that fill measured gaps

**Enforced by:** `tools/fetch_tiny.py --check` (`make test_tiny_upstreams`, offline)
and the per-entry build gate (`make test_tiny_miniz` as the first).
**Table:** `third_party/tiny/upstreams.json`. **Installs are recorded in** `PROVENANCE.md`.

## The rule (why this is a tool and not a link list)

A link list produces a tree full of half-vendored copies. An install here has
three properties, and a fourth that this session proved is necessary:

1. **PINNED** — the install records the sha256 of every byte written.
2. **LICENSED** — permissive only (public domain / MIT / BSD / CC0 / zlib /
   Unlicense / ISC / Apache). GPL and AGPL are refused by the checker.
3. **ONE COPY** — installing twice is a no-op; a second copy of one upstream is
   refused (one owner per concern).
4. **BUILD-GATED** — a hash proves the bytes, not that the code compiles. Every
   entry names an acceptance gate, and the gate must exercise the mechanism.

## What is available (sizes measured 2026-11-27, not remembered)

| id | size | licence | fills | what it actually buys |
|---|---|---|---|---|
| `miniz` | **425 KB** (10 files) | MIT / public domain | archive, office, files | deflate + ZIP + PNG read: archive browsing, DOCX/XLSX/ODT containers, PNG without libpng |
| `stb_image` | 272 KB | public domain / MIT | media, files | JPEG/PNG/GIF/BMP/TGA/PSD decode: image viewer + wallpapers |
| `stb_truetype` | 204 KB | public domain | a11y, aesthetics | real TTF/OTF rasterising at any size (we ship one bitmap face; Win11 ships 336 fonts) |
| `pl_mpeg` | 138 KB | MIT | media | MPEG-1 video + MP2 audio in one header: actual video playback |
| `stb_vorbis` | 195 KB | public domain / MIT | media | OGG Vorbis decode |
| `dr_wav` | 340 KB | public domain / MIT-0 | media | WAV read/write incl. 24-bit and float |
| `dr_mp3` | 200 KB | public domain / MIT-0 | media | MP3 decode — the most common music file on a user's disk |
| `monocypher` | 101 KB | public domain / BSD-2 / CC0 | security, online | X25519, EdDSA, ChaCha20-Poly1305, BLAKE2b: TLS primitives and file encryption without OpenSSL |
| `lz4` | 129 KB | BSD-2 | system, files | fast lossless compression for snapshots: the rollback/image leg |
| `cJSON` | 79 KB | MIT | dev, agentic | JSON for `.wua` manifests, agent messages, API replies |
| `utf8` | 73 KB | Unlicense | a11y, i18n | codepoint iteration, case mapping, width |
| `tinyexpr` | 25 KB | zlib | apps | expression parser for the calculator |
| `cozette` | **4 KB** (`.fnt`) | permissive (verify LICENSE) | aesthetics | a pixel-perfect programming face in four kilobytes |
| `feather-icons` | ~150 KB (287 × ~0.5 KB) | MIT | aesthetics | consistent 24 px stroke icons for UI actions |

Total for every code entry: **~2.3 MB**, and any single one can be taken alone.


## Policy: every entry is a slerm goal (enforced)

A vendored file is a **SCAFFOLD and a test oracle, never a shipped dependency**.
Every mechanism in the table has exactly one goal in `docs/GOAL-SLERM.json`
(`n >= 2001`, kept outside `GOALS-1000.json` so the 1000-goal / 16-domain
invariant survives), and `tools/slerm_backlog.py` (`make test_slerm_backlog`)
fails when a vendored id has no goal, a goal has no module/gate/criterion, or a
goal is marked DONE while `mk/tests.mk` has no `test_<stem>` target.

State: **22 goals — 6 DONE (expression, DEFLATE, UTF-8, PNG+BMP, JSON, BLAKE2b), 16 OPEN.**

### First closure: SLERM-2012, the expression evaluator

`tinyexpr` was never installed. `src/runtime/wubu_expr.c` is our own
recursive-descent parser and evaluator, and `make test_wubu_expr` prints
`28 passed, 0 failed`: 16 value oracles computed in Python at double precision,
2 variable-binding cases, 10 refusal cases. A parser that returns `0.0` for
`1/0` instead of naming the error is the fail-open shape the refusal half stops.

`status: DONE` is a check that a target exists, builds and passes — never a sentence.

## Case study: what "single file library" cost us

`miniz` is described as a single-file library. It is **ten files**: `miniz.h`
includes a `miniz_export.h` that **does not exist upstream** (404 on `main` and
`master` — CMake generates it), plus `miniz_common.h`, `miniz_tdef.h`,
`miniz_tinfl.h`, `miniz_zip.h`, and the code lives in `miniz.c` + `miniz_tdef.c` +
`miniz_tinfl.c` + `miniz_zip.c`. Without the shim it fails at line 115; with the
shim but without the extra `.c` files it compiles and fails at link on
`tdefl_init`/`tinfl_decompress`.

The accepted install is `third_party/tiny/miniz/` (425 KB) with a documented
`miniz_export.h` SHIM, and its gate is behavioural, not structural:

```
make test_tiny_miniz
[PASS] miniz: 51 bytes -> 62 compressed -> 51 back, identical
[PASS] miniz: corrupted stream refused rc=-3
```

The second line matters more than the first: a corrupted deflate stream must be
**refused**, not silently accepted. Archive browsing built on a fail-open inflate
is how a "supported format" becomes a data-integrity bug.

## Assets

| Need | Source | Size | Licence |
|---|---|---|---|
| programming font | Cozette `cozette.fnt` (release asset) | 4 KB | permissive — read LICENSE before vendoring |
| UI sans | Inter variable `InterVariable.woff2` | 344 KB | OFL-1.1 |
| action icons | Feather (287 SVGs) | ~0.5 KB each | MIT |
| textures, wallpapers | NASA imagery / Wikimedia public domain | varies | public domain |

## Refused (and why)

| Project | Why not |
|---|---|
| wolfSSL | GPL-3.0 (1.1 GB repo) |
| pandoc | GPL-2.0 |
| poppler / mupdf | GPL / AGPL — a PDF *reader* must be slermed or built on a permissive parser |
| OpenSSL | Apache-2.0 but 383 MB of tree for four primitives monocypher covers in 101 KB |
| mbedTLS | 145 MB, unasserted licence metadata |

## What a library does NOT buy

Every entry above fills a **mechanism** (decode, inflate, compress, sign). None
fills a **workflow**: "open an archive and extract a file" still needs a file
manager path, an empty state, an error message and a progress affordance. The
benchmark rows in `daily-driver-benchmarks.md` only move when the gate exercises
the workflow, not when the header compiles. That distinction is the whole reason
the ratchet and the benchmark doc exist beside this table.
