/* wubu_pwpolicy.c -- see wubu_pwpolicy.h. No allocation, no locale, no clock. */
#include "wubu_pwpolicy.h"
#include "wubu_utf8.h"
#include <string.h>
#include <ctype.h>

/* The passwords that appear at the top of every credential-stuffing list. A short list
 * catches the common case; the point is not to enumerate the internet, it is to refuse the
 * ten things a person types when they do not care. */
static const char *const COMMON[] = {
    "password", "password1", "password123", "123456", "12345678", "123456789",
    "qwerty", "qwerty123", "letmein", "welcome", "admin", "root", "toor",
    "iloveyou", "monkey", "dragon", "sunshine", "princess", "football",
    "wubuos", "wubu", "changeme", "secret", "abc123", "111111", "000000",
    NULL
};

static int is_lower(unsigned c) { return c >= 'a' && c <= 'z'; }
static int is_upper(unsigned c) { return c >= 'A' && c <= 'Z'; }
static int is_digit(unsigned c) { return c >= '0' && c <= '9'; }

/* Case-insensitive containment without strcasestr (POSIX-only, and locale-dependent). */
static int contains_ci(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (n == 0) return 0;
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == n) return 1;
    }
    return 0;
}

static int contains_reverse_ci(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (n == 0) return 0;
    char rev[128];
    if (n >= sizeof rev) return 0;
    for (size_t i = 0; i < n; i++) rev[i] = needle[n - 1 - i];
    rev[n] = '\0';
    return contains_ci(hay, rev);
}

/* Cheap patterns that make the length irrelevant. The first draft only refused a string
 * that was ENTIRELY one character or ENTIRELY a run, so "aaaaaaaaaA1" and "abcdefghijK1"
 * both passed while looking exactly like what they are: a padding character and a run with
 * the smallest legal tail. Dominance and longest-run catch those. */
static int padded_or_run(const char *pw, size_t n) {
    if (n < 2) return 1;
    size_t counts[256];
    memset(counts, 0, sizeof counts);
    for (size_t i = 0; i < n; i++) counts[(unsigned char)pw[i]]++;
    for (size_t c = 0; c < 256; c++)
        if (counts[c] * 10 >= n * 7) return 1;          /* one character is >= 70% of it */
    size_t best = 1, cur = 1;
    for (size_t i = 1; i < n; i++) {
        if (pw[i] == pw[i-1] + 1 || pw[i] == pw[i-1] - 1) { cur++; if (cur > best) best = cur; }
        else cur = 1;
    }
    return best >= 6;                                    /* a six-long straight run */
}

int wubu_pw_check(const char *pw, const char *user) {
    if (!pw) return WUBU_PW_ARGS;
    size_t bytes = strlen(pw);
    if (bytes == 0) return WUBU_PW_TOO_SHORT;
    if (bytes > WUBU_PW_MAX_BYTES) return WUBU_PW_TOO_LONG;
    /* Count characters, not bytes: a passphrase in a non-Latin script must not be judged
     * by its UTF-8 length any more than an ASCII one is judged by its letters. */
    size_t chars = wubu_utf8_count(pw, bytes);
    if (chars < WUBU_PW_MIN_CHARS) return WUBU_PW_TOO_SHORT;

    /* The common list is checked before the character-class rule: "Password123" failing
     * with "mix upper, lower, digits" is technically true and useless advice. */
    for (int i = 0; COMMON[i]; i++) if (contains_ci(pw, COMMON[i])) return WUBU_PW_COMMON;

    int classes = 0, lower = 0, upper = 0, digit = 0, other = 0;
    for (size_t i = 0; i < bytes; i++) {
        unsigned c = (unsigned char)pw[i];
        if (is_lower(c)) lower = 1;
        else if (is_upper(c)) upper = 1;
        else if (is_digit(c)) digit = 1;
        else other = 1;
    }
    classes = lower + upper + digit + other;
    if (classes < WUBU_PW_MIN_CLASSES) return WUBU_PW_FEW_CLASSES;

    if (user && *user) {
        if (contains_ci(pw, user) || contains_reverse_ci(pw, user)) return WUBU_PW_HAS_USER;
    }
    if (padded_or_run(pw, bytes)) return WUBU_PW_REPEATED;
    return WUBU_PW_OK;
}

const char *wubu_pw_reason(int code) {
    switch (code) {
        case WUBU_PW_OK:          return "ok";
        case WUBU_PW_TOO_SHORT:   return "use at least 10 characters";
        case WUBU_PW_TOO_LONG:    return "that password is too long";
        case WUBU_PW_FEW_CLASSES: return "mix upper case, lower case, digits or symbols";
        case WUBU_PW_COMMON:      return "that password is one attackers try first";
        case WUBU_PW_HAS_USER:    return "do not put your user name in the password";
        case WUBU_PW_REPEATED:    return "avoid repeated characters or straight runs";
        case WUBU_PW_ARGS:        return "bad arguments";
        default:                  return "invalid";
    }
}

int wubu_pw_strength(const char *pw) {
    if (!pw) return 0;
    size_t n = strlen(pw);
    int score = 0;
    int lower = 0, upper = 0, digit = 0, other = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned c = (unsigned char)pw[i];
        if (is_lower(c)) lower = 1;
        else if (is_upper(c)) upper = 1;
        else if (is_digit(c)) digit = 1;
        else other = 1;
    }
    score += (n >= 8) + (n >= 12) + (n >= 16) + (n >= 20);   /* 0..4 */
    score += lower + upper + digit + other;                   /* 0..4 */
    if (wubu_pw_check(pw, NULL) == WUBU_PW_OK) score += 2;
    if (score > 10) score = 10;
    return score * 10;
}
