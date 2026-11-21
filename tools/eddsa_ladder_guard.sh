#!/usr/bin/env bash
# eddsa_ladder_guard.sh -- ROOT-INDEPENDENT structural constant-time guard for
# the shipped Ed25519 Montgomery ladder.  Member of the DEFAULT test suite
# (`make test_ed25519_ladder_guard`, a prerequisite of test_medium_other).
#
# Unlike tools/eddsa_ct_evidence.sh and tools/eddsa_dudect_gate.sh, this guard
# does NOT resolve an mbedTLS dev root, compile anything or fetch anything.  It
# reads src/gui/wubu_pkg_ed25519.c as TEXT and mechanically checks the source
# invariants that make ge_mul_ladder constant-time (constant trip count, no
# secret-dependent branch/ternary/index, unconditional ge_cswap(...,swap), and
# a four-coordinate safe_cond_swap selection primitive).  It therefore ALWAYS
# runs -- including the no-root tier, closing that blind spot -- and it goes
# RED on the `if (swap) ge_cswap(..., 1)` regression the negative control
# injects.  The statistical dudect gate is unchanged and still runs when a
# dev root is present.
#
# This is structural evidence, NOT a formal constant-time proof.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
exec python3 "$ROOT/tools/eddsa_ladder_guard.py" \
    "$ROOT/src/gui/wubu_pkg_ed25519.c"
