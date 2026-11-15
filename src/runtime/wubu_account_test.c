/* wubu_account_test.c -- OOBE account-record gate. */
#include "wubu_account.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define OK(cond, msg) do { if (!(cond)) { printf("FAIL %s\n", msg); fails++; } } while (0)

int main(void)
{
    wubu_account_t rec;
    char line[WUBU_ACCOUNT_RECORD_MAX];

    /* Deterministic (pinned salt): build, serialise, verify. */
    OK(wubu_account_build(&rec, "wubu", "omar-x1", "us", "hunter2", "fixedsalt") == 0,
       "build with pinned salt");
    int n = wubu_account_record_str(&rec, line, sizeof(line));
    OK(n > 0 && strstr(line, "user=wubu") && strstr(line, "host=omar-x1")
       && strstr(line, "kbd=us") && strstr(line, "pw=$6$"),
       "record serialises all fields and the $6$ hash");

    /* The hash is the real thing: verify() accepts the password, rejects wrong. */
    OK(wubu_account_verify(&rec, "hunter2"), "correct password verifies");
    OK(!wubu_account_verify(&rec, "hunt3r2"), "wrong password rejected");
    OK(!wubu_account_verify(&rec, ""), "empty password rejected");

    /* Two boots with the same password must NOT reuse the same salt (shadow req),
     * so the hashes differ even with an unpinned build. Pin instead for exactly. */
    wubu_account_t r2;
    OK(wubu_account_build(&r2, "wubu", "omar-x1", "us", "hunter2", "fixedsalt") == 0,
       "rebuild with same pinned salt");
    OK(strcmp(rec.hash, r2.hash) == 0,
       "pinned salt yields the identical hash (deterministic)");
    OK(wubu_account_verify(&r2, "hunter2"), "rebuilt record verifies");

    /* Unpinned salts differ. */
    wubu_account_t r3;
    OK(wubu_account_build(&r3, "wubu", "omar-x1", "us", "hunter2", NULL) == 0,
       "build with generated salt");
    OK(strcmp(rec.hash, r3.hash) != 0,
       "generated salt differs from pinned (fresh salt per boot)");
    OK(wubu_account_verify(&r3, "hunter2"), "generated-salt record verifies");

    /* Fail-closed on names that would corrupt a shadow file. */
    OK(wubu_account_build(&r3, "wu:bu", "h", "us", "pw", "fixed") == -1, "colon refused");
    OK(wubu_account_build(&r3, "wu bu", "h", "us", "pw", "fixed") == -1, "space refused");
    OK(wubu_account_build(&r3, "a/b", "h", "us", "pw", "fixed") == -1, "slash refused");
    OK(wubu_account_build(&r3, "", "h", "us", "pw", "fixed") == -1, "empty user refused");
    OK(wubu_account_build(&r3, "wu", "h", "us", "", "fixed") == -1, "empty password refused");

    /* A record with a bad stored hash never verifies, it does not crash. */
    wubu_account_t bad = rec;
    bad.hash[0] = 'x';
    OK(!wubu_account_verify(&bad, "hunter2"), "corrupt stored hash never verifies");

    printf(fails ? "FAIL %d\n" : "PASS 15 checks\n", fails);
    return fails ? 1 : 0;
}
