/* wubu_install_test.c -- configurator core (P1 OOBE body; GAP F).
 *
 * Drives the five-step wizard with SCRIPTED answers (no terminal, no GUI),
 * and proves the plan is honest: fields land, the password is a salted
 * iterated hash (never the plaintext), the summary masks by length, and the
 * answers document round-trips exactly. Also knows-answer-tests the SHA-256
 * primitive the hash is built on.
 *
 * Self-contained (no egojar.h or sys/ includes) so it builds on the host AND the
 * kernel; links wubu_install.c + the kernel's wubu_sha256.c.
 */
#include "wubu_install.h"
#include "wubu_sha256.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

/* -- scripted IO: a settable answer script + confirm behavior ------------- */
/* wizard asks, in order: lang, keyboard, username, password(secret),
 * disk_mode, disk_target, encrypt, timezone. */
static const char *const g_script[] = {
    "en", "us", "wubu", "hunter2", "full", "0", "1", "UTC"
};
static const char *const *g_cur = g_script;
static int g_cur_len = (int)(sizeof g_script / sizeof g_script[0]);
static int g_ai = 0;
static int g_noconfirm = 0;

static int script_ask(const char *prompt, const char *def, char *out, size_t n) {
    (void)prompt; (void)def;
    if (g_ai >= g_cur_len) return 0;   /* out of script => cancel */
    snprintf(out, n, "%s", g_cur[g_ai++]);
    return 1;
}
static int script_secret(const char *prompt, char *out, size_t n) {
    (void)prompt;
    if (g_ai >= g_cur_len) return 0;
    snprintf(out, n, "%s", g_cur[g_ai++]);
    return 1;
}
static int script_salt(char *out, size_t n) { snprintf(out, n, "SALTsalt"); return 1; }
static int script_confirm(const char *summary) { (void)summary; return g_noconfirm ? 0 : 1; }

