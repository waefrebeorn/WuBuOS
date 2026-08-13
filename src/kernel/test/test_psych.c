/*
 * test_psych.c -- host tests for the HX psychology/timing/patience engine.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_psych.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)
#define NEAR(a, b, t) CHECK(((a) - (b)) < (t) && ((b) - (a)) < (t), #a " ~= " #b)

int main(void)
{
    printf("=== test_psych (HX human psychology + timing loops) ===\n");

    /* A01-A05: user model */
    {
        wubu_psych_user_t u;
        CHECK(wubu_psych_init(&u) == 0, "init");
        CHECK(wubu_psych_sane(&u) == 1, "sane");
        CHECK(strcmp(u.name, "user") == 0, "name");
        CHECK(wubu_psych_set_pref(&u, "pace", 55) == 0 && u.pace == 55, "pref");
        CHECK(wubu_psych_record_action(&u, 400) == 0, "record");
        CHECK(u.interaction_count == 1, "count");
        NEAR(u.avg_task_seconds, 0.4, 0.01);
    }

    /* A06-A10: skill, fatigue, mood, context */
    {
        wubu_psych_user_t u;
        wubu_psych_init(&u);
        wubu_psych_update_skill(&u, +40);
        CHECK(u.skill_level == 70, "skill up");
        wubu_psych_update_skill(&u, -200);
        CHECK(u.skill_level == 0, "skill floor");
        wubu_psych_update_fatigue(&u, 600);
        CHECK(u.fatigue == 100, "fatigue cap");
        wubu_psych_update_mood(&u, +30);
        CHECK(u.mood == 100, "mood cap");
        wubu_psych_session_tick(&u, 30);
        CHECK(u.session_minutes == 30, "session");
    }

    /* B01-B05: adaptive density/complexity */
    {
        CHECK(wubu_psych_density(80, 20) == 0, "sparse for tired novice");
        CHECK(wubu_psych_density(10, 80) == 2, "dense for expert");
        CHECK(wubu_psych_complexity(20, 20) == 0, "simple when sad");
        CHECK(wubu_psych_complexity(80, 80) == 2, "complex for happy expert");
    }

    /* B06-B08: novice/expert + disclosure */
    {
        uint32_t mode = 9;
        CHECK(wubu_psych_mode(70, &mode) == 0 && mode == 1, "expert mode");
        CHECK(wubu_psych_mode(20, &mode) == 0 && mode == 0, "novice mode");
        CHECK(wubu_psych_disclose(10, 30) >= 3, "disclosure grows");
        CHECK(wubu_psych_menu_depth(80, 10) == 3, "deep menus expert");
        CHECK(wubu_psych_menu_depth(20, 90) == 1, "shallow menus tired");
        char tip[64];
        CHECK(wubu_psych_help(0, "save", tip, sizeof(tip)) == 0, "help novice");
        CHECK(strstr(tip, "big button") != NULL, "novice tip");
    }

    /* B13-B14: workload */
    {
        CHECK(wubu_psych_workload(30, 60) == 30, "workload 30/h");
    }

    /* B15-B20: the control arbitration -- the human is priority */
    {
        wubu_psych_attention_t a;
        memset(&a, 0, sizeof(a));
        wubu_psych_input_seen(&a, 1000);
        CHECK(a.human_driving == 1, "human driving");
        CHECK(wubu_psych_may_act(&a, 1100, 500) == 0, "no act during human");
        /* the human lets go; the AGI waits the min-wait then may act */
        wubu_psych_ai_act(&a, 1000);
        CHECK(a.ai_driving == 1, "ai driving");
        CHECK(wubu_psych_may_act(&a, 1499, 500) == 0, "not yet");
        CHECK(wubu_psych_may_act(&a, 1500, 500) == 1, "may act now");
        /* patience budget */
        CHECK(wubu_psych_patience_left(&a, 1300, 1000) == 700, "patience left");
        CHECK(wubu_psych_patience_left(&a, 2500, 1000) == 0, "patience gone");
        /* the human grabs the input back -> the AGI yields */
        wubu_psych_ai_yield(&a);
        CHECK(a.human_driving == 1 && a.ai_driving == 0, "yielded");
    }

    /* B21-B30: the social rhythm */
    {
        wubu_psych_user_t u;
        wubu_psych_init(&u);
        CHECK(wubu_psych_should_speak(&u, 2000) == 1, "speak after thought");
        CHECK(wubu_psych_should_speak(&u, 100) == 0, "silent during work");
        CHECK(wubu_psych_should_assist(&u, 12000) == 1, "assist when stuck");
        CHECK(wubu_psych_should_assist(&u, 1000) == 0, "not stuck yet");
        CHECK(wubu_psych_should_yield(&u, 6000) == 1, "yield after patience");
        CHECK(wubu_psych_typing_pause(60) == 1000, "typing pause 60wpm");
    }

    /* B31-B40: reaction, Fitts, Hick, memory */
    {
        CHECK(wubu_psych_reaction(0, 100) == 250, "fast reaction");
        CHECK(wubu_psych_reaction(100, 0) == 550, "slow reaction");
        NEAR(wubu_psych_fitts(100, 50, 0, 1), 3.0, 0.01);
        CHECK(wubu_psych_hick(4) > 150.0f, "hick grows");
        NEAR(wubu_psych_recognition(3, 4), 0.75, 0.01);
        CHECK(wubu_psych_short_term(3) == 7, "magic seven");
    }

    /* B41-B60: load, prime, feedback, praise */
    {
        CHECK(wubu_psych_load(5, 2) == 15, "load");
        NEAR(wubu_psych_prime(1.0f, 0.5f), 1.5, 0.01);
        uint32_t tone = 9;
        wubu_psych_feedback(1, &tone);  CHECK(tone == 0, "positive tone");
        wubu_psych_feedback(-1, &tone); CHECK(tone == 1, "negative tone");
        wubu_psych_feedback(0, &tone);  CHECK(tone == 2, "neutral tone");
        CHECK(wubu_psych_praise_strength(90) == 3, "big praise");
        CHECK(wubu_psych_praise_strength(20) == 1, "small praise");
    }

    /* B61-B80: preference learning, implicit, drift, export */
    {
        NEAR(wubu_psych_pref_delta(80, 50), 0.3, 0.01);
        uint32_t signals[3] = { 80, 90, 70 };
        float score = 0;
        CHECK(wubu_psych_implicit(signals, 3, &score) == 0, "implicit");
        NEAR(score, 0.8, 0.01);
        uint32_t hist[3] = { 10, 20, 90 };
        CHECK(wubu_psych_drift(hist, 3, 50) == 1, "drift");
        CHECK(wubu_psych_drift(hist, 3, 100) == 0, "no drift");
        wubu_psych_user_t u;
        wubu_psych_init(&u);
        char buf[256];
        CHECK(wubu_psych_export(&u, buf, sizeof(buf)) == 0, "export");
        CHECK(strstr(buf, "user=user") != NULL, "export content");
        uint32_t h = wubu_psych_audit(&u);
        CHECK(h != 0, "audit hash");
        CHECK(wubu_psych_wipe(&u) == 0 && u.magic != WUBU_PSYCH_MAGIC, "wipe");
    }

    /* B91-B100: bench, energy, fuzz */
    {
        CHECK(wubu_psych_bench(1000) == 1000, "bench");
        NEAR(wubu_psych_energy(0.1f, 10), 1.0, 0.001);
        CHECK(wubu_psych_fuzz_guard(100, 100) == 1, "fuzz ok");
        CHECK(wubu_psych_fuzz_guard(150, 100) == 0, "fuzz bad");
    }

    /* the tandem loop */
    {
        wubu_psych_attention_t a;
        memset(&a, 0, sizeof(a));
        wubu_psych_proposal_t p;
        strcpy(p.title, "optimize the cache");
        strcpy(p.body, "the KV cache is 80% full -- compact it?");
        p.proposal_id = 7;
        CHECK(wubu_psych_propose(&a, &p, 2) == 0, "propose");
        CHECK(a.pending_proposal == 7, "pending");
        CHECK(wubu_psych_accept(&a, 7) == 0 && a.pending_proposal == 0, "accept");
        p.proposal_id = 8;
        wubu_psych_propose(&a, &p, 9);
        CHECK(p.urgency == 3, "urgency capped");
        CHECK(wubu_psych_accept(&a, 999) == -1, "wrong id rejected");
    }

    if (failures == 0) printf("ALL PSYCH TESTS PASSED\n");
    else printf("%d PSYCH FAILURES\n", failures);
    return failures ? 1 : 0;
}
