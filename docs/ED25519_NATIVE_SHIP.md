# Native EdDSA shipped path (hosted/product build)

This document is the contract for the shipped EdDSA backend selection in the
hosted/product build.  It replaces the earlier "opt-in only" posture: the
native OpenSSL core is now **auto-detected and linked** when its prerequisites
are already installed, and the in-repo constant-time ladder remains the
fallback.  No network fetch is part of the default build.

## Selection matrix

`tools/detect_native_eddsa.sh` runs at make parse time (from `mk/vars.mk`) and
writes `build/native_eddsa.mk` + `build/native_eddsa_config.h`.  It probes only
what is present; it never downloads, installs or invokes `make mbedtls-devroot`.

| OpenSSL 3 `EVP_PKEY_ED25519` (headers + linkable libcrypto) | cached mbedTLS dev root | shipped backend | link flags |
|---|---|---|---|
| present | present | **native with ladder fallback** | `-lcrypto` + `-lmbedcrypto` (+rpath) |
| absent  | present | ladder | `-lmbedcrypto` (+rpath) |
| present | absent  | fail-closed; native NOT enabled (no fetch) | none |
| absent  | absent  | fail-closed; still builds | none |

The native core is nested inside the same `WUBU_HAVE_MBEDTLS` guard as the
shared policy layer (non-canonical-S and small-subgroup-A rejection).  That is
deliberate: the native verifier never delegates policy to OpenSSL.  It also
means "native" always ships **with** the ladder, never as a native-only build.

`WUBU_MBEDTLS_ROOT` can point at an externally supplied root; otherwise the
already-cached repo-local path `.cache/mbedtls-devroot` is used.  Neither path
is fetched by `make hosted`.

## What actually links

`mk/targets.mk` compiles the two signing leaves into `build/hosted/` with the
detected flags (see the pattern rule in `mk/pattern-rules.mk`) and links the
detected libraries into `src/hosted/wubu`.  Using a dedicated object path
prevents a stale fail-closed object from a GUI/test build being reused by
`make hosted`.  All other targets keep the plain GUI objects, so no new link
dependency leaks into unrelated tests.

## Gates in the default test suite

Added to `test_medium_other` (hence to plain `make test`):

- `test_eddsa_build_config` -> `tools/eddsa_config_gate.sh`.  Asserts the
  detector matches an independent probe, emits ladder-only when OpenSSL is
  forced absent, emits a valid config when the dev root is forced absent, and
  **no network fetch is attempted** (PATH shims for `curl`/`wget`/`apt` abort
  if called).  Also compiles a TU with the emitted CFLAGS to assert the
  `native => ladder` invariant.
- `test_eddsa_runtime` -> `tools/eddsa_runtime_gate.sh`.  Uses the SAME
  detected config as the product.  With both backends it runs the full
  `tools/ed25519_native_gate.sh` differential (cross-sign/verify, OpenSSL CLI
  interoperability, tampered signature/digest/key, small-subgroup and
  non-canonical-S forgeries).  With ladder only it runs a sign/verify plus a
  tampered-signature negative.  With no backend it prints an explicit SKIP.
- `test_ed25519_ct_evidence` -> `tools/eddsa_ct_evidence.sh`.  Regenerates
  `docs/ED25519_CONSTANT_TIME_EVIDENCE.md` from a real compile and sweep and
  asserts the constant-selection count is invariant across six secret-scalar
  classes.

The pre-existing opt-in gates (`test_pkg_ed25519`, `test_pkg_ed25519_native`,
`test_ed25519_native`, `test_ed25519_timing`, `test_optin_crypto`) remain
available and unchanged.

## Negative controls

- Tampered signature, tampered digest, wrong public key: rejected on both
  backends by `test_ed25519_native`.
- Small-subgroup public key (`A` of low order) and non-canonical `S`: rejected
  by the shared policy layer **before** the native core runs.
- `WUBU_EDDSA_FORCE_LADDER` simulates OpenSSL unavailability; its output is
  byte-identical to the pure ladder and it reports `NATIVE_OPS=0`.
- Build-time config gate: PATH network shims are never invoked.

## Remaining gaps

1. **No mbedTLS-free native build.** If OpenSSL is present but no mbedTLS dev
   root is cached, the native core is not enabled because the shared policy
   checks live under `WUBU_HAVE_MBEDTLS`.  Closing this needs a bignum-free
   reimplementation of the low-order-point and canonical-S checks; it is not
   attempted here, and no fetch is performed to paper over it.
2. **Not a formal constant-time proof.** See
   `docs/ED25519_CONSTANT_TIME_EVIDENCE.md`: the evidence is structural plus a
   reproducible instruction/branch table and a dynamic swap count, not a
   mechanised proof over mbedTLS's assembly or the compiler.
3. **Compiler/toolchain scope.** The disassembly table describes this host,
   `CC` and `-O1`; a different toolchain is not covered by that table (the
   source-level audit and fixed 256-iteration schedule still are).
4. **Timing gate remains statistical.** `tools/ed25519_timing_gate.sh` shows a
   large legacy-vs-ladder differential; it is evidence of leak removal, not a
   proof of absence.
