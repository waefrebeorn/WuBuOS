/*
 * wubu_revolver.h -- runtime-configurable bounds with hard ceilings.
 *
 * A hardcoded limit silently becomes a bug twice over: it is either too small
 * for real work, or generous enough to be a memory-safety problem when someone
 * feeds it untrusted input.  A revolver keeps ONE default, makes the value
 * adjustable without a rebuild, and clamps to a ceiling that never changes.
 *
 * The ceiling is the important half.  Environment variables are user input; an
 * unbounded override turns configuration into an allocation primitive.  Every
 * revolver therefore refuses to go above its limit, and every value below 1.
 *
 * Values that size an ARRAY cannot be revoked at runtime (C needs a compile-time
 * extent).  For those, define NAME_DEF as the ceiling and use the revolver only
 * for the runtime bound check -- see STYXFS_MAX_FILES.
 */

#ifndef WUBU_REVOLVER_H
#define WUBU_REVOLVER_H

#include <stddef.h>

typedef struct {
    const char *name;      /* for diagnostics: which knob moved */
    long        def_value; /* the intended default */
    long        limit;     /* absolute ceiling; never exceeded */
    long        scale;     /* unit multiplier applied to parsed values */
    long        value;     /* current effective value */
    int         from_env;  /* 1 when the last change came from the environment */
} wubu_revolver_t;

/* Set defaults and the immutable ceiling.  limit <= 0 means "no ceiling beyond
 * the default", which is deliberately not offered to callers that size memory. */
void wubu_revolver_init(wubu_revolver_t *rev, const char *name,
                        long def_value, long limit, long scale);

/* Read `env` and adopt it when present and valid.
 * Clamps to [1, limit]; an unparseable, negative or zero value is IGNORED and
 * reported through `warn`, because silently falling back to the default would
 * hide a typo like WUBU_FS_FILES=6O4. */
void wubu_revolver_set_from_env(wubu_revolver_t *rev, const char *env,
                                void (*warn)(const char *msg, const char *env_name,
                                             const char *value));

/* Current effective value. */
long wubu_revolver_get(const wubu_revolver_t *rev);


#endif /* WUBU_REVOLVER_H */
