/*
 * wubu_tutor.c -- the learning & education frontier (HX-C). C11.
 */
#include "wubu_tutor.h"
#include <string.h>
#include <stdio.h>

static const char *topic_names[] = {
    "the kernel", "the cache", "the scheduler", "the memory", "the tools",
};

int wubu_tutor_lesson(uint32_t topic, uint32_t level, char *lesson, int cap)
{
    if (!lesson || cap <= 0) return -1;
    snprintf(lesson, cap, "Lesson %u/%u: %s (the core idea, then the why)",
             level, topic, topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_exercise(uint32_t topic, uint32_t level, char *ex, int cap)
{
    if (!ex || cap <= 0) return -1;
    snprintf(ex, cap, "Exercise %u: what happens when %s fills up?",
             level, topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_answer(uint32_t exercise, int32_t user_ans, int32_t correct,
                      char *fb, int cap)
{
    (void)exercise;
    if (!fb || cap <= 0) return -1;
    if (user_ans == correct)
        snprintf(fb, cap, "Correct!");
    else
        snprintf(fb, cap, "Not quite -- the answer is %d.", (int)correct);
    return 0;
}

uint32_t wubu_tutor_next_level(uint32_t correct_streak)
{
    return correct_streak >= 3 ? 1 : 0;   /* level up after 3 in a row */
}

int wubu_tutor_curriculum(uint32_t topic, uint32_t *units, int n)
{
    if (!units || n <= 0) return -1;
    for (int i = 0; i < n; i++) units[i] = topic * 10 + (uint32_t)i;
    return n;
}

int wubu_tutor_quiz(uint32_t topic, uint32_t n, char *q, int cap)
{
    if (!q || cap <= 0) return -1;
    snprintf(q, cap, "Quiz on %s (%u questions): question 1...",
             topic_names[topic % 5], n);
    return 0;
}

int wubu_tutor_grade(uint32_t correct, uint32_t total)
{
    if (total == 0) return 0;
    return (int)((correct * 100) / total);
}

int wubu_tutor_review(uint32_t topic, char *summary, int cap)
{
    if (!summary || cap <= 0) return -1;
    snprintf(summary, cap, "Review: %s -- the 3 key points: ...",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_spaced(uint32_t *due_days, uint32_t mastery)
{
    if (!due_days) return -1;
    /* mastery 0..100 -> review in 1..30 days (spaced repetition) */
    *due_days = mastery > 90 ? 30 : (mastery > 60 ? 14 : (mastery > 30 ? 7 : 1));
    return 0;
}

int wubu_tutor_retention(uint32_t days, float decay)
{
    return (int)(100.0f / (1.0f + decay * (float)days));
}

int wubu_tutor_explain(uint32_t topic, uint32_t level, char *text, int cap)
{
    if (!text || cap <= 0) return -1;
    if (level == 0)
        snprintf(text, cap, "%s, simply: it keeps things organized.",
                 topic_names[topic % 5]);
    else
        snprintf(text, cap, "%s: the mechanism is a queue with priorities.",
                 topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_example(uint32_t topic, char *text, int cap)
{
    if (!text || cap <= 0) return -1;
    snprintf(text, cap, "Example: think of %s as your desk -- order matters.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_analogy(uint32_t topic, char *text, int cap)
{
    if (!text || cap <= 0) return -1;
    snprintf(text, cap, "Analogy: %s is like a library -- catalog first, find fast.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_hint(uint32_t n, char *text, int cap)
{
    if (!text || cap <= 0) return -1;
    snprintf(text, cap, "Hint %u: think about the order of operations.",
             n);
    return 0;
}

int wubu_tutor_scaffold(uint32_t level, uint32_t *steps, int n)
{
    if (!steps || n <= 0) return -1;
    uint32_t base = level * 3;
    for (int i = 0; i < n; i++) steps[i] = base + (uint32_t)i;
    return n;
}

int wubu_tutor_feedback(int32_t user_ans, int32_t correct, char *fb, int cap)
{
    return wubu_tutor_answer(0, user_ans, correct, fb, cap);
}

int wubu_tutor_correct(int32_t user_ans, int32_t correct, char *fb, int cap)
{
    if (!fb || cap <= 0) return -1;
    snprintf(fb, cap, "Close! The right answer was %d (you said %d).",
             (int)correct, (int)user_ans);
    return 0;
}

uint32_t wubu_tutor_praise(uint32_t streak)
{
    return streak >= 5 ? 3 : (streak >= 2 ? 2 : 1);
}

uint32_t wubu_tutor_pace(uint32_t skill, uint32_t fatigue)
{
    if (fatigue > 70) return 1200;
    return skill > 60 ? 600 : 900;
}

int wubu_tutor_wait(uint32_t level, uint32_t *wait_ms)
{
    if (!wait_ms) return -1;
    /* higher levels get longer think-time */
    *wait_ms = 1500 + level * 500;
    return 0;
}

uint32_t wubu_tutor_mastery(const uint32_t *scores, int n)
{
    if (!scores || n <= 0) return 0;
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) sum += scores[i];
    return sum / (uint32_t)n;
}

float wubu_tutor_forget(uint32_t days, float half_life)
{
    if (half_life <= 0) return 0;
    return 1.0f - (float)days / (half_life + (float)days);
}

float wubu_tutor_transfer(float a, float b)
{
    return a * b;
}

int wubu_tutor_prereq(uint32_t topic, const uint32_t *prereqs, int n, uint32_t *ok)
{
    if (!prereqs || !ok) return -1;
    *ok = 1;
    for (int i = 0; i < n; i++)
        if (prereqs[i] > topic) { *ok = 0; break; }
    return 0;
}

uint32_t wubu_tutor_difficulty(uint32_t topic, uint32_t level)
{
    return (topic % 5) * 10 + level * 5;
}

int wubu_tutor_socratic(uint32_t topic, uint32_t q_n, char *q, int cap)
{
    if (!q || cap <= 0) return -1;
    snprintf(q, cap, "Socratic %u: why does %s behave this way?",
             q_n, topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_worked(const uint32_t *steps, int n, char *out, int cap)
{
    if (!steps || !out || cap <= 0) return -1;
    snprintf(out, cap, "Worked example: step 1 -> %u, step 2 -> %u, ...",
             steps[0], steps[n > 1 ? 1 : 0]);
    return 0;
}

int wubu_tutor_interleave(const uint32_t *topics, int n, uint32_t *order)
{
    if (!topics || !order || n <= 0) return -1;
    /* rotate by 2: a + b + a + c ... interleaving */
    int k = 0;
    for (int i = 0; i < n; i++) order[k++] = topics[i % n];
    return k;
}

int wubu_tutor_elaborate(uint32_t topic, char *out, int cap)
{
    if (!out || cap <= 0) return -1;
    snprintf(out, cap, "Elaboration: %s connects to everything else you know.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_retrieve(uint32_t topic, char *q, int cap)
{
    if (!q || cap <= 0) return -1;
    snprintf(q, cap, "Retrieval: write everything you know about %s.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_dual_code(uint32_t topic, char *visual_hint, int cap)
{
    if (!visual_hint || cap <= 0) return -1;
    snprintf(visual_hint, cap, "Picture %s as a diagram: boxes and arrows.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_goal(uint32_t *goal, uint32_t set, uint32_t target)
{
    if (!goal) return -1;
    if (set) *goal = target; else *goal = 0;
    return 0;
}

int wubu_tutor_streak(uint32_t *streak, uint32_t day, uint32_t yesterday)
{
    if (!streak) return -1;
    if (day == yesterday + 1) (*streak)++;
    else if (day > yesterday + 1) *streak = 1;
    return 0;
}

int wubu_tutor_challenge(uint32_t level, char *c, int cap)
{
    if (!c || cap <= 0) return -1;
    snprintf(c, cap, "Challenge: finish the %s drill under 60s.",
             level > 2 ? "hard" : "easy");
    return 0;
}

int wubu_tutor_badge(uint32_t milestone, char *badge, int cap)
{
    if (!badge || cap <= 0) return -1;
    static const char *names[] = { "first-step", "on-a-roll", "master" };
    snprintf(badge, cap, "badge-%s", names[milestone > 3 ? 2 : (milestone > 1 ? 1 : 0)]);
    return 0;
}

int wubu_tutor_leaderboard(const uint32_t *scores, int n, int *rank)
{
    if (!scores || !rank || n <= 0) return -1;
    int r = 1;
    for (int i = 1; i < n; i++)
        if (scores[i] > scores[0]) r++;
    *rank = r;
    return 0;
}

int wubu_tutor_reminder(uint32_t hours, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Practice reminder: %u hours since your last lesson.",
             hours);
    return 0;
}

int wubu_tutor_break(uint32_t minutes, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "You've earned a %u-minute break -- stretch!",
             minutes);
    return 0;
}

int wubu_tutor_focus(uint32_t minutes, uint32_t *go, char *line, int cap)
{
    if (!go || !line || cap <= 0) return -1;
    *go = minutes >= 25 ? 1 : 0;   /* a pomodoro is 25 minutes */
    if (*go) snprintf(line, cap, "Focus timer on: %u minutes.", minutes);
    return 0;
}

int wubu_tutor_note(const char *chunk, char *note, int cap)
{
    if (!chunk || !note || cap <= 0) return -1;
    snprintf(note, cap, "note: %.40s...", chunk);
    return 0;
}

int wubu_tutor_summarize(const char *text, char *sum, int cap)
{
    if (!text || !sum || cap <= 0) return -1;
    snprintf(sum, cap, "summary: %.40s", text);
    return 0;
}

int wubu_tutor_mindmap(uint32_t topic, char *map, int cap)
{
    if (!map || cap <= 0) return -1;
    snprintf(map, cap, "%s -> {parts, relations, uses}",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_rehearse(uint32_t session, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Rehearsal session %u: repeat, recall, refine.",
             session);
    return 0;
}

int wubu_tutor_self_test(uint32_t topic, char *q, int cap)
{
    return wubu_tutor_quiz(topic, 5, q, cap);
}

int wubu_tutor_teach_back(uint32_t topic, char *prompt, int cap)
{
    if (!prompt || cap <= 0) return -1;
    snprintf(prompt, cap, "Teach %s back to me in your own words.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_plan(uint32_t hours, uint32_t *blocks, int n, char *plan, int cap)
{
    if (!blocks || !plan || n <= 0 || cap <= 0) return -1;
    uint32_t per = hours / (uint32_t)n;
    for (int i = 0; i < n; i++) blocks[i] = per;
    snprintf(plan, cap, "Plan: %u blocks of %u minutes", n, per * 60 / n);
    return 0;
}

int wubu_tutor_review_daily(uint32_t *reviewed, char *line, int cap)
{
    if (!reviewed || !line || cap <= 0) return -1;
    (*reviewed)++;
    snprintf(line, cap, "Daily review %u done.", *reviewed);
    return 0;
}

int wubu_tutor_metric(uint32_t topic, uint32_t *score)
{
    (void)topic;
    if (!score) return -1;
    *score = 60;
    return 0;
}

uint32_t wubu_tutor_time_to_mastery(uint32_t difficulty, uint32_t minutes_day)
{
    if (minutes_day == 0) return 0;
    return (difficulty * 60) / minutes_day;   /* days */
}

int wubu_tutor_checkpoint(uint32_t topic, uint32_t mastery, uint32_t *store, int cap, int *n)
{
    if (!store || !n || *n >= cap) return -1;
    store[*n] = (topic << 8) | (mastery & 0xFF);
    (*n)++;
    return 0;
}

int wubu_tutor_resume(uint32_t topic, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Resuming %s -- right where you left off.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_progress(uint32_t done, uint32_t total, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "%u%% through the %s track",
             total ? (done * 100) / total : 100, topic_names[0]);
    return 0;
}

int wubu_tutor_goal_set(uint32_t topic, uint32_t days, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Goal: master %s in %u days.", topic_names[topic % 5], days);
    return 0;
}

int wubu_tutor_goal_check(uint32_t due, uint32_t *on_track)
{
    if (!on_track) return -1;
    *on_track = due > 0 ? 1 : 0;
    return 0;
}

int wubu_tutor_encourage_stuck(uint32_t attempts, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Attempt %u -- you're learning the hard way, that's real.",
             attempts);
    return 0;
}

int wubu_tutor_praise_small(uint32_t step, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Step %u done -- small wins count.", step);
    return 0;
}

int wubu_tutor_next_topic(const uint32_t *mastery, int n, uint32_t *next)
{
    if (!mastery || !next || n <= 0) return -1;
    uint32_t best = 0;
    for (int i = 1; i < n; i++)
        if (mastery[i] < mastery[best]) best = (uint32_t)i;
    *next = best;
    return 0;
}

int wubu_tutor_recommend(uint32_t topic, uint32_t level, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Next: %s at level %u.", topic_names[topic % 5], level);
    return 0;
}

int wubu_tutor_error_pattern(const int32_t *answers, int n, int32_t *pattern)
{
    if (!answers || !pattern || n <= 0) return -1;
    int32_t most = answers[0], best_count = 0;
    for (int i = 0; i < n; i++) {
        int32_t count = 0;
        for (int j = 0; j < n; j++)
            if (answers[j] == answers[i]) count++;
        if (count > best_count) { best_count = count; most = answers[i]; }
    }
    *pattern = most;
    return 0;
}

int wubu_tutor_common_mistake(uint32_t topic, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Common mistake on %s: mixing up the order.",
             topic_names[topic % 5]);
    return 0;
}

int wubu_tutor_verify_step(uint32_t step, uint32_t answer, char *fb, int cap)
{
    if (!fb || cap <= 0) return -1;
    snprintf(fb, cap, "Step %u: %s", step, answer ? "verified" : "re-check");
    return 0;
}

int wubu_tutor_reward(uint32_t points, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "+%u points!", points);
    return 0;
}

int wubu_tutor_penalty(uint32_t points, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "-%u (streak reset -- no big deal)", points);
    return 0;
}

int wubu_tutor_energy(float mj_per_minute, uint32_t minutes)
{
    return (int)(mj_per_minute * (float)minutes);
}

int wubu_tutor_fuzz_guard(uint32_t level, uint32_t topic)
{
    return level <= 10 && topic <= 100 ? 1 : 0;
}

int wubu_tutor_audit(uint32_t topic, uint32_t *score, uint32_t *attempts)
{
    (void)topic;
    if (score) *score = 0;
    if (attempts) *attempts = 0;
    return 0;
}

int wubu_tutor_export(const uint32_t *scores, int n, char *buf, int cap)
{
    if (!scores || !buf || cap <= 0) return -1;
    snprintf(buf, cap, "tutor-scores:%u", wubu_tutor_mastery(scores, n));
    return 0;
}

int wubu_tutor_reset(uint32_t *store, int cap)
{
    if (!store) return -1;
    memset(store, 0, sizeof(uint32_t) * (size_t)cap);
    return 0;
}
