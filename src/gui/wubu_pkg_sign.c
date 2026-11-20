/*
 * wubu_pkg_sign.c -- .wubu canonical digest + package verification owner.
 * The RFC 8032 field/scalar math lives in wubu_pkg_ed25519.c (optional
 * mbedTLS build); this file is the single owner of the signed-message
 * layout and of the fail-closed package gate.
 */
#include "wubu_pkg_sign.h"
#include "wubu_pkg_sign_internal.h"
#include "../runtime/wubu_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wubu_pkg_sign_set_err(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

static void digest_zero_span(wubu_SHA256_CTX *ctx, size_t n) {
    static const uint8_t zeros[64] = {0};
    while (n) {
        size_t take = n < sizeof(zeros) ? n : sizeof(zeros);
        wubu_sha256_update(ctx, zeros, take);
        n -= take;
    }
}

bool wubu_pkg_sign_digest(const uint8_t *header, size_t header_len,
                          size_t sig_offset, size_t sig_len,
                          const uint8_t *manifest, size_t manifest_len,
                          const char *payload_path,
                          uint8_t out_digest[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!header || !out_digest || !payload_path ||
        sig_offset > header_len || sig_len > header_len - sig_offset ||
        !manifest) return false;

    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, header, sig_offset);
    digest_zero_span(&ctx, sig_len);
    wubu_sha256_update(&ctx, header + sig_offset + sig_len,
                       header_len - sig_offset - sig_len);
    if (manifest_len) wubu_sha256_update(&ctx, manifest, manifest_len);

    FILE *f = fopen(payload_path, "rb");
    if (!f) return false;
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        wubu_sha256_update(&ctx, buf, n);
    bool bad = ferror(f) != 0;
    fclose(f);
    if (bad) return false;

    wubu_sha256_final(&ctx, out_digest);
    return true;
}

bool wubu_pkg_sign_digest_file(const char *pkg_path, size_t header_len,
                               size_t sig_offset, size_t sig_len,
                               size_t manifest_len,
                               uint8_t out_digest[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!pkg_path || !out_digest || sig_offset > header_len ||
        sig_len > header_len - sig_offset)
        return false;

    FILE *f = fopen(pkg_path, "rb");
    if (!f) return false;
    uint8_t *header = (uint8_t *)malloc(header_len ? header_len : 1);
    if (!header) { fclose(f); return false; }
    bool ok = fread(header, 1, header_len, f) == header_len;

    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    if (ok) {
        wubu_sha256_update(&ctx, header, sig_offset);
        digest_zero_span(&ctx, sig_len);
        wubu_sha256_update(&ctx, header + sig_offset + sig_len,
                           header_len - sig_offset - sig_len);
    }

    /* Manifest sits immediately after the header. */
    uint64_t want = (uint64_t)header_len + (uint64_t)manifest_len;
    uint8_t buf[8192];
    size_t left = manifest_len;
    while (ok && left) {
        size_t take = left < sizeof(buf) ? left : sizeof(buf);
        size_t rd = fread(buf, 1, take, f);
        if (rd != take) { ok = false; break; }
        wubu_sha256_update(&ctx, buf, rd);
        left -= rd;
    }

    /* Everything after header+manifest is payload. */
    while (ok) {
        size_t rd = fread(buf, 1, sizeof(buf), f);
        if (rd) wubu_sha256_update(&ctx, buf, rd);
        if (rd < sizeof(buf)) {
            if (ferror(f)) ok = false;
            break;
        }
    }
    if (ok) {
        long got = ftell(f);
        /* Reject truncated containers before a digest can be trusted. */
        if (got < 0 || (uint64_t)got < want) ok = false;
    }
    fclose(f);
    free(header);
    if (!ok) return false;

    wubu_sha256_final(&ctx, out_digest);
    return true;
}
#ifndef WUBU_HAVE_MBEDTLS

bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    (void)digest; (void)seed_hex; (void)out_sig;
    wubu_pkg_sign_set_err(err, errsz, "mbedTLS not compiled in: Ed25519 signing unavailable");
    return false;
}
bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz) {
    (void)digest; (void)pubkey_hex; (void)sig;
    wubu_pkg_sign_set_err(err, errsz, "mbedTLS not compiled in: Ed25519 verification unavailable");
    return false;
}

#endif /* !WUBU_HAVE_MBEDTLS */

bool wubu_pkg_sign_verify_package(const char *pkg_path, size_t header_len,
                                  size_t sig_offset, size_t sig_len,
                                  size_t manifest_len, const char *pubkey_hex,
                                  char *err, size_t errsz) {
    wubu_pkg_sign_set_err(err, errsz, "");
#ifndef WUBU_HAVE_MBEDTLS
    (void)pkg_path; (void)header_len; (void)sig_offset; (void)sig_len;
    (void)manifest_len;
    wubu_pkg_sign_set_err(err, errsz, "mbedTLS not compiled in: refusing to verify");
    return false;
#else
    if (!pkg_path || !pubkey_hex || !pubkey_hex[0] ||
        sig_len != WUBU_PKG_SIGNATURE_LEN) {
        wubu_pkg_sign_set_err(err, errsz, "no trust anchor supplied for Ed25519 verification");
        return false;
    }
    FILE *f = fopen(pkg_path, "rb");
    if (!f) { wubu_pkg_sign_set_err(err, errsz, "cannot open package"); return false; }
    uint8_t *header = (uint8_t *)malloc(header_len ? header_len : 1);
    if (!header) { fclose(f); wubu_pkg_sign_set_err(err, errsz, "out of memory"); return false; }
    bool ok = fread(header, 1, header_len, f) == header_len;
    fclose(f);
    if (!ok) { free(header); wubu_pkg_sign_set_err(err, errsz, "truncated package header"); return false; }

    const uint8_t *sig = header + sig_offset;
    bool absent = true;
    for (size_t i = 0; i < sig_len; i++)
        if (sig[i]) { absent = false; break; }
    uint8_t *sigcopy = (uint8_t *)malloc(sig_len);
    if (sigcopy) memcpy(sigcopy, sig, sig_len);
    free(header);
    if (!sigcopy) { wubu_pkg_sign_set_err(err, errsz, "out of memory"); return false; }
    if (absent) {
        free(sigcopy);
        wubu_pkg_sign_set_err(err, errsz, "package is unsigned (empty Ed25519 signature)");
        return false;
    }

    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    bool good = wubu_pkg_sign_digest_file(pkg_path, header_len, sig_offset,
                                          sig_len, manifest_len, digest);
    if (!good) {
        free(sigcopy);
        wubu_pkg_sign_set_err(err, errsz, "cannot digest package contents");
        return false;
    }
    good = wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sigcopy, err, errsz);
    free(sigcopy);
    return good;
#endif
}
