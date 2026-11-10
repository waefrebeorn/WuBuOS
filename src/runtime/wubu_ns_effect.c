/*
 * wubu_ns_effect.c -- the /n/effect control subtree.
 *
 *   /n/effect/list   -> the effect names a program can pick (read)
 *   /n/effect/stack  -> the live effect stack, one line per entry (read)
 *   /n/effect/set    -> write "<name> <strength> [rrggbb]" to append;
 *                        write "clear" (or an empty line) to empty the stack
 *
 * A program restyles what the user sees by writing here.  The stack it edits
 * is the SAME object the surface applies, so there is one owner for the effect
 * list and no second copy anywhere -- the same rule the VWS template library
 * follows.
 *
 * The engine itself is wubu_effect.{c,h}: pure C11, no dependencies, so the
 * runtime and the GUI share it instead of each keeping a pass implementation.
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_effect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WubuEffectStack *g_effect_stack;

const WubuEffectStack *wubu_ns_effect_stack(void) { return g_effect_stack; }

static void stack_str(char *buf, size_t cap) {
    size_t n = 0;
    buf[0] = '\0';
    int count = wubu_effect_stack_count(g_effect_stack);
    for (int i = 0; i < count; i++) {
        const WubuEffect *e = wubu_effect_stack_at(g_effect_stack, i);
        if (!e) continue;
        n += (size_t)snprintf(buf + n, cap > n ? cap - n : 0, "%s %d %06x\n",
                              wubu_effect_name(e->kind), e->strength, e->color);
        if (n >= cap) break;
    }
    if (count == 0 && cap > 1) memcpy(buf, "(none)\n", 8);
}

/* Publish the subtree.  Safe to call more than once. */
int wubu_ns_publish_effect(void) {
    char sub[128];
    if (ns_mkdir("effect") != 0) return -1;
    if (ns_write("effect/list", "none\ntint\nvignette\nglow\nscanline\n") != 0) return -1;
    return ns_write("effect/stack", "(none)\n");
}

/* Republish the live stack. */
int wubu_ns_effect_refresh(void) {
    char buf[1024];
    stack_str(buf, sizeof buf);
    return ns_write("effect/stack", buf);
}

/*
 * Parse one /n/effect/set payload.
 *
 *   "tint 50 ff0000"  append a tint at 50 percent toward red
 *   "clear"           empty the stack
 *
 * Returns 0 on success, -1 on a bad payload (nothing is changed then).
 */
int wubu_ns_effect_parse(const char *text) {
    if (!text) return -1;
    if (!g_effect_stack) g_effect_stack = wubu_effect_stack_new();
    if (!g_effect_stack) return -1;

    char verb[32];
    int strength = 0;
    unsigned color = 0;
    int n = 0;

    if (sscanf(text, "clear%n", &n) == 0 && n > 0) {
        wubu_effect_stack_clear(g_effect_stack);
        return 0;
    }

    int got = sscanf(text, "%31s %d %x", verb, &strength, &color);
    if (got < 2) return -1;

    const char *name = NULL;
    for (int i = 1; i < wubu_effect_kind_count(); i++) {
        const char *nm = wubu_effect_name_at(i);
        if (nm && strcmp(nm, verb) == 0) { name = nm; break; }
    }
    if (!name) return -1;

    WubuEffectKind kind = WUBU_EFFECT_NONE;
    for (int i = 1; i < wubu_effect_kind_count(); i++) {
        const char *nm = wubu_effect_name_at(i);
        if (nm && strcmp(nm, name) == 0) { kind = wubu_effect_kind_at(i); break; }
    }
    return wubu_effect_stack_add(g_effect_stack, kind, color, strength) ? 0 : -1;
}
