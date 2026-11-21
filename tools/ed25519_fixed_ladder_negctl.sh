#!/usr/bin/env bash
# ed25519_fixed_ladder_negctl.sh -- prove the fixed-ladder checker is not vacuous.
# Injects a real secret-dependent branch into a TEMP COPY of the shipped module
# (the working tree is never mutated), requires BOTH the source and the real
# gcc -O2 machine-code checks to go RED, then requires the shipped module to
# stay GREEN.  Fails closed if gcc/objdump are unavailable.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-negctl.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT
CHECKER="$ROOT/tools/ed25519_fixed_ladder_check.py"
SRC="$ROOT/src/gui/wubu_ed25519_fixed.c"

echo "[negctl] === fixed-width ladder negative control ==="
python3 - "$SRC" "$TMP/injected.c" <<'PY' || exit 2
import sys
src = open(sys.argv[1], encoding="utf-8").read()
old = "        ge_add(&dbl, &r0, &r0);"
new = ("        ge_add(&dbl, &r0, &r0);\n"
       "        if (bit) { dbl.X.l[0] ^= 1u; }  /* INJECTED CT REGRESSION */")
if src.count(old) != 1:
    print("[negctl] injection anchor not unique", file=sys.stderr); sys.exit(2)
open(sys.argv[2], "w", encoding="utf-8").write(src.replace(old, new))
PY
grep -q "INJECTED CT REGRESSION" "$TMP/injected.c" || { echo "[negctl] FAIL: injection missing"; exit 1; }
echo "[negctl] injected: if (bit) { dbl.X.l[0] ^= 1u; }"

if python3 "$CHECKER" source "$TMP/injected.c" >"$TMP/src.log" 2>&1; then
    echo "[negctl] FAIL: source checker ACCEPTED the secret-dependent branch" >&2
    cat "$TMP/src.log"; exit 1
fi
echo "[negctl] source checker RED as required:"
grep -E "FAIL:|FAILED" "$TMP/src.log" | head -3

command -v gcc >/dev/null && command -v objdump >/dev/null || {
    echo "[negctl] FAIL: no gcc/objdump; cannot prove machine-code non-vacuity" >&2; exit 1; }
gcc -O2 -std=c11 -I"$ROOT/src/gui" -c "$TMP/injected.c" -o "$TMP/injected.o" || {
    echo "[negctl] FAIL: injected source did not compile" >&2; exit 1; }
objdump -d -M intel "$TMP/injected.o" > "$TMP/injected.asm"
if python3 "$CHECKER" asm "$TMP/injected.asm" >"$TMP/asm.log" 2>&1; then
    echo "[negctl] FAIL: machine-code checker ACCEPTED the secret-dependent branch" >&2
    cat "$TMP/asm.log"; exit 1
fi
echo "[negctl] machine-code checker RED as required:"
grep -E "JCC|FAIL:" "$TMP/asm.log" | head -4

echo "[negctl] restoring SHIPPED source and requiring GREEN again"
python3 "$CHECKER" source "$SRC" >/dev/null || { echo "[negctl] FAIL: shipped source not green" >&2; exit 1; }
gcc -O2 -std=c11 -I"$ROOT/src/gui" -c "$SRC" -o "$TMP/shipped.o" || exit 2
objdump -d -M intel "$TMP/shipped.o" > "$TMP/shipped.asm"
python3 "$CHECKER" asm "$TMP/shipped.asm" >/dev/null || { echo "[negctl] FAIL: shipped asm not green" >&2; exit 1; }
echo "[negctl] PASS: detector red on injected branch, green on shipped ladder"
echo "FIXED_LADDER_NEGCTL PASSED"
