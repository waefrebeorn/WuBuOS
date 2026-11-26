/*
 * wubu_people_test.c -- the store, including the cases that make a contact list real rather than decorative:
 * persistence across a reload, de-duplication that ignores case, a bounded store, and untrusted input
 * (a very long line) that must be truncated rather than overflowed.
 */
#include "wubu_people.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL %s\n", m); fails++; } \
                         else printf("  ok   %s\n", m); } while (0)

int main(void)
{
    char tmp[] = "/tmp/wubu_people_XXXXXX";
    int fd = mkstemp(tmp);
    CHECK(fd >= 0, "temp store created");
    if (fd < 0) return 1;
    close(fd);
    unlink(tmp);                       /* a MISSING file must be an empty store, not an error */
    setenv("WUBU_PEOPLE_DB", tmp, 1);

    WubuPeople db;
    CHECK(wubu_people_load(&db) == 0 && db.count == 0, "a missing store is empty, not an error");

    char line[64];
    CHECK(wubu_people_line(&db, line, sizeof line) == 0 && strcmp(line, "no contacts") == 0,
          "an empty store says so");

    CHECK(wubu_people_add(&db, "Ada Lovelace", "ada@example.org", "555-0100") == 0, "a person is added");
    CHECK(wubu_people_add(&db, "Grace Hopper", "grace@example.org", "555-0101") == 0, "a second is added");
    CHECK(wubu_people_add(&db, "ada lovelace", "", "") == 2,
          "a duplicate is refused case-insensitively (two entries for one person is a bug)");
    CHECK(wubu_people_add(&db, "", "", "") == 2, "an empty name is refused");
    CHECK(db.count == 2, "the refused adds did not change the store");

    CHECK(wubu_people_save(&db) == 0, "the store saves");

    WubuPeople re;
    CHECK(wubu_people_load(&re) == 0 && re.count == 2, "the store reloads with both people");
    CHECK(strcmp(re.p[0].name, "Ada Lovelace") == 0 && strcmp(re.p[0].email, "ada@example.org") == 0,
          "fields survive the round trip");
    CHECK(strcmp(re.p[1].phone, "555-0101") == 0, "the third field survives too");

    int idx[8];
    int n = wubu_people_find(&re, "GRACE", idx, 8);
    CHECK(n == 1 && idx[0] == 1, "search is case-insensitive over names");
    n = wubu_people_find(&re, "555-0100", idx, 8);
    CHECK(n == 1 && idx[0] == 0, "search covers the phone field");
    n = wubu_people_find(&re, "example.org", idx, 8);
    CHECK(n == 2, "search covers the email field and finds both");
    n = wubu_people_find(&re, "nobody", idx, 8);
    CHECK(n == 0, "a search that matches nothing returns nothing");

    /* untrusted input: one absurd line must be truncated, not overflow */
    FILE *f = fopen(tmp, "w");
    CHECK(f != NULL, "store is writable for the hostile line");
    if (f) {
        char big[300];
        memset(big, 'A', sizeof big);
        big[sizeof big - 1] = 0;
        fprintf(f, "%s\tmail@example.org\t123\n", big);
        fclose(f);
    }
    WubuPeople big;
    CHECK(wubu_people_load(&big) == 0 && big.count == 1, "an overlong line still loads as one person");
    CHECK(strlen(big.p[0].name) == WUBU_PEOPLE_NAME_MAX - 1, "an overlong name is truncated to the field");
    CHECK(big.p[0].name[WUBU_PEOPLE_NAME_MAX - 1] == 0, "and the truncation is terminated");

    /* the bound */
    WubuPeople full;
    memset(&full, 0, sizeof full);
    for (int i = 0; i < WUBU_PEOPLE_MAX; i++) {
        char nm[32];
        snprintf(nm, sizeof nm, "Person %d", i);
        if (wubu_people_add(&full, nm, "", "") != 0) break;
    }
    CHECK(full.count == WUBU_PEOPLE_MAX, "the store fills to its bound");
    CHECK(wubu_people_add(&full, "One Too Many", "", "") == 1, "and refuses to grow past it");

    unlink(tmp);
    printf("wubu_people: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
