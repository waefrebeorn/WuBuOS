/* wubu_console_colonel.c — Live Colonel expression evaluator (ring 0)
 *
 * Extracted from wubu_console.c — the TempleOS-style live coding
 * evaluator: recursive-descent parser over register state r0..r7.
 * C11, freestanding.
 */
#include "wubu_console.h"
#include "klog.h"
#include <stdint.h>

/* The Live Colonel: ring-0 live coding, TempleOS-style. The Colonel
 * types an expression; the kernel evaluates it immediately and the
 * result returns to the console. The evaluator is a tiny freestanding
 * expression VM (no hosted JIT on metal): registers R0..R7 persist
 * across evals (the Colonel's live state), and the `recovery`
 * checkpoint ring snapshots them -- a live-coding mistake is one
 * rollback away. */

int64_t live_regs[8];      /* the Colonel's persistent registers */
static int      live_seq = 0;

/* The Live Colonel expression evaluator: a recursive descent over
 * + - * / % ( ) and the registers r0..r7 and integer literals. */
static int live_peek(const char *s, int *i)
{
    while (s[*i] == ' ' || s[*i] == '\t') (*i)++;
    return s[*i];
}
static int64_t live_expr(const char *s, int *i);

static int64_t live_primary(const char *s, int *i)
{
    int c = live_peek(s, i);
    if (c == '(') {
        (*i)++;
        int64_t v = live_expr(s, i);
        live_peek(s, i);
        if (s[*i] == ')') (*i)++;
        return v;
    }
    if (c == 'r' || c == 'R') {
        int reg = s[*i + 1] - '0';
        *i += 2;
        if (reg >= 0 && reg < 8) return live_regs[reg];
        return 0;
    }
    if (c == '-' || c == '+') {
        int sign = (c == '-') ? -1 : 1;
        (*i)++;
        return sign * live_primary(s, i);
    }
    /* integer literal */
    int64_t v = 0;
    while (s[*i] >= '0' && s[*i] <= '9') {
        v = v * 10 + (s[*i] - '0');
        (*i)++;
    }
    return v;
}

static int64_t live_mul(const char *s, int *i)
{
    int64_t v = live_primary(s, i);
    for (;;) {
        int c = live_peek(s, i);
        if (c == '*') { (*i)++; v *= live_primary(s, i); }
        else if (c == '/') { (*i)++; int64_t d = live_primary(s, i); v = d ? v / d : 0; }
        else if (c == '%') { (*i)++; int64_t d = live_primary(s, i); v = d ? v % d : 0; }
        else return v;
    }
}

static int64_t live_expr(const char *s, int *i)
{
    int64_t v = live_mul(s, i);
    for (;;) {
        int c = live_peek(s, i);
        if (c == '+') { (*i)++; v += live_mul(s, i); }
        else if (c == '-') { (*i)++; v -= live_mul(s, i); }
        else return v;
    }
}

/* `live <expr>` -- evaluate and print (Live Colonel, ring 0). */
int cmd_live(int argc, char **argv)
{
    if (argc < 2) {
        klog_printf("live: usage 'live <expr>' (registers r0..r7 persist)\n");
        return 0;
    }
    int i = 0;
    const char *s = argv[1];
    /* the rest of the line is the expression (tokens are space-split,
     * so rejoin is unnecessary for the supported grammar) */
    int64_t r = live_expr(s, &i);
    klog_printf("live[%d] = %d:%d\n", live_seq++,
                (int)(int32_t)(r >> 32), (int)(int32_t)(r & 0xFFFFFFFF));
    return 0;
}
