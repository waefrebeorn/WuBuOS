/*
 * wubu_bonzi.c -- Bonzi Buddy: bare-metal AGI agent persona (ring-0 task).
 *
 * Freestanding C11. Runs as a kernel task inside the AGI supervisor
 * (wubu_agi_kernel_run spawns it). Real dispatch — no theater:
 *
 *   HELP / ?          list commands
 *   STATUS            attestation validity + boot counter + regions +
 *                     promoted count + frozen state + trace count
 *   ATTEST            PCR4 digest + Secure Boot state + boot counter
 *   PCR <0..7>        any PCR digest
 *   FREEZE/UNFREEZE   toggle the self-improve loop (real supervisor state)
 *   PROMOTE / CYCLE   run one DA-3 self-improve cycle, report promotions
 *   SPANS             dump recent immutable trace spans
 *   HELLO / HI        greet
 *   <anything else>   route to the operator (span + reply)
 *
 * Every handled line emits an AGENT span ("bonzi.cmd=...") so the
 * supervisor's independent-verifier cycle scores the human loop, and the
 * reply is klog'd to the serial console. The gorilla + console are drawn
 * with vbe primitives; the framebuffer font covers ASCII 32..95, so all
 * on-screen text is uppercased (serial keeps natural case).
 */
#include "wubu_bonzi.h"
#include "wubu_agi_kernel.h"
#include "wubu_theme.h"
#include "wubu_attest.h"
#include "vbe.h"
#include "input.h"
#include "tasking.h"
#include "klog.h"
#include <stdio.h>
#include <string.h>
/* The WuBu Buddy mascot, baked from tools/assets/wubu_buddy_sheet*.jpg
 * (tools/mascot_encode.py). The bare-metal bonzi wears the REAL buddy -- NOT
 * a hand-drawn underlay/gorilla. */
#include "../gui/wubu_buddy_sprite.h"

#define BONZI_LOG_CAP   8
#define BONZI_LOG_LEN   96
#define BONZI_INPUT_CAP 63

struct wubu_bonzi {
    struct wubu_agi_kernel *agi;
    char  log[BONZI_LOG_CAP][BONZI_LOG_LEN];
    int   log_n;
    int   log_head;
    char  input[BONZI_INPUT_CAP + 1];
    int   input_len;
    char  reply[128];
    int   actions;
    uint32_t blink;
    uint64_t last_draw_ms;
    uint64_t last_beat_ms;
    int   fb_w, fb_h;
};

static struct wubu_bonzi g_bonzi;   /* single instance (kernel) */

/* ---- helpers --------------------------------------------------------- */

static char bn_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static void bn_upper_str(char *dst, const char *src, int n)
{
    int j = 0;
    for (int i = 0; src[i] && j < n - 1; i++) dst[j++] = bn_upper(src[i]);
    dst[j] = '\0';
}

static void bn_log(wubu_bonzi_t *b, const char *who, const char *msg)
{
    int idx = (b->log_head + b->log_n) % BONZI_LOG_CAP;
    snprintf(b->log[idx], BONZI_LOG_LEN, "%s: %s", who, msg);
    if (b->log_n < BONZI_LOG_CAP) b->log_n++;
    else b->log_head = (b->log_head + 1) % BONZI_LOG_CAP;
}

static void bn_hex(const uint8_t *in, int n, char *out)
{
    static const char d[] = "0123456789ABCDEF";
    for (int i = 0; i < n; i++) {
        out[i * 2]     = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 15];
    }
    out[n * 2] = '\0';
}

/* ---- real ring-0 dispatch ------------------------------------------- */

