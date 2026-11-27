# Slerm session 2026-11-27 — what closed, and what the gates now refuse

This is the handoff for the next session: the state of the slerm backlog, the defects the
new gates found, and the three next steps in the order they should be taken.

## Closures (13 of 25 goals, every one with a gate that compares to a foreign oracle)

| Goal | Module | Gate | Oracle | Mutation |
|---|---|---|---|---|
| SLERM-2012 expression | `src/runtime/wubu_expr.c` | `test_wubu_expr` 28/28 | Python-computed doubles at full precision | — |
| SLERM-2001 DEFLATE | `src/runtime/wubu_inflate.c` | `test_wubu_inflate` 34/34 | Python zlib, levels 0/1/6/9, both wrappers | — |
| SLERM-2011 UTF-8 | `src/runtime/wubu_utf8.c` | `test_wubu_utf8` 24/24 | RFC + invalid-form corpus | overlong check off → 2 fail |
| SLERM-2002 PNG+BMP | `src/runtime/wubu_pixmap.c` | `test_wubu_pixmap` 18/18 | Python PNG writer, all five filters | Paeth off → 3 fail |
| SLERM-2010 JSON | `src/runtime/wubu_json.c` (tightened, not duplicated) | `test_wubu_json` 41/41 | strict-refusal corpus | duplicate check off → 1 fail |
| SLERM-2008 BLAKE2b | `src/runtime/wubu_blake2b.c` | `test_wubu_blake2b` 14/14 | Python hashlib + RFC 7693 | — |
| SLERM-2022 ChaCha20-Poly1305 | `src/runtime/wubu_aead.c` | `test_wubu_aead` 41/41 | Python cryptography + RFC 8439 | AAD dropped → 8 fail |
| SLERM-2023 X25519 | `src/runtime/wubu_x25519.c` | `test_wubu_x25519` 12/12 | RFC 7748 §5.2 + iterated 1/1000 + cryptography | cswap off → 10 fail |
| SLERM-2021 GIF (2A) | `src/runtime/wubu_gif.c` | `test_wubu_gif` 8/8 | own Python GIF writer + Pillow cross-check | interlace off → 1 fail |
| SLERM-2017 WAV | `src/runtime/wubu_wav.c` | `test_wubu_wav` 11/11 | Python `wave` + edge-byte hand-built RIFF | 24-bit sign shift off → 1 fail |
| D6 agent log | `src/runtime/wubu_agent_log.c` | `test_wubu_agent_log` 24/24 | forged-record case (chain only) | no chaining → 1 fail |
| D1 password policy | `src/runtime/wubu_pwpolicy.c` | `test_wubu_pwpolicy` 25/25 | decision table, both directions | dominance off → 1 fail |
| D4 clipboard (A) | `src/bridge/wubu_clip.c` | `test_wubu_clip` 26/26 | preference-order cases | prefs ignored → 3 fail |

Independent proofs beyond the test files: a real ZIP entry (312 → 2 071 bytes exact) and
a real PNG (8 258 IDAT → 9 264 scanline bytes exact) through our own inflate.

## Defects the gates found (not typos — each was a wrong answer or a crash)

1. **DEFLATE** compared the *compressed* length to the *decompressed* size, so every
   stream over ~30 bytes reported "truncated"; `decode()` conflated "out of bits" with
   "no such code"; truncation was undetectable until the decoder reported bytes consumed.
2. **ZIP vs zlib header collision** — one raw stream in ~31 opens with bytes that pass
   the zlib header check. Without a retry-as-raw path, some archives are unreadable. The
   gate builds that collision on purpose.
3. **PNG** never compacted row 0 over its filter byte, and unfiltered palette images
   with the output channel count instead of one byte per index.
4. **JSON strictness exposed a writer bug**: `save_theme()` emitted `"auto_arrange": true,}`
   whenever the icon list was empty, so `settings.conf` had been *invalid JSON* all along
   and only the lenient reader made it look healthy.
5. **BLAKE2b** accepted a 65–128-byte key (RFC caps at 64) and `memcpy`'d from a NULL
   pointer when the length arrived without a key.
6. **X25519** had two silent-wrong-answer bugs: `E*(E*AA)` instead of `E*(AA+a24*E)`, and
   `(uint64_t)wide >> 51`, which discards every carry above bit 63.
7. **GIF** had a hidden 64x64 cap (4 KB scratch buffers) and read the graphic-control
   extension as `packed, transparent_index` when the layout is `packed, delay(2),
   transparent_index` -- so the transparent index was always 0.
8. **Gates that lied**: `scripts/owned_surface.py` printed `GATE FAILED` and exited 0
   (`__main__` dropped `main()`'s return), and `mk/tests.mk`'s `test` target echoed
   "All tests passed" unconditionally under `-k`. Both fixed and mutation-proven; `make
   test` now prints the verdict from a sub-make's exit status and exits with it.

## Structural work landed alongside

- `tools/slerm_backlog.py` (`test_slerm_backlog`): one goal per vendored mechanism; `DONE`
  is only accepted when `test_<stem>` exists and the module file is present.
- `tools/mk_db_equiv.py`: compares two `make -pq` dumps target by target, so a makefile
  refactor (2 300 343 → 2 166 451 bytes) is proven equivalent — 2 128 targets, 0 diffs.
- `docs/compendium/05-sources/tiny-upstreams.md`: 14 pinned permissive candidates,
  ~2.3 MB total, **none imported** — every mechanism they would have covered is own C11.

## State at handoff

**25 goals, 13 DONE, 12 OPEN.** Every closure above has a passing gate wired into the
default tier, and the three that follow are the ones with the most user-visible effect.

## Next three steps (in order)

1. **SLERM-2024 JPEG + GIF animation** (`src/runtime/wubu_jpeg.c`): the only image gap
   left; the file manager still cannot preview a photo, and GIF decoding is first-frame
   only. Baseline JPEG first (4:2:0/4:2:2/4:4:4), progressive second, compared against
   Pillow within a stated tolerance rather than asserted exactly.
2. **SLERM-2003 TrueType** (`src/runtime/wubu_truetype.c`): the tree ships one bitmap
   face while Win11 ships 336 fonts. Rasterising a real TTF at any size is the whole
   aesthetics gap in one module.
3. **D6 agent verdict log** (`/n/log/agent`) + **D4 WubuClip MIME types**: the agentic
   control plane and the clipboard are the two user-visible gaps that are about
   behaviour rather than format support.

## Standing rule for whoever picks this up

Run `make test` (not a hand-picked target) before believing a change. Three separate
breaks this session were invisible to targeted runs and only the full tier caught them:
a product link missing an object, a shell-gate source list missing a file, and a stale
owned-surface manifest that used to pass silently.
