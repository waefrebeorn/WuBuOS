#!/bin/bash
# ship.sh -- E2 (docs/GAP_ROADMAP): the ONLY ship path.
#
#   CLEAN -> BUILD -> RUN the gate; a RED gate ABORTS before any commit/push.
#   Only when the gate is green does it stage + commit + push, so a red test can
#   never be pushed again (kills the two "red-push failure" modes).
#
#   usage:  scripts/ship.sh <target> '<commit message>'
#   env:    SHIP_DRY=1   stage + print the commit but do NOT commit/push (test).
#
# The submodule pointer/content drift (src/runtime/container/wubucontainer) is
# deliberately NOT staged -- it is unrelated to any source change here.
set -euo pipefail
cd "$(dirname "$0")/.."

TARGET="${1:?usage: scripts/ship.sh <target> '<commit message>'}"

if [[ "$#" -ge 2 ]]; then MSG="$2"; else MSG=""; fi
if [[ -z "$MSG" ]]; then
    echo "ship.sh: a commit message is required: scripts/ship.sh <target> '<msg>'" >&2
    exit 2
fi

echo "=== ship: gate=[${TARGET}] -> clean -> build -> run ==="
make clean >/dev/null 2>&1 || true          # E2: CLEANS first
make "$TARGET" || {
    echo "ship.sh: GATE RED -- '$TARGET' failed; NOT committing, NOT pushing." >&2
    exit 1
}
echo "=== ship: gate GREEN ($TARGET) ==="

echo "=== stage (excluding submodule drift + artifacts) ==="
git add -A ':(exclude)src/runtime/container/wubucontainer'
git add -u ':(exclude)src/runtime/container/wubucontainer'
git status --short

# E3 cross-repo rule: a holyd/compiler change ships wabunos AND its consuming
# wuboOS gates. NOTE: this CANNOT be auto-detected from a wuboOS staged-path
# check -- `git add src/compiler/...` is refused ("beyond a symbolic link"),
# so wuboOS never stages compiler content (the compiler is its own repo,
# wabunos, reached via the src/compiler symlink). The rule is enforced by the
# two-repo workflow: compiler edits commit to wabunos/wubu-integration, and the
# wuboOS gates that consume it (test_wubu_tokenizer / test_wubu_selfbuild /
# test-module) must be kept green in the same review.

if [[ "${SHIP_DRY:-0}" == "1" ]]; then
    echo "=== ship: SHIP_DRY -- commit/push skipped (dry run), staged above ==="
    exit 0
fi

echo "=== ship: commit + push ==="
git commit -m "$MSG"
git push origin "$(git branch --show-current)"
git fetch origin -q
echo "=== ship: LOCAL=$(git rev-parse HEAD) REMOTE=$(git rev-parse "origin/$(git branch --show-current)") ==="
echo "=== ship: done (peer-check above) ==="
