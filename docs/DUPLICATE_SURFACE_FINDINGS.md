# Duplicate-surface findings

Defects where one concern has two owners in this tree.  The covenant forbids leaving
a second copy; each item below is recorded with enough evidence to act on, because
discovering them incidentally is how they survived this long.

Found while chasing implicit-function-declaration warnings (see `mk/sanitize.mk`,
which now builds with `-Werror=implicit-function-declaration`).

---

## 1. Two Proton implementations, one include guard — **FIXED (ABI hazard closed)**

*Status: the garbage-pointer hazard is closed.  The two APIs now carry distinct names
and every call site binds to the implementation it was compiled against.  What remains
is ordinary duplicated surface (three generations of Proton code still in the tree),
recorded at the bottom of this entry.*

### What was actually wrong

**Symptom.** `src/hosted/hosted_pe.c` called `wubu_proton_launch_with_prefix()` with
no visible prototype, so the returned value was read from garbage.  It happens to
work at `-O0` and fails under ASan/`-O1`, which is exactly why the sanitizer gate
was added.

**Root cause, in layers:**

1. The prototype lives in `src/gui/wubu_proton.h`; the TU included
   `src/runtime/wubu_proton.h`.
2. **Both headers use the same include guard, `WUBU_PROTON_H`.**  Whichever is
   included first hides the other completely -- verified by preprocessing: only the
   *call* survived, the declaration did not.
3. Renaming the guards to be unique immediately exposed that the two headers declare
   **the same types**: `PROTON_VERSION_DEFAULT`, `ProTON_VERSION_GE_LATEST`, ... and
   `enum ProtonVersion` conflict at compile time.  So this is not two cooperating
   headers -- it is **two full copies of one API**:
   `src/runtime/wubu_proton.[ch]` (+ `_dll`, `_pe`, `_dxvk.h`) and
   `src/gui/wubu_proton.[ch]` (+ `_internal.h`), each included by different TUs via
   the same bare `"wubu_proton.h"`.

**Why it was not "fixed" here.** Giving the headers unique guards is necessary but not
sufficient: with distinct guards, any TU that reaches both (`wubu_proton_dxvk.h` +
`wubu_proton_internal.h`, pulled in through the internal chain) now fails to compile.
Choosing which implementation owns Proton -- and deleting the other -- is a design
decision over duplicated state, not a mechanical rename, and belongs in its own pass.

**Evidence trail.** Guards renamed → build failed with `redeclaration of enumerator
'PROTON_VERSION_DEFAULT'` in `src/runtime/wubu_proton.h` via
`In file included from src/runtime/wubu_proton_dxvk.h:19` /
`src/gui/wubu_proton_internal.h:10`.  Guards reverted to restore the build; this note
is the record.

**THIS IS AN ABI COLLISION, NOT MERELY DUPLICATION -- severity raised.**

The three colliding symbols are declared with **different signatures** in the two
headers, so the linker's silent choice is not just a preference, it is a call through
the wrong prototype:

    symbol                     GUI header                        runtime header
    wubu_proton_init           int  (void)                        int  (wubu_proton_t *p)
    wubu_proton_shutdown       void (void)                        void (wubu_proton_t *p)
    wubu_proton_create_prefix  int  (const char*, const char*,    int  (const char*, const char*,
                                   ProtonVersion)                    int)

Because `$(RT_OBJS)` precedes `$(GUI_OBJS)` under `--allow-multiple-definition`, every
call site binds to the **runtime** bodies.  The third one is accidentally ABI-compatible
(`ProtonVersion` is an enum, i.e. `int`).  The first two are not: a GUI-side
`wubu_proton_init()` invokes `int wubu_proton_init(wubu_proton_t *p)` with whatever
happens to sit in the argument register -- a garbage pointer that the callee
dereferences.  That is a memory-safety fault, not a style problem.

Confirmed live, not theoretical: `src/gui/wubu_proton_exec.c:174` calls
`wubu_proton_create_prefix(...)` and is linked into the hosted binary via `GUI_OBJS`;
`src/gui/wubu_proton_test.c` calls all three.

**Therefore the two are not copies of one API -- they are two distinct APIs that were
given the same names.**  They cannot be merged by picking one; each needs its own
name.  The mechanical fix is a namespace split (e.g. `wubu_proton_ctx_init` /
`wubu_proton_ctx_shutdown` for the runtime manager), applied to both the declaration
and every caller, verified by build plus the two existing proton test suites.

