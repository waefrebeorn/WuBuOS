/* wubu_pwcrypt_test.c -- SHA-512 crypt ($6$) gate vs the canonical vectors.
 *
 * The vectors here are the reference source's own test vectors plus a live
 * `openssl passwd -6` capture on the dev box (recorded 2026-11-15). Hermetic:
 * the gate needs no external tool, so it runs anywhere in the tree.
 */
#include "wubu_pwcrypt.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, runs = 0;
#define CHECK(pw, salt, expect) do { \
    char out[WUBU_PWCRYPT_OUT]; \
    const char *r = wubu_pw_crypt_sha512(pw, salt, out); \
    runs++; \
    if (!r || strcmp(r, expect) != 0) { \
        printf("FAIL %s / %s\n   got %s\n   exp %s\n", pw, salt, r ? r : "(null)", expect); \
        fails++; \
    } \
} while (0)

int main(void)
{
    /* Canonical vector from the reference (phadej/crypt-sha512 / glibc). */
    CHECK("Hello world!", "saltstring",
          "$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/O817G3uBnIFNjnQJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1");
    /* openssl passwd -6 capture, dev box 2026-11-15. */
    CHECK("correct horse", "wubuSALT",
          "$6$wubuSALT$jYIDcW7O/Uq465L2Z/qagQSaSpKiV4YqRvktyWFUaZOZ75kNNV/fNTGpGcp0u0oQ4HtXArHztdXrMbxGw1Lla.");
    CHECK("hunter2", "s3cret",
          "$6$s3cret$KoCOqgLcTBqMtgIZ/FV9KStNF8W0Tl4pRMaDIVtk7iT8zg7BaOGufVsZSWBf2c4KsVtdQZDqqSzjjIMebAmM/.");

    /* The salt may arrive with its "$6$" prefix -- must strip it. */
    {
        char out[WUBU_PWCRYPT_OUT];
        const char *a = wubu_pw_crypt_sha512("Hello world!", "$6$saltstring", out);
        runs++;
        const char *b = "$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/O817G3uBnIFNjnQJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1";
        if (!a || strcmp(a, b) != 0) { printf("FAIL prefixed salt\n"); fails++; }
    }

    /* Refusals. */
    {
        char out[WUBU_PWCRYPT_OUT];
        if (wubu_pw_crypt_sha512(NULL, "s", out) != NULL) { printf("FAIL null pw\n"); fails++; }
        if (wubu_pw_crypt_sha512("pw", NULL, out) != NULL) { printf("FAIL null salt\n"); fails++; }
        if (wubu_pw_crypt_sha512("pw", "", out) != NULL) { printf("FAIL empty salt\n"); fails++; }
    }

    printf(fails ? "FAIL %d/%d\n" : "PASS %d checks\n", runs, fails);
    return fails ? 1 : 0;
}
