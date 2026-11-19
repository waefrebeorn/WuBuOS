/*
 * wubu_install.h -- configurator core (the P1 BODY of the OOBE; GAP F).
 *
 * Omarchy's installer is a tiny honest question set with a lot of front-loaded
 * flair. This module is the question set, decoupled from any UI: it ASKS
 * through injected callbacks, so the same engine can drive a console
 * configurator on a boot, a GUI greeter on the hosted desktop, or a scripted
 * test harness -- the answers are the document, the UI is a plug.
 *
 * A completed run fills a wubu_install_plan: language -> keyboard -> account
 * (username + password) -> disk -> summary confirm. The page password is
 * salted + iterated BEFORE it is stored, so the plan never carries the
 * plaintext; what leaves the module is a one-way hash. The plan serializes to
 * a line-encoded answers document (the /n/install capture) that round-trips
 * exactly, and a summary view masks the password (measured by its length, as
 * Omarchy's does) rather than echoing the hash.
 *
 * Depends only on the kernel's self-contained SHA-256
 * (src/kernel/wubu_sha256.{c,h}) for the password hash. NO egojar.h or
 * sys/ includes, so this builds on the host AND the kernel.
 */
#ifndef WUBU_INSTALL_H
#define WUBU_INSTALL_H

#include <stddef.h>

#define WUBU_INSTALL_LANG_MAX    32
#define WUBU_INSTALL_KB_MAX      32
#define WUBU_INSTALL_USER_MAX    64
#define WUBU_INSTALL_SALT_MAX    32
#define WUBU_INSTALL_HASH_MAX    160
#define WUBU_INSTALL_TZ_MAX      40
#define WUBU_INSTALL_DISK_MAX    24
#define WUBU_INSTALL_TARGET_MAX  64

/* Iterated-password work factor (rounds of SHA-256). 4096 is a sketch of the
 * crypt(3) price; the /n auth module owns the exact upgrade to $6$. */
#define WUBU_INSTALL_PW_ROUNDS   4096

/* The provisioning answers -- the /n/install document, one plan == one install.
 * Only pw_hash/pw_salt ever hold password material; pw_len is kept ONLY so the
 * summary can mask by length (a mild leak Omarchy accepts too). */
typedef struct {
    char lang[WUBU_INSTALL_LANG_MAX];
    char keyboard[WUBU_INSTALL_KB_MAX];
    char username[WUBU_INSTALL_USER_MAX];
    char pw_salt[WUBU_INSTALL_SALT_MAX];
    char pw_hash[WUBU_INSTALL_HASH_MAX];   /* "sha256$<salt>$<hex>" -- never the plaintext */
    int  pw_len;                            /* plaintext length, for the masked summary only */
    char timezone[WUBU_INSTALL_TZ_MAX];
    char disk_mode[WUBU_INSTALL_DISK_MAX];  /* "full" | "free" */
    char disk_target[WUBU_INSTALL_TARGET_MAX];
    int  encrypt;                           /* full-disk encryption by default */
} wubu_install_plan;

/* I/O the wizard needs, injected. Return 0 from any callback cancels the run.
 * ask:   a prompted value, `def` filled in if the user just presses enter.
 * ask_secret: a non-echoed value (the password).
 * rand_salt:  fill `out` with a fresh random salt (output up to n bytes, must
 *             be NUL-terminated). If NULL, a constant fallback is used -- fine
 *             for tests, never for a real install.
 * confirm:    show the summary, return 1 to commit / 0 to cancel. */
typedef struct {
    int (*ask)(const char *prompt, const char *def, char *out, size_t n);
    int (*ask_secret)(const char *prompt, char *out, size_t n);
    int (*rand_salt)(char *out, size_t n);
    int (*confirm)(const char *summary);
} wubu_install_io;

/* Run the five-step configurator into `plan`, using `io` for every question.
 * Returns 1 if the user confirmed (plan is filled and hash derived),
 * 0 if the user cancelled, -1 if io/plan is NULL or an answer is invalid
 * (empty username or password, bad disk mode). */
int wubu_install_wizard(wubu_install_plan *plan, const wubu_install_io *io);

/* One-way, salted, iterated password hash: "sha256$<salt>$<64 hex>". The salt
 * rides in the output so verification needs no external state. `out` holds at
 * least `n` bytes and is always NUL-terminated. Produces the same output for
 * the same (pw, salt) -- deterministic, so the hash is testable. */
void wubu_password_hash(const char *pw, const char *salt, char *out, size_t n);

/* Line-encoded answers document ("lang=en\nkeyboard=us\n..."), one field per
 * line, safe against every value the plan can hold (no embedded newlines).
 * wubu_install_serialize -> wubu_install_parse round-trips all fields. Each
 * returns 0 on success, -1 on error. */
int  wubu_install_serialize(const wubu_install_plan *p, char *out, size_t n);
int  wubu_install_parse(const char *text, wubu_install_plan *p);

/* Masked summary for the final confirmation screen: every meaningful field,
 * the password shown as pw_len asterisks (never the hash). Returns 0 on
 * success, -1 if p or out is null. */
int  wubu_install_summary(const wubu_install_plan *p, char *out, size_t n);

#endif /* WUBU_INSTALL_H */
