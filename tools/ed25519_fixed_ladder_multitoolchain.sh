#!/usr/bin/env bash
# ed25519_fixed_ladder_multitoolchain.sh -- structural constant-time gate for
# the SHIPPED bignum-free fixed Ed25519 ladder across MULTIPLE compiler
# configurations.  This is the second-toolchain closure of the single-config
# proof-style gate (tools/ed25519_fixed_ladder_gate.sh, gcc 13.3 -O2).
#
# For every discovered configuration it runs the SAME existing structural
# checks -- tools/ed25519_fixed_ladder_check.py source (once) and .py asm
# (per configuration) -- on the shipped module, then injects a REAL
# secret-dependent branch into a TEMP COPY and requires the checker to go RED
# on that configuration's real machine code.  Verdicts must be IDENTICAL across
# configurations (shipped GREEN, injected RED with a secret-governed JCC).
#
# Configurations span BOTH axes available on this host:
#   gcc 13.3 -O2  (baseline)      gcc 13.3 -O3  (optimization-level change)
#   gcc 12.4 -O2  (toolchain version change)
# No config is fabricated: a candidate is used only if its compiler exists and
# actually compiles the module.  If fewer than two DISTINCT configurations are
# usable the gate FAILS CLOSED with the exact probe transcript as
# VERIFIED_NEGATIVE evidence -- it never reports a PASS it did not measure.
#
# Modes:
#   (no arg)  full gate (source + per-config shipped + per-config negative
#             control + identical-verdict assertion)
#   --negctl  standalone per-config injected-branch non-vacuity control
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-mt.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

MODE=gate
[ "${1:-}" = "--negctl" ] && MODE=negctl
[ "${1:-}" = "" ] || [ "${1:-}" = "--negctl" ] || {
    echo "usage: ed25519_fixed_ladder_multitoolchain.sh [--negctl]" >&2; exit 2; }

CHECKER="$ROOT/tools/ed25519_fixed_ladder_check.py"
INJECT="$ROOT/tools/ed25519_fixed_ladder_inject.py"
SRC="$ROOT/src/gui/wubu_ed25519_fixed.c"

fail_closed() {
    echo "[mt_ct] FAIL: $*" >&2
    echo "$MT_MARKER FAILED" >&2
    exit 1
}
MT_MARKER=MULTI_TOOLCHAIN_GATE
[ "$MODE" = negctl ] && MT_MARKER=MULTI_TOOLCHAIN_NEGCTL

echo "[mt_ct] === multi-configuration bignum-free ladder structural CT gate ($MODE) ==="
for t in python3 objdump; do
    command -v "$t" >/dev/null 2>&1 || fail_closed "no $t; gate fails closed"
done
[ -f "$CHECKER" ] || fail_closed "missing $CHECKER"
[ -f "$INJECT" ]   || fail_closed "missing $INJECT"
[ -f "$SRC" ]      || fail_closed "missing $SRC"

# ---- configuration discovery (real probes, no fabricated PASS) -----------
# Each candidate is "Name|CC|BINARY|OPT".  BINARY is a distinct compiler
# executable; OPT is the optimization level.  Two configs are DISTINCT when
# (BINARY,OPT) differs, so -O2/-O3 on one binary and gcc-12 -O2 all count.
CANDIDATES=(
    "gcc13-O2|gcc 13.3 -O2|gcc|-O2"
    "gcc13-O3|gcc 13.3 -O3|gcc|-O3"
    "gcc12-O2|gcc 12.4 -O2|gcc-12|-O2"
)
# Optional override for portability / fail-closed self-test (same pipe format).
# The DISTINCT/uniqueness assertions below still apply: a single config or two
# identical configs FAIL CLOSED regardless of what is supplied here.
if [ -n "${MT_CANDIDATES:-}" ]; then
    IFS=';' read -r -a CANDIDATES <<<"$MT_CANDIDATES"
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
    # A config is usable only if it actually compiles the shipped module.
    if ! "$cc" "$opt" -std=c11 -I"$ROOT/src/gui" -c "$SRC" \
            -o "$TMP/probe-$name.o" >>"$PROBE_LOG" 2>&1; then
        echo "compile probe FAILED for $name" >> "$PROBE_LOG"
        continue
    fi
    ver=$("$cc" --version 2>/dev/null | head -1)
    OK_NAMES+=("$name"); OK_LABELS+=("$label"); OK_CC+=("$cc")
    OK_OPT+=("$opt"); OK_VER+=("$ver"); OK_KEY+=("$cc:$opt")
done
echo "[mt_ct] configuration probe transcript:"
sed 's/^/[probe] /' "$PROBE_LOG"

