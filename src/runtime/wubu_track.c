/*
 * wubu_track.c -- Tracker: the full-text tokenizer and filter chain.
 *
 * See wubu_track.h.
 */
#include "wubu_track.h"

#include <ctype.h>
#include <string.h>

/* The English stopword list Tracker uses.  Kept deliberately small: the point
 * is the RULE (drop function words) not the coverage. */
static const char *const g_stopwords[] = {
    "a", "an", "and", "are", "as", "at", "be", "but", "by", "for", "from",
    "has", "he", "in", "is", "it", "its", "of", "on", "or", "that", "the",
    "this", "to", "was", "were", "will", "with", NULL
};

bool wb_track_is_stopword(const char *term)
{
    if (!term || !*term) return false;      /* an empty term is not a stopword */
    for (int i = 0; g_stopwords[i]; i++)
        if (strcmp(g_stopwords[i], term) == 0) return true;
    return false;
}

const char *wb_track_normalize(const char *in, char *out, size_t n)
{
    if (!in) return "no input";
    if (!out || n == 0) return "no output buffer";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        if (o + 2 > n) return "the term does not fit";
        /* Case-fold to lower case.  An index that keeps case has two entries
         * where the user expects one. */
        out[o++] = (char)tolower(*p);
    }
    out[o] = '\0';
    return NULL;
}

const char *wb_track_stem(const char *in, char *out, size_t n)
{
    if (!in) return "no input";
    if (!out || n == 0) return "no output buffer";
    size_t l = strlen(in);
    if (l + 1 > n) return "the term does not fit";

    /* A consonant that Porter's first step undoubles.  'l', 's' and 'z' are
     * excluded: "press" -> "pres" and "fill" -> "fil" are over-stems. */
    int undoubleable(char c)
    {
        return c == 'b' || c == 'd' || c == 'f' || c == 'g' || c == 'm' ||
               c == 'n' || c == 'p' || c == 'r' || c == 't';
    }

    /* THE LENGTH GUARD IS THE POINT: a stem that leaves fewer than three
     * characters makes unrelated words collide -- "sing" -> "s" and "bring" ->
     * "br" is the classic over-stem.  Every rule below checks it. */
    size_t cut = l;
    bool replace_ie = false;

    if (l > 3 && strcmp(in + l - 3, "ies") == 0) {
        /* "ties" -> "tie", "parties" -> "partie" */
        cut = l - 1;                 /* drop the 's', keep the 'ie' */
        replace_ie = true;
    } else if (l - 3 >= 3 && strcmp(in + l - 3, "ing") == 0) {
        /* "running" -> "runn" -> "run": strip, then UNDOUBLE the consonant */
        cut = l - 3;
        if (cut > 3 && in[cut-1] == in[cut-2] && undoubleable(in[cut-1])) cut--;
    } else if (l - 2 >= 3 && strcmp(in + l - 2, "ed") == 0) {
        /* "stopped" -> "stopp" -> "stop" */
        cut = l - 2;
        if (cut > 3 && in[cut-1] == in[cut-2] && undoubleable(in[cut-1])) cut--;
    } else if (l - 1 >= 3 && in[l-1] == 's' && in[l-2] != 's' && in[l-2] != 'u') {
        /* "files" -> "file", "fires" -> "fire".  'ss' and 'us' are left alone:
         * "press" and "bus" are already stems. */
        cut = l - 1;
    }
    (void)replace_ie;

    memcpy(out, in, cut);
    out[cut] = '\0';
    return NULL;
}

uint32_t wb_track_tokenize(const char *text, wb_track_token_t *out, uint32_t max)
{
    if (!out || max == 0) return 0;
    if (!text) return 0;

    uint32_t n = 0;
    const char *p = text;
    uint32_t position = 0;

    while (*p) {
        /* skip separators */
        while (*p && !isalnum((unsigned char)*p)) p++;
        if (!*p) break;

        const char *start = p;
        bool numeric = true;
        while (*p && isalnum((unsigned char)*p)) {
            if (!isdigit((unsigned char)*p)) numeric = false;
            p++;
        }
        size_t len = (size_t)(p - start);
        position++;

        char raw[WB_TRACK_TERM_MAX * 2];
        if (len >= sizeof(raw)) len = sizeof(raw) - 1;
        memcpy(raw, start, len);
        raw[len] = '\0';

        char norm[WB_TRACK_TERM_MAX], stem[WB_TRACK_TERM_MAX];
        if (wb_track_normalize(raw, norm, sizeof(norm)) != NULL) continue;

        /* A term shorter than two characters is dropped: a one-character index
         * is nearly useless and disproportionately large. */
        if (strlen(norm) < 2) continue;
        /* A term longer than the maximum is dropped: a pathological input
         * (a 100 KB "word") would otherwise become an index entry that can
         * never be matched by a normal query. */
        if (strlen(norm) >= WB_TRACK_TERM_MAX) continue;

        /* STOPWORDS are dropped: indexing "the" makes every query about
         * anything match every document. */
        if (wb_track_is_stopword(norm)) continue;

        if (wb_track_stem(norm, stem, sizeof(stem)) != NULL) continue;

        if (n >= max) break;
        strncpy(out[n].text, stem, WB_TRACK_TERM_MAX - 1);
        out[n].text[WB_TRACK_TERM_MAX - 1] = '\0';
        out[n].type = numeric ? WB_TRACK_TOKEN_NUMBER : WB_TRACK_TOKEN_WORD;
        out[n].position = position;
        n++;
    }
    return n;
}
