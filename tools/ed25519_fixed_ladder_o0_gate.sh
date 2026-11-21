#!/usr/bin/env bash
# ed25519_fixed_ladder_o0_gate.sh -- STACK-AWARE structural constant-time gate
# for the shipped bignum-free Ed25519 ladder compiled at -O0.
#
# This closes the -O0 evidence gap left by tools/ed25519_fixed_ladder_multitoolchain.sh:
# at -O0 the scalar bit and swap byte live in stack slots, which the register-only
# checker cannot track, so a -O0 RED would be vacuous.  This gate uses
# tools/ed25519_fixed_ladder_o0_check.py, which tracks BOTH registers and stack
# slots, and proves non-vacuity with a REAL injected `if (bit)` branch compiled
# at -O0 into a temp copy (working tree untouched).
#
# Configurations span the toolchain axis at a fixed -O0:
#   gcc 13.3 -O0, gcc 12.4 -O0
# A candidate is usable only if its compiler exists and actually compiles the
# module.  If fewer than two DISTINCT binaries are usable the gate FAILS CLOSED
# with the exact probe transcript as VERIFIED_NEGATIVE evidence -- it never
# reports a PASS it did not measure.
#
# Modes:
#   (no arg)  full gate (source + per-config shipped GREEN + injected RED
#             + identical-verdict assertion)
#   --negctl  standalone per-config injected-branch non-vacuity control
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-o0.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

MODE=gate
[ "${1:-}" = "--negctl" ] && MODE=negctl
[ "${1:-}" = "" ] || [ "${1:-}" = "--negctl" ] || {
    echo "usage: ed25519_fixed_ladder_o0_gate.sh [--negctl]" >&2; exit 2; }

CHECKER="$ROOT/tools/ed25519_fixed_ladder_o0_check.py"
SRCCHECK="$ROOT/tools/ed25519_fixed_ladder_check.py"
INJECT="$ROOT/tools/ed25519_fixed_ladder_inject.py"
SRC="$ROOT/src/gui/wubu_ed25519_fixed.c"

fail_closed() {
    echo "[o0_ct] FAIL: $*" >&2
    echo "$MARKER FAILED" >&2
    exit 1
}
MARKER=O0_CT_GATE
[ "$MODE" = negctl ] && MARKER=O0_CT_NEGCTL

echo "[o0_ct] === stack-aware -O0 structural CT gate ($MODE) ==="
for t in python3 objdump; do
    command -v "$t" >/dev/null 2>&1 || fail_closed "no $t; gate fails closed"
done
[ -f "$CHECKER" ] || fail_closed "missing $CHECKER"
[ -f "$SRCCHECK" ] || fail_closed "missing $SRCCHECK"
[ -f "$INJECT" ]   || fail_closed "missing $INJECT"
[ -f "$SRC" ]      || fail_closed "missing $SRC"

# ---- configuration discovery (real probes, no fabricated PASS) -----------
# Spec: "Name|CC|BINARY|OPT".  Both configs use -O0, so DISTINCT requires a
# different compiler binary.
CANDIDATES=(
    "gcc13-O0|gcc 13.3 -O0|gcc|-O0"
    "gcc12-O0|gcc 12.4 -O0|gcc-12|-O0"
)
# Portability / fail-closed self-test override (same pipe format).  The
# DISTINCT/uniqueness assertions below still apply: one config fails closed.
if [ -n "${O0_CANDIDATES:-}" ]; then
    IFS=';' read -r -a CANDIDATES <<<"$O0_CANDIDATES"
fi
OK_NAMES=(); OK_LABELS=(); OK_CC=(); OK_OPT=(); OK_VER=(); OK_KEY=()
PROBE_LOG="$TMP/config_probe.txt"
: > "$PROBE_LOG"
for spec in "${CANDIDATES[@]}"; do
    IFS='|' read -r name label cc opt <<<"$spec"
    {
        echo "--- candidate $name ($cc $opt) ---"
        echo "command -v $cc => $(command -v "$cc" 2>&1 || true)"
        "$cc" --version 2>&1 | head -1 || true
    } >> "$PROBE_LOG"
    command -v "$cc" >/dev/null 2>&1 || continue
    if ! "$cc" "$opt" -std=c11 -I"$ROOT/src/gui" -c "$SRC" \
            -o "$TMP/probe-$name.o" >>"$PROBE_LOG" 2>&1; then
        echo "compile probe FAILED for $name" >> "$PROBE_LOG"
        continue
    fi
    ver=$("$cc" --version 2>/dev/null | head -1)
    OK_NAMES+=("$name"); OK_LABELS+=("$label"); OK_CC+=("$cc")
    OK_OPT+=("$opt"); OK_VER+=("$ver"); OK_KEY+=("$cc:$opt")
done
echo "[o0_ct] configuration probe transcript:"
sed 's/^/[probe] /' "$PROBE_LOG"

