# Submodule standard

Every repository under this tree is **ours**, including the ones checked out as
submodules. A submodule is not an exception to the conventions: it is the same
codebase, in its own repository. This document is the checklist a submodule must
pass, and the reason each item exists.

## Why a submodule is held to the same standard

The parent's gates compile submodule sources (`src/gui/wet`, `src/gui/vws`,
`src/tools/wubugrep`, `src/brain`, `src/kernel/WuBuKernel`). A submodule that
drifts — a different C standard, a second copy of a helper, a README that lies
about how to build — breaks the parent's gates that nobody thought to re-run.

## The checklist

1. **Front door.** `README.md` says what the repo is, how to build, how to verify,
   and where the docs are. "See the code" is not a front door.
2. **Verify step.** A `make test` target, or an explicit statement that no gate
   exists yet and what a real one would need. A repo without a verify step is a
   repo whose breakage is discovered by the parent, late.
3. **`docs/` front door.** Either a real docs directory or `docs/README.md`
   pointing at the parent's compendium section that describes it.
4. **`.gitignore` hygiene.** Object files, `.d` dependency files, build trees and
   test binaries never appear as untracked. Untracked build output is how a
   `git add -A` commits a 300 MB binary.
5. **Own code only.** No vendored third-party source without a provenance note.
   The parent's `PROVENANCE.md` rule applies repo by repo.
6. **One naming layer.** The first token of a new file names its layer, per
   `docs/naming.md`, not "wubu" by default.
7. **Pinned and pushed.** The parent pins a commit that exists on a branch in the
   submodule's own remote. A pin to an unpushed local commit is a build that only
   works on one machine.

## State, measured 2026-11-25

| Submodule | Makefile | `test` target | `docs/` |
|---|---|---|---|
| `src/gui/wet` | yes | no | no |
| `src/gui/vws` | yes | yes | no |
| `src/tools/wubugrep` | yes | yes | yes |
| `src/kernel/WuBuKernel` | yes | yes | yes |
| `src/apps/wubupad` | no | no | yes |
| `src/apps/wubuoffice` | no | no | yes |
| `src/brain` | yes | yes | yes |
| `src/lua/www` | no | no | yes |
| `src/audio/bigmac` | yes | yes | yes |
| `src/wemu` | yes | yes | no |

Every row got a `## Standard` block in its README stating its layer, build,
verify and docs paths, so the next reader does not have to guess which of these
is true. The gaps above (no verify step in `wet`, no `docs/` in `vws`/`wemu`)
are declared, not hidden.
