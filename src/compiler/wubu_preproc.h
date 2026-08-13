/*
 * wubu_preproc.h -- minimal C preprocessor for the HolyC compiler.
 * Expands #define (object + function-like) macros and strips directives.
 */
#ifndef WUBU_PREPROC_H
#define WUBU_PREPROC_H

/* Preprocess a source string: strip directives, expand macros.
 * Returns a malloc'd string (caller frees) or NULL on failure. */
char *wubu_preprocess(const char *src);

#endif
