/* wubu_pwpolicy_test.c -- the gate for D1.
 *
 * The rules are ours, so this gate cannot lean on a foreign oracle: what it does instead is
 * pin the DECISIONS. Each case names the rule it is testing, and the acceptances are the
 * interesting half -- a policy that refuses a good passphrase is a policy that teaches users
 * to write passwords on a sticky note.
 */
#include "wubu_pwpolicy.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;

static void expect(const char *pw, const char *user, int code) {
    int got = wubu_pw_check(pw, user);
    if (got == code) { pass++; return; }
    printf("  [FAIL] \"%s\" (user %s): got %d (%s), want %d (%s)\n", pw,
           user ? user : "-", got, wubu_pw_reason(got), code, wubu_pw_reason(code));
    fail++;
}

int main(void) {
    printf("=== wubu_pwpolicy (gate D1) ===\n");

    /* refusals */
    expect("", NULL, WUBU_PW_TOO_SHORT);
    expect("short1A", NULL, WUBU_PW_TOO_SHORT);
    expect("aaaaaaaaaaaa", NULL, WUBU_PW_FEW_CLASSES);     /* one class, repeated */
    expect("abcdefghij", NULL, WUBU_PW_FEW_CLASSES);       /* lower only */
    expect("password", NULL, WUBU_PW_TOO_SHORT);           /* 8 chars: length first */
    expect("password123", NULL, WUBU_PW_COMMON);           /* 11 chars, but on the list */
    expect("Password123", NULL, WUBU_PW_COMMON);           /* on the list, any case */
    expect("qwertyQWERTY1", NULL, WUBU_PW_COMMON);
    expect("wubuosAdmin1", NULL, WUBU_PW_COMMON);
    expect("AliceStrong1", "alice", WUBU_PW_HAS_USER);
    expect("ecilAStrong1", "alice", WUBU_PW_HAS_USER);     /* reversed name too */
    expect("aaaaaaaaaA1", NULL, WUBU_PW_REPEATED);
    expect("abcdefghijK1", NULL, WUBU_PW_REPEATED);        /* straight run */
    {
        char big[300];
        memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = '\0';
        expect(big, NULL, WUBU_PW_TOO_LONG);
    }

    /* acceptances: these must NOT be refused */
    expect("Correct-Horse-9", NULL, WUBU_PW_OK);
    expect("wubu os desktop 7", NULL, WUBU_PW_COMMON);     /* the brand name is a guess */
    expect("quiet river lantern 4", NULL, WUBU_PW_OK);
    expect("Tr0ub4dor&3x", NULL, WUBU_PW_OK);
    expect("9x!Qmv#2Lp", NULL, WUBU_PW_OK);
    expect("Correct-Horse-9", "alice", WUBU_PW_OK);
    /* a non-Latin passphrase is counted in CHARACTERS, not bytes: 10 CJK characters is
     * 30 bytes, and a byte-based rule would call it long enough on its own */
    expect("密码密码密码密码密码", NULL, WUBU_PW_FEW_CLASSES);   /* long enough, but one class */
    expect("密码密码密码密码密A1", NULL, WUBU_PW_OK);

    /* reasons are user-facing sentences, not codes */
    if (strstr(wubu_pw_reason(WUBU_PW_TOO_SHORT), "10") != NULL) pass++;
    else { printf("  [FAIL] the short-password reason does not say the minimum\n"); fail++; }

    /* the strength bar never contradicts an acceptance: 100 must imply OK */
    {
        const char *good[] = { "Correct-Horse-9", "9x!Qmv#2Lp", "Tr0ub4dor&3x" };
        int ok = 1;
        for (int i = 0; i < 3; i++) if (wubu_pw_strength(good[i]) < 50) ok = 0;
        if (ok) pass++; else { printf("  [FAIL] a strong accepted password scored under 50\n"); fail++; }
        if (wubu_pw_strength("abc") < wubu_pw_strength("Correct-Horse-9")) pass++;
        else { printf("  [FAIL] strength does not order weak below strong\n"); fail++; }
    }

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
