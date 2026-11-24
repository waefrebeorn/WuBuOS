#!/usr/bin/env bash
# default_optin_gates.sh -- deterministic, loopback-only crypto/GUI gates that
# now run in the DEFAULT `make test` tier (test_default_optin).
#
# Honesty contract:
#   * every gate prints an explicit `[<tag>] SKIP: ...` line when its optional
#     mbedTLS dev root is absent.  Exiting zero with NEITHER a SKIP line NOR a
#     PASS marker is treated as a FAIL here -- no silent PASS.
#   * this wrapper prints SKIP_COUNT=<n> / EXPECTED_SKIP_COUNT=<n> and asserts
#     they match: 0 with a dev root (all four execute), 4 without one (all
#     four skip).  A partial skip set is a FAIL, not quiet partial coverage.
#   * no gate fetches anything: all resolve the root through
#     tools/mbedtls_devroot.sh, which only validates an ALREADY-present root.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

GATES=(
  "eddsa_ct_evidence.sh:ct"
  "eddsa_dudect_gate.sh:dudect"
  "sync_panel_gate.sh:sync_panel"
  "hosted_tls_gate.sh:hosted_tls"
)

TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-default-optin.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

if bash "$ROOT/tools/mbedtls_devroot.sh" >/dev/null 2>&1; then
    EXPECTED_SKIPS=0
    ENV_NOTE="mbedTLS dev root present: all gates must execute"
else
    EXPECTED_SKIPS=${#GATES[@]}
    ENV_NOTE="mbedTLS dev root absent: all gates must SKIP (no fetch, no silent PASS)"
fi

skips=0
fails=0
# The four gates are independent and the two timing gates dominate the wall
# time (the whole tier measured 417 s of the suite's 430 s, and >180 s of that
# was this wrapper running them one after another). Run them concurrently --
# same scripts, same assertions, same fail-closed accounting; only the
# scheduling changed. Each gate writes its own log and rc file so the
# accounting below is identical to the serial loop.
pids=()
for spec in "${GATES[@]}"; do
    script=${spec%%:*}
    tag=${spec##*:}
    ( bash "$ROOT/tools/$script" >"$TMP/$tag.log" 2>&1; echo $? >"$TMP/$tag.rc" ) &
    pids+=($!)
done
for pid in "${pids[@]}"; do wait "$pid" 2>/dev/null || true; done

for spec in "${GATES[@]}"; do
    script=${spec%%:*}
    tag=${spec##*:}
    log="$TMP/$tag.log"
    rc=$(cat "$TMP/$tag.rc" 2>/dev/null || echo 99)
    if [ "$rc" = "0" ]; then
        cat "$log"
        if grep -q "^\[$tag\] SKIP:" "$log"; then
            skips=$((skips + 1))
        elif ! grep -qi 'PASS' "$log"; then
            echo "[default_optin] FAIL: $script exited 0 with neither SKIP nor PASS (silent pass)" >&2
            fails=$((fails + 1))
        fi
    else
        cat "$log" >&2
        echo "[default_optin] FAIL: $script exited $rc" >&2
        fails=$((fails + 1))
    fi
done

echo "[default_optin] SKIP_COUNT=$skips EXPECTED_SKIP_COUNT=$EXPECTED_SKIPS GATES=${#GATES[@]}"
echo "[default_optin] env: $ENV_NOTE"
if [ "$fails" -ne 0 ]; then
    echo "[default_optin] FAIL: $fails gate(s) failed" >&2
    exit 1
fi
if [ "$skips" -ne "$EXPECTED_SKIPS" ]; then
    echo "[default_optin] FAIL: skip count $skips != $EXPECTED_SKIPS (partial coverage is not honest)" >&2
    exit 1
fi
echo "[default_optin] PASS: opt-in crypto/GUI gates in default tier; skip count asserted"
