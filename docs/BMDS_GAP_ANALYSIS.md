# BMDS gap audit — what jthwho/bmds has that WuBuOS does not

**Reference:** `github.com/jthwho/bmds` — "Bare Metal Do Stuff", a DOS-like OS for
the ESP32-P4 (Olimex ESP32-P4-PC). Kernel (fs + tmpfs, consoles/PTYs, shell, window
system + GUI toolkit, program loader), ELF programs behind numbered syscalls, MIT
licence, one author, 281 commits, v0.2.0.

Method: read the upstream README/feature list, then checked each headline claim
against this tree by searching the actual sources. Status per item is
**have / partial / missing**, with the file that proves it.

---

## Headline gaps

### 1. Signed packages and a trust model — **MISSING**
BMDS: `SignedContainer` shared by `.bmup`/`.bmpk`/`.bmpi`, a 144-byte header carrying
a key id, a keyring of built-in plus added keys (`pkg key`), named signers,
`signed-by`.
WuBuOS: `src/runtime/wubu_crypto.h` has CRC32 and SHA-256 only. There is **no
signature primitive at all** — no RSA, Ed25519 or any asymmetric code — so nothing
can verify a package came from anyone. `wubu_spellmod.h`'s `WUSPELL\0` magic
identifies a container but authenticates nothing.

*This is the single largest structural gap.* Without it, `pkg install` from a repo
installs whatever the wire delivered.

### 2. Dependency solving — **CORRECTED** (a solver DOES exist at package level)
This entry previously claimed "no solver: no dependency walk, no version comparison,
no cycle detection". That was WRONG, and the error is instructive: the search was done
in `src/runtime/wubu_ns_pkg.c`, which is the 9P namespace BRIDGE -- a thin control
plane that forwards to `pkg_install`/`pkg_remove`. The real package manager lives in
`src/gui/wubu_pkgmgr*.c` (~2,940 lines across db / install / manifest / remote /
resolve / txn / verify), and it contains an actual solver:

  * `wubu_pkgmgr_resolve_deps()` -- builds a TRANSITIVE dependency closure by querying
    the sqlite deps table repeatedly until the set stabilises, then topologically
    orders it with Kahn's algorithm (in_degree + adjacency) before returning a load
    order. That is a dependency walk and an ordering, not a stub.
  * `wubu_pkgmgr_check_conflicts()` -- conflict checking against installed packages.
  * `wubu_pkgmgr_test.c` -- 671 lines of gate coverage.

