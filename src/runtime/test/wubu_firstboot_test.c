/* wubu_firstboot_test.c -- deferred provisioning (first-boot apply, GAP F).
 *
 * Proves the configurator's plan is CONSUMED, not just collected: the first
 * boot writes the user / hostname / keymap / language / timezone manifest into
 * a target root, the passwd line carries only the salted hash (the plaintext
 * password never touches disk), and verification is one-way-but-checkable.
 *
 * Self-contained, pure C11 + stdio; links wubu_firstboot.c + wubu_install.c +
 * the kernel's wubu_sha256.c.
 */
#include "wubu_firstboot.h"
#include "wubu_install.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static void read_file(const char *path, char *out, size_t n)
{
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    size_t got = fread(out, 1, n - 1, f); out[got] = 0; fclose(f);
    while (got && (out[got - 1] == '\n' || out[got - 1] == '\r')) out[--got] = 0;
}

int main(void)
{
    const char *base = getenv("TMPDIR");
    if (!base || !base[0]) base = getenv("TMP");
    if (!base || !base[0]) base = getenv("TEMP");
    if (!base || !base[0]) base = ".";
    char root[512];
    snprintf(root, sizeof root, "%s", base);
    char p_file[600], h_file[600], k_file[600], l_file[600], t_file[600];
    snprintf(p_file, sizeof p_file, "%s/wubu_passwd",   root);
    snprintf(h_file, sizeof h_file, "%s/wubu_hostname", root);
    snprintf(k_file, sizeof k_file, "%s/wubu_keymap",   root);
    snprintf(l_file, sizeof l_file, "%s/wubu_language", root);
    snprintf(t_file, sizeof t_file, "%s/wubu_timezone", root);

    /* ---- a plan as the wizard would leave it (hash already derived) ------ */
    wubu_install_plan plan;
    memset(&plan, 0, sizeof plan);
    snprintf(plan.lang,       sizeof plan.lang,       "%s", "en");
    snprintf(plan.keyboard,   sizeof plan.keyboard,   "%s", "us");
    snprintf(plan.username,   sizeof plan.username,   "%s", "wubu");
    snprintf(plan.pw_salt,    sizeof plan.pw_salt,    "%s", "SALTsalt");
    snprintf(plan.disk_mode,  sizeof plan.disk_mode,  "%s", "full");
    snprintf(plan.disk_target,sizeof plan.disk_target,"%s", "0");
    snprintf(plan.timezone,   sizeof plan.timezone,   "%s", "UTC");
    plan.encrypt = 1; plan.pw_len = 7;
    wubu_password_hash("hunter2", plan.pw_salt, plan.pw_hash, sizeof plan.pw_hash);

    /* ---- provision writes the manifest ----------------------------------- */
    CHECK(wubu_firstboot_provision(&plan, root) == 0, "provisioning the plan succeeds");
    {
        char buf[512];
        read_file(p_file, buf, sizeof buf);
        CHECK(strstr(buf, "wubu:") != NULL, "the passwd line names the user");
        CHECK(strstr(buf, plan.pw_hash) != NULL, "the passwd line carries the hash");
        CHECK(strstr(buf, "hunter2") == NULL, "the plaintext password never reaches disk");
        read_file(h_file, buf, sizeof buf);
        CHECK(strcmp(buf, "wubu") == 0, "hostname is provisioned");
        read_file(k_file, buf, sizeof buf);
        CHECK(strcmp(buf, "us") == 0, "keymap is provisioned");
        read_file(l_file, buf, sizeof buf);
        CHECK(strcmp(buf, "en") == 0, "language is provisioned");
        read_file(t_file, buf, sizeof buf);
        CHECK(strcmp(buf, "UTC") == 0, "timezone is provisioned");
    }

    /* ---- one-way but verifiable -------------------------------------------- */
    CHECK(wubu_firstboot_verify("hunter2", &plan) == 1, "the right password verifies");
    CHECK(wubu_firstboot_verify("wrong", &plan) == 0, "the wrong password fails");
    {
        wubu_install_plan bad = plan; bad.pw_hash[0] = 0;
        CHECK(wubu_firstboot_verify("hunter2", &bad) == -1,
              "a plan with no hash cannot be verified");
    }

    /* ---- refuse a plan that would write a lie ------------------------------ */
    {
        wubu_install_plan bad = plan; bad.username[0] = 0;
        CHECK(wubu_firstboot_provision(&bad, root) == -1,
              "an empty username refuses to provision");
        wubu_install_plan bad2 = plan; snprintf(bad2.pw_hash, sizeof bad2.pw_hash, "%s", "garbage");
        CHECK(wubu_firstboot_provision(&bad2, root) == -1,
              "a malformed hash refuses to provision");
        CHECK(wubu_firstboot_provision(NULL, root) == -1, "a NULL plan is refused");
        CHECK(wubu_firstboot_provision(&plan, NULL) == -1, "a NULL root is refused");
    }

    /* ---- cleanup ------------------------------------------------------------ */
    remove(p_file); remove(h_file); remove(k_file); remove(l_file); remove(t_file);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
