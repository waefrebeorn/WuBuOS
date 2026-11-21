#!/usr/bin/env bash
# ed25519_fixed_ladder_gate.sh -- proof-style gate for the bignum-free ladder.
#
# Asserts, with real tool output and no network:
#   1. detector non-vacuity (injected secret branch must go RED, shipped GREEN);
#   2. structural C contract (fixed 256 trip count, mask-only cswap, zero
#      mbedTLS/bignum symbols) over the SHIPPED src/gui/wubu_ed25519_fixed.c;
#   3. real gcc -O2 machine code (trip count survives; no secret-governed
#      branch or index; cswap lowers to neg+pand+por);
#   4. functional correctness against an INDEPENDENT pure-Python RFC 8032
#      scalar-mult oracle (11 base-point vectors + 3 arbitrary-point vectors).
# Fails closed (exit 1, never a fake PASS) on any missing tool or obligation.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-gate.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail_closed() {
    echo "[fixed_gate] FAIL: $*" >&2
    echo "FIXED_LADDER_GATE FAILED" >&2
    exit 1
}

echo "[fixed_gate] === bignum-free fixed-width Ed25519 ladder gate ==="
command -v python3 >/dev/null || fail_closed "no python3"
command -v gcc >/dev/null || fail_closed "no gcc"
command -v objdump >/dev/null || fail_closed "no objdump"
CHECKER="$ROOT/tools/ed25519_fixed_ladder_check.py"
SRC="$ROOT/src/gui/wubu_ed25519_fixed.c"
[ -f "$SRC" ] || fail_closed "missing $SRC"
echo "[fixed_gate] gcc: $(gcc --version | head -1)"

echo "[fixed_gate] (1/4) injected-regression negative control first"
bash "$ROOT/tools/ed25519_fixed_ladder_negctl.sh" \
    || fail_closed "negative control / detector non-vacuity failed"

echo "[fixed_gate] (2/4) source structural contract"
python3 "$CHECKER" source "$SRC" || fail_closed "source contract violated"

echo "[fixed_gate] (3/4) real gcc -O2 machine code"
gcc -O2 -std=c11 -I"$ROOT/src/gui" -c "$SRC" -o "$TMP/mod.o" \
    || fail_closed "could not compile the shipped fixed-width module"
objdump -d -M intel "$TMP/mod.o" > "$TMP/mod.asm" \
    || fail_closed "objdump failed on the shipped module"
echo "[fixed_gate] audited $(wc -l < "$TMP/mod.asm") lines of real -O2 disassembly"
python3 "$CHECKER" asm "$TMP/mod.asm" || fail_closed "machine-code contract violated"

echo "[fixed_gate] (4/4) differential correctness vs independent Python oracle"
python3 "$ROOT/tools/gen_ed25519_fixed_vectors.py" > "$TMP/vectors.txt" \
    || fail_closed "vector generator failed"
gcc -O2 -std=c11 -I"$ROOT/src/gui" \
    "$ROOT/tests/fixtures/fixed_ct/probe.c" "$SRC" -o "$TMP/probe" \
    || fail_closed "could not build the vector probe"
checked=0
while read -r kind a b c; do
    case "$kind" in
        SC) args=("$a"); expected="$b" ;;
        PT) args=("$a" "$b"); expected="$c" ;;
        \#*|"") continue ;;
        *) fail_closed "unrecognised vector line: $kind" ;;
    esac
    got=$("$TMP/probe" "${args[@]}") || fail_closed "probe failed for $kind $a"
    if [ "$got" != "$expected" ]; then
        echo "[fixed_gate] VECTOR MISMATCH $kind $a: got $got expected $expected" >&2
        fail_closed "fixed ladder disagrees with RFC 8032 oracle"
    fi
    checked=$((checked + 1))
done < "$TMP/vectors.txt"
[ "$checked" -ge 14 ] || fail_closed "expected >= 14 vectors, checked $checked"
echo "[fixed_gate] $checked/$checked scalar-mult vectors match the Python oracle"

echo "[fixed_gate] PASS: bignum-free ladder is structurally CT, compiler-verified CT, and RFC 8032 correct"
echo "FIXED_LADDER_GATE PASSED"
