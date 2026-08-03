/*
 * wubu_bonzi_study.h -- the Bonzi Buddy GUI study (HX-D, 100 gaps). C11.
 *
 * The study: the classic companion desktop buddy -- presence, emotion,
 * reaction, personality. The AGI's desktop persona must be human-timed:
 * it reacts within the perception window, waits within the patience
 * window, and yields when the human takes the controls. This module is
 * the companion psychology + the study metrics (what works, what
 * doesn't, measured -- the prestige ledger for the GUI persona).
 */
#ifndef WUBU_BONZI_STUDY_H
#define WUBU_BONZI_STUDY_H

#include <stdint.h>
#include "wubu_psych.h"   /* the companion loop drives the user model */

/* HX-D01..D05: empathy, warmth, humor, playfulness, personality. */
int wubu_bs_empathy(uint32_t user_mood, uint32_t *response_depth);
uint32_t wubu_bs_warmth(uint32_t session_minutes);
uint32_t wubu_bs_humor(uint32_t mood, uint32_t skill);
uint32_t wubu_bs_playfulness(uint32_t fatigue);
int wubu_bs_personality(const char *name, uint32_t *openness, uint32_t *warmth);

/* HX-D06..D10: consistency, honesty, calibration, boundaries, consent. */
int wubu_bs_consistent(const uint32_t *history, int n, uint32_t tol);
int wubu_bs_honest(uint32_t confidence, uint32_t knowledge);
int wubu_bs_calibrate(float confidence, int correct, float *calib);
int wubu_bs_boundary(uint32_t topic, uint32_t *allowed);
int wubu_bs_consent(uint32_t action, uint32_t *approved);

/* HX-D11..D15: loneliness, encouragement, celebration, commiseration, presence. */
uint32_t wubu_bs_loneliness_support(uint32_t user_mood);
uint32_t wubu_bs_encourage(uint32_t progress);
uint32_t wubu_bs_celebrate(uint32_t milestone);
uint32_t wubu_bs_commiserate(uint32_t loss_severity);
uint32_t wubu_bs_presence(uint32_t idle_ms, uint32_t mood);

/* HX-D16..D20: idle chat, small talk, stories, jokes, riddles. */
int wubu_bs_idle_topic(uint32_t idle_ms, uint32_t *topic);
int wubu_bs_small_talk(const char *input, char *reply, int cap);
int wubu_bs_story(uint32_t seq, char *story, int cap);
int wubu_bs_joke(uint32_t mood, char *joke, int cap);
int wubu_bs_riddle(uint32_t level, char *riddle, int cap);

/* HX-D21..D25: the avatar, the expressions, the animations. */
int wubu_bs_avatar(float *params, int n, uint32_t mood);
int wubu_bs_expression(uint32_t mood, uint32_t *expr);   /* 0 happy..4 sad */
int wubu_bs_animate(uint32_t expr, uint32_t *frames, int n);
int wubu_bs_microexp(uint32_t mood, float *intensity);
int wubu_bs_eye_gaze(uint32_t user_x, uint32_t user_y, float *gaze);

/* HX-D26..D30: the speech, the voice, the pacing. */
int wubu_bs_speech(uint32_t mood, char *line, int cap);
int wubu_bs_voice(uint32_t mood, float *pitch, float *rate);
uint32_t wubu_bs_pacing(uint32_t mood, uint32_t skill);
int wubu_bs_echo(uint32_t user_words, char *reply, int cap);
int wubu_bs_confirm(uint32_t action_id, char *confirm, int cap);

/* HX-D31..D35: the interruptions, the availability. */
int wubu_bs_may_interrupt(uint32_t urgency, uint32_t user_busy, uint32_t *go);
int wubu_bs_available(uint32_t busy, uint32_t session);
uint32_t wubu_bs_backoff(uint32_t rejected_count);
int wubu_bs_dnd(uint32_t *enabled, uint32_t set);

/* HX-D36..D40: the study metrics (the prestige ledger). */
int wubu_bs_log_interaction(uint32_t *log, int cap, int *n, uint32_t kind, int rc);
float wubu_bs_success_rate(const uint32_t *log, int n, uint32_t kind);
uint32_t wubu_bs_best_timing(const uint32_t *timings, int n);
int wubu_bs_annotate(const char *note, char *ledger, int cap);
uint32_t wubu_bs_study_score(const uint32_t *metrics, int n);

/* HX-D41..D60: the companion loop (the tandem with the user). */
int wubu_bs_tick(wubu_psych_user_t *u, uint32_t idle_ms, uint32_t *action);
int wubu_bs_react(wubu_psych_user_t *u, uint32_t event, char *line, int cap);
int wubu_bs_offer(wubu_psych_user_t *u, uint32_t task, char *offer, int cap);
int wubu_bs_checkin(wubu_psych_user_t *u, uint32_t *go, char *line, int cap);
int wubu_bs_goodbye(wubu_psych_user_t *u, char *line, int cap);
int wubu_bs_welcome(wubu_psych_user_t *u, char *line, int cap);
int wubu_bs_remind(wubu_psych_user_t *u, uint32_t task, char *line, int cap);
int wubu_bs_praise(wubu_psych_user_t *u, uint32_t progress, char *line, int cap);
int wubu_bs_suggest(wubu_psych_user_t *u, uint32_t stuck_ms, char *line, int cap);
int wubu_bs_learn(const char *fact, uint32_t *store, int cap, int *n);

/* HX-D61..D100: the engineering close (the study's full surface). */
uint32_t wubu_bs_word_reply(uint32_t mood);
int wubu_bs_greeting(uint32_t hour, char *line, int cap);
int wubu_bs_farewell(uint32_t mood, char *line, int cap);
int wubu_bs_thanks(uint32_t action, char *line, int cap);
int wubu_bs_apology(char *line, int cap);
int wubu_bs_question(uint32_t topic, char *q, int cap);
int wubu_bs_answer(uint32_t topic, uint32_t confidence, char *a, int cap);
int wubu_bs_opinion(uint32_t topic, int32_t stance, char *o, int cap);
int wubu_bs_interest(uint32_t topic, uint32_t *score);
int wubu_bs_followup(uint32_t topic, char *q, int cap);
int wubu_bs_clarify(const char *input, char *q, int cap);
int wubu_bs_confirm_task(uint32_t task, char *line, int cap);
int wubu_bs_progress_report(uint32_t done, uint32_t total, char *line, int cap);
int wubu_bs_eta(uint32_t remaining_ms, char *line, int cap);
int wubu_bs_retry(uint32_t attempt, char *line, int cap);
int wubu_bs_fallback(const char *input, char *line, int cap);
int wubu_bs_unknown(const char *input, char *line, int cap);
int wubu_bs_repeat(char *line, int cap);
int wubu_bs_volume(uint32_t *vol, uint32_t set);
int wubu_bs_mute(uint32_t *muted, uint32_t set);

#endif