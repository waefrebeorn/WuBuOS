#!/usr/bin/env bash
# default_crypto_negative_control.sh -- prove the DEFAULT test tier detects a
# regression in the shipped Ed25519 ladder.
#
# Regression injected into the working-tree source (restored by an EXIT trap):
# the unconditional constant-time ge_cswap() in the 256-iteration ladder is
# replaced by a secret-dependent `if (swap)` branch.  That is precisely the
# constant-time regression the default-tier evidence gate exists to catch.
# The control runs the SAME default-tier bundle wiring uses, requires it to go
# red, restores the source, and requires it to go green again.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
SRC="src/gui/wubu_pkg_ed25519.c"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-default-crypto-nc.XXXXXX") || exit 2
cp "$SRC" "$TMP/original.c"
restore() { cp "$TMP/original.c" "$SRC"; }
trap 'restore; rm -rf "$TMP"' EXIT

if ! bash tools/mbedtls_devroot.sh >/dev/null 2>&1; then
    echo "[negative_control] SKIP: no mbedTLS dev root; the default-tier CT gate also skips (no fetch)."
    exit 0
fi

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
if [ $? -ne 0 ]; then
    echo "[negative_control] FAIL: could not inject the ladder regression" >&2
    exit 1
fi

echo "[negative_control] injected secret-dependent cswap into $SRC (working tree only)"
echo "[negative_control] running DEFAULT-tier bundle against the regressed ladder"
if bash tools/default_optin_gates.sh >"$TMP/regressed.log" 2>&1; then
    cat "$TMP/regressed.log"
    echo "[negative_control] FAIL: default tier did NOT detect the ladder regression" >&2
    exit 1
fi
cat "$TMP/regressed.log"
grep -m1 'FAIL: conditional-selection count varied' "$TMP/regressed.log" || {
    echo "[negative_control] FAIL: detection came from an unexpected cause" >&2
    exit 1
}
echo "[negative_control] DETECTED: default tier went red on the injected ladder regression"

restore
trap - EXIT
echo "[negative_control] restored $SRC; re-running the bundle on the shipped ladder"
if ! bash tools/default_optin_gates.sh >"$TMP/restored.log" 2>&1; then
    cat "$TMP/restored.log" >&2
    echo "[negative_control] FAIL: bundle red after restoring the source" >&2
    exit 1
fi
tail -3 "$TMP/restored.log"
echo "[negative_control] PASS: regression detected by the default tier; shipped ladder green after restore"
