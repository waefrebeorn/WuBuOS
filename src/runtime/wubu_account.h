/* wubu_account.h -- first-boot account record, shadow-compatible password hash.
 *
 * Closes the OOBE gap against omarchy's deferred provisioning: omarchy defers
 * keyboard/username/password to first boot and stores the password as a $6$
 * SHA-512 crypt hash (openssl passwd -6). WuBuOS now has the primitive
 * (wubu_pwcrypt.c); this module owns the ACCOUNT RECORD -- a single-line,
 * sentinel-embeddable string carrying user/hostname/keyboard and the hash.
 * The signature the first-boot greeter persists is an account record, not just
 * a theme choice.
 *
 * C11, no heap. Fail-closed: invalid input produces no record (-1) and never
 * writes a partial hash into a caller buffer that would verify later.
 */
#ifndef WUBU_ACCOUNT_H
#define WUBU_ACCOUNT_H

#include <stdbool.h>
#include <stddef.h>

/* One record's plaintext bound: user+host+kbd plus the 116-char hash line. */
#define WUBU_ACCOUNT_RECORD_MAX  512
#define WUBU_ACCOUNT_NAME_MAX     64
#define WUBU_ACCOUNT_SALT_MAX     17   /* 16 chars, like /etc/shadow salts */

typedef struct {
    char user[WUBU_ACCOUNT_NAME_MAX];
    char hostname[WUBU_ACCOUNT_NAME_MAX];
    char keyboard[WUBU_ACCOUNT_NAME_MAX];
    char hash[WUBU_ACCOUNT_RECORD_MAX];   /* "$6$salt$..." shadow form */
    /* Fixed identity (SteamOS parity): SteamOS enforces the steamos user's UID/GID
     * with 01-steamos-enforce-UID-GID.conf so the desktop sees the same ownership
     * across the A/B rootfs. WuBu's first-boot user is pinned the same way -- the
     * guest desktop and any persisted guest files always resolve to one owner. */
    unsigned int uid;
    unsigned int gid;
} wubu_account_t;

/* The pinned identity (mirrors SteamOS enforce-UID-GID: steamos=1000/1000). */
#define WUBU_ACCOUNT_UID  1000
#define WUBU_ACCOUNT_GID  1000

/* Build `rec` for the given account. `password` is hashed with a freshly
 * generated salt (time+pid seeded); pass `fixed_salt` (<=16 chars) to pin it
 * for hermetically testable runs. Returns 0 / -1 on invalid input (empty user
 * or password, or a username containing ':'/whitespace -- both would corrupt
 * a shadow-style file). Fail-closed: on error `rec` is untouched. */
int wubu_account_build(wubu_account_t *rec, const char *user,
                       const char *hostname, const char *keyboard,
                       const char *password, const char *fixed_salt);

/* Serialise the record to one line the onboarding sentinel can hold:
 * "user=NAME host=HOST kbd=KBD pw=$6$salt$HASH". Returns length. */
int wubu_account_record_str(const wubu_account_t *rec, char *out, size_t cap);

/* True iff `password` matches the hash stored in `rec`. Never prompts, never
 * logs; the hash comparison is constant-time via the module's strcmp-equivalent
 * guard. */
bool wubu_account_verify(const wubu_account_t *rec, const char *password);

#endif /* WUBU_ACCOUNT_H */