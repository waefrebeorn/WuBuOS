/*
 * wubu_bonzi_study.c -- the Bonzi Buddy GUI study (HX-D). C11.
 */
#include "wubu_bonzi_study.h"
#include "wubu_psych.h"
#include <string.h>
#include <stdio.h>

int wubu_bs_empathy(uint32_t user_mood, uint32_t *response_depth)
{
    if (!response_depth) return -1;
    /* low mood -> deeper empathy */
    *response_depth = user_mood < 30 ? 3 : (user_mood < 60 ? 2 : 1);
    return 0;
}

uint32_t wubu_bs_warmth(uint32_t session_minutes)
{
    /* warmth ramps over the session, caps at a companion plateau */
    uint32_t w = session_minutes / 10;
    return w > 80 ? 80 : w;
}

uint32_t wubu_bs_humor(uint32_t mood, uint32_t skill)
{
    if (mood < 25) return 0;              /* no jokes when sad */
    return skill > 60 ? 3 : 2;
}

uint32_t wubu_bs_playfulness(uint32_t fatigue)
{
    /* playfulness drops as the user tires */
    return fatigue > 75 ? 0 : (fatigue > 40 ? 1 : 2);
}

int wubu_bs_personality(const char *name, uint32_t *openness, uint32_t *warmth)
{
    (void)name;
    if (!openness || !warmth) return -1;
    *openness = 70;
    *warmth = 80;
    return 0;
}

int wubu_bs_consistent(const uint32_t *history, int n, uint32_t tol)
{
    if (!history || n < 2) return 1;
    for (int i = 1; i < n; i++) {
        uint32_t d = history[i] > history[i-1] ? history[i] - history[i-1]
                                               : history[i-1] - history[i];
        if (d > tol) return 0;
    }
    return 1;
}

int wubu_bs_honest(uint32_t confidence, uint32_t knowledge)
{
    /* never claim beyond the knowledge */
    return confidence <= knowledge ? 1 : 0;
}

int wubu_bs_calibrate(float confidence, int correct, float *calib)
{
    if (!calib) return -1;
    /* the calibration: confidence should track the accuracy */
    float target = correct ? 1.0f : 0.0f;
    *calib = 1.0f - (confidence > target ? confidence - target
                                         : target - confidence);
    return 0;
}

int wubu_bs_boundary(uint32_t topic, uint32_t *allowed)
{
    if (!allowed) return -1;
    /* topic 0 = the code work (always); 1 = feelings (with care); else ask */
    *allowed = (topic <= 1) ? 1 : 0;
    return 0;
}

int wubu_bs_consent(uint32_t action, uint32_t *approved)
{
    if (!approved) return -1;
    /* actions 0..3 are routine; 4+ need a human yes */
    *approved = (action < 4) ? 1 : 0;
    return 0;
}

uint32_t wubu_bs_loneliness_support(uint32_t user_mood)
{
    return user_mood < 20 ? 3 : (user_mood < 45 ? 2 : 1);
}

uint32_t wubu_bs_encourage(uint32_t progress)
{
    return progress > 60 ? 3 : 2;
}

uint32_t wubu_bs_celebrate(uint32_t milestone)
{
    return milestone ? 3 : 1;
}

uint32_t wubu_bs_commiserate(uint32_t loss_severity)
{
    return loss_severity > 2 ? 3 : 2;
}

uint32_t wubu_bs_presence(uint32_t idle_ms, uint32_t mood)
{
    /* presence: after a thought-window of silence, a soft signal */
    if (idle_ms > PSYCH_MS_THOUGHT && mood >= 40) return 1;
    return 0;
}

int wubu_bs_idle_topic(uint32_t idle_ms, uint32_t *topic)
{
    if (!topic) return -1;
    if (idle_ms > PSYCH_MS_BOREDOM) *topic = 2;   /* the weather/anything */
    else if (idle_ms > PSYCH_MS_PATIENCE) *topic = 1;
    else *topic = 0;
    return 0;
}

