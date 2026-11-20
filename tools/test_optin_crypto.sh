#!/usr/bin/env bash
# test_optin_crypto.sh -- ONE deterministic entry point for every opt-in
# crypto gate.
#
# 1. Fetches (once, idempotent) the SHA-256-pinned mbedTLS dev root into
#    <repo>/.cache/mbedtls-devroot via `make mbedtls-devroot`.
# 2. Exports that root to every gate (pkg_key_recovery_gate.sh requires
#    WUBU_MBEDTLS_ROOT in the environment; the others self-resolve but honour
#    the explicit root too).
# 3. Runs each gate in a fixed order, fail-fast: the first gate that
#    ultimately fails aborts the whole aggregate (set -e).
#
# Determinism: paths are derived from this script's location, never from a
# developer's $HOME/.hermes/cache/scratch; developer overrides of the root
# destination are cleared. No secrets, no key material is committed.
#
# Host-only OpenSSL is NOT assumed: the one OpenSSL-dependent gate
# (test_ed25519_native) is gated here and skipped with a clear line when the
# dev headers/libcrypto/CLI are unavailable, so this target stays green on a
# bare runner while still exercising all mbedTLS-only gates.
#
# test_ed25519_timing is a statistical differential (Pearson r/t vs fixed
# thresholds) and is measurably noisy under CPU contention. It is retried up
# to three times and passes ONLY on a real gate PASS; the gate's own criteria
# are unchanged. If every attempt fails, the aggregate fails.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Clear any developer-supplied root so the pinned repo-local copy is used.
unset WUBU_MBEDTLS_ROOT MBEDTLS_DEVROOT

# 1. Ensure the pinned, repo-local dev root (network only if not cached).
make -C "$ROOT" mbedtls-devroot

# 2. Resolve and export it to every gate.
DEVROOT=$("$ROOT/tools/mbedtls_devroot.sh")
export WUBU_MBEDTLS_ROOT="$DEVROOT"
export LD_LIBRARY_PATH="$DEVROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

GATES=(
    test_pkg_ed25519
    test_pkg_key_revoke
    test_pkg_revoke_feed
    test_pkg_revoke_sync
    test_sync_panel
    test_pkg_key_policy
    test_pkg_key_recovery
    test_ed25519_timing
    test_ed25519_native
    test_oci_tls
)

run_gate() {
    local gate=$1
    echo "================================================================"
    echo "[test_optin_crypto] RUN   $gate"
    echo "================================================================"
    make -C "$ROOT" "$gate"
    echo "[test_optin_crypto] PASS  $gate"
}

# OpenSSL prerequisite for the single native backend gate.
openssl_ready() {
    [ -f /usr/include/openssl/evp.h ] &&
        compgen -G '/usr/lib/*/libcrypto.so' >/dev/null &&
        command -v openssl >/dev/null 2>&1 &&
        command -v xxd >/dev/null 2>&1
}

# Fail-fast loop. No statistical retries: the timing gate now judges leak
# removal against a same-run baseline and is deterministic enough for CI.
for gate in "${GATES[@]}"; do
    if [ "$gate" = "test_ed25519_native" ] && ! openssl_ready; then
        echo "[test_optin_crypto] SKIP  $gate (OpenSSL dev headers/libcrypto/CLI absent; install libssl-dev openssl xxd)"
        continue
    fi
    run_gate "$gate"
done

echo "[test_optin_crypto] ALL OPT-IN CRYPTO GATES PASSED"
