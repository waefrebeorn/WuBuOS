# The Spell Book

WuBuOS's answer to the DLL.

A **spell** is a reusable module. Like a DLL it is a self-contained unit with an
export table and metadata; unlike a DLL it declares what it *provides* and what
it *needs*, it lives at a known address in an organized book, and casting it is
fail-closed and idempotent.

We are wizards. This is the vocabulary:

| term | meaning | analogue |
|---|---|---|
| **spell** | a reusable module | DLL / `.so` |
| **book** | a broad need | a subsystem |
| **genre** | a category inside a book | a directory |
| **casting** | loading and activating a spell | `dlopen` + init |
| **grimoire** | the whole collection | the module tree |

## The layout

```
src/spells/<book>/<genre>/<spell>/<spell>.spell   <- the manifest
                                  <sources>       <- the code
                                  <tests>         <- its gate
```

- the **book** is the broad need (`graphics`, `networking`, `storage`, ...);
- the **genre** is the category inside it (`render`, `relay`, ...);
- the **leaf directory is the spell's own name**, and the manifest inside it is
  `<name>.spell`.

## The manifest

A `key value` text file — the module's export table, not its implementation.
`#` begins a comment.

```
spell     <name>
version   <semver>
book      <book>          # must match the directory
genre     <genre>         # must match the directory
origin    first-party | third-party
provides  <capability>    # what other spells may resolve to this one
needs     <capability>    # what another SPELL must provide (resolved, fail-closed)
requires  <capability>    # a platform prerequisite (reported, not resolved)
source    <file.c>        # must exist beside the manifest
test      <file_test.c>
```

### `needs` and `requires` are different, deliberately

- **`needs`** is a capability another *spell* provides. The book resolves it,
  casts the provider first, and **refuses to cast** if it is unmet.
- **`requires`** is something the platform supplies — a Vulkan driver, libc, a
  kernel feature. It is **reported**, not resolved: treating a driver as an
  unmet *need* would make every spell in the book look broken.

### Conflicts are visible, never silently resolved

Two spells that claim the same capability are a **conflict**. The book records
both names and the contested capability; it does not pick a winner. Silently
preferring one is how a module gets hijacked.

## Casting

Casting resolves dependencies first, in order, and refuses to cast if any need
is unmet or if the spell needs itself. Casting an already-cast spell is a no-op.

## The gate

`test_spellbook` scans the real tree. A spell filed under the wrong book, a
manifest that disagrees with its directory, a declared source that is missing, a
duplicate name, an unknown key and two spells claiming one capability all fail
the gate — so the organization is *checked*, not merely intended.
