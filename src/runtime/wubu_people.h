/*
 * wubu_people.h -- the contact store behind the People surface (PeopleExperienceHost parity).
 *
 * A contacts page with no persistent store is a mock: you add a person, reboot, and they are gone. This is
 * the store first, so the surface has something true to show.
 *
 * Format: one person per line, TAB separated, "name<TAB>email<TAB>phone". Blank lines and lines starting
 * with '#' are skipped, so the file is hand-editable and survives a comment. Bounded in people and in
 * field length -- a contacts file is untrusted input like any other.
 *
 * Path: $WUBU_PEOPLE_DB, else ~/.local/share/wubu/people.tsv.
 */
#ifndef WUBU_PEOPLE_H
#define WUBU_PEOPLE_H

#include <stddef.h>

#define WUBU_PEOPLE_MAX      128
#define WUBU_PEOPLE_NAME_MAX 64
#define WUBU_PEOPLE_MAIL_MAX 96
#define WUBU_PEOPLE_PHONE_MAX 32

typedef struct {
    char name[WUBU_PEOPLE_NAME_MAX];
    char email[WUBU_PEOPLE_MAIL_MAX];
    char phone[WUBU_PEOPLE_PHONE_MAX];
} WubuPerson;

typedef struct {
    WubuPerson p[WUBU_PEOPLE_MAX];
    int         count;
} WubuPeople;

/* The database path in use: $WUBU_PEOPLE_DB, else ~/.local/share/wubu/people.tsv. Never cached. */
const char *wubu_people_default_path(void);

/* Load the store. A missing file is an EMPTY STORE, not an error (returns 0 with count 0). */
int wubu_people_load(WubuPeople *out);

/* Save the store. Written whole, so a crash mid-write cannot leave a half-person behind (the caller's
 * file is replaced by a complete one). */
int wubu_people_save(const WubuPeople *in);

/* Add a person. Returns 0 on success, 1 when the store is full, 2 when the name is empty or a duplicate
 * (compared case-insensitively -- two "Ada Lovelace" entries are a bug, not a feature), -1 on bad args. */
int wubu_people_add(WubuPeople *in, const char *name, const char *email, const char *phone);

/* Case-insensitive substring search over name/email/phone. Fills up to max indices, returns how many. */
int wubu_people_find(const WubuPeople *in, const char *query, int *idx, int max);

/* One line about the store: "3 contacts" / "no contacts". */
int wubu_people_line(const WubuPeople *in, char *out, size_t cap);

#endif
