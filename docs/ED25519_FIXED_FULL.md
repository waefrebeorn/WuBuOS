# Full bignum-free fixed-width EdDSA path

`test_pkg_ed25519` selects `WUBU_EDDSA_FIXED_LADDER` by default.  On that path
**no mbedTLS integer type is compiled**: the legacy MPI backend in
`src/gui/wubu_pkg_ed25519.c` is guarded out in its entirety, and sign/verify
run through two self-contained leaves:

- `src/gui/wubu_ed25519_fixed.c` — field arithmetic, point decode/encode,
  point addition, the 256-iteration constant-trip-count ladder, and fixed-width
  scalar mod-L arithmetic (`2^252 + 27742317777372353535851937790883648493`),
  all in 64-bit little-endian limbs.
- `src/gui/wubu_ed25519_fixed_sign.c` — RFC 8032 sign/verify orchestration.
  mbedTLS SHA-512 is used for hashing; it is a hash, not a bignum.  Policy is
  enforced locally: non-canonical `S` and small-subgroup `A` are rejected
  before the group equation is accepted.

`WUBU_EDDSA_FIXED=0` still selects the documented legacy mbedTLS MPI backend;
`WUBU_EDDSA_NATIVE=1` still selects the OpenSSL EVP hybrid.  Neither changed.

## Gate

`make test_ed25519_fixed_full` (`tools/ed25519_fixed_full_gate.sh`) asserts:

1. zero `mbedtls_mpi`/`bignum` symbols in the shipped fixed-path sources;
2. zero such symbols in the **preprocessed** fixed-mode dispatcher, proving
   the bignum header and the whole MPI backend are compiled out;
3. detector non-vacuity: `tools/ed25519_fixed_full_negctl.sh` injects a real
   `mbedtls_mpi` into temp copies (working tree untouched), requires both
   checks to go RED on the injection and GREEN on the shipped sources;
4. RFC 8032 correctness: `tests/fixtures/fixed_full/probe.c` cross-checks
   signatures against OpenSSL EVP byte-for-byte in both directions, anchors
   test vector 1's key pair, and exercises tamper / non-canonical-`S` /
   identity-public-key negatives.

The checker is `tools/ed25519_fixed_bignum_check.py`.  It audits identifiers,
not prose: comments and string literals are stripped before the symbol scan.
`mbedtls_sha512_*` is explicitly allowed.  A standalone negctl target is
available as `make test_ed25519_fixed_full_negctl`.