int main(void) {
    /* ---- SHA-256 known-answer (the primitive the hash is built on) ----- */
    {
        uint8_t d[WUBU_SHA256_SZ];
        wubu_sha256("abc", 3, d);
        char hx[WUBU_SHA256_SZ * 2 + 1];
        for (int i = 0; i < WUBU_SHA256_SZ; i++) snprintf(hx + i * 2, 3, "%02x", d[i]);
        CHECK(strcmp(hx, "ba7816bf8f01cfea414140de5dae2223"
                           "b00361a396177a9cb410ff61f20015ad") == 0,
              "SHA-256(\"abc\") is the FIPS vector");
    }

    /* ---- password hash properties -------------------------------------- */
    {
        char a[WUBU_INSTALL_HASH_MAX], b[WUBU_INSTALL_HASH_MAX];
        wubu_password_hash("hunter2", "salt1", a, sizeof a);
        wubu_password_hash("hunter2", "salt1", b, sizeof b);
        CHECK(strcmp(a, b) == 0, "same (pw, salt) -> same hash (deterministic)");
        CHECK(strncmp(a, "sha256$salt1$", 13) == 0,
              "the hash carries its scheme and salt");
        CHECK(strlen(a) == 13 + 64 && a[13 + 64] == 0,
              "the hash is one real digest, not a stub");
        wubu_password_hash("hunter2", "salt2", b, sizeof b);
        CHECK(strcmp(a, b) != 0, "different salt -> different hash");
        CHECK(strstr(a, "hunter2") == NULL, "the plaintext never sits in the hash");
    }

    /* ---- the happy-path wizard ------------------------------------------ */
    wubu_install_plan plan;
    memset(&plan, 0, sizeof plan);
    g_cur = g_script; g_cur_len = (int)(sizeof g_script / sizeof g_script[0]);
    g_ai = 0; g_noconfirm = 0;
    wubu_install_io io = { script_ask, script_secret, script_salt, script_confirm };
    int rc = wubu_install_wizard(&plan, &io);
    CHECK(rc == 1, "the wizard completed and confirmed");
    CHECK(strcmp(plan.lang, "en") == 0, "language landed");
    CHECK(strcmp(plan.keyboard, "us") == 0, "keyboard landed");
    CHECK(strcmp(plan.username, "wubu") == 0, "username landed");
    CHECK(strcmp(plan.disk_mode, "full") == 0, "disk mode is full");
    CHECK(plan.encrypt == 1, "full encryption is the default and stuck");
    CHECK(plan.pw_len == 7, "password length was captured for the mask");
    CHECK(strncmp(plan.pw_hash, "sha256$SALTsalt$", 16) == 0,
          "the password was hashed, salted, iterated into the plan");
    CHECK(strstr(plan.pw_hash, "hunter2") == NULL,
          "the plaintext password is ABSENT from the plan");
    CHECK(strcmp(plan.timezone, "UTC") == 0, "timezone defaulted and accepted");

    /* ---- the summary masks, never echoes --------------------------------- */
    {
        char sum[512];
        wubu_install_summary(&plan, sum, sizeof sum);
        CHECK(strstr(sum, "wubu") != NULL, "the summary shows the username");
        CHECK(strstr(sum, "*******") != NULL, "the summary masks the password by length");
        CHECK(strstr(sum, plan.pw_hash) == NULL, "the summary never shows the hash");
    }

    /* ---- serialize / parse round trip ------------------------------------ */
    {
        char doc[1024];
        CHECK(wubu_install_serialize(&plan, doc, sizeof doc) == 0, "plan serializes");
        wubu_install_plan q;
        memset(&q, 0, sizeof q);
        CHECK(wubu_install_parse(doc, &q) == 0, "document parses");
        CHECK(strcmp(q.lang, plan.lang) == 0 && strcmp(q.keyboard, plan.keyboard) == 0 &&
              strcmp(q.username, plan.username) == 0 &&
              strcmp(q.pw_salt, plan.pw_salt) == 0 && strcmp(q.pw_hash, plan.pw_hash) == 0 &&
              strcmp(q.timezone, plan.timezone) == 0 &&
              strcmp(q.disk_mode, plan.disk_mode) == 0 &&
              strcmp(q.disk_target, plan.disk_target) == 0 &&
              q.pw_len == plan.pw_len && q.encrypt == plan.encrypt,
              "the /n/install document round-trips every field");
    }

    /* ---- cancel at the summary commit point -------------------------------- */
    {
        wubu_install_plan p; memset(&p, 0, sizeof p);
        g_cur = g_script; g_ai = 0; g_noconfirm = 1;
        wubu_install_io no = { script_ask, script_secret, script_salt, script_confirm };
        int rc2 = wubu_install_wizard(&p, &no);
        CHECK(rc2 == 0, "refusing the summary cancels the whole run");
    }

    /* ---- an out-of-script ask cancels (never hangs) -------------------------- */
    {
        wubu_install_plan p; memset(&p, 0, sizeof p);
        g_cur = g_script; g_cur_len = 3; g_ai = 0; g_noconfirm = 0; /* script ends before disk */
        wubu_install_io io3 = { script_ask, script_secret, script_salt, script_confirm };
        int rc4 = wubu_install_wizard(&p, &io3);
        CHECK(rc4 == 0, "running out of script cancels the run instead of hanging");
    }

    /* ---- reject an invalid answer ------------------------------------------- */
    {
        static const char *const bad[] = { "en", "us", "wubu", "" }; /* empty password */
        wubu_install_plan p; memset(&p, 0, sizeof p);
        g_cur = bad; g_cur_len = 4; g_ai = 0; g_noconfirm = 0;
        wubu_install_io io2 = { script_ask, script_secret, script_salt, script_confirm };
        int rc3 = wubu_install_wizard(&p, &io2);
        CHECK(rc3 == -1, "an empty password is rejected, never accepted");
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
