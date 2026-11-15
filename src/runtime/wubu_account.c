/* wubu_account.c -- see wubu_account.h. */
#include "wubu_account.h"
#include "wubu_pwcrypt.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

/* Shadow-file safety: ':' terminates fields, whitespace and '/' invite weird
 * usernames. Refusing is cheaper than escaping a record that later verifies
 * into a different user than the one typed. */
static int name_ok(const char *s)
{
    if (!s || !s[0]) return 0;
    for (const char *p = s; *p; p++)
        if (*p == ':' || *p == '/' || isspace((unsigned char)*p) || *p == '=')
            return 0;
    return 1;
}

int wubu_account_build(wubu_account_t *rec, const char *user,
                       const char *hostname, const char *keyboard,
                       const char *password, const char *fixed_salt)
{
    if (!rec || !name_ok(user) || !password || !password[0]) return -1;

    /* Salt: caller-pinned for hermetic runs, else time+pid so two fresh boots use
     * different salts even for the same password (a shadow requirement). */
    char salt[WUBU_ACCOUNT_SALT_MAX];
    if (fixed_salt && fixed_salt[0]) {
        size_t n = strlen(fixed_salt);
        if (n >= WUBU_ACCOUNT_SALT_MAX) return -1;
        memcpy(salt, fixed_salt, n + 1);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        /* 1 hex + 8 + 7 = 16 chars exactly; bounds the fields so snprintf can
         * never run past the 16-char salt (the format-truncation warning). */
        snprintf(salt, sizeof(salt), "5%08lx%07lx",
                 (unsigned long)(ts.tv_sec % 100000000u),
                 (unsigned long)(ts.tv_nsec / 100u) % 10000000u);
        salt[16] = 0;
    }

    char hash[WUBU_PWCRYPT_OUT];
    if (!wubu_pw_crypt_sha512(password, salt, hash)) return -1;

    wubu_account_t tmp;
    strncpy(tmp.user, user, sizeof(tmp.user) - 1);  tmp.user[sizeof(tmp.user)-1] = 0;
    strncpy(tmp.hostname, hostname ? hostname : "", sizeof(tmp.hostname) - 1);
    tmp.hostname[sizeof(tmp.hostname)-1] = 0;
    strncpy(tmp.keyboard, keyboard ? keyboard : "us", sizeof(tmp.keyboard) - 1);
    tmp.keyboard[sizeof(tmp.keyboard)-1] = 0;
    strncpy(tmp.hash, hash, sizeof(tmp.hash) - 1);  tmp.hash[sizeof(tmp.hash)-1] = 0;

    *rec = tmp;                     /* only commit on success */
    return 0;
}

int wubu_account_record_str(const wubu_account_t *rec, char *out, size_t cap)
{
    if (!rec || !out || cap == 0) return -1;
    return snprintf(out, cap, "user=%s host=%s kbd=%s pw=%s", rec->user,
                    rec->hostname, rec->keyboard, rec->hash);
}

bool wubu_account_verify(const wubu_account_t *rec, const char *password)
{
    if (!rec || !password) return false;
    /* Extract the stored hash's salt (between the 2nd and 3rd '$'), rehash with
     * it, and compare -- the standard shadow verification. */
    const char *d1 = strchr(rec->hash, '$');
    const char *d2 = d1 ? strchr(d1 + 1, '$') : NULL;    /* after "$6" */
    const char *d3 = d2 ? strchr(d2 + 1, '$') : NULL;    /* after the salt */
    if (!d3) return false;                               /* malformed */
    const char *salt = d2 + 1;                           /* between d2 and d3 */
    char hash[WUBU_PWCRYPT_OUT];
    if (!wubu_pw_crypt_sha512(password, salt, hash)) return false;
    return strcmp(hash, rec->hash) == 0;
}
