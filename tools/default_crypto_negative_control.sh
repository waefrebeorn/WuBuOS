#!/usr/bin/env bash
# default_crypto_negative_control.sh -- prove the DEFAULT test tier detects a
# regression in the shipped Ed25519 ladder, WITH OR WITHOUT an mbedTLS dev root.
#
# Regression injected into the working-tree source (restored by an EXIT trap):
# the unconditional constant-time ge_cswap() in the 256-iteration ladder is
# replaced by a secret-dependent `if (swap)` branch.  This is precisely the
# constant-time regression the default-tier ladder guard exists to catch.
#
# The ROOT-INDEPENDENT structural guard (tools/eddsa_ladder_guard.sh, the
# test_ed25519_ladder_guard Make target) is the primary detector and ALWAYS
# runs -- so a no-root build is not a blind spot.  When a dev root is present
# the statistical/evidence bundle is also required to go red, as before.
# The control requires the detector(s) to go red on the regressed source,
# restores it, and requires them to go green again.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
SRC="src/gui/wubu_pkg_ed25519.c"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-default-crypto-nc.XXXXXX") || exit 2
cp "$SRC" "$TMP/original.c"
restore() { cp "$TMP/original.c" "$SRC"; }
trap 'restore; rm -rf "$TMP"' EXIT

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

# 1) Root-independent structural guard MUST detect the regression.
echo "[negative_control] running ROOT-INDEPENDENT ladder guard against the regressed ladder"
if bash tools/eddsa_ladder_guard.sh >"$TMP/guard_regressed.log" 2>&1; then
    cat "$TMP/guard_regressed.log"
    echo "[negative_control] FAIL: ladder guard did NOT detect the regression" >&2
    exit 1
fi
cat "$TMP/guard_regressed.log"
grep -m1 'FAIL: secret-dependent branch in ladder' "$TMP/guard_regressed.log" || {
    echo "[negative_control] FAIL: guard detection came from an unexpected cause" >&2
    exit 1
}
echo "[negative_control] DETECTED: structural ladder guard went red on the injected regression (no root required)"

# 2) With a dev root, the statistical/evidence bundle must ALSO go red.
if bash tools/mbedtls_devroot.sh >/dev/null 2>&1; then
    echo "[negative_control] mbedTLS root present: also requiring the evidence/dudect bundle to go red"
    if bash tools/default_optin_gates.sh >"$TMP/bundle_regressed.log" 2>&1; then
        cat "$TMP/bundle_regressed.log"
        echo "[negative_control] FAIL: default bundle did NOT detect the ladder regression" >&2
        exit 1
    fi
    cat "$TMP/bundle_regressed.log"
    grep -m1 'FAIL: conditional-selection count varied' "$TMP/bundle_regressed.log" || {
        echo "[negative_control] FAIL: bundle detection came from an unexpected cause" >&2
        exit 1
    }
else
    echo "[negative_control] no mbedTLS root: dudect/CT-evidence gates SKIP by design; structural guard is the detector"
fi

restore
trap - EXIT
echo "[negative_control] restored $SRC; re-running the detector(s) on the shipped ladder"

# 3) Structural guard MUST pass again on the shipped source.
if ! bash tools/eddsa_ladder_guard.sh >"$TMP/guard_restored.log" 2>&1; then
    cat "$TMP/guard_restored.log" >&2
    echo "[negative_control] FAIL: ladder guard red after restoring the source" >&2
    exit 1
fi
tail -3 "$TMP/guard_restored.log"

# 4) With a dev root, the bundle must go green again.
if bash tools/mbedtls_devroot.sh >/dev/null 2>&1; then
    if ! bash tools/default_optin_gates.sh >"$TMP/bundle_restored.log" 2>&1; then
        cat "$TMP/bundle_restored.log" >&2
        echo "[negative_control] FAIL: bundle red after restoring the source" >&2
        exit 1
    fi
    tail -3 "$TMP/bundle_restored.log"
fi

echo "[negative_control] PASS: regression detected (root-independent); shipped ladder green after restore"
