/*
 * wubu_psych.h -- the human interaction psychology + timing loops (HX-A/B). C11.
 *
 * The doctrine: the AGI and the user share the desktop. The AGI must
 * understand the human's timing -- reaction windows, patience decay,
 * attention, fatigue, mood -- so it knows WHEN to act, WHEN to wait,
 * and WHEN to yield control. When the human takes the mouse/keyboard,
 * the AGI yields: the human is the priority driver.
 *
 * This module is the psychology engine: the user model (profile,
 * preferences, habits, pace, fatigue, mood, context), the timing
 * loops (the human reaction / patience / attention windows), and the
 * control arbitration (who drives the input at any instant).
 */
#ifndef WUBU_PSYCH_H
#define WUBU_PSYCH_H

#include <stdint.h>

/* ---- the user model (HX-A) ---- */

/* The user state: everything the AGI knows about the human. */
typedef struct {
    uint32_t magic;
    /* identity */
    char     name[32];
    /* skill + pace */
    uint32_t skill_level;        /* 0..100 */
    uint32_t pace;               /* clicks/min baseline */
    /* state */
    uint32_t fatigue;            /* 0..100 */
    uint32_t mood;               /* 0..100 */
    uint32_t attention;          /* 0..100 (current window) */
    uint32_t session_minutes;
    /* timing constants */
    uint32_t reaction_ms;        /* the observed reaction time */
    uint32_t patience_ms;        /* how long before the human wants control */
    /* learned */
    uint32_t interaction_count;
    float    avg_task_seconds;
} wubu_psych_user_t;

#define WUBU_PSYCH_MAGIC 0x50535943u  /* "PSYC" */

/* HX-A01..A05: profile, prefs, tastes, habits, patterns. */
int wubu_psych_init(wubu_psych_user_t *u);
int wubu_psych_set_pref(wubu_psych_user_t *u, const char *key, uint32_t val);
int wubu_psych_record_action(wubu_psych_user_t *u, uint32_t action_ms);

/* HX-A06..A10: skill, pace, fatigue, mood, context. */
int wubu_psych_update_skill(wubu_psych_user_t *u, int delta);
int wubu_psych_update_fatigue(wubu_psych_user_t *u, uint32_t work_minutes);
int wubu_psych_update_mood(wubu_psych_user_t *u, int delta);
int wubu_psych_session_tick(wubu_psych_user_t *u, uint32_t minutes);

/* ---- the timing loops (HX-B: adaptive interaction) ---- */

/* The human timing model: the classic perception windows. */
#define PSYCH_MS_PERCEPTION   100   /* the "instant" window */
#define PSYCH_MS_GESTURE      300   /* a gesture / click cycle */
#define PSYCH_MS_THOUGHT      1500  /* a short thought */
#define PSYCH_MS_PATIENCE     5000  /* the default patience window */
#define PSYCH_MS_BOREDOM      15000 /* the attention loss window */

/* HX-B01..B05: adaptive layout/theme/font/density/complexity. */
uint32_t wubu_psych_density(uint32_t fatigue, uint32_t skill);
uint32_t wubu_psych_complexity(uint32_t skill, uint32_t mood);

/* HX-B06..B08: novice/expert + progressive disclosure. */
int wubu_psych_mode(uint32_t skill, uint32_t *mode);   /* 0 novice, 1 expert */
uint32_t wubu_psych_disclose(uint32_t session, uint32_t skill);

/* HX-B09..B12: contextual menus/help/shortcuts/defaults. */
uint32_t wubu_psych_menu_depth(uint32_t skill, uint32_t fatigue);
int wubu_psych_help(uint32_t mode, const char *action, char *tip, int cap);

/* HX-B13..B14: workload + context adaptation. */
uint32_t wubu_psych_workload(uint32_t tasks, uint32_t window_minutes);

/* ---- the patience model (the "human wants control" loop) ---- */

