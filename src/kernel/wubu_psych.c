/*
 * wubu_psych.c -- the human interaction psychology + timing loops (HX). C11.
 */
#include "wubu_psych.h"
#include <string.h>
#include <stdio.h>

/* ---- the user model ---- */
int wubu_psych_init(wubu_psych_user_t *u)
{
    if (!u) return -1;
    memset(u, 0, sizeof(*u));
    u->magic = WUBU_PSYCH_MAGIC;
    strcpy(u->name, "user");
    u->skill_level = 30;
    u->pace = 40;
    u->reaction_ms = PSYCH_MS_GESTURE;
    u->patience_ms = PSYCH_MS_PATIENCE;
    u->mood = 70;
    u->attention = 80;
    return 0;
}

int wubu_psych_set_pref(wubu_psych_user_t *u, const char *key, uint32_t val)
{
    if (!u || !key) return -1;
    /* the preferences live in the caller's key store; here we just
     * acknowledge + nudge the model (agnostic, data-driven). */
    if (strcmp(key, "pace") == 0) u->pace = val;
    if (strcmp(key, "patience") == 0) u->patience_ms = val;
    return 0;
}

int wubu_psych_record_action(wubu_psych_user_t *u, uint32_t action_ms)
{
    if (!u) return -1;
    float total = u->avg_task_seconds * (float)u->interaction_count;
    u->interaction_count++;
    u->avg_task_seconds = (total + (float)action_ms / 1000.0f) /
                          (float)u->interaction_count;
    /* the reaction time is an EMA toward the observed */
    u->reaction_ms = (u->reaction_ms * 3 + action_ms) / 4;
    return 0;
}

int wubu_psych_update_skill(wubu_psych_user_t *u, int delta)
{
    if (!u) return -1;
    int s = (int)u->skill_level + delta;
    u->skill_level = s < 0 ? 0 : (s > 100 ? 100 : (uint32_t)s);
    return 0;
}

int wubu_psych_update_fatigue(wubu_psych_user_t *u, uint32_t work_minutes)
{
    if (!u) return -1;
    uint32_t f = u->fatigue + work_minutes / 5;
    u->fatigue = f > 100 ? 100 : f;
    return 0;
}

int wubu_psych_update_mood(wubu_psych_user_t *u, int delta)
{
    if (!u) return -1;
    int m = (int)u->mood + delta;
    u->mood = m < 0 ? 0 : (m > 100 ? 100 : (uint32_t)m);
    return 0;
}

int wubu_psych_session_tick(wubu_psych_user_t *u, uint32_t minutes)
{
    if (!u) return -1;
    u->session_minutes += minutes;
    /* attention decays with session length, recovers with mood */
    if (u->attention > 20) u->attention -= minutes / 15;
    return 0;
}

/* ---- the timing loops ---- */
uint32_t wubu_psych_density(uint32_t fatigue, uint32_t skill)
{
    /* high fatigue or low skill -> sparse UI */
    if (fatigue > 70 || skill < 25) return 0;
    if (skill > 70) return 2;
    return 1;
}

uint32_t wubu_psych_complexity(uint32_t skill, uint32_t mood)
{
    if (mood < 30) return 0;             /* sad -> simple */
    return skill > 60 ? 2 : 1;
}

int wubu_psych_mode(uint32_t skill, uint32_t *mode)
{
    if (!mode) return -1;
    *mode = skill >= 60 ? 1 : 0;
    return 0;
}

uint32_t wubu_psych_disclose(uint32_t session, uint32_t skill)
{
    /* progressive disclosure: more advanced controls appear over time */
    uint32_t cap = 3 + session / 20;
    return cap > (5 + skill / 25) ? (5 + skill / 25) : cap;
}

uint32_t wubu_psych_menu_depth(uint32_t skill, uint32_t fatigue)
{
    if (fatigue > 70) return 1;
    return skill > 60 ? 3 : 2;
}

int wubu_psych_help(uint32_t mode, const char *action, char *tip, int cap)
{
    if (!action || !tip || cap <= 0) return -1;
    if (mode == 0)
        snprintf(tip, cap, "Try clicking %s -- it's the big button", action);
    else
        snprintf(tip, cap, "%s: %d ms via the shortcut", action, 300);
    return 0;
}

