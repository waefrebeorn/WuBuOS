#!/usr/bin/env bash
# formal_ct_tool_probe.sh -- EXACT VERIFIED_NEGATIVE evidence for the absence
# of formal / memory-access constant-time tooling on this host.
#
# Probes the WHOLE local ecosystem for the tools named by the formal-CT task:
#   ct-verif, ctgrind, valgrind            (formal / memory-access CT tools)
#   z3 cvc5 cvc4 yices boolector bitwuzla stp boogie smtinterpol (SMT solvers)
#   z3 / cvc5 Python bindings, and their shared libraries, apt cache, dpkg,
#   and on-disk executables under /home/wubu /usr/local /opt.
#
# This gate NEVER asserts a PASS it did not measure.  On this host no formal
# CT engine exists, so the gate prints FORMAL_CT_TOOL_PROBE VERIFIED_NEGATIVE,
# names every missing tool, states why no PASS is asserted, and exits 1
# (fail-closed).  If a formal tool IS present it still asserts no PASS: the
# real formal check against the shipped bignum-free ladder would have to be
# implemented and run first.
#
# NON-VACUITY: a detector that always says MISSING would make this evidence
# worthless.  The gate proves the same detector returns FOUND for known-present
# tools (gcc, python3, objdump, the sys module, libc.so.6), MISSING for
# deliberately absent sentinels, and flips MISSING->FOUND for a temporary
# on-PATH `ct-verif` shim.  A failed control hard-fails the gate.
#
# Reference: docs/EDDSA_FORMAL_CT_TOOL_PROBE.md
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

FORMAL_BINS=(ct-verif ctgrind valgrind boogie)
SMT_BINS=(z3 cvc5 cvc4 yices boolector bitwuzla stp smtinterpol)
PY_MODS=(z3 cvc5)
SMT_LIBS=(libz3 libboolector libyices libcvc4 libcvc5)
ALL_MISSING=1

probe_bin() { command -v "$1" >/dev/null 2>&1 && echo FOUND || echo MISSING; }
probe_py()  { python3 -c "import $1" >/dev/null 2>&1 && echo FOUND || echo MISSING; }
probe_lib() { ldconfig -p 2>/dev/null | grep -qi -- "$1" && echo FOUND || echo MISSING; }

fail() {
    echo "[formal_probe] FAIL: $*" >&2
    echo "FORMAL_CT_TOOL_PROBE FAILED"
    exit 2
}

echo "============================================================"
echo " formal CT tool probe -- $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo " host: $(uname -srm)"
echo " gcc:  $(gcc --version 2>/dev/null | head -1 || true)"
echo " py:   $(python3 --version 2>&1 | head -1 || true)"
echo "============================================================"

echo
echo "[1] PATH binaries -- formal / memory-access CT tools"
for t in "${FORMAL_BINS[@]}"; do
    r=$(probe_bin "$t"); echo "  BIN  $t = $r"; [ "$r" = FOUND ] && ALL_MISSING=0
done
echo "[2] PATH binaries -- SMT solvers"
for t in "${SMT_BINS[@]}"; do
    r=$(probe_bin "$t"); echo "  BIN  $t = $r"; [ "$r" = FOUND ] && ALL_MISSING=0
done
echo "[3] Python SMT bindings"
for m in "${PY_MODS[@]}"; do
    r=$(probe_py "$m"); echo "  PY   $m = $r"; [ "$r" = FOUND ] && ALL_MISSING=0
done
echo "[4] shared libraries (ldconfig -p)"
for l in "${SMT_LIBS[@]}"; do
    r=$(probe_lib "$l"); echo "  LIB  $l = $r"; [ "$r" = FOUND ] && ALL_MISSING=0
done

echo "[5] ecosystem sweep -- apt cache / dpkg package names / on-disk executables"
APT=$(ls /var/cache/apt/archives/ 2>/dev/null | grep -cE '^(valgrind|z3|cvc4|cvc5|boolector|yices|bitwuzla|stp)_' || true)
DPKG=$(dpkg-query -W -f='${Package}\n' 2>/dev/null | grep -cxE 'valgrind|z3|cvc4|cvc5|boolector|yices|bitwuzla|stp' || true)
DISK=$(find /home/wubu /usr/local /opt -type f \( -name z3 -o -name cvc5 -o -name cvc4 -o -name valgrind -o -name ct-verif -o -name boolector -o -name yices \) 2>/dev/null | wc -l)
echo "  apt-cache matching .deb    = $APT"
echo "  dpkg matching package names = $DPKG"
echo "  on-disk CT/SMT executables = $DISK"
if [ "${APT:-0}" -gt 0 ] || [ "${DPKG:-0}" -gt 0 ] || [ "${DISK:-0}" -gt 0 ]; then
    ALL_MISSING=0
