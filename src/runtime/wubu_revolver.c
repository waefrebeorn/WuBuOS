/*
 * wubu_revolver.c -- see wubu_revolver.h.
 *
 * One owner for the whole tree: every knob that must be adjustable without a
 * rebuild goes through here, so the clamping rules live in exactly one place and
 * cannot drift between subsystems.
 */

#include "wubu_revolver.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Print to stderr when the caller does not supply a reporter.  A warning that
 * nobody receives is the same as silently accepting the typo, which is the one
 * outcome this function exists to prevent. */
static void default_warn(const char *msg, const char *env_name, const char *value)
{
    fprintf(stderr, "wubu: %s (%s=%s)\n", msg, env_name, value ? value : "");
}

void wubu_revolver_init(wubu_revolver_t *rev, const char *name,
                        long def_value, long limit, long scale)
{
    if (!rev) return;
    memset(rev, 0, sizeof(*rev));
    rev->name      = name;
    rev->def_value = def_value;
    rev->limit     = limit;
    rev->scale     = (scale > 0) ? scale : 1;

    long v = def_value * rev->scale;
    if (limit > 0 && v > limit) v = limit;
    if (v < 1) v = 1;
    rev->value = v;
    rev->from_env = 0;
}

static long clamp(long v, long limit)
{
    if (v < 1) return 1;
    if (limit > 0 && v > limit) return limit;
    return v;
}

void wubu_revolver_set_from_env(wubu_revolver_t *rev, const char *env,
                                void (*warn)(const char *, const char *, const char *))
{
    if (!rev || !env || !*env) return;
    if (!warn) warn = default_warn;

    const char *raw = getenv(env);
    if (!raw || !*raw) return;

    errno = 0;
    char *end = NULL;
    long parsed = strtol(raw, &end, 10);

    /* Refuse anything that is not a clean decimal integer.  A trailing unit
     * suffix ("64K") or a typo ("6O4") must NOT be read as a smaller number:
     * silently truncating strtol's partial parse would hand back a limit nobody
     * asked for. */
    if (errno == ERANGE) {
        warn("value out of range, keeping the current setting", env, raw);
        return;
    }
    if (end == raw || (*end != '\0' && *end != ' ')) {
        warn("not a plain integer, ignoring", env, raw);
        return;
    }

    if (parsed < 1) {
        warn("must be at least 1, ignoring", env, raw);
        return;
    }

    long scaled = parsed * rev->scale;
    if (rev->limit > 0 && scaled > rev->limit) {
        warn("above the hard ceiling, clamping to it", env, raw);
        scaled = rev->limit;
    }
    rev->value = scaled;
    rev->from_env = 1;
}

long wubu_revolver_get(const wubu_revolver_t *rev)
{
    return rev ? rev->value : 0;
}