N=${#OK_NAMES[@]}
# printf on an empty array yields one blank line, so guard N=0 explicitly.
if [ "$N" -eq 0 ]; then
    DISTINCT=0; BINARIES=0
else
    DISTINCT=$(printf '%s\n' "${OK_KEY[@]}" | sort -u | wc -l)
    BINARIES=$(printf '%s\n' "${OK_CC[@]}" | sort -u | wc -l)
fi
if [ "$N" -lt 2 ] || [ "$DISTINCT" -lt 2 ] || [ "$BINARIES" -lt 2 ]; then
    echo "[o0_ct] VERIFIED_NEGATIVE: fewer than two DISTINCT usable compiler" >&2
    echo "binaries at -O0 ($N configs, $DISTINCT distinct (binary,opt), $BINARIES" >&2
    echo "binaries). Exact probe transcript above; no PASS is fabricated." >&2
    echo "Stack-aware -O0 structural CT evidence remains unavailable on this host." >&2
    fail_closed "second -O0 compiler binary unavailable"
fi
echo "[o0_ct] usable configs: $N (distinct=$DISTINCT binaries=$BINARIES)"
for i in "${!OK_NAMES[@]}"; do
    echo "[o0_ct]   ${OK_NAMES[$i]}: ${OK_VER[$i]} (${OK_OPT[$i]})"
done

# ---- injected-branch fixture (temp copy; working tree untouched) ---------
if ! python3 "$INJECT" "$SRC" "$TMP/injected.c" || \
   ! grep -q "INJECTED CT REGRESSION" "$TMP/injected.c"; then
    fail_closed "could not build injected fixture"
fi
echo "[o0_ct] injected: if (bit) { dbl.X.l[0] ^= 1u; }  (temp copy only)"

# ---- source contract (toolchain-independent, once) -----------------------
if [ "$MODE" = gate ]; then
    echo "[o0_ct] source structural contract (once):"
    python3 "$SRCCHECK" source "$SRC" || fail_closed "source contract violated"
fi

# ---- per-configuration shipped GREEN + injected RED ----------------------
VERDICTS=()
for i in "${!OK_NAMES[@]}"; do
    name=${OK_NAMES[$i]}; label=${OK_LABELS[$i]}
    cc=${OK_CC[$i]}; opt=${OK_OPT[$i]}
    echo "[o0_ct] --- config $name ($label) ---"

    # shipped: must GREEN under the stack-aware checker
    if ! "$cc" "$opt" -std=c11 -I"$ROOT/src/gui" -c "$SRC" \
            -o "$TMP/$name.shipped.o" >"$TMP/$name.shipped.build" 2>&1; then
        cat "$TMP/$name.shipped.build" >&2
        fail_closed "$name: shipped module did not compile"
    fi
    objdump -d -M intel "$TMP/$name.shipped.o" > "$TMP/$name.shipped.asm" 2>/dev/null \
        || fail_closed "$name: objdump failed on shipped object"
    if python3 "$CHECKER" asm "$TMP/$name.shipped.asm" "$label" \
            >"$TMP/$name.shipped.log" 2>&1; then
        shipped=GREEN
    else
        shipped=RED
        cat "$TMP/$name.shipped.log" >&2
    fi

    # injected: must RED with a secret-governed JCC in REAL -O0 machine code
    if ! "$cc" "$opt" -std=c11 -I"$ROOT/src/gui" -c "$TMP/injected.c" \
            -o "$TMP/$name.injected.o" >"$TMP/$name.injected.build" 2>&1; then
        cat "$TMP/$name.injected.build" >&2
        fail_closed "$name: injected module did not compile"
    fi
    objdump -d -M intel "$TMP/$name.injected.o" > "$TMP/$name.injected.asm" 2>/dev/null \
        || fail_closed "$name: objdump failed on injected object"
    if python3 "$CHECKER" asm "$TMP/$name.injected.asm" "$label" \
            >"$TMP/$name.injected.log" 2>&1; then
        injected=GREEN
    else
        injected=RED
    fi
    jcc=$(grep -m1 -E '\[fixed_ladder_o0\] JCC @' "$TMP/$name.injected.log" || true)
    secretjcc=0
    grep -q 'secret-governed branch/index site' "$TMP/$name.injected.log" && secretjcc=1
    echo "[o0_ct]   shipped=$shipped injected=$injected secret_governed_jcc=$secretjcc"
    [ -n "$jcc" ] && echo "[o0_ct]   $jcc"
    VERDICTS+=("$name:$shipped:$injected:$secretjcc")
done

# ---- per-config obligations ----------------------------------------------
for v in "${VERDICTS[@]}"; do
    IFS=: read -r _ shipped injected sj <<<"$v"
    [ "$shipped" = GREEN ] || fail_closed "shipped verdict not GREEN for $v"
    [ "$injected" = RED ]  || fail_closed "injected branch NOT detected for $v (vacuous)"
    [ "$sj" = 1 ]          || fail_closed "no secret-governed JCC evidence for $v"
done

if [ "$MODE" = gate ]; then
    UNIQUE=$(printf '%s\n' "${VERDICTS[@]}" | sed 's/^[^:]*://' | sort -u | wc -l)
    [ "$UNIQUE" -eq 1 ] || fail_closed "verdicts differ across configurations: ${VERDICTS[*]}"
    echo "[o0_ct] identical verdicts across all $N configurations (distinct verdict patterns=$UNIQUE)"
    echo "[o0_ct] PASS: source + real -O0 machine code bound under $N compiler binaries;"
    echo "[o0_ct]       every configuration is non-vacuous; verdicts identical."
    echo "[o0_ct] Non-formal structural evidence -- see ED25519_FIXED_LADDER_O0.md"
    echo "$MARKER PASSED"
else
    echo "[o0_ct] PASS: every configuration's stack-aware detector is non-vacuous (injected branch RED)"
    echo "$MARKER PASSED"
fi
