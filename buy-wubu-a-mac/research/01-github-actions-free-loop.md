# 01 — The primary loop: GitHub Actions macOS, free and unlimited

Verified 2026-11-19 against docs.github.com (GitHub-hosted runners
reference): "Use of the standard GitHub-hosted runners is free and
unlimited on public repositories." macOS labels currently offered:

- Intel x86_64, 4-core / 14 GB: `macos-15-intel`, `macos-26-intel`
  (older docs still list `macos-13`; use the -intel labels above).
- Apple Silicon M1, 3-core / 7 GB: `macos-latest`, `macos-15`,
  `macos-26`, `xcode-27`.
- Free/Pro/Team plans: 5 concurrent macOS jobs. Enough for a
  two-architecture matrix per push.

waefrebeorn/wubucompositor is PUBLIC (anonymous `git ls-remote`
succeeded without credentials, 2026-11-19). The same applies across the
federation's public repos. So the loop costs nothing.

## Why this fits the constraint set

- No hardware purchase, no payment plan.
- No human: the trigger is `git push` or `workflow_dispatch`; the result
  is a log an agent reads. The whole cycle is CLI.
- Real macOS, both architectures: the Intel runner matches the 2012
  iMac's x86_64; the M1 runner covers Apple Silicon users.

## PROVEN: the first loop iteration (2026-11-19)

- Run 44494569113 (push 014e37e): the Apple `_NSGetExecutablePath`
  branch COMPILED on real macOS, Intel AND M1 — its first compile
  ever. The gate failed on ONE assertion: the derived guest's
  executability, because CI runners have no sibling wubuos checkout.
  A machine property, not a code bug — the loop catching exactly what
  it exists to catch.
- Fix 9659d05: executability check made machine-conditional; the
  derivation shape stays asserted everywhere.
- Run 44494759630: GREEN on both — verify (macos-15) 13s,
  verify (macos-15-intel) 23s. Spawn derivation + both env knobs +
  real fork/exec verified on macOS, free, no human in the loop.

The agent protocol below is not theory; it is what just happened.

## Agent protocol (no telephone tag)

    # fire
    gh workflow run macos-verify.yml --repo waefrebeorn/wubucompositor
    # or just: git push
    # watch (bounded; the agent polls, nobody chats)
    gh run list --workflow macos-verify.yml --limit 1
    gh run watch <run-id> --exit-status
    # on failure: read, fix, push again
    gh run view <run-id> --log-failed
    # on green: the package loop finalizes
    gh release create macos-verify-<date> --notes "..." <artifacts>

Artifacts (binaries, logs, PPM captures) come back via
actions/upload-artifact; `gh run download` fetches them locally.

## What runs today vs next

Phase 1 (ready now, workflows/macos-verify.yml): compile the portable
leaf `src/wcc_spawn.c` (the Apple `_NSGetExecutablePath` branch has NEVER
been compiled on a Mac — this loop is how it gets verified) and run the
headless spawn gate on both architectures. No Wayland/X11 deps needed.

Phase 2 (when the Quartz host backend lands): full compositor build.
Needs wayland + epoll-shim + xkbcommon on the runner (MacPorts recipe,
see 05-wayland-on-macos.md; Homebrew's wayland coverage is thinner).
Then the live capture gate (`--screenshot`) runs on real macOS.

Phase 3: the wubuos side — hosted client's Apple branches + the VSL
Mach-O tests on macOS runners (test_vsl_macho already parses real
headers; running it ON macOS is the next honesty step).

## Limits, stated

- 5 concurrent macOS jobs on the free plan; matrix of 2 is fine.
- macOS runners can queue at peak demand (GitHub says so).
- Private repos would burn paid minutes (macOS is ~10x Linux rate);
  keep the verify repos public.
- Runners are VMs, not bare metal: GPU/Metal behavior on the M1 runner
  is real Apple Silicon but virtualized; the iMac fallback covers
  bare-metal quirks if they ever matter.