# Duplicate-surface findings

Defects where one concern has two owners in this tree.  The covenant forbids leaving
a second copy; each item below is recorded with enough evidence to act on, because
discovering them incidentally is how they survived this long.

Found while chasing implicit-function-declaration warnings (see `mk/sanitize.mk`,
which now builds with `-Werror=implicit-function-declaration`).

---

## 1. Two Proton implementations, one include guard — **OPEN**

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

**Do next:** decide the single owner (the GUI one is the richer implementation --
it declares prefix/game management the runtime header lacks), port the runtime-side
callers onto it, then delete `src/runtime/wubu_proton*.{c,h}`.  Search for
`wubu_proton*.o` in `mk/targets.mk` to find every linker reference.

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
