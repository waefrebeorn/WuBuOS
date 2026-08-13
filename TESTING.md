# Testing

Fast, incremental tests for the WuBuOS kernel.  Tests are **hosted** — they
link against the kernel sources but compile with `-D_GNU_SOURCE` and run as
user-space binaries, so you can iterate fast without booting hardware.

## Layout

```
src/kernel/              ← kernel sources only (.c, .h, .S, .ld)
  test/                  ← all selftest + standalone test sources
    wubu_<mod>_selftest.c ← per-module selftests (229 files)
    test_*.c              ← legacy standalone tests (28 files)
  wubu_test_stubs.c      ← test-only stubs for excluded symbols
build/                   ← all build artifacts (gitignored)
  testobj/               ← cached core-module .o (built once)
  testbin/               ← test binaries (test_hw_<mod>)
  obj/                   ← object files
  dep/                   ← dependency files
```

## Quick start

```bash
# One specific test (cached objects reused — fast after first build):
make test_hw_audio

# A family:
make test_all

# Run everything and get a tally:
bash tools/run_hw_tests.sh
```

## How it works

1. **Cached core objects** — `mk/tests.mk` precompiles all ~283 verified
   kernel modules into `build/testobj/*.o` once (pattern rule:
   `$(CACHE)/%.o`).  Only sources that changed get recompiled.

2. **Per-test link** — each `test_hw_<mod>` target compiles just three fresh
   files — the selftest, the module under test, and `wubu_test_stubs.c` —
   then links them against the cached objects.  The module's own `.o` is
   excluded from the cache to avoid a double-definition.

3. **Two speed classes** (decided by `regenerate_tests_mk.py`):
   - **Type A** (77 modules) — links only the 5-file minimal runtime
     (`libc, libc_string, memory, klog, wubu_pci`).  Builds in ~0.2 s.
   - **Type B** (138 modules) — links the full 283-object core chain.
     First build ~8 s; incremental ~0.4 s.

## Managing test targets

`tools/regenerate_tests_mk.py` scans `src/kernel/test/` for new selftests and
rewrites `mk/tests.mk`.  Always run it after adding a new module:

```bash
python3 tools/regenerate_tests_mk.py
```

The script is idempotent — it preserves the `mk/tests.mk` preamble and
regenerates only the test targets + cached-object infrastructure.

## Stub routing

`src/kernel/wubu_test_stubs.c` provides test-only symbols (e.g.
`wubu_rtc_driver_for`, `wubu_hid_driver_for`) that route a vendor/chip string
to its driver name.  These are **stubs** for test determinism — the real
routing happens in the kernel's hw_detect layer.  If a test does a routing
CHECK you must extend the corresponding stub to mirror the real logic.
