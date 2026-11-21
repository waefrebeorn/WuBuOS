#!/usr/bin/env bash
# eddsa_ir_ct_negative_control.sh -- prove the compiler-IR checker is not
# vacuous, using the SAME `if (swap)` regression the source guard's control
# injects.  Runs without any compiler (fixture models + source guard) and,
# when a local clang is present, also compiles the regressed ladder to -O2 IR
# and requires the checker to flag it.  The working-tree source is restored
# by an EXIT trap.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_ir_toolchain.sh
source "$ROOT/tools/eddsa_ir_toolchain.sh"

SRC="src/gui/wubu_pkg_ed25519.c"
CHECK="tools/eddsa_ir_ct_check.py"
FIX="tests/fixtures/ir_ct"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-ir-negctl.XXXXXX") || exit 2
cp "$SRC" "$TMP/original.c"
restore() { cp "$TMP/original.c" "$SRC"; }
trap 'restore; rm -rf "$TMP"' EXIT

fail() { echo "[ir_negctl] FAIL: $*" >&2; exit 1; }

echo "[ir_negctl] === analyzer self-test (no compiler needed) ==="
python3 "$CHECK" "$FIX/ladder_clean.ll" || fail "clean IR fixture was not accepted"
echo "[ir_negctl] clean -O2 IR fixture accepted"

python3 "$CHECK" --expect leak --leak-kind branch "$FIX/ladder_leak.ll" \
    || fail "injected secret branch was not detected"
echo '[ir_negctl] DETECTED: secret-dependent br from the `if (swap)` model'

python3 "$CHECK" --expect leak --leak-kind index "$FIX/ladder_index.ll" \
    || fail "secret-derived GEP index was not detected"
echo "[ir_negctl] DETECTED: secret-derived memory index model"

if python3 "$CHECK" --expect leak "$FIX/ladder_clean.ll" >"$TMP/vacuity.log" 2>&1; then
    fail "clean IR reported as a leak (detector has no discrimination)"
fi
grep -m1 'vacuous checker' "$TMP/vacuity.log" >/dev/null \
    || fail "vacuity check failed for an unexpected reason"
echo "[ir_negctl] vacuity check: clean fixture correctly NOT reported as a leak"

# Inject the real source-level regression (restored by the EXIT trap).
OLD='        TRY(ge_cswap(&r0, &r1, swap));'
NEW='        if (swap) TRY(ge_cswap(&r0, &r1, 1));  /* INJECTED CT REGRESSION */'
python3 - "$SRC" "$OLD" "$NEW" <<'PY'
import sys
path, old, new = sys.argv[1:4]
s = open(path, encoding="utf-8").read()
n = s.count(old)
if n != 1:
    sys.exit("expected exactly one ladder cswap anchor, found %d" % n)
open(path, "w", encoding="utf-8").write(s.replace(old, new, 1))
PY
[ $? -eq 0 ] || fail "could not inject the ladder regression"
echo "[ir_negctl] injected 'if (swap) ge_cswap(...,1)' into $SRC (working tree only)"

if bash tools/eddsa_ladder_guard.sh >"$TMP/srcguard.log" 2>&1; then
    cat "$TMP/srcguard.log"
    fail "source ladder guard did not detect the injected regression"
fi
grep -m1 'FAIL: secret-dependent branch in ladder' "$TMP/srcguard.log" >/dev/null \
    || fail "source guard failed for an unexpected reason"
echo "[ir_negctl] DETECTED: source ladder guard went red on the injected regression"

CLANG=$(find_ir_clang)
if [ -n "$CLANG" ]; then
    echo "[ir_negctl] local clang: $($CLANG --version | head -1)"
    emit_ladder_ir "$CLANG" "$SRC" "$TMP/regressed.ll" \
        "clang -O2 regressed source (negctl)" \
        || fail "failed to emit -O2 IR from regressed source"
    if python3 "$CHECK" --expect leak --leak-kind any --flag-secret-select \
            "$TMP/regressed.ll" >"$TMP/ir.log" 2>&1; then
        tail -4 "$TMP/ir.log"
        echo "[ir_negctl] DETECTED: -O2 compiler IR of the regressed ladder is flagged"
    else
        cat "$TMP/ir.log"
        fail "-O2 compiler IR of the regressed ladder was not flagged"
    fi
else
    echo "[ir_negctl] NEGCTL_SKIP: no local clang with -emit-llvm/-O2; compiler-level negative control not exercised (the gate fails closed on this host)"
fi

restore
trap - EXIT
echo "[ir_negctl] restored $SRC; re-checking shipped ladder"
bash tools/eddsa_ladder_guard.sh >"$TMP/restored.log" 2>&1 \
    || { cat "$TMP/restored.log"; fail "source guard red after restore"; }
tail -2 "$TMP/restored.log"
echo '[ir_negctl] PASS: `if (swap)` regression detected by the IR checker and source guard; shipped ladder green after restore'
