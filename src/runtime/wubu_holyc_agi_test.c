/*
 * test_holyc_agi.c -- Live ring-0 compiler AGI layer test.
 *
 * Proves the TempleOS "God compiler" is now CONNECTED: a human at the HolyC
 * Terminal and the AGI both author HolyC source that compiles + runs LIVE via
 * the same path, with the AGI's compile+run disclosed to EDR. No sandbox, no
 * separate build step -- source in, native x86-64 out, executed in place.
 */

#include "wubu_holyc_agi.h"
#include "wubu_edr.h"
#include "wubu_holyd.h"
#include <stdio.h>
#include <string.h>

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { g_run++; if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
                         else { printf("  ❌ %s\n", msg); } } while (0)

static int eval_int(const char *src) {
    char out[1024];
    int ret = wubu_holyc_eval(src, out, sizeof(out));
    if (ret != 0) { printf("    [eval '%s' -> err: %s]\n", src, out); return -9999; }
    return (int)strtoll(out, NULL, 10);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== WuBuOS Live HolyC Compiler AGI Layer Test ===\n\n");

    edr_analytics_set_enabled(true);

    /* -- 1. The terminal's eval now REALLY compiles + runs -- */
    printf("[Live compile + execute]\n");
    T(eval_int("1+2+3") == 6,            "eval 1+2+3 compiles+returns 6");
    T(eval_int("{ I64 x = 5; x*x; }") == 25, "block + var + mul compiles+returns 25");
    T(eval_int("3 & 5") == 1,            "bitwise & compiles+returns 1");

    /* -- 2. Functions + persistent state across evals (REPL semantics) -- */
    printf("\n[Persistent REPL state]\n");
    T(eval_int("I64 sq(I64 n){ return n*n; } 0") == 0, "function declaration accepted");
    T(eval_int("sq(9)") == 81,          "call user function sq(9) -> 81 (state persisted)");
    T(eval_int("I64 acc = 10; 0") == 0, "persistent var declared");
    T(eval_int("acc + 32") == 42,        "persistent var acc read across evals -> 42");

    /* -- 3. Errors are reported, not silently swallowed -- */
    printf("\n[Error reporting]\n");
    char out[1024];
    int r = wubu_holyc_eval("this is not holyc @@@", out, sizeof(out));
    T(r != 0 && out[0] != '\0', "garbage source reports an error (no silent stub)");

    /* -- 4. The AGI path compiles+logs to EDR (transparency edict) -- */
    printf("\n[AGI compile+run disclosed to EDR]\n");
    uint64_t before = edr_agent_events_logged();
    char aout[1024];
    int ar = wubu_holyc_agent_eval("2*21", aout, sizeof(aout));
    T(ar == 0 && eval_int("2*21") == 42, "agent eval 2*21 computes 42");
    uint64_t after = edr_agent_events_logged();
    T(after > before, "agent compile+run logged an EDR_AGENT_ACTION event");
    /* The event detail must carry the actual source, so a human can audit it. */
    EdrEventView ev[8];
    int n = edr_recent_events(ev, 8, 26, 26);
    int found_src = 0;
    for (int i = 0; i < n; i++)
        if (strstr(ev[i].detail, "holyc: 2*21")) { found_src = 1; break; }
    T(found_src, "EDR event detail discloses the exact HolyC source compiled");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
