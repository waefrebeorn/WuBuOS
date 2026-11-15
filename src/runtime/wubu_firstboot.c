/*
 * wubu_firstboot.c -- deferred provisioning: consume the configurator's plan.
 *
 * The plan is the /n/install answers document; this is the step that makes it
 * a machine: the first boot writes the user, hostname, keymap, language and
 * timezone. It is deliberately a MANIFEST writer -- chroot-style flat files
 * under a target root -- so the exact same code runs against a restore volume
 * (a live env applying to a mounted target) and the OS's own first-boot
 * supervisor. It never needs the plaintext password: the plan already holds the
 * salted hash, and wubu_firstboot_verify proves it is one-way but checkable.
 */
#include "wubu_firstboot.h"
#include <stdio.h>
#include <string.h>

/* Write one manifest file under `root`; the parent must exist and be writable.
 * Returns 0 on success. */
static int write_file(const char *root, const char *name, const char *body)
{
    char path[512];
    if (snprintf(path, sizeof path, "%s/%s", root, name) >= (int)sizeof path)
        return 1;
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    int ok = fputs(body, f) >= 0 && fputc('\n', f) != EOF;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : 1;
}

/* A plan's password hash is usable iff it is present and shaped like the module
 * writes them: "sha256$<salt>$<64 hex>". Anything else is refused rather than
 * silently written out -- a bad hash on the passwd line is worse than none. */
static int hash_wellformed(const char *h)
{
    if (!h || strncmp(h, "sha256$", 7) != 0) return 0;
    const char *dot = strchr(h + 7, '$');
    if (!dot) return 0;
    const char *dig = dot + 1;
    size_t dl = strlen(dig);
    if (dl != 64) return 0;
    for (size_t i = 0; i < dl; i++) {
        char c = dig[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

int wubu_firstboot_provision(const wubu_install_plan *plan, const char *root)
{
    if (!plan || !root || !root[0]) return -1;
    if (!plan->username[0] || !hash_wellformed(plan->pw_hash)) return -1;

    char passwd[320];
    snprintf(passwd, sizeof passwd, "%s:%s:/bin/sh", plan->username, plan->pw_hash);
    if (write_file(root, "wubu_passwd", passwd) != 0) return -1;
    if (write_file(root, "wubu_hostname", plan->username[0] ? plan->username : "wubu") != 0)
        return -1;
    if (write_file(root, "wubu_keymap", plan->keyboard) != 0) return -1;
    if (write_file(root, "wubu_language", plan->lang) != 0) return -1;
    if (write_file(root, "wubu_timezone", plan->timezone) != 0) return -1;
    return 0;
}

int wubu_firstboot_verify(const char *password, const wubu_install_plan *plan)
{
    if (!plan || !plan->pw_salt[0] || !hash_wellformed(plan->pw_hash)) return -1;
    char rehash[WUBU_INSTALL_HASH_MAX];
    wubu_password_hash(password ? password : "", plan->pw_salt, rehash, sizeof rehash);
    return strcmp(rehash, plan->pw_hash) == 0 ? 1 : 0;
}