**MEASURED IMPACT (added after finding the above).**  Both implementations are
linked into ONE binary, under `-Wl,--allow-multiple-definition`, so identical global
symbols do not error -- the linker keeps whichever object appears first.  The hosted
recipe orders `$(RT_OBJS) $(GUI_OBJS)`, so **the runtime definitions win and the GUI
copies of these functions are dead code**:

    wubu_proton_init            wubu_proton_create_prefix
    wubu_proton_shutdown

Their bodies are NOT copies (`md5` differs), so this is two genuinely different state
machines sharing a name.  Consequence: GUI-side code compiled against the GUI header
calls the runtime implementation, while any GUI-local helper touching its own statics
operates on a parallel, never-initialised universe.  It can appear to work as long as
every call routes through the public API -- which is exactly what makes it dangerous.

Before consolidating, check *which* machine the running desktop actually drives: put a
temporary trace on `wubu_proton_init` and see which file's version executes at boot.
That single experiment decides the owner instead of guessing.

**What was done.**  The runtime trio was renamed to a ctx namespace, in the header,
the definitions, and every non-GUI caller:

    wubu_proton_init          -> wubu_proton_ctx_init
    wubu_proton_shutdown      -> wubu_proton_ctx_shutdown
    wubu_proton_create_prefix -> wubu_proton_ctx_create_prefix

Touched: `src/runtime/wubu_proton.h`, `wubu_proton.c`, `wubu_proton_api.c`,
`wubu_proton_test.c`, `wubu_apps_test.c`, and the `ProtonInit`/`ProtonShutdown`
mappings in `src/kernel/zealos_parity.h`.  The GUI side (`src/gui/wubu_proton*.{c,h}`,
`wubu_proton_exec.c`) was deliberately left untouched -- it keeps the names it was
written against and now actually reaches its own bodies instead of the runtime ones.

Verified by: clean `make -k -j24` exit 0; the three symbols no longer appear in both
object sets (`nm` intersection empty); `test_proton` 32/32; `test_proton2` builds and
passes (its link line was missing `wubu_guest_root.c`, now added); `test_nt_abi`,
`test_docs`, `test_mime_contract`, `test_vsl_nt_contract`, `test_apps` all green.

**Still true:** these are three separate Proton implementations (`proton`, `proton2`,
GUI) sharing one concern.  Renaming made them coexist safely; consolidating them into
one is still worth doing, but it is now a normal refactor with no correctness hazard
attached.

---

## 2. Other duplicate include guards (first-party)

Same hazard class; not yet proven harmful, listed so nobody re-discovers them.
Third-party copies (SDL2, Lua `lv`) are excluded -- those directories are separate
vendored trees and are never co-included.

| Guard | Owners |
|---|---|
| `WB_LV_H`, `WB_LV_INTERNAL_H` | `src/lua/www/~WuBu~/src/lv/src/`, `src/gui/vws/externals/wet/externals/wua/src/`, `src/gui/wet/externals/wua/src/` |
| `WET_H`, `WET_INTERNAL_H`, `WET_LUA_H` | `src/gui/vws/externals/wet/` and `src/gui/wet/` |

The two `wet` trees are themselves near-copies (the WET framework), matching the same
pattern as Proton: one concern, two homes.

**How to check for more:** the scan that produced these lists is a five-line walk
over `src/**/*.h` comparing each file's `#ifndef`/`#define` pair.  Re-run it before
adding new headers, and prefer a guard derived from the path
(`WUBU_<DIR>_<FILE>_H`) so collisions cannot happen.


---

## 3. `-Wl,--allow-multiple-definition` hides all of this — **OPEN**

The hosted link passes `--allow-multiple-definition` (see `mk/targets.mk`).  With it,
any two objects defining the same global symbol link successfully and the first one
silently wins.  That flag is why a duplicated implementation could be added without
anybody noticing, and it will hide the next one too.

Cheap way to see the current blast radius without changing behaviour:

```
nm --defined-only -g <objs> | awk '{print $3}' | sort | uniq -d
```

Run over the objects before/after any consolidation.  Replacing the blanket flag with
an explicit per-symbol decision is the real fix, but that requires knowing every
legitimate case it currently papers over -- do not flip it blind.
