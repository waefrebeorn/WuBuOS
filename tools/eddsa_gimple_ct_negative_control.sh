#!/usr/bin/env bash
# eddsa_gimple_ct_negative_control.sh -- prove the compiled-GIMPLE detector is
# not vacuous, using the SAME `if (swap)` regression the source guard injects.
# It injects the regression into the working-tree source, recompiles at -O2
# with a LOCAL gcc, and requires the checker to flag a secret-dependent branch
# in the emitted GIMPLE.  The source is restored by an EXIT trap and the clean
# ladder is re-checked green.  If no capable gcc exists, NO compiled PASS is
# asserted: a verified negative is recorded and the control fails closed.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_gimple_toolchain.sh
source "$ROOT/tools/eddsa_gimple_toolchain.sh"

SRC="src/gui/wubu_pkg_ed25519.c"
# A killed run cannot run its EXIT trap: a previous run may have left the
# injected regression in the tree (it did once -- the constant-time ladder
# shipped an `if (swap)` branch for weeks). Refuse to start on a dirty source.
if grep -q 'INJECTED CT REGRESSION' "$SRC"; then
    echo "[negctl] FAIL: $SRC still carries an INJECTED CT REGRESSION -- restore it" >&2
    exit 1
fi

CHECK="tools/eddsa_gimple_ct_check.py"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-gimple-negctl.XXXXXX") || exit 2
cp "$SRC" "$TMP/original.c"
restore() { cp "$TMP/original.c" "$SRC"; }
trap 'restore; rm -rf "$TMP"' EXIT

fail() {
    echo "[gimple_negctl] FAIL: $*" >&2
    echo "GIMPLE_NEGCTL FAILED" >&2
    exit 1
}

OLD='        TRY(ge_cswap(&r0, &r1, swap));'
NEW='        if (swap) TRY(ge_cswap(&r0, &r1, 1));  /* INJECTED CT REGRESSION */'
inject() {
    python3 - "$SRC" "$OLD" "$NEW" <<'PY'
import sys
path, old, new = sys.argv[1:4]
s = open(path, encoding="utf-8").read()
n = s.count(old)
if n != 1:
    sys.exit("expected exactly one ladder cswap anchor, found %d" % n)
open(path, "w", encoding="utf-8").write(s.replace(old, new, 1))
PY
}

echo "[gimple_negctl] === injected \`if (swap)\` regression negative control ==="
CC=$(find_gimple_gcc)

if [ -z "$CC" ]; then
    # No compiler.  Never fake a compiled PASS.  Still prove the regression is
    # real by requiring the root-independent source guard to go red on it,
    # then record a verified negative for the missing compiled toolchain.
    echo "[gimple_negctl] VERIFIED-NEGATIVE: no local gcc with -fdump-tree-optimized/-O2"
    inject || fail "could not inject the ladder regression"
    if bash tools/eddsa_ladder_guard.sh >"$TMP/srcguard.log" 2>&1; then
        cat "$TMP/srcguard.log"
        fail "source ladder guard did not detect the injected regression"
    fi
    grep -m1 'FAIL: secret-dependent branch in ladder' "$TMP/srcguard.log" >/dev/null \
        || fail "source guard failed for an unexpected reason"
    restore; trap - EXIT
    echo "[gimple_negctl] VERIFIED NEGATIVE: source guard caught the regression, but compiled GIMPLE detector was NOT exercised (no gcc). Gate fails closed; no PASS asserted."
    exit 1
fi

echo "[gimple_negctl] gcc: $($CC --version | head -1)"

# Inject the real source-level regression.
inject || fail "could not inject the ladder regression"
echo "[gimple_negctl] injected 'if (swap) ge_cswap(...,1)' into $SRC (working tree only)"

NOTE="gcc $($CC --version | head -1) -O2 -fdump-tree-optimized regressed src/gui/wubu_pkg_ed25519.c"
emit_gimple_dump "$CC" "$SRC" "$TMP" "$NOTE" "$TMP/regressed.gimple" \
    || fail "failed to emit -O2 GIMPLE from the regressed source"

if python3 "$CHECK" --expect leak --leak-kind branch "$TMP/regressed.gimple" \
        >"$TMP/det.log" 2>&1; then
    tail -4 "$TMP/det.log"
    echo "[gimple_negctl] DETECTED: injected \`if (swap)\` became a secret-dependent branch in real -O2 GIMPLE"
else
    cat "$TMP/det.log"
    fail "injected secret branch was NOT detected in -O2 GIMPLE (vacuous checker)"
fi

# Restore the shipped source and require it green again.
restore
trap - EXIT
echo "[gimple_negctl] restored $SRC; re-checking shipped ladder at -O2"
NOTE="gcc $($CC --version | head -1) -O2 -fdump-tree-optimized shipped src/gui/wubu_pkg_ed25519.c"
emit_gimple_dump "$CC" "$SRC" "$TMP" "$NOTE" "$TMP/clean.gimple" \
    || fail "failed to emit -O2 GIMPLE from the restored source"
if python3 "$CHECK" "$TMP/clean.gimple" >"$TMP/clean.log" 2>&1; then
    tail -2 "$TMP/clean.log"
else
    cat "$TMP/clean.log"
    fail "shipped ladder is red after restore"
fi

echo "[gimple_negctl] PASS: \`if (swap)\` regression flagged in compiled GIMPLE; shipped ladder green after restore"