uint32_t wubu_psych_workload(uint32_t tasks, uint32_t window_minutes)
{
    if (window_minutes == 0) return 0;
    return (tasks * 60) / window_minutes;   /* tasks/hour */
}

/* ---- the control arbitration (the human is priority) ---- */
int wubu_psych_input_seen(wubu_psych_attention_t *a, uint64_t now_ms)
{
    if (!a) return -1;
    a->last_input_ms = now_ms;
    a->human_driving = 1;
    a->ai_driving = 0;
    a->patience_used_ms = 0;
    return 0;
}

int wubu_psych_ai_act(wubu_psych_attention_t *a, uint64_t now_ms)
{
    if (!a) return -1;
    a->ai_driving = 1;
    a->human_driving = 0;
    a->last_input_ms = now_ms;
    return 0;
}

int wubu_psych_ai_yield(wubu_psych_attention_t *a)
{
    if (!a) return -1;
    a->ai_driving = 0;
    a->human_driving = 1;
    return 0;
}

int wubu_psych_may_act(const wubu_psych_attention_t *a, uint64_t now_ms,
                       uint32_t min_wait_ms)
{
    if (!a) return 0;
    /* never act while the human is actively driving */
    if (a->human_driving && !a->ai_driving) return 0;
    if (now_ms < a->last_input_ms + min_wait_ms) return 0;
    return 1;
}

uint32_t wubu_psych_patience_left(const wubu_psych_attention_t *a,
                                  uint64_t now_ms, uint32_t budget_ms)
{
    if (!a) return 0;
    uint64_t waited = now_ms - a->last_input_ms;
    if (waited >= budget_ms) return 0;
    return (uint32_t)(budget_ms - waited);
}

/* the social rhythm: when should the AGI speak / assist / yield */
int wubu_psych_should_speak(const wubu_psych_user_t *u, uint64_t idle_ms)
{
    if (!u) return 0;
    /* after a full thought-window of silence, a gentle ping is welcome */
    return idle_ms > PSYCH_MS_THOUGHT && u->mood >= 40 ? 1 : 0;
}

int wubu_psych_should_assist(const wubu_psych_user_t *u, uint32_t stuck_ms)
{
    if (!u) return 0;
    /* stuck past 3x the user's patience -> offer help (novices sooner) */
    uint32_t th = (u->skill_level < 30) ? PSYCH_MS_PATIENCE
                                        : PSYCH_MS_PATIENCE * 2;
    return stuck_ms > th ? 1 : 0;
}

int wubu_psych_should_yield(const wubu_psych_user_t *u, uint64_t idle_ms)
{
    if (!u) return 0;
    /* the AGI has been talking too long -> yield the floor */
    return idle_ms > (uint64_t)u->patience_ms ? 1 : 0;
}

uint32_t wubu_psych_wait_before_hint(uint32_t stuck_ms, uint32_t skill)
{
    uint32_t grace = (skill < 30) ? 1500 : 3000;
    return stuck_ms > grace ? stuck_ms - grace : 0;
}

uint32_t wubu_psych_typing_pause(uint32_t wpm)
{
    if (wpm == 0) return PSYCH_MS_THOUGHT;
    return 60000 / wpm;   /* ms per word */
}

/* ---- the psychology ---- */
uint32_t wubu_psych_reaction(uint32_t fatigue, uint32_t attention)
{
    /* base 250ms, +fatigue, +low attention */
    uint32_t r = 250 + fatigue * 2 + (100 - attention);
    return r;
}

float wubu_psych_fitts(float distance_px, float width_px, float a, float b)
{
    if (width_px <= 0) return 0;
    float mt = a + b * (float)(1.0 + (double)distance_px / width_px);
    return mt;   /* ms (log-ish via the ratio) */
}

float wubu_psych_hick(uint32_t n_choices)
{
    if (n_choices == 0) return 0;
    return 150.0f + 150.0f * (float)(n_choices > 1 ? 1 : 0);
    /* simplified: the choice count effect is a +b*log2(n) model */
}

