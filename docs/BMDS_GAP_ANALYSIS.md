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

### 2. Dependency solving — **MISSING**
BMDS: `PkgSolver` on its own stack — ordering, automatic marks, chains, cycles,
refusing breaking downgrades; `pkg autoremove`, `pkg mark`; dpkg-compatible
`PkgVersion` rules.
WuBuOS: `src/runtime/wubu_ns_pkg.c` exposes publish/install/remove/add_repo, and
`wubu_pkgbuild.c` parses and expands. No solver: no dependency walk, no version
comparison, no cycle detection, no autoremove.

### 3. Crash-safe installation — **MISSING**
BMDS: a per-package database with a **pending record a power cut is finished from**;
checks everything first, writes beside each file, renames into place; unowned
identical files taken over.
WuBuOS: no pending/journal concept anywhere in the package path (searched
`wubu_ns_pkg.c` for pending/journal/power-cut: zero hits). A cut during install can
leave a half-written package with no recovery.

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

### 6. Fuzzing discipline — **PARTIAL**
BMDS: host fuzzers run at scale (model3d_fuzz 10M inputs clean), plus ASan/UBSan
host runs.
WuBuOS: `test_jit_fuzzer` exists for the JIT, and hostile-input sweeps fork with
`alarm(1)` (mime_contract, vsl_nt_contract). But there is **no ASan/UBSan build
gate** anywhere in `mk/*.mk`, and no systematic fuzzer for parsers that matter
most — `.desktop`, guest FS images, `.spell` containers, multiaddr/CID.

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