static void bn_dispatch(wubu_bonzi_t *b, const char *line)
{
    char cmd[64];
    char r[128];
    wubu_agi_kernel_t *k = b->agi;
    bn_upper_str(cmd, line, sizeof(cmd));

    if (strcmp(cmd, "HELP") == 0 || strcmp(cmd, "?") == 0) {
        snprintf(r, sizeof(r),
                 "HELP: STATUS ATTEST PCR 0-7 FREEZE UNFREEZE PROMOTE SPANS HELLO");
    } else if (strcmp(cmd, "HELLO") == 0 || strcmp(cmd, "HI") == 0) {
        snprintf(r, sizeof(r),
                 "HI! I'M BONZI. THE AGI LOOP IS LIVE ON METAL. TYPE HELP.");
    } else if (strcmp(cmd, "STATUS") == 0) {
        snprintf(r, sizeof(r),
                 "ATTEST=%s BOOT=%u SB=%u REGIONS=%d PROMOTED=%d FROZEN=%s SPANS=%d",
                 wubu_attest_valid() ? "VALID" : "ABSENT",
                 wubu_attest_boot_counter(),
                 wubu_attest_sb_enabled() ? 1u : 0u,
                 wubu_agi_kernel_region_count(k),
                 wubu_agi_kernel_promoted_total(k),
                 wubu_agi_kernel_is_frozen(k) ? "YES" : "NO",
                 wubu_agi_kernel_trace_count(k));
    } else if (strcmp(cmd, "ATTEST") == 0) {
        uint8_t p4[WUBU_AGI_PCR_SZ];
        char hex[2 * WUBU_AGI_PCR_SZ + 1];
        if (wubu_attest_pcr4_digest(p4) == 0) {
            bn_hex(p4, WUBU_AGI_PCR_SZ, hex);
            snprintf(r, sizeof(r), "PCR4=%s BOOT=%u SB=%u SETUP=%u",
                     hex, wubu_attest_boot_counter(),
                     wubu_attest_sb_enabled() ? 1u : 0u,
                     wubu_attest_setup_mode() ? 1u : 0u);
        } else {
            snprintf(r, sizeof(r),
                     "NO FIRMWARE ATTESTATION -- ROOT OF TRUST ABSENT");
        }
    } else if (strncmp(cmd, "PCR", 3) == 0) {
        int idx = -1;
        if (cmd[3] == ' ' && cmd[4] >= '0' && cmd[4] <= '7' && cmd[5] == '\0')
            idx = cmd[4] - '0';
        if (idx >= 0) {
            uint8_t p[WUBU_AGI_PCR_SZ];
            char hex[2 * WUBU_AGI_PCR_SZ + 1];
            if (wubu_attest_pcr((unsigned)idx, p) == 0) {
                bn_hex(p, WUBU_AGI_PCR_SZ, hex);
                snprintf(r, sizeof(r), "PCR%d=%s", idx, hex);
            } else {
                snprintf(r, sizeof(r), "PCR%d UNAVAILABLE (NO ATTESTATION)", idx);
            }
        } else {
            snprintf(r, sizeof(r), "USE: PCR 0..7");
        }
    } else if (strcmp(cmd, "FREEZE") == 0) {
        wubu_agi_kernel_freeze(k, true);
        snprintf(r, sizeof(r), "SELF-IMPROVE LOOP FROZEN (PROMOTION DISABLED)");
    } else if (strcmp(cmd, "UNFREEZE") == 0) {
        wubu_agi_kernel_freeze(k, false);
        snprintf(r, sizeof(r), "SELF-IMPROVE LOOP UNFROZEN (GATE: VERIFIER+ATTEST)");
    } else if (strcmp(cmd, "PROMOTE") == 0 || strcmp(cmd, "CYCLE") == 0) {
        int n = wubu_agi_kernel_cycle(k);
        snprintf(r, sizeof(r), "CYCLE RAN: %d PROMOTED (TOTAL %d)",
                 n, wubu_agi_kernel_promoted_total(k));
    } else if (strcmp(cmd, "SPANS") == 0) {
        int n = wubu_agi_kernel_trace_count(k);
        snprintf(r, sizeof(r), "%d SPANS IN THE IMMUTABLE TRACE", n);
        bn_log(b, "Bonzi", r);
        for (int i = (n > 3) ? (n - 3) : 0; i < n; i++) {
            char sp[192];
            if (wubu_agi_kernel_span_data(k, i, sp, sizeof(sp)) == 0)
                bn_log(b, "  span", sp);
        }
        snprintf(r, sizeof(r), "SPANS: %d RECORDED", n);
    } else {
        snprintf(r, sizeof(r), "ROUTING '%s' TO THE OPERATOR", line);
    }

    snprintf(b->reply, sizeof(b->reply), "%s", r);
    bn_log(b, "Bonzi", r);
    b->actions++;

    /* Every human interaction is a trace span the supervisor scores. */
    {
        char span[192];
        snprintf(span, sizeof(span), "bonzi.cmd=%s actions=%d", cmd, b->actions);
        wubu_agi_kernel_agent_emit(k, 0, span);
    }
    if (klog_printf)
        klog_printf("bonzi: %s => %s\n", line, r);
}

/* ---- drawing (vbe primitives, integer math) ------------------------- */