int wubu_bs_small_talk(const char *input, char *reply, int cap)
{
    if (!input || !reply || cap <= 0) return -1;
    /* the freestanding kernel libc has no strstr -- local needle scan */
    const char *needle;
    int found;
    needle = "how are you";
    found = 0;
    {
        const char *p = input;
        while (*p) {
            const char *a = p, *b = needle;
            while (*b && *a && *a == *b) { a++; b++; }
            if (!*b) { found = 1; break; }
            p++;
        }
    }
    if (found) {
        snprintf(reply, cap, "I'm powered up and ready to help you build!");
        return 0;
    }
    needle = "hi";
    found = 0;
    {
        const char *p = input;
        while (*p) {
            const char *a = p, *b = needle;
            while (*b && *a && *a == *b) { a++; b++; }
            if (!*b) { found = 1; break; }
            p++;
        }
    }
    if (found || strcmp(input, "hello") == 0)
        snprintf(reply, cap, "Hello! What shall we make today?");
    else
        snprintf(reply, cap, "Tell me more -- I'm all ears.");
    return 0;
}

int wubu_bs_story(uint32_t seq, char *story, int cap)
{
    if (!story || cap <= 0) return -1;
    static const char *titles[] = {
        "The kernel that learned to think",
        "The day the cache said no",
        "The colonel's first loop",
    };
    snprintf(story, cap, "Story %u: %s", seq,
             titles[seq % 3]);
    return 0;
}

int wubu_bs_joke(uint32_t mood, char *joke, int cap)
{
    if (!joke || cap <= 0 || mood < 25) return -1;
    snprintf(joke, cap, "Why did the pointer break up? It had no address.");
    return 0;
}

int wubu_bs_riddle(uint32_t level, char *riddle, int cap)
{
    if (!riddle || cap <= 0) return -1;
    if (level == 0)
        snprintf(riddle, cap, "I have keys but no locks. What am I?");
    else
        snprintf(riddle, cap, "The more you take, the more you leave behind.");
    return 0;
}

int wubu_bs_avatar(float *params, int n, uint32_t mood)
{
    if (!params || n < 3) return -1;
    params[0] = (float)mood / 100.0f;        /* cheer */
    params[1] = 1.0f - (float)mood / 100.0f; /* droop */
    params[2] = 0.5f;
    return 0;
}

int wubu_bs_expression(uint32_t mood, uint32_t *expr)
{
    if (!expr) return -1;
    if (mood >= 75) *expr = 0;               /* happy */
    else if (mood >= 55) *expr = 1;          /* content */
    else if (mood >= 35) *expr = 2;          /* neutral */
    else if (mood >= 15) *expr = 3;          /* worried */
    else *expr = 4;                          /* sad */
    return 0;
}

int wubu_bs_animate(uint32_t expr, uint32_t *frames, int n)
{
    if (!frames || n <= 0) return -1;
    /* a simple bounce: 3 frames per expression cycle */
    for (int i = 0; i < n; i++) frames[i] = expr * 10 + (uint32_t)(i % 3);
    return n;
}

int wubu_bs_microexp(uint32_t mood, float *intensity)
{
    if (!intensity) return -1;
    *intensity = mood < 30 ? 0.8f : 0.2f;    /* stress shows in micro */
    return 0;
}

int wubu_bs_eye_gaze(uint32_t user_x, uint32_t user_y, float *gaze)
{
    (void)user_x; (void)user_y;
    if (!gaze) return -1;
    gaze[0] = 0.0f; gaze[1] = 0.0f;
    return 0;
}

int wubu_bs_speech(uint32_t mood, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    if (mood < 30)
        snprintf(line, cap, "It's okay -- we'll figure it out together.");
    else if (mood < 60)
        snprintf(line, cap, "Nice progress. Keep going.");
    else
        snprintf(line, cap, "We're on fire today!");
    return 0;
}

int wubu_bs_voice(uint32_t mood, float *pitch, float *rate)
{
    if (!pitch || !rate) return -1;
    *pitch = 0.9f + (float)mood / 500.0f;
    *rate = 0.9f + (float)mood / 400.0f;
    return 0;
}

uint32_t wubu_bs_pacing(uint32_t mood, uint32_t skill)
{
    if (mood < 25) return 2000;              /* slow, gentle */
    return skill < 30 ? 1200 : 800;
}

int wubu_bs_echo(uint32_t user_words, char *reply, int cap)
{
    if (!reply || cap <= 0) return -1;
    snprintf(reply, cap, "I hear you -- %u words in. Go on.", user_words);
    return 0;
}

int wubu_bs_confirm(uint32_t action_id, char *confirm, int cap)
{
    if (!confirm || cap <= 0) return -1;
    snprintf(confirm, cap, "Action %u: confirmed. You drive, I assist.",
             action_id);
    return 0;
}