fi

echo
echo "[N] NON-VACUITY CONTROLS -- prove the detector is live, not an always-MISSING stub"
for t in gcc python3 objdump; do
    r=$(probe_bin "$t")
    [ "$r" = FOUND ] || fail "known-present control '$t' reported $r (detector broken)"
    echo "  present-control  BIN  $t = FOUND (expected) OK"
done
r=$(probe_py sys); [ "$r" = FOUND ] || fail "known-present control 'sys' module reported $r"
echo "  present-control  PY   sys = FOUND (expected) OK"
r=$(probe_lib libc.so.6); [ "$r" = FOUND ] || fail "known-present control 'libc.so.6' reported $r"
echo "  present-control  LIB  libc.so.6 = FOUND (expected) OK"

SENT=wubu_formal_ct_sentinel_9f3a7c
r=$(probe_bin "$SENT"); [ "$r" = MISSING ] || fail "absent sentinel BIN reported $r"
echo "  absent-control   BIN  $SENT = MISSING (expected) OK"
r=$(probe_py wubu_no_such_pymod_9f3a7c); [ "$r" = MISSING ] || fail "absent sentinel PY reported $r"
echo "  absent-control   PY   sentinel = MISSING (expected) OK"
r=$(probe_lib wubu_no_such_lib_9f3a7c); [ "$r" = MISSING ] || fail "absent sentinel LIB reported $r"
echo "  absent-control   LIB  sentinel = MISSING (expected) OK"

TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-formal-probe.XXXXXX") || fail "mktemp failed"
printf '#!/bin/sh\nexit 0\n' > "$TMP/ct-verif"
chmod +x "$TMP/ct-verif"
BASE=$(probe_bin ct-verif)
WITH=$(PATH="$TMP:/usr/bin:/bin" bash -c 'command -v ct-verif >/dev/null 2>&1 && echo FOUND || echo MISSING')
[ "$WITH" = FOUND ] || fail "temporary on-PATH 'ct-verif' shim was NOT detected (FOUND expected)"
echo "  shim-control     ct-verif: baseline=$BASE, shim-on-PATH=$WITH (MISSING->FOUND) OK"
rm -rf "$TMP"

echo
echo "=== VERDICT ==="
if [ "$ALL_MISSING" -ne 1 ]; then
    echo "FORMAL_CT_TOOL_PROBE TOOL_PRESENT"
    echo "A formal CT tool appears available. This probe does NOT assert a PASS from mere"
    echo "presence: a real formal/memory-access CT check against the shipped bignum-free"
    echo "ladder (src/gui/wubu_ed25519_fixed.c) must be implemented and executed first."
    echo "No PASS asserted."
    exit 1
fi

echo "FORMAL_CT_TOOL_PROBE VERIFIED_NEGATIVE"
echo "Probed categories: PATH binaries, Python bindings, shared libraries, apt cache,"
echo "dpkg package names, and on-disk executables. Every formal CT tool, SMT solver,"
echo "binding and library is MISSING on this host."
echo "MISSING tools: ${FORMAL_BINS[*]} ${SMT_BINS[*]} ${PY_MODS[*]} ${SMT_LIBS[*]}"
echo "Why no PASS is asserted:"
echo "  - no ct-verif / SMT engine exists to discharge a non-interference obligation"
echo "    for the fixed 256-iteration scalar ladder;"
echo "  - no valgrind / ctgrind exists to run a secret-shadow memory-access check on"
echo "    the shipped binary (ctgrind is built on valgrind, which is absent);"
echo "  - the existing source / GIMPLE / IR guards and dudect timing statistics are"
echo "    structural / statistical EVIDENCE, not a mechanised constant-time proof;"
echo "  - no network fetch is permitted, so the absent tools cannot be installed here."
echo "Non-vacuity controls passed: present tools=FOUND, sentinels=MISSING, on-PATH shim=FOUND."
echo "This is exact VERIFIED_NEGATIVE evidence. It does NOT assert a constant-time proof."
exit 1
