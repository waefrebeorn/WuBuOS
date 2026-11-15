/* wubu_install_tty_test.c -- the stdio configurator front end (P1 OOBE).
 *
 * Runs the REAL stdio path: answers are piped in through a scripted stdin, and
 * the confirmed plan must be written out as a /n/install answers doc on disk
 * that parses back to the same plan -- proving the front end, not just the
 * engine, works. Also proves the cancel path writes nothing.
 *
 * Self-contained: links wubu_install_tty.c + wubu_install.c + kernel sha256.
 * The password goes through stdin, so transient echo (Windows) is irrelevant.
 */
#include "wubu_install_tty.h"
#include "wubu_install.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static void write_lines(const char *path, const char *const *lines, int n)
{
    FILE *f = fopen(path, "w");
    if (!f) { CHECK(0, "input file opened"); return; }
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", lines[i]);
    fclose(f);
}

int main(void)
{
    const char *base = getenv("TMPDIR");
    if (!base || !base[0]) base = getenv("TMP");
    if (!base || !base[0]) base = getenv("TEMP");
    if (!base || !base[0]) base = ".";
    char in[512], planp[512], out[512], planp_cancel[512];
    snprintf(in,   sizeof in,   "%s/wubu_tty_in.txt",   base);
    snprintf(planp,sizeof planp,"%s/wubu_tty_plan.txt", base);
    snprintf(out,  sizeof out,  "%s/wubu_tty_out.txt",  base);
    snprintf(planp_cancel,sizeof planp_cancel,"%s/wubu_tty_plan_cancel.txt", base);

    /* ---- happy path: 8 answers + a confirming 'y' ------------------------- */
    static const char *const happy[] = {
        "en", "us", "wubu", "hunter2", "full", "0", "1", "UTC", "y"
    };
    write_lines(in, happy, 9);
    CHECK(freopen(in, "r", stdin) != NULL, "stdin switched to the scripted answers");
    CHECK(freopen(out, "w", stdout) != NULL, "stdout silenced");
    wubu_install_plan p;
    memset(&p, 0, sizeof p);
    int rc = wubu_install_run_tty(&p, planp);
    CHECK(rc == 1, "the tty wizard confirmed and wrote the plan");
    CHECK(strcmp(p.username, "wubu") == 0 && strcmp(p.lang, "en") == 0 &&
          strcmp(p.disk_mode, "full") == 0,
          "answers flowed from stdin into the plan");
    CHECK(p.encrypt == 1, "encryption stayed on by default through the front end");
    CHECK(strncmp(p.pw_hash, "sha256$wubutty$", 15) == 0,
          "the password was hashed by the real front end");
    CHECK(strstr(p.pw_hash, "hunter2") == NULL, "the plaintext never reached the plan");

    /* ---- the written /n/install document is the same plan ----------------- */
    {
        FILE *f = fopen(planp, "r");
        CHECK(f != NULL, "the answers document was written to disk");
        if (f) {
            char doc[1024]; size_t got = fread(doc, 1, sizeof doc - 1, f); doc[got] = 0;
            fclose(f);
            CHECK(strstr(doc, "hunter2") == NULL, "the document holds no plaintext password");
            wubu_install_plan q;
            memset(&q, 0, sizeof q);
            CHECK(wubu_install_parse(doc, &q) == 0, "the document parses back");
            CHECK(strcmp(q.username, p.username) == 0 &&
                  strcmp(q.pw_hash, p.pw_hash) == 0 &&
                  q.pw_len == p.pw_len && q.encrypt == p.encrypt,
                  "the on-disk document round-trips the plan");
            CHECK((q.pw_len == 7), "the password length is preserved in the document");
        }
    }

    /* ---- cancel: refusing at the summary writes nothing --------------------- */
    {
        static const char *const no[] = {
            "en", "us", "wubu", "hunter2", "full", "0", "1", "UTC", "n"
        };
        write_lines(in, no, 9);
        CHECK(freopen(in, "r", stdin) != NULL, "stdin switched to the cancelling answers");
        wubu_install_plan c;
        memset(&c, 0, sizeof c);
        int rc2 = wubu_install_run_tty(&c, planp_cancel);
        CHECK(rc2 == 0, "answering 'n' at the summary cancels");
        FILE *probe = fopen(planp_cancel, "r");
        CHECK(probe == NULL, "a cancelled install writes no document");
        if (probe) fclose(probe);
    }

    /* ---- empty password is rejected by the real front end -------------------- */
    {
        static const char *const empty_pw[] = { "en", "us", "wubu", "", "full", "0", "1", "UTC", "y" };
        write_lines(in, empty_pw, 9);
        CHECK(freopen(in, "r", stdin) != NULL, "stdin switched to the empty-password answers");
        wubu_install_plan e;
        memset(&e, 0, sizeof e);
        int rc3 = wubu_install_run_tty(&e, planp_cancel);
        CHECK(rc3 == -1, "an empty password is rejected through the front end");
        FILE *probe = fopen(planp_cancel, "r");
        CHECK(probe == NULL, "no document for a rejected password");
        if (probe) fclose(probe);
    }

    /* ---- cleanup ------------------------------------------------------------ */
    remove(in); remove(out); remove(planp); remove(planp_cancel);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
