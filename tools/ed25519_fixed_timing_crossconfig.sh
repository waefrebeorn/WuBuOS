#!/usr/bin/env bash
# ed25519_fixed_timing_crossconfig.sh -- CROSS-CONFIGURATION timing evidence for
# the SHIPPED DEFAULT fixed-width Ed25519 ladder (WUBU_EDDSA_FIXED_LADDER,
# src/gui/wubu_ed25519_fixed.c).
#
# This closes the "one toolchain, one optimisation level" limitation recorded
# in docs/ED25519_FIXED_TIMING.md.  It does NOT reimplement the timing gate:
# for every usable compiler configuration it invokes the existing gates
# unchanged (tools/ed25519_fixed_timing_gate.sh and
# tools/ed25519_fixed_timing_negctl.sh), passing the compiler via CC and the
# optimisation level via EDDSA_TIMING_OPT.  Each of those gates builds the same
# self-contained probe, runs ONE interleaved same-run Welch-t pass with FIXED
# thresholds and NO retries, and requires the shipped ladder to stay flat
# (|t_shipped| <= 10) AND the secret-dependent control to be flagged
# (|t_control| > 10).
#
# A configuration is "gcc13-O1|gcc 13.3 -O1|gcc|-O1" style:
#   name | human label | compiler binary | optimisation flag
# Two configurations are DISTINCT when (binary, optimisation flag) differs, so
# -O1/-O2 on one binary and gcc-12 -O1/-O2 all count.  A candidate is usable
# only if its compiler exists AND actually compiles the probe source.  If
# fewer than two DISTINCT, diverse configurations are usable the gate FAILS
# CLOSED with the exact probe transcript as VERIFIED_NEGATIVE evidence -- it
# never reports a PASS it did not measure.
#
# Modes:
#   (no arg)  full gate: per-config shipped gate (flat + non-vacuous)
#   --negctl  standalone per-config negative control (detector flags the
#             deliberately secret-dependent path)
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-timing-ftx.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

MODE=gate
[ "${1:-}" = "--negctl" ] && MODE=negctl
[ -z "${1:-}" ] || [ "${1:-}" = "--negctl" ] || {
    echo "usage: ed25519_fixed_timing_crossconfig.sh [--negctl]" >&2; exit 2; }

TIMING_GATE="$ROOT/tools/ed25519_fixed_timing_gate.sh"
TIMING_NEGCTL="$ROOT/tools/ed25519_fixed_timing_negctl.sh"
PROBE_SRC="$ROOT/tools/ed25519_fixed_timing_probe.c"
SHIPPED_SRC="$ROOT/src/gui/wubu_ed25519_fixed.c"
SHIPPED_HDR="$ROOT/src/gui/wubu_ed25519_fixed.h"

MARKER=FIXED_TIMING_CROSSCONFIG_GATE
[ "$MODE" = negctl ] && MARKER=FIXED_TIMING_CROSSCONFIG_NEGCTL

fail_closed() {
    echo "[ftx] FAIL: $*" >&2
    echo "$MARKER FAILED" >&2
    exit 1
}

echo "[ftx] === cross-configuration fixed-EdDSA timing gate (mode=$MODE) ==="
for t in cc grep; do
    command -v "$t" >/dev/null 2>&1 || fail_closed "no $t; gate fails closed"
done
for f in "$TIMING_GATE" "$TIMING_NEGCTL" "$PROBE_SRC" "$SHIPPED_SRC" "$SHIPPED_HDR"; do
    [ -f "$f" ] || fail_closed "missing required gate input: $f"
done

# ---- configuration discovery (real compile probes, no fabricated PASS) ----
CANDIDATES=(
    "gcc13-O1|gcc 13.3 -O1|gcc|-O1"
    "gcc13-O2|gcc 13.3 -O2|gcc|-O2"
    "gcc12-O1|gcc 12.4 -O1|gcc-12|-O1"
    "gcc12-O2|gcc 12.4 -O2|gcc-12|-O2"
)
# Portability / fail-closed self-test override (same pipe format).  The
# DISTINCT/diversity assertions below still apply: one config, or two configs
# that differ on neither axis, FAIL CLOSED regardless of what is supplied.
if [ -n "${FTC_CANDIDATES:-}" ]; then
    IFS=';' read -r -a CANDIDATES <<<"$FTC_CANDIDATES"
fi