/* The attention state: who drives the input right now. */
typedef struct {
    uint32_t human_driving;      /* 1 = human has the mouse/keyboard */
    uint32_t ai_driving;         /* 1 = the AGI is demonstrating */
    uint64_t last_input_ms;      /* when the human last touched input */
    uint32_t patience_used_ms;   /* the human's accumulated wait */
    uint32_t pending_proposal;   /* the AGI's proposal id */
} wubu_psych_attention_t;

/* HX-B15..B20: the control arbitration -- when the human takes the
 * input, the AGI yields. */
int wubu_psych_input_seen(wubu_psych_attention_t *a, uint64_t now_ms);
int wubu_psych_ai_act(wubu_psych_attention_t *a, uint64_t now_ms);
int wubu_psych_ai_yield(wubu_psych_attention_t *a);
int wubu_psych_may_act(const wubu_psych_attention_t *a, uint64_t now_ms,
                       uint32_t min_wait_ms);
uint32_t wubu_psych_patience_left(const wubu_psych_attention_t *a,
                                  uint64_t now_ms, uint32_t budget_ms);

/* HX-B21..B30: the human timing loops (when the AGI should speak /
 * act / wait -- the social rhythm). */
int wubu_psych_should_speak(const wubu_psych_user_t *u, uint64_t idle_ms);
int wubu_psych_should_assist(const wubu_psych_user_t *u, uint32_t stuck_ms);
int wubu_psych_should_yield(const wubu_psych_user_t *u, uint64_t idle_ms);
uint32_t wubu_psych_wait_before_hint(uint32_t stuck_ms, uint32_t skill);
uint32_t wubu_psych_typing_pause(uint32_t wpm);

/* ---- the psychology (the human response model) ---- */

/* HX-B31..B40: reaction, Fitts, Hick, recognition, memory. */
uint32_t wubu_psych_reaction(uint32_t fatigue, uint32_t attention);
float wubu_psych_fitts(float distance_px, float width_px, float a, float b);
float wubu_psych_hick(uint32_t n_choices);
float wubu_psych_recognition(uint32_t seen, uint32_t total);
uint32_t wubu_psych_short_term(uint32_t items);

/* HX-B41..B50: the cognitive load, the interference, the priming. */
uint32_t wubu_psych_load(uint32_t tasks, uint32_t complexity);
float wubu_psych_prime(float baseline, float prime_strength);

/* HX-B51..B60: the feedback loops (positive/negative/neutral). */
int wubu_psych_feedback(int rc, uint32_t *tone);   /* 0 pos, 1 neg, 2 neut */
uint32_t wubu_psych_praise_strength(uint32_t progress);

/* HX-B61..B70: the preference learning (explicit + implicit). */
float wubu_psych_pref_delta(uint32_t rating, uint32_t old);
int wubu_psych_implicit(const uint32_t *signals, int n, float *score);

/* HX-B71..B80: the drift detection + the privacy. */
int wubu_psych_drift(const uint32_t *history, int n, uint32_t tol);
int wubu_psych_export(const wubu_psych_user_t *u, char *buf, int cap);
int wubu_psych_wipe(wubu_psych_user_t *u);

/* HX-B81..B90: the audit + the tests. */
uint32_t wubu_psych_audit(const wubu_psych_user_t *u);
int wubu_psych_sane(const wubu_psych_user_t *u);

/* HX-B91..B100: the bench + the energy + the fuzz. */
uint64_t wubu_psych_bench(uint32_t iterations);
float wubu_psych_energy(float mj_per_update, uint32_t updates);
int wubu_psych_fuzz_guard(uint32_t mood, uint32_t fatigue);

/* the tandem descriptor: what the AGI shows on the shared desktop. */
typedef struct {
    char     title[48];
    char     body[96];
    uint32_t proposal_id;
    uint32_t urgency;        /* 0 quiet .. 3 interrupt */
} wubu_psych_proposal_t;

/* HX: propose an action to the human (the tandem loop). */
int wubu_psych_propose(wubu_psych_attention_t *a, wubu_psych_proposal_t *p,
                       uint32_t urgency);
int wubu_psych_accept(wubu_psych_attention_t *a, uint32_t proposal_id);
int wubu_psych_decline(wubu_psych_attention_t *a, uint32_t proposal_id);

#endif