What may still be genuinely absent (NOT re-verified here, so stated as open questions
rather than claims): dpkg-compatible VERSION comparison/refusal of breaking downgrades,
automatic-mark semantics, `autoremove`, and cycle REPORTING (Kahn's algorithm will
simply leave a cyclic remainder unordered; whether that is surfaced as an error or
silently ignored needs reading `resolve_deps`' tail).

Search correctly next time: `grep -rln pkg_install src/` finds the manager; grepping a
single plausible-looking filename found a forwarding shim instead.

Module level (.spell) was the OTHER half and was genuinely missing -- see below.
### 3. Crash-safe installation — **MISSING IN EFFECT** (transaction shell exists, recovery does not)
BMDS: a per-package database with a **pending record a power cut is finished from**;
checks everything first, writes beside each file, renames into place; unowned
identical files taken over.

WuBuOS has the SCAFFOLDING and not the safety:

  * `wubu_pkgmgr_txn_begin()` creates `/tmp/wubu_rollback_<timestamp>` --
    and nothing ever writes a backup into it.
  * `wubu_pkgmgr_txn_add()` records install/remove/upgrade/downgrade items --
    purely in memory, lost on exit.
  * `wubu_pkgmgr_txn_commit()` replays those items through the normal install path.
  * **`wubu_pkgmgr_txn_rollback()` is a STUB**: its body is `return true` with the
    comment "Restore from rollback_dir" and no code. Calling it after a failed
    install changes nothing.

Meanwhile `wubu_pkgmgr_install()` extracts the payload to `/tmp/wubu_install_<id>`
and copies files into `install_root` directly. A failure part-way through leaves the
already-copied files installed with no record of what was written, so neither a retry
nor a rollback can tell what to undo. On a power cut the same holds: partial files in
place, no journal, no pending record.

So the honest state: a transaction TYPE exists and a rollback DIRECTORY is created,
but no backup is taken, no journal is written, and the rollback entry point is a
no-op. Nothing here actually survives a crash or undoes a partial install.

The guest store (hosted_fs_persist.c) is a separate and BETTER story -- see below --
which makes this gap more visible: the tree already knows how to do temp-then-rename
correctly in one place and not in the other.
### 4. Release, installer and board support metadata — **MISSING**
BMDS: browser installer reading `firmware/latest.json`, per-chip `flash.json`
letting the chip revision pick an artifact, `tools/release` writing the release
page and packaging zips, post-install setup typed into the console (hostname,
timezone, password).
WuBuOS: `tools/` has no installer or release tooling at all. There is also no
per-hardware-variant artifact selection — the boot image targets one machine.

### 5. ABI compatibility as a written guarantee — **PARTIAL**
BMDS: `docs/ABI.md` documents the programs' interface and how it stays compatible;
syscalls "only ever added to".
WuBuOS: 412 of 512 NT syscall slots transliterated and a collision gate
(`test_vsl_nt_collision`), plus `docs/NT_STACK_SLERM_PLAN.md` — but no document
states the compatibility rule: what may change, what may never, how a slot is
retired, how a caller detects capability. The behaviour exists; the contract does
not.

### 6. Sanitizers — **CLOSED** (`make test_sanitize`)
Was PARTIAL: `test_jit_fuzzer` covered the JIT and hostile-input sweeps forked with
`alarm(1)`, but there was **no ASan/UBSan build gate anywhere in `mk/*.mk`**.
A hostile sweep proves "did not crash"; a sanitizer proves "did not read or write out
of bounds, did not overflow, did not execute undefined behaviour". Different claims,
and only one was being made.

`mk/sanitize.mk` now builds and runs four input-handling surfaces fresh under
`-fsanitize=address,undefined -fno-sanitize-recover=all`:

  .desktop/MIME parser · revolver env parsing · guest FS image loader ·
  the full NT syscall sweep (412 slots, forked)

**It earned its keep on the first run.** Two mime_contract checks failed only under
the sanitizers: `mime_contract_test.c` includes `wubu_mime.h` but calls
`parse_desktop_file()`, whose prototype lives in `wubu_mime_internal.h`. That is an
implicit declaration -- undefined behaviour in C99+, so the returned `bool` was read
from garbage and whether the check passed depended on stack layout and optimizer.
Fixed by including the right header. Any other translation unit with the same shape
is still latent.

---

## What we already have (do NOT rebuild)

| BMDS area | WuBuOS equivalent |
|---|---|
| Filesystem + tmpfs | `styxfs` (9P-style namespace, guest sandbox root, fs.img persistence) |
| Consoles / PTYs | terminal with real pty scrollback |
| Window system + GUI toolkit | wm + window_chrome + explorer engine + start menu |
| Program loader | `.app` container + PE-style core; ELF-free by design |
| Shell | Run dialog, `~WuBu~` shell surface |
| Package *parse/build* | `wubu_pkgbuild.{c,h}`, `wubu_ns_pkg.c` |
| Crypto primitives | SHA-256, CRC32 |
| Boot measurement | measurement-bound SHA-256 chain, `EDR_EV_BOOT_ATTEST` |
| Doc front door | `test_docs` gate; PROVENANCE.md |

Note the deliberate differences: BMDS is C++20 inside with musl and ELF programs;
WuBuOS is strict C11 with its own containers. Those are design choices, not gaps,
and should stay.

---

## Suggested order (highest leverage first)

1. **Write the ABI contract** (`docs/ABI.md`): slot retirement, capability probing,
   "only ever added". Cheapest, and it protects everything else. Pure documentation
   plus a test that asserts reserved slots fail closed.
2. **ASan/UBSan gate**: one make target compiling the parser suite with both
   sanitizers. Immediately converts the existing hostile-input tests into memory
   error detectors.
3. **Fuzz the real parsers**: reuse the JIT fuzzer's harness shape for `.desktop`,
   `.spell`, `WUBFS01` images, multiaddr/CID.
4. **Dependency solver**: build on `wubu_pkgbuild`'s existing parse; start with
   version comparison and cycle detection, both unit-testable without any network.
5. **Crash-safe package DB**: pending-record protocol around the existing install
   path; test by simulating a cut (fork, kill mid-install, verify recovery).
6. **Signatures**: needs an asymmetric primitive first — the largest single piece of
   new code here, and the one to do last because everything above is useless for
   distribution without it but valuable without it internally.

Not ordered by ease: 1 and 2 are days, 6 is the biggest and should not block the
others.

---

## Honest framing

BMDS is impressive engineering discipline, not magic: its most enviable features are
**contracts and tests** (ABI.md, signed containers, crash-safe DB, fuzzers, release
metadata), not algorithms. The kernel pieces we lack are mostly ones we replaced
with our own designs (Styx/9P instead of FAT+tmpfs, C11 containers instead of ELF).

The gap that matters most for the stated goal — "run Linux/Windows/macOS on our
Styx/9P TempleOS" — is **(6) signatures plus (1) the ABI contract**: nothing can be
distributed or trusted across that boundary until a container's origin can be
checked and the syscall surface can be relied upon not to move.
