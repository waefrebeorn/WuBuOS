/* wubu_pwpolicy.h -- what a password must satisfy before an account exists.
 *
 * Own implementation (docs/GOAL-SLERM.json, D1). The greeter hashes whatever it is
 * given with $6$ SHA-512, which is correct cryptography wrapped around no policy at all:
 * "a" is accepted, "password" is accepted, and the user's own name is accepted.
 *
 * This module is the policy, and it answers with a REASON rather than a boolean, because
 * "your password is not good enough" is the message that makes people choose worse ones.
 * The rules are deliberately conservative and stated: a policy nobody can explain is
 * indistinguishable from a bug.
 */
#ifndef WUBU_PWPOLICY_H
#define WUBU_PWPOLICY_H

#include <stddef.h>

enum {
    WUBU_PW_OK           =  0,
    WUBU_PW_TOO_SHORT    = -1,   /* fewer than WUBU_PW_MIN_CHARS characters */
    WUBU_PW_TOO_LONG     = -2,   /* more than WUBU_PW_MAX_BYTES bytes */
    WUBU_PW_FEW_CLASSES  = -3,   /* needs WUBU_PW_MIN_CLASSES of the four classes */
    WUBU_PW_COMMON       = -4,   /* on the short list of passwords that are tried first */
    WUBU_PW_HAS_USER     = -5,   /* contains the user name or its reverse */
    WUBU_PW_REPEATED     = -6,   /* one character repeated, or a single run like abcdef */
    WUBU_PW_ARGS         = -7
};

#define WUBU_PW_MIN_CHARS 10
#define WUBU_PW_MAX_BYTES 256
#define WUBU_PW_MIN_CLASSES 3

/* Check `pw` for the account `user` (may be NULL). Returns WUBU_PW_OK or a reason. */
int wubu_pw_check(const char *pw, const char *user);

/* The reason as a sentence a person can act on. */
const char *wubu_pw_reason(int code);

/* 0..100, for a strength meter. Never used to accept or refuse: the rules above decide,
 * because a bar that saturates at 100 for "Passw0rd!" teaches the wrong lesson. */
int wubu_pw_strength(const char *pw);

#endif
