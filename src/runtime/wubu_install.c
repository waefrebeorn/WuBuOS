/*
 * wubu_install.c -- configurator core (P1 OOBE body; docs/GAP_ROADMAP GAP F).
 *
 * The wiki question set: language, keyboard, account (username + password),
 * disk, then a masked summary that is the single commit point. Nothing is
 * written or installed here -- the module only COLLECTS the answers into a
 * plan and derives the password hash. Installing from the plan (formats a
 * disk, streams a root image, creates the user) is the next layer, by design:
 * the wizard is the body Omarchy front-loads graphics around, and it must be
 * testable before any disk is touched.
 */
#include "wubu_install.h"
#include "wubu_sha256.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -- password hash ----------------------------------------------------- */

void wubu_password_hash(const char *pw, const char *salt, char *out, size_t n)
{
    if (!out || n == 0) return;
    out[0] = 0;
    if (!pw || !salt) return;

    uint8_t h[WUBU_SHA256_SZ];
    /* h = sha256(salt || pw)  -- the salt makes equal passwords differ. */
    {
        uint8_t ctx[WUBU_SHA256_CTX_SZ] = {0};
        wubu_sha256_init((wubu_sha256_ctx *)ctx);
        wubu_sha256_update((wubu_sha256_ctx *)ctx, salt, strlen(salt));
        wubu_sha256_update((wubu_sha256_ctx *)ctx, pw, strlen(pw));
        wubu_sha256_final((wubu_sha256_ctx *)ctx, h);
    }
    /* Iterate: h(i) = sha256(salt || h(i-1)), WUBU_INSTALL_PW_ROUNDS times.
     * The price buys time against a brute-force pass; it is a sketch of the
     * crypt(3) work factor, upgraded by the auth module. */
    for (int i = 0; i < WUBU_INSTALL_PW_ROUNDS; i++) {
        uint8_t ctx[WUBU_SHA256_CTX_SZ] = {0};
        wubu_sha256_init((wubu_sha256_ctx *)ctx);
        wubu_sha256_update((wubu_sha256_ctx *)ctx, salt, strlen(salt));
        wubu_sha256_update((wubu_sha256_ctx *)ctx, h, WUBU_SHA256_SZ);
        wubu_sha256_final((wubu_sha256_ctx *)ctx, h);
    }
    /* sha256$<salt>$<hex>. The salt rides in the output; a verifier re-derives
     * the digest from it without state. */
    char hx[WUBU_SHA256_SZ * 2 + 1];
    for (size_t i = 0; i < WUBU_SHA256_SZ; i++)
        snprintf(hx + i * 2, 3, "%02x", h[i]);
    snprintf(out, n, "sha256$%s$%s", salt, hx);
}

/* -- plan serialization ------------------------------------------------- */

int wubu_install_serialize(const wubu_install_plan *p, char *out, size_t n)
{
    if (!p || !out) return -1;
    int w = 0;
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "lang=%s\n", p->lang);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "keyboard=%s\n", p->keyboard);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "username=%s\n", p->username);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "pw_salt=%s\n", p->pw_salt);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "pw_hash=%s\n", p->pw_hash);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "pw_len=%d\n", p->pw_len);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "timezone=%s\n", p->timezone);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "disk_mode=%s\n", p->disk_mode);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "disk_target=%s\n", p->disk_target);
    w += snprintf(out + w, n > (size_t)w ? n - (size_t)w : 0, "encrypt=%d\n", p->encrypt);
    return (size_t)w < n ? 0 : -1;
}

/* Pull one "name=value" line into `out`. Returns 0 if this line IS the named
 * field (value copied), nonzero if it is some other or unknown field. */
static int field_value(const char *line, const char *name, char *out, size_t n)
{
    size_t nl = strlen(name);
    if (strncmp(line, name, nl) != 0 || line[nl] != '=') return 1;
    size_t v = 0;
    for (const char *s = line + nl + 1; *s && *s != '\n' && *s != '\r'; s++) {
        if (v + 1 < n) out[v++] = *s;
    }
    out[v] = 0;
    return 0;
}

int wubu_install_parse(const char *text, wubu_install_plan *p)
{
    if (!text || !p) return -1;
    memset(p, 0, sizeof *p);
    char line[256];
    const char *cur = text;
    /* Round-trip completeness is asserted by the CALLER comparing to the
     * serialized source; a bare parse must never require every field (forward
     * compat) but must never invent one. Unknown lines are skipped. */
    while (*cur) {
        size_t i = 0;
        while (cur[i] && cur[i] != '\n' && i + 1 < sizeof line) { line[i] = cur[i]; i++; }
        line[i] = 0;
        cur += i;
        if (*cur == '\n') cur++;
        if (!line[0]) continue;
        char v[32];
        if      (field_value(line, "lang",        p->lang,        sizeof p->lang)        == 0) continue;
        else if (field_value(line, "keyboard",    p->keyboard,    sizeof p->keyboard)    == 0) continue;
        else if (field_value(line, "username",    p->username,    sizeof p->username)    == 0) continue;
        else if (field_value(line, "pw_salt",     p->pw_salt,     sizeof p->pw_salt)     == 0) continue;
        else if (field_value(line, "pw_hash",     p->pw_hash,     sizeof p->pw_hash)     == 0) continue;
        else if (field_value(line, "pw_len",      v, sizeof v)    == 0) { p->pw_len = atoi(v); }
        else if (field_value(line, "timezone",    p->timezone,    sizeof p->timezone)    == 0) continue;
        else if (field_value(line, "disk_mode",   p->disk_mode,   sizeof p->disk_mode)   == 0) continue;
        else if (field_value(line, "disk_target", p->disk_target, sizeof p->disk_target) == 0) continue;
        else if (field_value(line, "encrypt",     v, sizeof v)    == 0) p->encrypt = atoi(v);
    }
    return 0;
}