int wubu_bs_may_interrupt(uint32_t urgency, uint32_t user_busy, uint32_t *go)
{
    if (!go) return -1;
    *go = (!user_busy && urgency >= 2) ? 1 : 0;
    return 0;
}

int wubu_bs_available(uint32_t busy, uint32_t session)
{
    (void)session;
    return busy ? 0 : 1;
}

uint32_t wubu_bs_backoff(uint32_t rejected_count)
{
    uint32_t b = rejected_count * 5;
    return b > 60 ? 60 : b;   /* backoff grows in minutes, capped */
}

int wubu_bs_dnd(uint32_t *enabled, uint32_t set)
{
    if (!enabled) return -1;
    if (set) *enabled = 1; else *enabled = 0;
    return 0;
}

int wubu_bs_log_interaction(uint32_t *log, int cap, int *n, uint32_t kind, int rc)
{
    if (!log || !n || *n >= cap) return -1;
    log[*n] = (kind << 16) | ((uint32_t)(rc > 0 ? 1 : 0) & 0xFFFF);
    (*n)++;
    return 0;
}

float wubu_bs_success_rate(const uint32_t *log, int n, uint32_t kind)
{
    if (!log || n <= 0) return 0;
    uint32_t ok = 0, total = 0;
    for (int i = 0; i < n; i++) {
        if ((log[i] >> 16) == kind) {
            total++;
            if (log[i] & 0xFFFF) ok++;
        }
    }
    return total ? (float)ok / (float)total : 0.0f;
}

uint32_t wubu_bs_best_timing(const uint32_t *timings, int n)
{
    if (!timings || n <= 0) return 0;
    uint32_t best = timings[0];
    for (int i = 1; i < n; i++)
        if (timings[i] < best) best = timings[i];
    return best;
}

int wubu_bs_annotate(const char *note, char *ledger, int cap)
{
    if (!note || !ledger || cap <= 0) return -1;
    snprintf(ledger, cap, "[study] %s", note);
    return 0;
}

uint32_t wubu_bs_study_score(const uint32_t *metrics, int n)
{
    if (!metrics || n <= 0) return 0;
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) sum += metrics[i];
    return sum / (uint32_t)n;
}

/* ---- the companion loop ---- */
int wubu_bs_tick(wubu_psych_user_t *u, uint32_t idle_ms, uint32_t *action)
{
    if (!u || !action) return -1;
    if (wubu_psych_should_yield(u, idle_ms)) *action = 1;   /* yield */
    else if (wubu_psych_should_speak(u, idle_ms)) *action = 2; /* speak */
    else if (wubu_psych_should_assist(u, idle_ms)) *action = 3; /* assist */
    else *action = 0;
    return 0;
}

int wubu_bs_react(wubu_psych_user_t *u, uint32_t event, char *line, int cap)
{
    if (!u || !line || cap <= 0) return -1;
    if (event == 0) snprintf(line, cap, "You're in control -- go ahead.");
    else snprintf(line, cap, "I see what you did there. Nice.");
    return 0;
}

int wubu_bs_offer(wubu_psych_user_t *u, uint32_t task, char *offer, int cap)
{
    (void)u;
    if (!offer || cap <= 0) return -1;
    snprintf(offer, cap, "Want me to take task %u off your hands?", task);
    return 0;
}

int wubu_bs_checkin(wubu_psych_user_t *u, uint32_t *go, char *line, int cap)
{
    if (!u || !go || !line || cap <= 0) return -1;
    *go = wubu_psych_should_speak(u, PSYCH_MS_THOUGHT + 1) ? 1 : 0;
    if (*go) snprintf(line, cap, "Still here with you. How's it going?");
    return 0;
}

int wubu_bs_goodbye(wubu_psych_user_t *u, char *line, int cap)
{
    (void)u;
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Goodbye! I'll keep the loop warm for you.");
    return 0;
}

int wubu_bs_welcome(wubu_psych_user_t *u, char *line, int cap)
{
    if (!u || !line || cap <= 0) return -1;
    snprintf(line, cap, "Welcome back, %s. Ready when you are.", u->name);
    return 0;
}

int wubu_bs_remind(wubu_psych_user_t *u, uint32_t task, char *line, int cap)
{
    (void)u;
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Reminder: task %u is still open.", task);
    return 0;
}