/* Blit the WuBu Buddy sprite (96x96 RGBA, baked) at (x0,y0) at integer
 * `scale`. Alpha-gated: only pixels with alpha > 128 write through, keeping
 * the transparent margins around the baked buddy.
 *
 * Two-stage, measured: the per-pixel version cost 344k guest steps (26% of the
 * boot) because every pixel paid an alpha test, a scale loop and a bounds
 * check. The sprite is STATIC, so the opaque runs of every output row are
 * computed once per scale and cached; each frame expands one source row into a
 * small staging row and copies the opaque runs with `rep movsl`. Only the
 * runs' positions are cached -- the colours still come from the sprite, so a
 * change to the sprite data cannot go stale without changing the run table. */
#define BN_MAX_SCALE 4
#define BN_ROW_MAX (WUBU_BUDDY_SPRITE_SIZE * BN_MAX_SCALE)
typedef struct { uint16_t start, len; } bn_run_t;

static uint8_t  g_bn_runs_baked[BN_MAX_SCALE + 1];
static uint16_t g_bn_row_first[(WUBU_BUDDY_SPRITE_SIZE * BN_MAX_SCALE) + 1];
static bn_run_t g_bn_run[8192];
static uint32_t g_bn_row[BN_ROW_MAX];

/* The expanded COLOUR rows are cached too, for the scales that fit: expanding
 * 96 source rows into a staging row every frame still cost ~120k steps (2.8x
 * better than per-pixel, not the 10x the run copy promised). At scale 1-2 the
 * whole sprite is 96 x (96*2) x 4 B = 73 KB, which is cheap next to a boot
 * that then pays ~192 `rep movsl` instead of ~120k steps. Scale 3-4 (never
 * used by the shell: bn_draw calls scale 2) fall back to per-frame expansion
 * rather than carrying 590 KB of BSS for a case nothing hits. */
#define BN_CACHE_MAX_SCALE 2
static uint32_t g_bn_px[WUBU_BUDDY_SPRITE_SIZE][WUBU_BUDDY_SPRITE_SIZE * BN_CACHE_MAX_SCALE];

static void bn_bake_runs(int scale)
{
    int r = 0, out_rows = 0;
    for (int yy = 0; yy < WUBU_BUDDY_SPRITE_SIZE; yy++) {
        const unsigned char *srow = &wubu_buddy_rgba[yy * WUBU_BUDDY_SPRITE_SIZE * 4];
        int n = 0;
        /* one expanded row of this scale, expressed as opaque spans */
        static uint8_t op[BN_ROW_MAX];
        for (int xx = 0; xx < WUBU_BUDDY_SPRITE_SIZE; xx++) {
            uint8_t a = (uint8_t)(srow[xx * 4 + 3] > 128);
            for (int sx = 0; sx < scale; sx++) op[n++] = a;
        }
        int i = 0;
        while (i < n) {
            while (i < n && !op[i]) i++;
            int s = i;
            while (i < n && op[i]) i++;
            if (i > s && r < 8192) { g_bn_run[r].start = (uint16_t)s; g_bn_run[r].len = (uint16_t)(i - s); r++; }
        }
        if (scale <= BN_CACHE_MAX_SCALE) {
            for (int xx = 0; xx < WUBU_BUDDY_SPRITE_SIZE; xx++) {
                uint32_t c = ((uint32_t)srow[xx * 4] << 16) |
                             ((uint32_t)srow[xx * 4 + 1] << 8) |
                             (uint32_t)srow[xx * 4 + 2];
                for (int sx = 0; sx < scale; sx++) g_bn_px[yy][xx * scale + sx] = c;
            }
        }
        g_bn_row_first[out_rows++] = (uint16_t)r;
    }
    /* row_first is written per SOURCE row here; the blit walks it that way */
    g_bn_runs_baked[scale] = 1;
}

