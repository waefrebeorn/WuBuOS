/*
 * wubu_track_test.c -- the full-text tokenizer and filter chain.
 *
 * The load-bearing rule: STEMMING MUST LEAVE AT LEAST THREE CHARACTERS.
 * "running" -> "run" is the point; "sing" -> "s" and "bring" -> "br" is the
 * classic over-stem that makes unrelated words collide.
 */
#include "wubu_track.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static int find(wb_track_token_t *t, uint32_t n, const char *text)
{
    for (uint32_t i = 0; i < n; i++)
        if (strcmp(t[i].text, text) == 0) return (int)i;
    return -1;
}

int main(void)
{
    printf("=== test_wubu_track ===\n");
    char out[128];

    /* -- case folding -- */
    CHECK(wb_track_normalize("Firefox", out, sizeof(out)) == NULL &&
          strcmp(out, "firefox") == 0, "a term is CASE-FOLDED");
    CHECK(wb_track_normalize("MiXeD", out, sizeof(out)) == NULL &&
          strcmp(out, "mixed") == 0, "mixed case folds");
    CHECK(wb_track_normalize("", out, sizeof(out)) == NULL && out[0] == '\0',
          "an empty term folds to empty");
    CHECK(wb_track_normalize(NULL, out, sizeof(out)) != NULL, "a null input is refused");
    CHECK(wb_track_normalize("x", NULL, 0) != NULL, "a null output is refused");
    CHECK(wb_track_normalize("toolong", out, 4) != NULL,
          "a term that does not fit is refused");

    /* -- stopwords -- */
    CHECK(wb_track_is_stopword("the"), "'the' is a stopword");
    CHECK(wb_track_is_stopword("is"), "'is' is a stopword");
    CHECK(wb_track_is_stopword("and"), "'and' is a stopword");
    CHECK(!wb_track_is_stopword("firefox"), "'firefox' is not");
    CHECK(!wb_track_is_stopword(""), "an EMPTY term is NOT a stopword");
    CHECK(!wb_track_is_stopword(NULL), "a null term is not a stopword");

    /* -- stemming, with the length guard -- */
    CHECK(wb_track_stem("running", out, sizeof(out)) == NULL &&
          strcmp(out, "run") == 0, "'running' -> 'run'");
    CHECK(wb_track_stem("files", out, sizeof(out)) == NULL &&
          strcmp(out, "file") == 0, "'files' -> 'file'");
    CHECK(wb_track_stem("fires", out, sizeof(out)) == NULL &&
          strcmp(out, "fire") == 0, "'fires' -> 'fire'");
    /* THE GUARD */
    CHECK(wb_track_stem("sing", out, sizeof(out)) == NULL &&
          strcmp(out, "sing") == 0,
          "'sing' is NOT stemmed to 's' (that would collide with everything)");
    CHECK(wb_track_stem("bring", out, sizeof(out)) == NULL &&
          strcmp(out, "bring") == 0, "'bring' is NOT stemmed to 'br'");
    CHECK(wb_track_stem("ties", out, sizeof(out)) == NULL &&
          strcmp(out, "tie") == 0, "'ties' -> 'tie'");
    CHECK(wb_track_stem("run", out, sizeof(out)) == NULL &&
          strcmp(out, "run") == 0, "a three-character term is unchanged");
    CHECK(wb_track_stem(NULL, out, sizeof(out)) != NULL, "a null input is refused");

    /* -- tokenizing a document -- */
    {
        wb_track_token_t t[64];
        uint32_t n = wb_track_tokenize(
            "The quick brown Fox jumps over the lazy dog", t, 64);
        CHECK(n > 0, "a document yields tokens");
        CHECK(find(t, n, "the") < 0,
              "STOPWORDS are dropped ('the' is gone)");
        CHECK(find(t, n, "quick") >= 0, "'quick' survives");
        CHECK(find(t, n, "fox") >= 0, "'Fox' is case-folded to 'fox'");
        CHECK(find(t, n, "jump") >= 0, "'jumps' is stemmed to 'jump'");
        CHECK(find(t, n, "lazi") >= 0 || find(t, n, "lazy") >= 0,
              "'lazy' survives (its stem depends on the rule order)");
    }

    /* -- positions are ordinals, for phrase search -- */
    {
        wb_track_token_t t[64];
        uint32_t n = wb_track_tokenize("alpha beta gamma", t, 64);
        int a = find(t, n, "alpha"), b = find(t, n, "beta"), c = find(t, n, "gamma");
        CHECK(a >= 0 && b >= 0 && c >= 0, "all three tokens are present");
        CHECK(t[a].position == 1 && t[b].position == 2 && t[c].position == 3,
              "and their POSITIONS are the ordinals (phrase search needs adjacency)");
    }

    /* -- a one-character term is dropped -- */
    {
        wb_track_token_t t[64];
        uint32_t n = wb_track_tokenize("a b c I", t, 64);
        CHECK(n == 0,
              "one-character terms are dropped (a one-char index is nearly useless)");
    }

    /* -- numbers are kept but FLAGGED -- */
    {
        wb_track_token_t t[64];
        uint32_t n = wb_track_tokenize("build 42 things", t, 64);
        int i = find(t, n, "42");
        CHECK(i >= 0, "a number is tokenized");
        CHECK(i >= 0 && t[i].type == WB_TRACK_TOKEN_NUMBER,
              "and FLAGGED as a number");
        int j = find(t, n, "build");
        CHECK(j >= 0 && t[j].type == WB_TRACK_TOKEN_WORD,
              "while a word is flagged as a word");
    }

    /* -- empty and null input -- */
    {
        wb_track_token_t t[8];
        CHECK(wb_track_tokenize("", t, 8) == 0, "an empty document yields nothing");
        CHECK(wb_track_tokenize("   \n\t  ", t, 8) == 0,
              "a whitespace-only document yields nothing");
        CHECK(wb_track_tokenize(NULL, t, 8) == 0, "a null document yields nothing");
        CHECK(wb_track_tokenize("text", NULL, 8) == 0, "a null array yields nothing");
        CHECK(wb_track_tokenize("text", t, 0) == 0, "a zero-size array yields nothing");
    }

    /* -- the array is bounded -- */
    {
        wb_track_token_t t[4];
        uint32_t n = wb_track_tokenize("aa bb cc dd ee ff", t, 4);
        CHECK(n == 4, "tokenizing stops at the array's bound");
    }

    /* -- a pathological long term is dropped -- */
    {
        char big[256];
        memset(big, 'x', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        wb_track_token_t t[8];
        uint32_t n = wb_track_tokenize(big, t, 8);
        CHECK(n == 0, "a 255-character 'word' is dropped, not indexed");
    }

    if (failures == 0) printf("ALL WUBU_TRACK TESTS PASSED\n");
    else printf("%d WUBU_TRACK FAILURES\n", failures);
    return failures ? 1 : 0;
}