/* -- masked summary ------------------------------------------------------ */

int wubu_install_summary(const wubu_install_plan *p, char *out, size_t n)
{
    if (!p || !out) return -1;
    size_t mask = (size_t)(p->pw_len > 0 ? p->pw_len : 8);
    if (mask > 40) mask = 40;
    char m[41];
    memset(m, '*', mask);
    m[mask] = 0;
    snprintf(out, n,
             "Language : %s\n"
             "Keyboard : %s\n"
             "Username : %s\n"
             "Password : %s\n"
             "Timezone : %s\n"
             "Disk     : %s @ %s%s",
             p->lang, p->keyboard, p->username, m, p->timezone,
             p->disk_mode, p->disk_target,
             p->encrypt ? "  [encrypted]" : "");
    return 0;
}

/* -- the five-step wizard ----------------------------------------------- */

static int default_salt(char *out, size_t n)
{
    /* Constant fallback when no rand_salt is injected. Fine for tests and
     * throwaway installs; a REAL install injects a random salt. */
    snprintf(out, n, "WuBuOS");
    return 1;
}

int wubu_install_wizard(wubu_install_plan *plan, const wubu_install_io *io)
{
    if (!plan || !io || !io->ask || !io->ask_secret || !io->confirm) return -1;
    memset(plan, 0, sizeof *plan);

    /* 1 — language */ {
        char def[WUBU_INSTALL_LANG_MAX] = "en";
        if (!io->ask("Language", def, plan->lang, sizeof plan->lang)) return 0;
        if (!plan->lang[0]) return -1;
    }
    /* 2 — keyboard */ {
        char def[WUBU_INSTALL_KB_MAX] = "us";
        if (!io->ask("Keyboard layout", def, plan->keyboard, sizeof plan->keyboard)) return 0;
        if (!plan->keyboard[0]) return -1;
    }
    /* 3 — account: username then password (masked, hashed before it sits in the plan). */
    {
        char def[WUBU_INSTALL_USER_MAX] = "";
        if (!io->ask("Username", def, plan->username, sizeof plan->username)) return 0;
        if (!plan->username[0]) return -1;
        char pw[256];
        if (!io->ask_secret("Password", pw, sizeof pw)) return 0;
        if (!pw[0]) return -1;
        char salt[WUBU_INSTALL_SALT_MAX];
        int (*rs)(char *, size_t) = io->rand_salt ? io->rand_salt : default_salt;
        if (!rs(salt, sizeof salt)) return -1;
        plan->pw_len = (int)strlen(pw);
        snprintf(plan->pw_salt, sizeof plan->pw_salt, "%s", salt);
        /* Blank the plaintext before it lingers on the stack any longer than a
         * boundary line requires. */
        wubu_password_hash(pw, salt, plan->pw_hash, sizeof plan->pw_hash);
        memset(pw, 0, sizeof pw);
    }
    /* 4 — disk: mode + target, encryption on by default. */
    {
        char def[WUBU_INSTALL_DISK_MAX] = "full";
        if (!io->ask("Disk mode (full/free)", def, plan->disk_mode, sizeof plan->disk_mode)) return 0;
        if (strcmp(plan->disk_mode, "full") && strcmp(plan->disk_mode, "free")) return -1;
        char tdef[WUBU_INSTALL_TARGET_MAX] = "0";  /* 0 = auto-detect */
        if (!io->ask("Disk target (0=auto)", tdef, plan->disk_target, sizeof plan->disk_target)) return 0;
        if (!plan->disk_target[0]) return -1;
        char e[4] = "1";
        if (!io->ask("Encrypt install? (1/0)", "1", e, sizeof e)) return 0;
        plan->encrypt = (e[0] == '1' || e[0] == 'y' || e[0] == 'Y');
    }
    /* timezone — gives the plan a default clock region; default "UTC". */
    {
        char def[WUBU_INSTALL_TZ_MAX] = "UTC";
        if (!io->ask("Timezone", def, plan->timezone, sizeof plan->timezone)) return 0;
        if (!plan->timezone[0]) return -1;
    }
    /* 5 — summary: the single commit point, password masked. */
    {
        char sum[512];
        wubu_install_summary(plan, sum, sizeof sum);
        return io->confirm(sum) ? 1 : 0;
    }
}
