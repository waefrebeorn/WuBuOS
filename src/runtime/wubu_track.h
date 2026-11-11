/*
 * wubu_track.h -- Tracker: the full-text tokenizer and filter chain.
 *
 * Tracker indexes documents into terms.  The chain is: split into tokens,
 * CASE-FOLD, DROP STOPWORDS and degenerate terms, then STEM.  Each stage has a
 * rule that is easy to get wrong in a way that silently degrades search:
 *
 *   * A term is CASE-FOLDED.  An index that keeps case has two entries where
 *     the user expects one, and "Firefox" does not match "firefox".
 *   * STOPWORDS are dropped.  Indexing "the" makes the index enormous and
 *     every query about anything match every document.
 *   * A term SHORTER THAN TWO characters is dropped.  A one-character index is
 *     nearly useless and disproportionately large.
 *   * A term LONGER THAN 31 characters is dropped.  A pathological input (a
 *     100 KB "word") would otherwise become an index entry that can never be
 *     matched by a normal query.
 *   * STEMMING MUST LEAVE AT LEAST THREE CHARACTERS.  "running" -> "run" is
 *     the point; "sing" -> "s" and "bring" -> "br" is the classic over-stem
 *     that makes unrelated words collide.
 *   * A token's POSITION is its ordinal in the document, because phrase search
 *     needs adjacency, not just presence.
 *   * Numbers are tokenized but FLAGGED, so a query can target them.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_TRACK_H
#define WUBU_TRACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WB_TRACK_TERM_MAX   32
#define WB_TRACK_TOKENS_MAX 4096

typedef enum {
    WB_TRACK_TOKEN_WORD = 0,
    WB_TRACK_TOKEN_NUMBER,
    WB_TRACK_TOKEN_OTHER
} wb_track_token_type_t;

typedef struct {
    char                 text[WB_TRACK_TERM_MAX];
    wb_track_token_type_t type;
    uint32_t             position;
} wb_track_token_t;

/* Is this term a stopword?  An empty term is not a stopword -- it is nothing,
 * and is dropped by the length rule instead. */
bool wb_track_is_stopword(const char *term);

/* Case-fold a term to lower case.  Returns NULL on success. */
const char *wb_track_normalize(const char *in, char *out, size_t n);

/* Stem a term, never producing fewer than three characters. */
const char *wb_track_stem(const char *in, char *out, size_t n);

/* Tokenize and filter a document.  Returns the token count. */
uint32_t wb_track_tokenize(const char *text, wb_track_token_t *out,
                           uint32_t max);

#endif /* WUBU_TRACK_H */