float wubu_psych_recognition(uint32_t seen, uint32_t total)
{
    if (total == 0) return 0;
    return (float)seen / (float)total;
}

uint32_t wubu_psych_short_term(uint32_t items)
{
    /* the magic number 7 +/- 2 */
    if (items <= 5) return 7;
    if (items <= 9) return 7;
    return 5;
}

uint32_t wubu_psych_load(uint32_t tasks, uint32_t complexity)
{
    return tasks * (complexity + 1);
}

float wubu_psych_prime(float baseline, float prime_strength)
{
    return baseline * (1.0f + prime_strength);
}

int wubu_psych_feedback(int rc, uint32_t *tone)
{
    if (!tone) return -1;
    if (rc > 0) *tone = 0;       /* positive */
    else if (rc < 0) *tone = 1;  /* negative */
    else *tone = 2;              /* neutral */
    return 0;
}

uint32_t wubu_psych_praise_strength(uint32_t progress)
{
    return progress > 80 ? 3 : (progress > 40 ? 2 : 1);
}

float wubu_psych_pref_delta(uint32_t rating, uint32_t old)
{
    return (float)((int)rating - (int)old) / 100.0f;
}

int wubu_psych_implicit(const uint32_t *signals, int n, float *score)
{
    if (!signals || !score || n <= 0) return -1;
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) sum += signals[i];
    *score = (float)sum / (float)(n * 100);
    return 0;
}

int wubu_psych_drift(const uint32_t *history, int n, uint32_t tol)
{
    if (!history || n < 2) return 0;
    uint32_t first = history[0], last = history[n - 1];
    return (first > last ? first - last : last - first) > tol ? 1 : 0;
}

int wubu_psych_export(const wubu_psych_user_t *u, char *buf, int cap)
{
    if (!u || !buf || cap <= 0) return -1;
    snprintf(buf, cap,
             "user=%s skill=%u pace=%u fatigue=%u mood=%u attention=%u "
             "reaction=%ums patience=%ums",
             u->name, u->skill_level, u->pace, u->fatigue, u->mood,
             u->attention, u->reaction_ms, u->patience_ms);
    return 0;
}

int wubu_psych_wipe(wubu_psych_user_t *u)
{
    if (!u) return -1;
    memset(u, 0, sizeof(*u));
    return 0;
}

uint32_t wubu_psych_audit(const wubu_psych_user_t *u)
{
    if (!u) return 0;
    /* an audit hash over the user state (privacy: never leaks) */
    const uint8_t *p = (const uint8_t *)u;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(*u); i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

int wubu_psych_sane(const wubu_psych_user_t *u)
{
    if (!u) return 0;
    return u->magic == WUBU_PSYCH_MAGIC &&
           u->skill_level <= 100 && u->fatigue <= 100 &&
           u->mood <= 100 && u->attention <= 100;
}

uint64_t wubu_psych_bench(uint32_t iterations)
{
    wubu_psych_user_t u;
    wubu_psych_init(&u);
    uint64_t start = 0;
    (void)start;
    for (uint32_t i = 0; i < iterations; i++)
        wubu_psych_record_action(&u, 300 + (i % 100));
    return u.interaction_count;
}

float wubu_psych_energy(float mj_per_update, uint32_t updates)
{
    return mj_per_update * (float)updates;
}

int wubu_psych_fuzz_guard(uint32_t mood, uint32_t fatigue)
{
    return mood <= 100 && fatigue <= 100 ? 1 : 0;
}

/* ---- the tandem loop ---- */
int wubu_psych_propose(wubu_psych_attention_t *a, wubu_psych_proposal_t *p,
                       uint32_t urgency)
{
    if (!a || !p) return -1;
    a->pending_proposal = p->proposal_id;
    p->urgency = urgency > 3 ? 3 : urgency;
    return 0;
}

int wubu_psych_accept(wubu_psych_attention_t *a, uint32_t proposal_id)
{
    if (!a || a->pending_proposal != proposal_id) return -1;
    a->pending_proposal = 0;
    return 0;
}

int wubu_psych_decline(wubu_psych_attention_t *a, uint32_t proposal_id)
{
    return wubu_psych_accept(a, proposal_id);   /* same clear */
}
