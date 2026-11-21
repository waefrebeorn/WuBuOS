#!/usr/bin/env bash
# ed25519_fixed_ladder_default.sh -- default-tier adapter for the proof-style
# fixed-width ladder CT gate.
#
# Runs the REAL gate (tools/ed25519_fixed_ladder_gate.sh) whenever the local
# toolchain permits: python3, gcc and objdump.  With any of those missing it
# prints an explicit SKIP and exits 0, so a lean/stripped build host is not a
# red gate.  The standalone targets `make test_ed25519_fixed_ladder` and
# `test_ed25519_fixed_ladder_negctl` remain strict and fail closed.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
missing=""
for tool in python3 gcc objdump; do
    command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
done
if [ -n "$missing" ]; then
    echo "SKIP: proof-style fixed-ladder CT gate (missing:$missing)"
    echo "      run 'make test_ed25519_fixed_ladder' on a host with python3/gcc/objdump"
    exit 0
fi
echo "[default_gate] toolchain present; running proof-style fixed-ladder CT gate"
exec bash "$ROOT/tools/ed25519_fixed_ladder_gate.sh"
