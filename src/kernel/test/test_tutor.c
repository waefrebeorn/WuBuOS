/*
 * test_tutor.c -- host tests for the HX-C learning/education frontier.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_tutor.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)
#define NEAR(a, b, t) CHECK(((a) - (b)) < (t) && ((b) - (a)) < (t), #a " ~= " #b)

int main(void)
{
    printf("=== test_tutor (HX-C learning + recursive loop) ===\n");

    /* C01-C05: lesson, exercise, answer, level, curriculum */
    {
        char buf[128];
        CHECK(wubu_tutor_lesson(0, 1, buf, sizeof(buf)) == 0, "lesson");
        CHECK(strlen(buf) > 0, "lesson nonempty");
        CHECK(wubu_tutor_exercise(0, 1, buf, sizeof(buf)) == 0, "exercise");
        CHECK(wubu_tutor_answer(1, 5, 5, buf, sizeof(buf)) == 0, "answer");
        CHECK(strstr(buf, "Correct") != NULL, "correct fb");
        CHECK(wubu_tutor_answer(1, 4, 5, buf, sizeof(buf)) == 0, "answer wrong");
        CHECK(wubu_tutor_next_level(3) == 1, "level up");
        CHECK(wubu_tutor_next_level(1) == 0, "no level up");
        uint32_t units[3];
        CHECK(wubu_tutor_curriculum(1, units, 3) == 3, "curriculum");
    }

    /* C06-C10: quiz, grade, review, spaced, retention */
    {
        char buf[128];
        CHECK(wubu_tutor_quiz(0, 5, buf, sizeof(buf)) == 0, "quiz");
        CHECK(wubu_tutor_grade(7, 10) == 70, "grade");
        CHECK(wubu_tutor_review(0, buf, sizeof(buf)) == 0, "review");
        uint32_t due = 0;
        CHECK(wubu_tutor_spaced(&due, 95) == 0 && due == 30, "spaced master");
        CHECK(wubu_tutor_spaced(&due, 10) == 0 && due == 1, "spaced novice");
        CHECK(wubu_tutor_retention(5, 1.0f) == 16, "retention decays");
    }

    /* C11-C15: explain, example, analogy, hint, scaffold */
    {
        char buf[128];
        CHECK(wubu_tutor_explain(0, 0, buf, sizeof(buf)) == 0, "explain simple");
        CHECK(strstr(buf, "simply") != NULL, "simple wording");
        CHECK(wubu_tutor_explain(0, 1, buf, sizeof(buf)) == 0, "explain deep");
        CHECK(wubu_tutor_example(0, buf, sizeof(buf)) == 0, "example");
        CHECK(wubu_tutor_analogy(0, buf, sizeof(buf)) == 0, "analogy");
        CHECK(wubu_tutor_hint(2, buf, sizeof(buf)) == 0, "hint");
        uint32_t steps[3];
        CHECK(wubu_tutor_scaffold(1, steps, 3) == 3, "scaffold");
        CHECK(steps[0] == 3, "scaffold base");
    }

    /* C16-C20: feedback, correct, praise, pace, wait */
    {
        char buf[128];
        CHECK(wubu_tutor_feedback(5, 5, buf, sizeof(buf)) == 0, "feedback");
        CHECK(wubu_tutor_correct(4, 5, buf, sizeof(buf)) == 0, "correct");
        CHECK(strstr(buf, "5") != NULL, "correct answer shown");
        CHECK(wubu_tutor_praise(6) == 3, "big praise");
        CHECK(wubu_tutor_pace(70, 10) == 600, "fast pace expert");
        CHECK(wubu_tutor_pace(20, 90) == 1200, "slow pace tired");
        uint32_t wait = 0;
        CHECK(wubu_tutor_wait(2, &wait) == 0 && wait == 2500, "wait grows");
    }

    /* C21-C30: mastery, forget, transfer, prereq, difficulty */
    {
        uint32_t scores[3] = { 60, 80, 70 };
        CHECK(wubu_tutor_mastery(scores, 3) == 70, "mastery avg");
        NEAR(wubu_tutor_forget(10, 10.0f), 0.5, 0.01);
        NEAR(wubu_tutor_transfer(0.8f, 0.5f), 0.4, 0.01);
        uint32_t prereqs[2] = { 1, 2 };
        uint32_t ok = 0;
        CHECK(wubu_tutor_prereq(3, prereqs, 2, &ok) == 0 && ok == 1, "prereq ok");
        CHECK(wubu_tutor_prereq(1, prereqs, 2, &ok) == 0 && ok == 0, "prereq missing");
        CHECK(wubu_tutor_difficulty(2, 3) == 35, "difficulty");
    }

    /* C31-C40: Socratic, worked, interleave, elaborate, retrieve, dual */
    {
        char buf[128];
        CHECK(wubu_tutor_socratic(0, 1, buf, sizeof(buf)) == 0, "socratic");
        uint32_t steps[2] = { 1, 2 };
        CHECK(wubu_tutor_worked(steps, 2, buf, sizeof(buf)) == 0, "worked");
        uint32_t topics[3] = { 1, 2, 3 }, order[3];
        CHECK(wubu_tutor_interleave(topics, 3, order) == 3, "interleave");
        CHECK(wubu_tutor_elaborate(0, buf, sizeof(buf)) == 0, "elaborate");
        CHECK(wubu_tutor_retrieve(0, buf, sizeof(buf)) == 0, "retrieve");
        CHECK(wubu_tutor_dual_code(0, buf, sizeof(buf)) == 0, "dual code");
    }

    /* C41-C50: goal, streak, challenge, badge, leaderboard, break */
    {
        char buf[128];
        uint32_t goal = 0;
        CHECK(wubu_tutor_goal(&goal, 1, 5) == 0 && goal == 5, "goal set");
        uint32_t streak = 2;
        CHECK(wubu_tutor_streak(&streak, 5, 4) == 0 && streak == 3, "streak");
        CHECK(wubu_tutor_streak(&streak, 9, 4) == 0 && streak == 1, "streak reset");
        CHECK(wubu_tutor_challenge(1, buf, sizeof(buf)) == 0, "challenge");
        CHECK(wubu_tutor_badge(2, buf, sizeof(buf)) == 0, "badge");
        CHECK(strstr(buf, "on-a-roll") != NULL, "badge name");
        uint32_t scores[3] = { 50, 90, 70 };
        int rank = 0;
        CHECK(wubu_tutor_leaderboard(scores, 3, &rank) == 0 && rank == 3, "rank");
        CHECK(wubu_tutor_reminder(5, buf, sizeof(buf)) == 0, "reminder");
        CHECK(wubu_tutor_break(10, buf, sizeof(buf)) == 0, "break");
    }

    /* C51-C60: focus, note, summarize, mindmap, teach-back, plan */
    {
        char buf[128];
        uint32_t go = 0;
        CHECK(wubu_tutor_focus(30, &go, buf, sizeof(buf)) == 0 && go == 1, "focus on");
        CHECK(wubu_tutor_focus(10, &go, buf, sizeof(buf)) == 0 && go == 0, "focus off");
        CHECK(wubu_tutor_note("the cache uses an LRU policy", buf, sizeof(buf)) == 0, "note");
        CHECK(strstr(buf, "note") != NULL, "note prefix");
        CHECK(wubu_tutor_summarize("long text here", buf, sizeof(buf)) == 0, "summarize");
        CHECK(wubu_tutor_mindmap(0, buf, sizeof(buf)) == 0, "mindmap");
        CHECK(wubu_tutor_rehearse(2, buf, sizeof(buf)) == 0, "rehearse");
        CHECK(wubu_tutor_self_test(0, buf, sizeof(buf)) == 0, "self test");
        CHECK(wubu_tutor_teach_back(0, buf, sizeof(buf)) == 0, "teach back");
        uint32_t blocks[4];
        CHECK(wubu_tutor_plan(4, blocks, 4, buf, sizeof(buf)) == 0, "plan");
        uint32_t reviewed = 0;
        CHECK(wubu_tutor_review_daily(&reviewed, buf, sizeof(buf)) == 0, "daily");
        CHECK(reviewed == 1, "reviewed count");
        uint32_t score = 0;
        CHECK(wubu_tutor_metric(0, &score) == 0 && score > 0, "metric");
    }

    /* C61-C100: the engineering close */
    {
        char buf[128];
        CHECK(wubu_tutor_time_to_mastery(60, 30) == 120, "time to mastery");
        uint32_t store[4];
        int n = 0;
        CHECK(wubu_tutor_checkpoint(1, 80, store, 4, &n) == 0 && n == 1, "checkpoint");
        CHECK(wubu_tutor_resume(0, buf, sizeof(buf)) == 0, "resume");
        CHECK(wubu_tutor_progress(5, 10, buf, sizeof(buf)) == 0, "progress");
        CHECK(strstr(buf, "50%") != NULL, "progress pct");
        CHECK(wubu_tutor_goal_set(0, 7, buf, sizeof(buf)) == 0, "goal set line");
        uint32_t on_track = 0;
        CHECK(wubu_tutor_goal_check(3, &on_track) == 0 && on_track == 1, "on track");
        CHECK(wubu_tutor_encourage_stuck(4, buf, sizeof(buf)) == 0, "encourage stuck");
        CHECK(wubu_tutor_praise_small(1, buf, sizeof(buf)) == 0, "praise small");
        uint32_t mastery[3] = { 90, 40, 70 };
        uint32_t next = 0;
        CHECK(wubu_tutor_next_topic(mastery, 3, &next) == 0 && next == 1, "next topic");
        CHECK(wubu_tutor_recommend(1, 2, buf, sizeof(buf)) == 0, "recommend");
        int32_t answers[4] = { 2, 5, 2, 2 };
        int32_t pattern = 0;
        CHECK(wubu_tutor_error_pattern(answers, 4, &pattern) == 0 && pattern == 2, "error pattern");
        CHECK(wubu_tutor_common_mistake(0, buf, sizeof(buf)) == 0, "common mistake");
        CHECK(wubu_tutor_verify_step(1, 1, buf, sizeof(buf)) == 0, "verify");
        CHECK(wubu_tutor_reward(50, buf, sizeof(buf)) == 0, "reward");
        CHECK(wubu_tutor_penalty(10, buf, sizeof(buf)) == 0, "penalty");
        CHECK(wubu_tutor_energy(0.1f, 60) == 6, "energy");
        CHECK(wubu_tutor_fuzz_guard(5, 50) == 1, "fuzz ok");
        CHECK(wubu_tutor_fuzz_guard(99, 50) == 0, "fuzz bad");
        uint32_t ts = 0, ta = 0;
        CHECK(wubu_tutor_audit(0, &ts, &ta) == 0, "audit");
        CHECK(wubu_tutor_export(mastery, 3, buf, sizeof(buf)) == 0, "export");
        CHECK(wubu_tutor_reset(store, 4) == 0 && store[0] == 0, "reset");
    }

    if (failures == 0) printf("ALL TUTOR TESTS PASSED\n");
    else printf("%d TUTOR FAILURES\n", failures);
    return failures ? 1 : 0;
}
