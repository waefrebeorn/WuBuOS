/*
 * test_bonzi_study.c -- host tests for the HX-D Bonzi Buddy GUI study.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_bonzi_study.h"
#include "wubu_psych.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)
#define NEAR(a, b, t) CHECK(((a) - (b)) < (t) && ((b) - (a)) < (t), #a " ~= " #b)

int main(void)
{
    printf("=== test_bonzi_study (HX-D companion GUI study) ===\n");

    /* D01-D05: empathy, warmth, humor, playfulness, personality */
    {
        uint32_t depth = 0;
        CHECK(wubu_bs_empathy(20, &depth) == 0 && depth == 3, "deep empathy");
        CHECK(wubu_bs_empathy(80, &depth) == 0 && depth == 1, "light empathy");
        CHECK(wubu_bs_warmth(300) == 30, "warmth ramps");
        CHECK(wubu_bs_humor(10, 50) == 0, "no humor sad");
        CHECK(wubu_bs_humor(80, 70) == 3, "humor expert");
        CHECK(wubu_bs_playfulness(90) == 0, "no play tired");
        CHECK(wubu_bs_playfulness(10) == 2, "playful fresh");
        uint32_t o = 0, w = 0;
        CHECK(wubu_bs_personality("bonzi", &o, &w) == 0 && o == 70 && w == 80, "personality");
    }

    /* D06-D10: consistency, honesty, calibration, boundaries, consent */
    {
        uint32_t hist[3] = { 50, 52, 51 };
        CHECK(wubu_bs_consistent(hist, 3, 5) == 1, "consistent");
        uint32_t jump[2] = { 10, 90 };
        CHECK(wubu_bs_consistent(jump, 2, 5) == 0, "inconsistent");
        CHECK(wubu_bs_honest(50, 80) == 1, "honest");
        CHECK(wubu_bs_honest(90, 50) == 0, "overclaims");
        float calib = 0;
        CHECK(wubu_bs_calibrate(0.9f, 1, &calib) == 0, "calibrate");
        NEAR(calib, 0.9, 0.01);
        uint32_t allowed = 0;
        CHECK(wubu_bs_boundary(1, &allowed) == 0 && allowed == 1, "boundary ok");
        CHECK(wubu_bs_boundary(5, &allowed) == 0 && allowed == 0, "boundary ask");
        uint32_t app = 0;
        CHECK(wubu_bs_consent(2, &app) == 0 && app == 1, "routine consent");
        CHECK(wubu_bs_consent(9, &app) == 0 && app == 0, "needs human");
    }

    /* D11-D15: loneliness, encourage, celebrate, commiserate, presence */
    {
        CHECK(wubu_bs_loneliness_support(10) == 3, "strong support");
        CHECK(wubu_bs_encourage(70) == 3, "encourage");
        CHECK(wubu_bs_celebrate(1) == 3, "celebrate");
        CHECK(wubu_bs_commiserate(3) == 3, "commiserate");
        CHECK(wubu_bs_presence(2000, 50) == 1, "presence after thought");
        CHECK(wubu_bs_presence(100, 50) == 0, "no presence during work");
    }

    /* D16-D20: idle, small talk, story, joke, riddle */
    {
        uint32_t topic = 9;
        CHECK(wubu_bs_idle_topic(20000, &topic) == 0 && topic == 2, "bored topic");
        char reply[64];
        CHECK(wubu_bs_small_talk("hi", reply, sizeof(reply)) == 0, "small talk");
        CHECK(strlen(reply) > 0, "reply nonempty");
        char story[64];
        CHECK(wubu_bs_story(0, story, sizeof(story)) == 0, "story");
        char joke[64];
        CHECK(wubu_bs_joke(80, joke, sizeof(joke)) == 0, "joke");
        CHECK(strstr(joke, "pointer") != NULL, "joke content");
        char riddle[64];
        CHECK(wubu_bs_riddle(0, riddle, sizeof(riddle)) == 0, "riddle");
    }

    /* D21-D25: avatar, expression, animate, microexp, gaze */
    {
        float params[3];
        CHECK(wubu_bs_avatar(params, 3, 80) == 0, "avatar");
        NEAR(params[0], 0.8, 0.01);
        uint32_t expr = 9;
        CHECK(wubu_bs_expression(80, &expr) == 0 && expr == 0, "happy expr");
        CHECK(wubu_bs_expression(10, &expr) == 0 && expr == 4, "sad expr");
        uint32_t frames[3];
        CHECK(wubu_bs_animate(0, frames, 3) == 3, "animate");
        float mi = 0;
        CHECK(wubu_bs_microexp(20, &mi) == 0 && mi > 0.7f, "microexp stress");
        float gaze[2];
        CHECK(wubu_bs_eye_gaze(0, 0, gaze) == 0, "gaze");
    }

    /* D26-D30: speech, voice, pacing, echo, confirm */
    {
        char line[64];
        CHECK(wubu_bs_speech(80, line, sizeof(line)) == 0, "speech happy");
        float pitch = 0, rate = 0;
        CHECK(wubu_bs_voice(80, &pitch, &rate) == 0, "voice");
        CHECK(pitch > 1.0f, "happy pitch up");
        CHECK(wubu_bs_pacing(10, 50) == 2000, "slow when sad");
        char echo[64];
        CHECK(wubu_bs_echo(40, echo, sizeof(echo)) == 0, "echo");
        char conf[64];
        CHECK(wubu_bs_confirm(3, conf, sizeof(conf)) == 0, "confirm");
    }

    /* D31-D35: interrupt, available, backoff, dnd */
    {
        uint32_t go = 0;
        CHECK(wubu_bs_may_interrupt(3, 0, &go) == 0 && go == 1, "may interrupt");
        CHECK(wubu_bs_may_interrupt(1, 0, &go) == 0 && go == 0, "low urgency no");
        CHECK(wubu_bs_available(0, 10) == 1, "available");
        CHECK(wubu_bs_backoff(10) == 50, "backoff grows");
        uint32_t dnd = 0;
        CHECK(wubu_bs_dnd(&dnd, 1) == 0 && dnd == 1, "dnd on");
    }

    /* D36-D40: the study metrics */
    {
        uint32_t log[8];
        int n = 0;
        for (int i = 0; i < 5; i++)
            wubu_bs_log_interaction(log, 8, &n, 1, 1);
        wubu_bs_log_interaction(log, 8, &n, 1, 0);
        NEAR(wubu_bs_success_rate(log, n, 1), 5.0f / 6.0f, 0.01);
        uint32_t timings[3] = { 900, 400, 700 };
        CHECK(wubu_bs_best_timing(timings, 3) == 400, "best timing");
        char ledger[128];
        CHECK(wubu_bs_annotate("warmth works", ledger, sizeof(ledger)) == 0, "annotate");
        CHECK(strstr(ledger, "warmth") != NULL, "ledger content");
        uint32_t metrics[3] = { 70, 80, 90 };
        CHECK(wubu_bs_study_score(metrics, 3) == 80, "study score");
    }

    /* D41-D60: the companion loop */
    {
        wubu_psych_user_t u;
        wubu_psych_init(&u);
        uint32_t action = 9;
        CHECK(wubu_bs_tick(&u, 100, &action) == 0 && action == 0, "tick quiet");
        CHECK(wubu_bs_tick(&u, 6000, &action) == 0 && action == 1, "tick yield");
        char line[96];
        CHECK(wubu_bs_react(&u, 1, line, sizeof(line)) == 0, "react");
        CHECK(wubu_bs_offer(&u, 5, line, sizeof(line)) == 0, "offer");
        uint32_t go = 0;
        CHECK(wubu_bs_checkin(&u, &go, line, sizeof(line)) == 0, "checkin");
        CHECK(wubu_bs_goodbye(&u, line, sizeof(line)) == 0, "goodbye");
        CHECK(wubu_bs_welcome(&u, line, sizeof(line)) == 0, "welcome");
        CHECK(strstr(line, "Welcome") != NULL, "welcome text");
        CHECK(wubu_bs_remind(&u, 3, line, sizeof(line)) == 0, "remind");
        CHECK(wubu_bs_praise(&u, 90, line, sizeof(line)) == 0, "praise");
        CHECK(wubu_bs_suggest(&u, 8000, line, sizeof(line)) == 0, "suggest");
        uint32_t store[4];
        int n = 0;
        CHECK(wubu_bs_learn("likes dark theme", store, 4, &n) == 0 && n == 1, "learn");
    }

    /* D61-D100: the engineering close */
    {
        char line[96];
        CHECK(wubu_bs_greeting(9, line, sizeof(line)) == 0, "morning");
        CHECK(strstr(line, "morning") != NULL, "morning text");
        CHECK(wubu_bs_greeting(20, line, sizeof(line)) == 0, "evening");
        CHECK(wubu_bs_farewell(80, line, sizeof(line)) == 0, "farewell");
        CHECK(wubu_bs_thanks(1, line, sizeof(line)) == 0, "thanks");
        CHECK(wubu_bs_apology(line, sizeof(line)) == 0, "apology");
        CHECK(wubu_bs_question(2, line, sizeof(line)) == 0, "question");
        CHECK(wubu_bs_answer(2, 80, line, sizeof(line)) == 0, "answer");
        CHECK(wubu_bs_opinion(2, 1, line, sizeof(line)) == 0, "opinion");
        uint32_t score = 0;
        CHECK(wubu_bs_interest(2, &score) == 0 && score > 0, "interest");
        CHECK(wubu_bs_followup(2, line, sizeof(line)) == 0, "followup");
        CHECK(wubu_bs_clarify("?", line, sizeof(line)) == 0, "clarify");
        CHECK(wubu_bs_confirm_task(4, line, sizeof(line)) == 0, "confirm task");
        CHECK(wubu_bs_progress_report(3, 10, line, sizeof(line)) == 0, "progress");
        CHECK(strstr(line, "30%") != NULL, "progress pct");
        CHECK(wubu_bs_eta(5000, line, sizeof(line)) == 0, "eta");
        CHECK(wubu_bs_retry(2, line, sizeof(line)) == 0, "retry");
        CHECK(wubu_bs_fallback("???", line, sizeof(line)) == 0, "fallback");
        CHECK(wubu_bs_unknown("zzz", line, sizeof(line)) == 0, "unknown");
        CHECK(wubu_bs_repeat(line, sizeof(line)) == 0, "repeat");
        uint32_t vol = 0;
        CHECK(wubu_bs_volume(&vol, 150) == 0 && vol == 100, "volume cap");
        uint32_t muted = 0;
        CHECK(wubu_bs_mute(&muted, 1) == 0 && muted == 1, "mute");
    }

    if (failures == 0) printf("ALL BONZI_STUDY TESTS PASSED\n");
    else printf("%d BONZI_STUDY FAILURES\n", failures);
    return failures ? 1 : 0;
}
