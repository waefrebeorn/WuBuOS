/*
 * wubu_dmcrypt_selftest.c -- verifies kernel-owned dm-crypt routing.
 */
#include "wubu_dmcrypt.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_dmcrypt_selftest ===\n\n");
    wubu_hw_detect();
    wubu_dmcrypt_probe();
    printf("  crypt=%d luks=%d aes=%d xts=%d dm=%d\n",
           wubu_dmcrypt_present(), wubu_dmcrypt_luks(), wubu_dmcrypt_aes(),
           wubu_dmcrypt_xts(), wubu_dmcrypt_dm());

    CHECK(strcmp(wubu_dmcrypt_cipher_for("aes"), "aes") == 0,
          "aes -> aes");
    CHECK(strcmp(wubu_dmcrypt_cipher_for("serpent"), "serpent") == 0,
          "serpent -> serpent");
    CHECK(strcmp(wubu_dmcrypt_cipher_for("twofish"), "twofish") == 0,
          "twofish -> twofish");
    CHECK(strcmp(wubu_dmcrypt_cipher_for("camellia"), "camellia") == 0,
          "camellia -> camellia");
    CHECK(strcmp(wubu_dmcrypt_cipher_for("zzz"), "aes") == 0,
          "zzz -> aes fallback");

    CHECK(strcmp(wubu_dmcrypt_mode_for("xts"), "xts") == 0,
          "xts -> xts");
    CHECK(strcmp(wubu_dmcrypt_mode_for("cbc"), "cbc") == 0,
          "cbc -> cbc");
    CHECK(strcmp(wubu_dmcrypt_mode_for("ecb"), "ecb") == 0,
          "ecb -> ecb");
    CHECK(strcmp(wubu_dmcrypt_mode_for("gcm"), "gcm") == 0,
          "gcm -> gcm");
    CHECK(strcmp(wubu_dmcrypt_mode_for("zzz"), "xts") == 0,
          "zzz -> xts fallback");

    char s[256];
    wubu_dmcrypt_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dmcrypt summary generated");

    printf("\n=== DMCRYPT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