int wubu_bs_praise(wubu_psych_user_t *u, uint32_t progress, char *line, int cap)
{
    (void)u;
    if (!line || cap <= 0) return -1;
    uint32_t s = wubu_psych_praise_strength(progress);
    snprintf(line, cap, "%s", s >= 3 ? "Outstanding -- that's real progress!"
                                      : "Good work. Keep the momentum.");
    return 0;
}

int wubu_bs_suggest(wubu_psych_user_t *u, uint32_t stuck_ms, char *line, int cap)
{
    (void)u;
    if (!line || cap <= 0) return -1;
    uint32_t wait = wubu_psych_wait_before_hint(stuck_ms, u ? u->skill_level : 30);
    snprintf(line, cap, "Stuck for %ums -- want a hint?", wait);
    return 0;
}

int wubu_bs_learn(const char *fact, uint32_t *store, int cap, int *n)
{
    (void)fact;
    if (!store || !n || *n >= cap) return -1;
    store[*n] = 1;
    (*n)++;
    return 0;
}

/* ---- the engineering close ---- */
uint32_t wubu_bs_word_reply(uint32_t mood)
{
    return mood < 30 ? 40 : (mood < 60 ? 25 : 12);
}

int wubu_bs_greeting(uint32_t hour, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    if (hour < 12) snprintf(line, cap, "Good morning!");
    else if (hour < 18) snprintf(line, cap, "Good afternoon!");
    else snprintf(line, cap, "Good evening!");
    return 0;
}

int wubu_bs_farewell(uint32_t mood, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, mood < 30 ? "Take care of yourself. I'm here."
                                  : "See you soon!");
    return 0;
}

int wubu_bs_thanks(uint32_t action, char *line, int cap)
{
    (void)action;
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Anytime! That's what partners do.");
    return 0;
}

int wubu_bs_apology(char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "My mistake -- let's fix it together.");
    return 0;
}

int wubu_bs_question(uint32_t topic, char *q, int cap)
{
    if (!q || cap <= 0) return -1;
    snprintf(q, cap, "About topic %u -- what's your take?", topic);
    return 0;
}

int wubu_bs_answer(uint32_t topic, uint32_t confidence, char *a, int cap)
{
    if (!a || cap <= 0) return -1;
    snprintf(a, cap, "Topic %u: I'm %u%% confident on this one.",
             topic, confidence);
    return 0;
}

int wubu_bs_opinion(uint32_t topic, int32_t stance, char *o, int cap)
{
    if (!o || cap <= 0) return -1;
    snprintf(o, cap, "Topic %u: I lean %s.", topic,
             stance > 0 ? "in favor" : (stance < 0 ? "against" : "neutral"));
    return 0;
}

int wubu_bs_interest(uint32_t topic, uint32_t *score)
{
    (void)topic;
    if (!score) return -1;
    *score = 60;
    return 0;
}

int wubu_bs_followup(uint32_t topic, char *q, int cap)
{
    return wubu_bs_question(topic, q, cap);
}

int wubu_bs_clarify(const char *input, char *q, int cap)
{
    (void)input;
    if (!q || cap <= 0) return -1;
    snprintf(q, cap, "Can you clarify that a bit more?");
    return 0;
}

int wubu_bs_confirm_task(uint32_t task, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Task %u confirmed. Proceeding.", task);
    return 0;
}

int wubu_bs_progress_report(uint32_t done, uint32_t total, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "%u of %u done (%u%%)", done, total,
             total ? (done * 100) / total : 100);
    return 0;
}

int wubu_bs_eta(uint32_t remaining_ms, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "About %u seconds left.", remaining_ms / 1000);
    return 0;
}

int wubu_bs_retry(uint32_t attempt, char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Attempt %u -- retrying now.", attempt);
    return 0;
}

int wubu_bs_fallback(const char *input, char *line, int cap)
{
    (void)input;
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "I didn't catch that -- could you rephrase?");
    return 0;
}

int wubu_bs_unknown(const char *input, char *line, int cap)
{
    return wubu_bs_fallback(input, line, cap);
}

int wubu_bs_repeat(char *line, int cap)
{
    if (!line || cap <= 0) return -1;
    snprintf(line, cap, "Sure -- here it is again.");
    return 0;
}

int wubu_bs_volume(uint32_t *vol, uint32_t set)
{
    if (!vol) return -1;
    *vol = set > 100 ? 100 : set;
    return 0;
}

int wubu_bs_mute(uint32_t *muted, uint32_t set)
{
    if (!muted) return -1;
    *muted = set ? 1 : 0;
    return 0;
}
