#!/usr/bin/env bash
# eddsa_callee_ct_negative_control.sh -- prove BOTH callee-CT detectors are
# non-vacuous:
#   1. inject the real `if (swap)` regression into the shipped ladder, recompile
#      at -O2 with a LOCAL gcc, and require the interprocedural GIMPLE checker
#      to flag a secret-dependent branch;
#   2. compile an injected branchy cswap at -O2, disassemble it, and require
#      the register-level assembly checker to flag a secret-governed jcc.
# The source is restored by an EXIT trap and the shipped ladder re-checked
# green.  If no capable gcc exists, NO compiled PASS is asserted: the gate
# fails closed.  No network, no install, no toolchain guessing.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_gimple_toolchain.sh
source "$ROOT/tools/eddsa_gimple_toolchain.sh"
# shellcheck source=tools/eddsa_callee_toolchain.sh
source "$ROOT/tools/eddsa_callee_toolchain.sh"

SRC="src/gui/wubu_pkg_ed25519.c"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-callee-negctl.XXXXXX") || exit 2
cp "$SRC" "$TMP/original.c"
restore() { cp "$TMP/original.c" "$SRC"; }
trap 'restore; rm -rf "$TMP"' EXIT

fail() {
    echo "[callee_negctl] FAIL: $*" >&2
    echo "CALLEE_NEGCTL FAILED" >&2
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

echo "[callee_negctl] === injected secret-dependent-cswap negative control ==="
CC=$(find_gimple_gcc)
[ -n "$CC" ] || fail "no local gcc with -fdump-tree-optimized/-O2; NO compiled PASS asserted (fail closed)"
command -v objdump >/dev/null 2>&1 || fail "objdump absent; cannot exercise assembly detector"
echo "[callee_negctl] gcc: $($CC --version | head -1)"

# --- 1) GIMPLE interprocedural detector ---
inject || fail "could not inject the ladder 'if (swap)' regression"
echo "[callee_negctl] injected 'if (swap) ge_cswap(...,1)' into $SRC (working tree only)"
NOTE="gcc $($CC --version | head -1) -O2 -fdump-tree-optimized regressed src/gui/wubu_pkg_ed25519.c"
emit_gimple_dump "$CC" "$SRC" "$TMP" "$NOTE" "$TMP/regressed.gimple" \
    || fail "failed to emit -O2 GIMPLE from the regressed source"
if python3 tools/eddsa_callee_ct_check.py --expect leak --leak-kind branch \
        "$TMP/regressed.gimple" >"$TMP/gimple_det.log" 2>&1; then
    tail -6 "$TMP/gimple_det.log"
    echo "[callee_negctl] GIMPLE DETECTED: injected 'if (swap)' became a secret-dependent branch in real -O2 GIMPLE"
else
    cat "$TMP/gimple_det.log"
    fail "injected secret branch was NOT detected across ladder/callees (vacuous GIMPLE checker)"
fi

# --- 2) assembly register-taint detector ---
gcc -O2 -c -o "$TMP/branchy.o" tests/fixtures/callee_ct/branchy_cswap_probe.c \
    || fail "could not compile the injected branchy-cswap fixture"
objdump -d -M intel "$TMP/branchy.o" > "$TMP/branchy.asm" \
    || fail "could not disassemble the injected branchy-cswap fixture"
if python3 tools/eddsa_callee_asm_check.py "$TMP/branchy.asm" --taint-reg edi \
        --expect leak --label "injected branchy cswap (NEGATIVE)" \
        >"$TMP/asm_det.log" 2>&1; then
    tail -4 "$TMP/asm_det.log"
    echo "[callee_negctl] ASM DETECTED: injected branchy cswap has a secret-governed jcc in real -O2 machine code"
else
    cat "$TMP/asm_det.log"
    fail "injected branchy cswap was NOT detected (vacuous assembly checker)"
fi

# --- 3) restore and require green again ---
restore
trap - EXIT
echo "[callee_negctl] restored $SRC; re-checking shipped ladder at -O2"
NOTE="gcc $($CC --version | head -1) -O2 -fdump-tree-optimized shipped src/gui/wubu_pkg_ed25519.c"
emit_gimple_dump "$CC" "$SRC" "$TMP" "$NOTE" "$TMP/clean.gimple" \
    || fail "failed to emit -O2 GIMPLE from the restored source"
if python3 tools/eddsa_callee_ct_check.py "$TMP/clean.gimple" >"$TMP/clean.log" 2>&1; then
    tail -2 "$TMP/clean.log"
else
    cat "$TMP/clean.log"
    fail "shipped ladder is red after restore"
fi

echo "[callee_negctl] PASS: both detectors flagged the injected secret-dependent cswap; shipped ladder green after restore"
