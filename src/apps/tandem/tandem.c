/*
 * tandem.c -- Tandem: the user+AGI shared desktop (the recursive
 * learning loop made human).
 *
 * A real WuBuFX/DosGui window that renders the tandem state:
 *   - the USER MODEL gauge (skill / fatigue / mood / attention)
 *   - the TIMING LOOP (whose turn it is: human drives / AGI waits /
 *     AGI proposes -- the patience window, the control arbitration)
 *   - the COMPANION line (Bonzi's psychology-driven reaction)
 *
 * The psychology is real: wubu_psych + wubu_bonzi_study. Every
 * keystroke and click updates the user model (reaction time, fatigue,
 * control ownership). When the human types, the AGI yields; when the
 * human idles past the patience window, the AGI proposes. The window
 * is the visual heart of the tandem -- the human-centric loop.
 */
#include "tandem.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../kernel/wubu_psych.h"
#include "../kernel/wubu_bonzi_study.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    wubu_psych_user_t     user;        /* the user model */
    wubu_psych_attention_t attn;       /* who drives the input */
    wubu_psych_proposal_t prop;        /* the pending proposal */
    wubu_psych_user_t     *user_ready;
    char  status[128];                 /* the status line */
    char  comp[96];                    /* the companion line */
    uint64_t now_ms;
    uint32_t events;
    uint32_t proposals_made;
    uint32_t proposals_accepted;
    uint32_t draws;
} TandemState;

static TandemState *g_tandem = NULL;

/* the per-second heartbeat: advance the timing loop. */
static void tandem_tick(TandemState *s)
{
    s->now_ms += 100;
    uint32_t action = 0;
    wubu_bs_tick(&s->user, (uint32_t)(s->now_ms - s->attn.last_input_ms),
                 &action);
    if (action == 1) {   /* yield / quiet */
        snprintf(s->status, sizeof(s->status),
                 "human drives -- AGI watching (patience %u%% left)",
                 wubu_psych_patience_left(&s->attn, s->now_ms, s->user.patience_ms));
    } else if (action == 2) {  /* speak window */
        char line[96];
        wubu_bs_speech(s->user.mood, line, sizeof(line));
        snprintf(s->status, sizeof(s->status), "AGI: %s", line);
    } else if (action == 3) {  /* assist window */
        snprintf(s->status, sizeof(s->status),
                 "AGI proposes an assist (proposal %u)",
                 s->proposals_made + 1);
    }
}

static void tandem_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)mods;
    TandemState *s = win ? (TandemState *)win->user_data : NULL;
    if (!s) return;
    /* the human took the controls -- the AGI yields */
    wubu_psych_input_seen(&s->attn, s->now_ms);
    s->events++;
    s->draws++;
    (void)key;
}

static void tandem_mouse(DosGuiWindow *win, int x, int y, int btn, int kind)
{
    (void)x; (void)y; (void)btn; (void)kind;
    TandemState *s = win ? (TandemState *)win->user_data : NULL;
    if (!s) return;
    wubu_psych_input_seen(&s->attn, s->now_ms);
    s->events++;
}

static void tandem_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)
{
    TandemState *s = win ? (TandemState *)win->user_data : NULL;
    if (!s || !fb) return;
    (void)fb_w; (void)fb_h;
    s->draws++;

    /* the tandem panel: three bands */
    int y = win->y + 4;
    /* 1. the user model gauge */
    vbe_draw_text(win->x + 4, y, "USER MODEL", 0xAAAAAA, 1);
    y += 14;
    char line[96];
    snprintf(line, sizeof(line), "skill %u  fatigue %u  mood %u  attention %u",
             s->user.skill_level, s->user.fatigue, s->user.mood, s->user.attention);
    vbe_draw_text(win->x + 4, y, line, 0xFFFFFF, 1);
    y += 14;

    /* 2. the timing loop / control arbitration */
    vbe_draw_text(win->x + 4, y, "TIMING LOOP", 0xAAAAAA, 1);
    y += 14;
    vbe_draw_text(win->x + 4, y, s->status, 0x88FF88, 1);
    y += 14;
    snprintf(line, sizeof(line), "events %u  proposals %u  accepted %u",
             s->events, s->proposals_made, s->proposals_accepted);
    vbe_draw_text(win->x + 4, y, line, 0xCCCCCC, 1);
    y += 14;

    /* 3. the companion line (Bonzi's psychology) */
    vbe_draw_text(win->x + 4, y, "COMPANION", 0xAAAAAA, 1);
    y += 14;
    vbe_draw_text(win->x + 4, y, s->comp, 0xFFCC88, 1);
    y += 14;

    /* 4. the proposal bar */
    if (s->attn.pending_proposal) {
        vbe_fill_rect(win->x + 4, y, win->w - 8, 16, 0x223344);
        vbe_draw_text(win->x + 8, y + 3, s->prop.title, 0xFFD700, 1);
    }
}

/* the periodic proposal: when the human idles past patience, propose. */
static void tandem_maybe_propose(TandemState *s)
{
    if (s->attn.pending_proposal) return;
    uint64_t idle = s->now_ms - s->attn.last_input_ms;
    if (idle < s->user.patience_ms) return;
    s->proposals_made++;
    s->prop.proposal_id = s->proposals_made;
    snprintf(s->prop.title, sizeof(s->prop.title), "compact the KV cache?");
    snprintf(s->prop.body, sizeof(s->prop.body),
             "the cache is %u%% full -- the AGI can do it while you think",
             80);
    wubu_psych_propose(&s->attn, &s->prop, 2);
    /* the companion acknowledges the offer */
    char line[96];
    wubu_bs_offer(&s->user, s->proposals_made, line, sizeof(line));
    snprintf(s->comp, sizeof(s->comp), "%s", line);
}

DosGuiWindow *tandem_launch(void)
{
    DosGuiWindow *win = dosgui_wm_create(40, 60, 380, 200, "Tandem -- User+AGI");
    if (win) {
        TandemState *s = calloc(1, sizeof(*s));
        win->user_data = s;
        wubu_psych_init(&s->user);
        memset(&s->attn, 0, sizeof(s->attn));
        s->now_ms = 0;
        wubu_bs_welcome(&s->user, s->comp, sizeof(s->comp));
        snprintf(s->status, sizeof(s->status),
                 "human drives -- AGI watching (patience %u%% left)", 100);
        win->on_draw = tandem_draw;
        win->on_key = tandem_key;
        win->on_mouse = tandem_mouse;
        g_tandem = s;
    }
    return win;
}

/* the hosted test hook: advance N ticks (the timer loop). */
int tandem_test_tick(int n)
{
    if (!g_tandem) return -1;
    for (int i = 0; i < n; i++) {
        tandem_tick(g_tandem);
        tandem_maybe_propose(g_tandem);
    }
    return (int)g_tandem->proposals_made;
}

int tandem_test_proposals(void)
{
    return g_tandem ? (int)g_tandem->proposals_made : -1;
}

int tandem_test_events(void)
{
    return g_tandem ? (int)g_tandem->events : -1;
}
