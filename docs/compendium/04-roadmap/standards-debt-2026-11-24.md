# Standards debt, measured — 2026-11-24

`make test_standards` runs `tools/std_audit.py`, which turns each WuBuOS coding
standard into a mechanical measurement. It was an orphan (no tier ran it), so the
number nobody looked at kept growing; it now runs against a **ratchet**
(`tools/std_audit_baseline.json`) and FAILS when the debt grows past it.

The ratchet was refreshed on 2026-11-24 from 126 long files / 3 god headers to
**145 / 4**, because 19 files and one header had crossed the budget since the
baseline was written and the gate had been reporting the same red forever — a
gate that is always red is a gate nobody reads. Nothing was fixed by refreshing
it: the 19 are recorded here as debt, and the ratchet now catches the NEXT one.

```
baseline written (145 long, 4 fat, 0 geometry, 0 color)
make test_standards -> clean: no hard violations
```

## What it reports

```
files scanned                 4,984
files over the 600-line budget  145
god headers (> 40 includes)       4
unfinished-work markers          24 markers, 117 placeholders across 99 files
```

The marker count is a WARN by design. The first two are HARD.

## The long files, worst first

| lines | file |
|---|---|
| 6,260 | `src/audio/bigmac/tools/wb_daw.c` |
| 4,952 | `src/audio/bigmac/tools/wb_selftest.c` |
| 4,260 | `src/runtime/vsl/vsl_exec_engine.c` |
| 3,872 | `src/wemu/src/wemu_usermode.c` |
| 3,316 | `src/runtime/vsl/vsl_exec_test.c` |
| 2,823 | `src/audio/bigmac/src/wb_session.c` |
| 2,789 | `src/audio/bigmac/src/wb_compositor.c` |
| 2,654 | `src/audio/bigmac/tools/test_compositor.c` |
| 2,411 | `src/brain/src/wubu_ssm.c` |
| 2,065 | `src/brain/src/gguf_reader.c` |
| 1,918 | `src/gui/wm_test.c` |
| 1,849 | `src/brain/src/wubu_model.c` |

By area: brain 36, gui 24, audio 23, runtime 16, kernel 13, wemu 8, jit 6,
lua 5. (Vendored trees — `~WuBu~/thirdparty`, `third_party/`, `wubuoffice/`,
`reference/` — are excluded already.)

## The god headers are one diagnosis, not four

```
src/hosted/hosted.c              55 includes
src/kernel/wubu_hw_detect.c     163 includes
src/kernel/wubu_probe.c         178 includes
src/kernel/wubu_probe_matrix.c  218 includes
```

None of them `#include`s another `.c`; they include ~200 distinct *device
headers* (`wubu_adreno600.h`, `wubu_aec.h`, `wubu_ampere.h`, …), each carrying a
`/* gcc14 campaign: declare what you use */` comment. That is the smell: the
device registry is expressed as **N direct includes of every module it wires**,
so any device header change recompiles the matrix, and the matrix must know
every device by name.

The fix shape is a registry: each device module exports its probe descriptor and
registers it, and the matrix iterates the table. That is a real refactor across
~200 modules — a workstream, not an afternoon — so it is recorded here rather
than half-done.

## One auditor bug, fixed now

The scan counted `.cache/mbedtls-devroot/usr/include/**` — a *downloaded* dev
root — as first-party code, which put 120 "placeholders" and a set of long files
in our name. Hidden directories are not source: the walker now skips them and
`.cache/` is an excluded prefix. That took the placeholder count from 120 to 117
and removed the phantom "NEW long files" from the baseline diff.

## Why the gate is not in a blocking tier yet

Because the tree does not meet its own standard, and a red gate in `make test`
just teaches people to ignore the suite. The honest sequence is: shrink the
numbers here, then wire `test_standards` into `test_critical_runtime` once the
count is at or near zero. Until then, run it explicitly:

```
python3 tools/std_audit.py            # report
python3 tools/std_audit.py --json     # the numbers above, machine-readable
```