OK_NAME=(); OK_LABEL=(); OK_CC=(); OK_OPT=(); OK_VER=(); OK_KEY=()
PROBE_LOG="$TMP/config_probe.txt"
: > "$PROBE_LOG"
for spec in "${CANDIDATES[@]}"; do
    IFS='|' read -r name label cc opt <<<"$spec"
    {
        echo "--- candidate $name ($cc $opt) ---"
        echo "command -v $cc => $(command -v "$cc" 2>&1 || true)"
        "$cc" --version 2>&1 | head -1 || true
    } >> "$PROBE_LOG"
    command -v "$cc" >/dev/null 2>&1 || { echo "compiler unavailable" >> "$PROBE_LOG"; continue; }
    # A config is usable only if it actually compiles the REAL probe source.
    if ! "$cc" "$opt" -std=c11 -D_POSIX_C_SOURCE=200809L -I"$ROOT/src/gui" \
            -c "$PROBE_SRC" -o "$TMP/probe-$name.o" >>"$PROBE_LOG" 2>&1; then
        echo "compile probe FAILED for $name" >> "$PROBE_LOG"
        continue
    fi
    ver=$("$cc" --version 2>/dev/null | head -1)
    OK_NAME+=("$name"); OK_LABEL+=("$label"); OK_CC+=("$cc")
    OK_OPT+=("$opt"); OK_VER+=("$ver"); OK_KEY+=("$cc:$opt")
done
echo "[ftx] configuration probe transcript:"
sed 's/^/[probe] /' "$PROBE_LOG"

N=${#OK_NAME[@]}
if [ "$N" -eq 0 ]; then
    DISTINCT=0; BINARIES=0; OPTS=0
else
    DISTINCT=$(printf '%s\n' "${OK_KEY[@]}" | sort -u | wc -l)
    BINARIES=$(printf '%s\n' "${OK_CC[@]}" | sort -u | wc -l)
    OPTS=$(printf '%s\n' "${OK_OPT[@]}" | sort -u | wc -l)
fi
if [ "$N" -lt 2 ] || [ "$DISTINCT" -lt 2 ] || \
   { [ "$BINARIES" -lt 2 ] && [ "$OPTS" -lt 2 ]; }; then
    echo "[ftx] VERIFIED_NEGATIVE: fewer than two DISTINCT, diverse usable compiler" >&2
    echo "configurations ($N configs, $DISTINCT distinct (binary,opt), $BINARIES" >&2
    echo "binaries, $OPTS opt levels). Exact probe transcript above; no PASS is" >&2
    echo "fabricated. Cross-configuration timing evidence is unavailable on this host." >&2
    fail_closed "second compiler configuration unavailable"
fi
echo "[ftx] usable configs: $N (distinct=$DISTINCT binaries=$BINARIES optlevels=$OPTS)"
for i in "${!OK_NAME[@]}"; do
    echo "[ftx]   ${OK_NAME[$i]}: ${OK_VER[$i]} (${OK_OPT[$i]})"
done

# ---- per-configuration run of the EXISTING gates (no logic duplicated) ----
RESULTS=()
for i in "${!OK_NAME[@]}"; do
    name=${OK_NAME[$i]}; label=${OK_LABEL[$i]}
    cc=${OK_CC[$i]}; opt=${OK_OPT[$i]}
    echo "[ftx] --- config $name ($label) ---"

    if [ "$MODE" = gate ]; then
        out="$TMP/$name.gate.out"
        if ! CC="$cc" EDDSA_TIMING_OPT="$opt" bash "$TIMING_GATE" >"$out" 2>&1; then
            cat "$out" >&2
            fail_closed "$name: shipped fixed-path timing gate did not pass"
        fi
        grep -q 'FIXED_TIMING_GATE PASSED' "$out" || {
            cat "$out" >&2
            fail_closed "$name: no FIXED_TIMING_GATE PASSED marker"
        }
        ts=$(grep -E '^fixed_timing_(shipped|control)_t=' "$out" | tr '\n' ' ')
        echo "[ftx]   $name: gate PASSED (shipped flat, control flagged)"
    else
        out="$TMP/$name.negctl.out"
        if ! CC="$cc" EDDSA_TIMING_OPT="$opt" bash "$TIMING_NEGCTL" >"$out" 2>&1; then
            cat "$out" >&2
            fail_closed "$name: timing negative control did not pass"
        fi
        grep -q 'FIXED_TIMING_NEGCTL PASSED' "$out" || {
            cat "$out" >&2
            fail_closed "$name: no FIXED_TIMING_NEGCTL PASSED marker"
        }
        ts=$(grep -E '^fixed_timing_(shipped|control)_t=' "$out" | tr '\n' ' ')
        echo "[ftx]   $name: negative control PASSED (detector non-vacuous)"
    fi
    [ -n "$ts" ] && echo "[ftx]   $name: $ts"
    RESULTS+=("$name:$opt:$cc")
done

if [ "$MODE" = gate ]; then
    echo "[ftx] PASS: shipped ladder flat AND variable control flagged under all $N configurations"
    echo "[ftx] Reused tools/ed25519_fixed_timing_gate.sh; fixed thresholds, no retries."
    echo "[ftx] Non-formal empirical evidence -- see docs/ED25519_FIXED_TIMING_CROSSCONFIG.md."
else
    echo "[ftx] PASS: detector flags the secret-dependent control under all $N configurations"
    echo "[ftx] Reused tools/ed25519_fixed_timing_negctl.sh; fixed thresholds, no retries."
fi
echo "$MARKER PASSED"