N=${#OK_NAMES[@]}
# Distinct (binary, opt) keys.
DISTINCT=$(printf '%s\n' "${OK_KEY[@]}" | sort -u | wc -l)
# Diversity requires either two opt levels on one binary or a binary change.
BINARIES=$(printf '%s\n' "${OK_CC[@]}" | sort -u | wc -l)
OPTS=$(printf '%s\n' "${OK_OPT[@]}" | sort -u | wc -l)
if [ "$N" -lt 2 ] || [ "$DISTINCT" -lt 2 ] ||
   { [ "$BINARIES" -lt 2 ] && [ "$OPTS" -lt 2 ]; }; then
    echo "[mt_ct] VERIFIED_NEGATIVE: fewer than two DISTINCT usable compiler" >&2
    echo "configurations ($N configs, $DISTINCT distinct (binary,opt), $BINARIES" >&2
    echo "binaries, $OPTS opt levels). Exact probe transcript above; no PASS" >&2
    echo "is fabricated. Structural CT evidence remains bound to ONE config." >&2
    fail_closed "second compiler configuration unavailable"
fi
echo "[mt_ct] usable configs: $N (distinct=$DISTINCT binaries=$BINARIES optlevels=$OPTS)"
for i in "${!OK_NAMES[@]}"; do
    echo "[mt_ct]   ${OK_NAMES[$i]}: ${OK_VER[$i]} (${OK_OPT[$i]})"
done

# ---- injected-branch fixture (temp copy; working tree untouched) ---------
if ! python3 "$INJECT" "$SRC" "$TMP/injected.c" || \
   ! grep -q "INJECTED CT REGRESSION" "$TMP/injected.c"; then
    fail_closed "could not build injected fixture"
fi
echo "[mt_ct] injected: if (bit) { dbl.X.l[0] ^= 1u; }  (temp copy only)"

# ---- source check (toolchain-independent, once) --------------------------
if [ "$MODE" = gate ]; then
    echo "[mt_ct] source structural contract (once):"
    python3 "$CHECKER" source "$SRC" || fail_closed "source contract violated"
fi

# ---- per-configuration shipped GREEN + injected RED ----------------------
VERDICTS=()
for i in "${!OK_NAMES[@]}"; do
    name=${OK_NAMES[$i]}; label=${OK_LABELS[$i]}
    cc=${OK_CC[$i]}; opt=${OK_OPT[$i]}
    echo "[mt_ct] --- config $name ($label) ---"

    # shipped: must GREEN
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

    # injected: must RED with a secret-governed JCC in real machine code
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
    jcc=$(grep -m1 -E '\[fixed_ladder\] JCC @' "$TMP/$name.injected.log" || true)
    secretjcc=0
    grep -q 'secret-governed branch/index site' "$TMP/$name.injected.log" && secretjcc=1
    echo "[mt_ct]   shipped=$shipped injected=$injected secret_governed_jcc=$secretjcc"
    [ -n "$jcc" ] && echo "[mt_ct]   $jcc"
    VERDICTS+=("$name:$shipped:$injected:$secretjcc")
done

# ---- per-config obligations ---------------------------------------------
for v in "${VERDICTS[@]}"; do
    IFS=: read -r _ shipped injected sj <<<"$v"
    [ "$shipped" = GREEN ] || fail_closed "shipped verdict not GREEN for $v"
    [ "$injected" = RED ]  || fail_closed "injected branch NOT detected for $v (vacuous)"
    [ "$sj" = 1 ]          || fail_closed "no secret-governed JCC evidence for $v"
done

if [ "$MODE" = gate ]; then
    # Identical verdicts: every config's (shipped,injected,secretjcc) equal.
    UNIQUE=$(printf '%s\n' "${VERDICTS[@]}" | sed 's/^[^:]*://' | sort -u | wc -l)
    [ "$UNIQUE" -eq 1 ] || fail_closed "verdicts differ across configurations: ${VERDICTS[*]}"
    echo "[mt_ct] identical verdicts across all $N configurations (distinct verdict patterns=$UNIQUE)"
    echo "[mt_ct] PASS: source + real machine code bound under $N compiler configurations;"
    echo "[mt_ct]       every configuration is non-vacuous; verdicts identical."
    echo "[mt_ct] Non-formal structural evidence -- see ED25519_FIXED_LADDER_MULTI_TOOLCHAIN.md"
    echo "$MT_MARKER PASSED"
else
    echo "[mt_ct] PASS: every configuration's detector is non-vacuous (injected branch RED)"
    echo "$MT_MARKER PASSED"
fi