static void bn_draw_buddy(int x0, int y0, int scale)
{
    if (scale < 1) scale = 1;
    if (scale > BN_MAX_SCALE) scale = BN_MAX_SCALE;
    if (!g_bn_runs_baked[scale]) bn_bake_runs(scale);
    int sw = vbe_surface_width();
    int first = 0;
    for (int yy = 0; yy < WUBU_BUDDY_SPRITE_SIZE; yy++) {
        const unsigned char *srow = &wubu_buddy_rgba[yy * WUBU_BUDDY_SPRITE_SIZE * 4];
        const bn_run_t *runs = &g_bn_run[first];
        int nruns = (int)g_bn_row_first[yy] - first;
        first = g_bn_row_first[yy];
        if (nruns <= 0) continue;
        const uint32_t *row_px;
        if (scale <= BN_CACHE_MAX_SCALE) {
            row_px = g_bn_px[yy];          /* baked once by bn_bake_runs */
        } else {
            /* expansion is only for scales the shell never asks for */
            for (int xx = 0; xx < WUBU_BUDDY_SPRITE_SIZE; xx++) {
                const unsigned char *px = &srow[xx * 4];
                uint32_t c = ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | px[2];
                for (int sx = 0; sx < scale; sx++) g_bn_row[xx * scale + sx] = c;
            }
            row_px = g_bn_row;
        }
        for (int sy = 0; sy < scale; sy++) {
            uint32_t *dst = vbe_scanline(y0 + yy * scale + sy);
            if (!dst) continue;
            for (int k = 0; k < nruns; k++) {
                int a = x0 + runs[k].start, b = a + runs[k].len;
                if (a < 0) a = 0;
                if (b > sw) b = sw;
                if (a >= b) continue;
                uint32_t *src = (uint32_t *)&row_px[runs[k].start + (a - (x0 + runs[k].start))];
                uint32_t *d = dst + a;
                unsigned long cnt = (unsigned long)(b - a);
                __asm__ volatile ("rep movsl"
                                  : "+S"(src), "+D"(d), "+c"(cnt)
                                  : : "memory");
            }
        }
    }
}

static void bn_draw(wubu_bonzi_t *b)
{
    int w = b->fb_w, h = b->fb_h;
    if (w < 320) w = 1920;
    if (h < 240) h = 1080;

    vbe_clear(0x00101018);
    /* The WuBu Buddy mascot, baked sprite, with a gentle idle bob: a 16-step
     * triangle wave on the draw counter floats it ~16px up and back (motion
     * pixel art, not a static sticker). */
    int _ph = (int)(b->blink % 16);
    int _dy = (_ph < 8) ? _ph : 16 - _ph;
    bn_draw_buddy(60, h - 340 - _dy * 2, 2);

    /* Speech bubble (last reply) */
    {
        const WubuKTheme *t = wubu_theme_get();
        int bx = 240, by = h - 410;
        int bw = (w - 260 > 640) ? 640 : w - 260;
        int bh = 64;
        uint32_t bubble = t ? t->speech_bubble : 0x00FFFFFFu;
        uint32_t border = t ? t->speech_border : 0x00402070u;
        vbe_fill_rect_rounded(bx, by, bw, bh, 10, bubble);
        vbe_rect_rounded(bx, by, bw, bh, 10, border);
        char up[128];
        bn_upper_str(up, b->reply, sizeof(up));
        vbe_draw_text(bx + 12, by + 12, up, 0x00401060, 1);
    }

    /* Chat log (top-left) */
    {
        int pw = (w - 24 > 560) ? 560 : w - 24;
        vbe_fill_rect(12, 12, pw, BONZI_LOG_CAP * 14 + 12, 0x00F0F0F0);
        vbe_rect(12, 12, pw, BONZI_LOG_CAP * 14 + 12, 0x00808080);
        int shown = b->log_n < BONZI_LOG_CAP ? b->log_n : BONZI_LOG_CAP;
        for (int i = 0; i < shown; i++) {
            int idx = (b->log_head + b->log_n - shown + i) % BONZI_LOG_CAP;
            char up[128];
            bn_upper_str(up, b->log[idx], sizeof(up));
            vbe_draw_text(18, 18 + i * 14, up, 0x00000000, 1);
        }
    }

    /* Input line (bottom) */
    {
        vbe_fill_rect(12, h - 30, w - 24, 20, 0x00FFFFFF);
        vbe_rect(12, h - 30, w - 24, 20, 0x00808080);
        char ib[96];
        snprintf(ib, sizeof(ib), "> %s", b->input);
        char up[96];
        bn_upper_str(up, ib, sizeof(up));
        vbe_draw_text(18, h - 27, up, 0x00000000, 1);
    }

    vbe_swap();
}

/* ---- public API ------------------------------------------------------ */

