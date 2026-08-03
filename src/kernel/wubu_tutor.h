/*
 * wubu_tutor.h -- the learning & education frontier (HX-C, 100 gaps). C11.
 *
 * The recursive learning loop made human: the AGI and the user learn
 * together on the shared desktop. The AGI tutors (explains, quizzes,
 * corrects, paces), the user teaches it back (corrections, ratings,
 * new facts), and the loop closes -- every interaction updates both
 * models. The tutor respects the human timing: it waits for the
 * thought window, checks in at the patience boundary, and never
 * lectures past the boredom window.
 */
#ifndef WUBU_TUTOR_H
#define WUBU_TUTOR_H

#include <stdint.h>

/* HX-C01..C05: the tutor, the curriculum, the lesson, the exercise. */
int wubu_tutor_lesson(uint32_t topic, uint32_t level, char *lesson, int cap);
int wubu_tutor_exercise(uint32_t topic, uint32_t level, char *ex, int cap);
int wubu_tutor_answer(uint32_t exercise, int32_t user_ans, int32_t correct,
                      char *fb, int cap);
uint32_t wubu_tutor_next_level(uint32_t correct_streak);
int wubu_tutor_curriculum(uint32_t topic, uint32_t *units, int n);

/* HX-C06..C10: the quiz, the test, the review, the spaced repetition. */
int wubu_tutor_quiz(uint32_t topic, uint32_t n, char *q, int cap);
int wubu_tutor_grade(uint32_t correct, uint32_t total);
int wubu_tutor_review(uint32_t topic, char *summary, int cap);
int wubu_tutor_spaced(uint32_t *due_days, uint32_t mastery);
int wubu_tutor_retention(uint32_t days, float decay);

/* HX-C11..C15: the explanation, the example, the analogy, the hint. */
int wubu_tutor_explain(uint32_t topic, uint32_t level, char *text, int cap);
int wubu_tutor_example(uint32_t topic, char *text, int cap);
int wubu_tutor_analogy(uint32_t topic, char *text, int cap);
int wubu_tutor_hint(uint32_t n, char *text, int cap);
int wubu_tutor_scaffold(uint32_t level, uint32_t *steps, int n);

/* HX-C16..C20: the feedback, the correction, the praise, the pacing. */
int wubu_tutor_feedback(int32_t user_ans, int32_t correct, char *fb, int cap);
int wubu_tutor_correct(int32_t user_ans, int32_t correct, char *fb, int cap);
uint32_t wubu_tutor_praise(uint32_t streak);
uint32_t wubu_tutor_pace(uint32_t skill, uint32_t fatigue);
int wubu_tutor_wait(uint32_t level, uint32_t *wait_ms);

/* HX-C21..C30: the learning model (mastery, forgetting, transfer). */
uint32_t wubu_tutor_mastery(const uint32_t *scores, int n);
float wubu_tutor_forget(uint32_t days, float half_life);
float wubu_tutor_transfer(float a, float b);
int wubu_tutor_prereq(uint32_t topic, const uint32_t *prereqs, int n, uint32_t *ok);
uint32_t wubu_tutor_difficulty(uint32_t topic, uint32_t level);

/* HX-C31..C40: the pedagogy (Socratic, worked examples, interleaving). */
int wubu_tutor_socratic(uint32_t topic, uint32_t q_n, char *q, int cap);
int wubu_tutor_worked(const uint32_t *steps, int n, char *out, int cap);
int wubu_tutor_interleave(const uint32_t *topics, int n, uint32_t *order);
int wubu_tutor_elaborate(uint32_t topic, char *out, int cap);
int wubu_tutor_retrieve(uint32_t topic, char *q, int cap);
int wubu_tutor_dual_code(uint32_t topic, char *visual_hint, int cap);

/* HX-C41..C50: the motivation (goals, streaks, challenges, badges). */
int wubu_tutor_goal(uint32_t *goal, uint32_t set, uint32_t target);
int wubu_tutor_streak(uint32_t *streak, uint32_t day, uint32_t yesterday);
int wubu_tutor_challenge(uint32_t level, char *c, int cap);
int wubu_tutor_badge(uint32_t milestone, char *badge, int cap);
int wubu_tutor_leaderboard(const uint32_t *scores, int n, int *rank);
int wubu_tutor_reminder(uint32_t hours, char *line, int cap);
int wubu_tutor_break(uint32_t minutes, char *line, int cap);

/* HX-C51..C60: the study skills (focus, note, summarize, mindmap). */
int wubu_tutor_focus(uint32_t minutes, uint32_t *go, char *line, int cap);
int wubu_tutor_note(const char *chunk, char *note, int cap);
int wubu_tutor_summarize(const char *text, char *sum, int cap);
int wubu_tutor_mindmap(uint32_t topic, char *map, int cap);
int wubu_tutor_rehearse(uint32_t session, char *line, int cap);
int wubu_tutor_self_test(uint32_t topic, char *q, int cap);
int wubu_tutor_teach_back(uint32_t topic, char *prompt, int cap);
int wubu_tutor_plan(uint32_t hours, uint32_t *blocks, int n, char *plan, int cap);
int wubu_tutor_review_daily(uint32_t *reviewed, char *line, int cap);
int wubu_tutor_metric(uint32_t topic, uint32_t *score);

/* HX-C61..C100: the engineering close. */
uint32_t wubu_tutor_time_to_mastery(uint32_t difficulty, uint32_t minutes_day);
int wubu_tutor_checkpoint(uint32_t topic, uint32_t mastery, uint32_t *store, int cap, int *n);
int wubu_tutor_resume(uint32_t topic, char *line, int cap);
int wubu_tutor_progress(uint32_t done, uint32_t total, char *line, int cap);
int wubu_tutor_goal_set(uint32_t topic, uint32_t days, char *line, int cap);
int wubu_tutor_goal_check(uint32_t due, uint32_t *on_track);
int wubu_tutor_encourage_stuck(uint32_t attempts, char *line, int cap);
int wubu_tutor_praise_small(uint32_t step, char *line, int cap);
int wubu_tutor_next_topic(const uint32_t *mastery, int n, uint32_t *next);
int wubu_tutor_recommend(uint32_t topic, uint32_t level, char *line, int cap);
int wubu_tutor_error_pattern(const int32_t *answers, int n, int32_t *pattern);
int wubu_tutor_common_mistake(uint32_t topic, char *line, int cap);
int wubu_tutor_verify_step(uint32_t step, uint32_t answer, char *fb, int cap);
int wubu_tutor_reward(uint32_t points, char *line, int cap);
int wubu_tutor_penalty(uint32_t points, char *line, int cap);
int wubu_tutor_energy(float mj_per_minute, uint32_t minutes);
int wubu_tutor_fuzz_guard(uint32_t level, uint32_t topic);
int wubu_tutor_audit(uint32_t topic, uint32_t *score, uint32_t *attempts);
int wubu_tutor_export(const uint32_t *scores, int n, char *buf, int cap);
int wubu_tutor_reset(uint32_t *store, int cap);

#endif