wubu_bonzi_t *wubu_bonzi_init(struct wubu_agi_kernel *k)
{
    memset(&g_bonzi, 0, sizeof(g_bonzi));
    g_bonzi.agi   = k;
    g_bonzi.fb_w  = vbe_width();
    g_bonzi.fb_h  = vbe_height();
    snprintf(g_bonzi.reply, sizeof(g_bonzi.reply),
             "HI! I'M BONZI. THE AGI LOOP IS LIVE ON METAL. TYPE HELP.");
    /* Do NOT log the greeting into the chat history: it is the SPEECH BUBBLE
     * (last reply) at startup. Logging it too made the same line render in
     * both the bubble and the top-left log panel -- a visible duplicate. */
    if (klog_printf)
        klog_printf("WuBuOS AGI: Bonzi Buddy loop active (fb=%dx%d)\n",
                    g_bonzi.fb_w, g_bonzi.fb_h);
    bn_draw(&g_bonzi);
    return &g_bonzi;
}

int wubu_bonzi_tick(wubu_bonzi_t *b)
{
    if (!b) return 0;
    int acted = 0;

    /* Drain pending keyboard events (always responsive). */
    KeyEvent ev;
    while (input_key_poll(&ev) == 1) {
        if (ev.kind != KEY_EVENT_DOWN) continue;
        uint32_t kc = ev.keycode;
        if (kc == '\r' || kc == '\n') {
            if (b->input_len > 0) {
                b->input[b->input_len] = '\0';
                bn_dispatch(b, b->input);
                acted++;
                b->input_len = 0;
            }
        } else if (kc == '\b' || kc == 0x08 || kc == 0x7F) {
            if (b->input_len > 0) b->input_len--;
        } else if (kc >= 32 && kc < 127 && b->input_len < BONZI_INPUT_CAP) {
            b->input[b->input_len++] = (char)kc;
        }
    }

    /* Paced heartbeat: proof the loop is alive on serial + in the trace. */
    uint64_t now = wubu_agi_kernel_uptime_ms(b->agi);
    if (now - b->last_beat_ms >= 1000) {
        b->last_beat_ms = now;
        char span[192];
        snprintf(span, sizeof(span),
                 "bonzi.heartbeat regions=%d attest=%s promoted=%d frozen=%s",
                 wubu_agi_kernel_region_count(b->agi),
                 wubu_attest_valid() ? "VALID" : "ABSENT",
                 wubu_agi_kernel_promoted_total(b->agi),
                 wubu_agi_kernel_is_frozen(b->agi) ? "yes" : "no");
        wubu_agi_kernel_agent_emit(b->agi, 0, span);
        if (klog_printf)
            klog_printf("bonzi: heartbeat regions=%d attest=%s\n",
                        wubu_agi_kernel_region_count(b->agi),
                        wubu_attest_valid() ? "VALID" : "ABSENT");

        /* Gap D7: the supervisor watchdog. The AGI self-improve loop
         * must promote continuously; a frozen loop (or one whose cycle
         * is stuck) makes last_promote_tick go stale. Alert loudly --
         * the kernel's A4 watchdog covers the tasks, this covers the
         * supervisor's actual output. */
        extern uint64_t task_tick_count(void);
        uint64_t last = wubu_agi_kernel_last_promote_tick(b->agi);
        uint64_t now_tick = task_tick_count();
        if (!wubu_agi_kernel_is_frozen(b->agi) && last > 0 &&
            now_tick - last > 5000) {   /* 50 s of no promotions */
            if (klog_printf)
                klog_printf("SUPERVISOR WATCHDOG: no promotion for %u ticks (loop stalled?)\n",
                            (unsigned)(now_tick - last));
            wubu_agi_kernel_agent_emit(b->agi, 1, "supervisor.stall detected");
        }
    }

    /* Paced redraw (~5 Hz) + mouth animation. */
    if (now - b->last_draw_ms >= 200) {
        b->last_draw_ms = now;
        b->blink++;
        bn_draw(b);
    }
    return acted;
}

int wubu_bonzi_handle_line(wubu_bonzi_t *b, const char *line)
{
    if (!b || !line || !*line) return 0;
    bn_dispatch(b, line);
    return 1;
}

const char *wubu_bonzi_last_reply(const wubu_bonzi_t *b)
{
    return b ? b->reply : "";
}

int wubu_bonzi_action_count(const wubu_bonzi_t *b)
{
    return b ? b->actions : 0;
}

/* ---- kernel task entry (spawned by wubu_agi_kernel_run) -------------- */

void wubu_bonzi_task(void *arg)
{
    struct wubu_agi_kernel *k = (struct wubu_agi_kernel *)arg;
    wubu_bonzi_t *b = wubu_bonzi_init(k);
    if (!b) return;
    for (;;) {
        wubu_bonzi_tick(b);
        task_sleep(2);       /* ~20 ms @ 100 Hz clock tick */
    }
}
