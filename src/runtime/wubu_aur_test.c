/*
 * wubu_aur_test.c -- the AUR RPC client core.
 *
 * The load-bearing rules: a name with '&' or '+' must be ENCODED (otherwise it
 * becomes a second query parameter -- a request-injection bug, not a formatting
 * nicety), a long list must be SPLIT rather than truncated, an error response
 * must be surfaced rather than parsed as results, and a resultcount that
 * disagrees with the body is refused as a truncation.
 */
#include "wubu_aur.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_aur ===\n");
    char url[WUBU_AUR_URL_MAX];
    char enc[512];
    wubu_aur_envelope_t env;

    /* -- URL encoding -- */
    CHECK(wubu_aur_urlencode("foo", enc, sizeof(enc)) == NULL &&
          strcmp(enc, "foo") == 0, "an unreserved string is unchanged");
    CHECK(wubu_aur_urlencode("a b", enc, sizeof(enc)) == NULL &&
          strcmp(enc, "a%20b") == 0, "a space becomes %20");
    CHECK(wubu_aur_urlencode("a+b", enc, sizeof(enc)) == NULL &&
          strcmp(enc, "a%2Bb") == 0,
          "'+' is ENCODED -- leaving it would make the server read a space");
    CHECK(wubu_aur_urlencode("a&b=c", enc, sizeof(enc)) == NULL &&
          strcmp(enc, "a%26b%3Dc") == 0,
          "'&' and '=' are ENCODED -- leaving them would inject a parameter");
    CHECK(wubu_aur_urlencode("A-Z_a.z~9", enc, sizeof(enc)) == NULL &&
          strcmp(enc, "A-Z_a.z~9") == 0, "RFC 3986 unreserved characters pass");
    CHECK(wubu_aur_urlencode("café", enc, sizeof(enc)) == NULL &&
          strcmp(enc, "caf%C3%A9") == 0, "a UTF-8 byte is encoded as %XX");
    CHECK(wubu_aur_urlencode(NULL, enc, sizeof(enc)) != NULL, "a null input is refused");
    CHECK(wubu_aur_urlencode("x", NULL, 0) != NULL, "a null output is refused");
    CHECK(wubu_aur_urlencode("xxxx", enc, 3) != NULL,
          "a buffer too small is refused (no silent truncation)");

    /* -- request building -- */
    CHECK(wubu_aur_build_info(url, sizeof(url), "foo", WUBU_AUR_INFO) == NULL,
          "an info request builds");
    CHECK(strstr(url, "arg[]=foo") != NULL, "with the name as a query argument");
    CHECK(strstr(url, "/info?") != NULL, "at the info endpoint");
    CHECK(wubu_aur_build_info(url, sizeof(url), "foo,bar", WUBU_AUR_MULTIINFO) == NULL,
          "a multiinfo request builds");
    CHECK(strstr(url, "/multiinfo?") != NULL, "at the multiinfo endpoint");
    /* The AUR's own documented API uses the literal 'arg[]' parameter name;
     * it is the VALUE that must be encoded. */
    CHECK(strstr(url, "arg[]=foo%2Cbar") != NULL,
          "the 'arg[]' name is literal (as the AUR documents) and the VALUE is encoded");
    CHECK(wubu_aur_build_info(url, sizeof(url), "foo&bar=baz", WUBU_AUR_INFO) == NULL &&
          strstr(url, "arg[]=foo%26bar%3Dbaz") != NULL,
          "a hostile name cannot inject a parameter (the value is encoded)");
    CHECK(wubu_aur_build_info(url, sizeof(url), "foo&bar=baz", WUBU_AUR_INFO) == NULL &&
          strstr(url, "&bar=") == NULL,
          "and no second parameter was created");
    CHECK(wubu_aur_build_info(url, sizeof(url), "", WUBU_AUR_INFO) != NULL,
          "an empty name list is refused");
    CHECK(wubu_aur_build_info(url, sizeof(url), NULL, WUBU_AUR_INFO) != NULL,
          "a null name list is refused");
    CHECK(wubu_aur_build_info(url, sizeof(url), "foo", WUBU_AUR_SEARCH) != NULL,
          "an info request cannot use the search type");

    /* -- a long list must be SPLIT, not truncated -- */
    {
        char big[WUBU_AUR_URL_MAX];
        int o = 0;
        for (int i = 0; i < WUBU_AUR_MAX_NAMES + 1; i++)
            o += snprintf(big + o, sizeof(big) - (size_t)o, "%sp%d", i ? "," : "", i);
        CHECK(wubu_aur_split_count(big) == 2,
              "an over-long list needs two requests");
        CHECK(wubu_aur_build_info(url, sizeof(url), big, WUBU_AUR_MULTIINFO) != NULL,
              "and building one oversized request is refused -- truncating would "
              "report the tail as ABSENT");
        CHECK(wubu_aur_split_count("a") == 1, "one name needs one request");
        CHECK(wubu_aur_split_count("") == 0, "no names need no request");
        /* exactly at the cap is one request */
        char at_cap[WUBU_AUR_URL_MAX];
        o = 0;
        for (int i = 0; i < WUBU_AUR_MAX_NAMES; i++)
            o += snprintf(at_cap + o, sizeof(at_cap) - (size_t)o, "%sp%d", i ? "," : "", i);
        CHECK(wubu_aur_split_count(at_cap) == 1,
              "exactly at the cap is still one request");
    }

    /* -- search -- */
    CHECK(wubu_aur_build_search(url, sizeof(url), "libre", WUBU_AUR_BY_NAME_DESC) == NULL,
          "a search builds");
    CHECK(strstr(url, "/search/libre?") != NULL, "with the pattern in the path");
    CHECK(strstr(url, "by=name-desc") != NULL, "and the field as a query parameter");
    CHECK(wubu_aur_build_search(url, sizeof(url), "a b&c", WUBU_AUR_BY_NAME) == NULL &&
          strstr(url, "/search/a%20b%26c?") != NULL,
          "the pattern is encoded in the path too");
    CHECK(wubu_aur_build_search(url, sizeof(url), "", WUBU_AUR_BY_NAME) != NULL,
          "an empty pattern is refused");
    CHECK(wubu_aur_build_search(url, sizeof(url), "x", (wubu_aur_by_t)99) != NULL,
          "an unknown search field is refused");

    /* -- the envelope: a good response -- */
    CHECK(wubu_aur_check_envelope(
              "{\"resultcount\":2,\"type\":\"multiinfo\",\"results\":[{\"Name\":\"a\"},{\"Name\":\"b\"}]}",
              &env) == NULL, "a good envelope validates");
    CHECK(env.valid && !env.is_error, "and is not an error");
    CHECK(env.resultcount == 2 && env.n_results == 2,
          "the count agrees with the results");
    CHECK(strcmp(env.type, "multiinfo") == 0, "the type is recorded");

    CHECK(wubu_aur_check_envelope(
              "{\"resultcount\":0,\"type\":\"multiinfo\",\"results\":[]}", &env) == NULL,
          "an empty result set is valid");
    CHECK(env.resultcount == 0 && env.n_results == 0, "with zero results");

    /* -- an error response is surfaced, not parsed as results -- */
    CHECK(wubu_aur_check_envelope(
              "{\"type\":\"error\",\"error\":\"Too many args\",\"results\":[],\"resultcount\":0}",
              &env) == NULL, "an error envelope validates");
    CHECK(env.is_error, "and is flagged as an error");
    CHECK(strcmp(env.error, "Too many args") == 0,
          "with the server's message preserved");
    CHECK(strcmp(env.type, "error") == 0, "and the type is 'error'");

    /* -- a truncated body is REFUSED -- */
    CHECK(wubu_aur_check_envelope(
              "{\"resultcount\":5,\"type\":\"multiinfo\",\"results\":[{\"Name\":\"a\"}]}",
              &env) != NULL,
          "resultcount disagreeing with the results is refused (a truncated body)");
    CHECK(wubu_aur_check_envelope(
              "{\"resultcount\":0,\"type\":\"multiinfo\",\"results\":[{\"Name\":\"a\"}]}",
              &env) != NULL, "and so is a smaller count than the results present");

    /* -- malformed bodies -- */
    CHECK(wubu_aur_check_envelope(NULL, &env) != NULL, "a null body is refused");
    CHECK(wubu_aur_check_envelope("", &env) != NULL, "an empty body is refused");
    CHECK(wubu_aur_check_envelope("not json", &env) != NULL, "a non-JSON body is refused");
    CHECK(wubu_aur_check_envelope("[]", &env) != NULL, "a non-object body is refused");
    CHECK(wubu_aur_check_envelope("{\"results\":[]}", &env) != NULL,
          "a body with no type is refused");
    CHECK(wubu_aur_check_envelope("{\"type\":\"multiinfo\"}", &env) != NULL,
          "a body with no resultcount is refused");
    CHECK(wubu_aur_check_envelope(
              "{\"type\":\"multiinfo\",\"resultcount\":-1,\"results\":[]}", &env) != NULL,
          "a negative resultcount is refused");
    CHECK(wubu_aur_check_envelope(
              "{\"type\":\"bogus\",\"resultcount\":0,\"results\":[]}", &env) != NULL,
          "an unknown response type is refused");
    CHECK(wubu_aur_check_envelope(
              "{\"type\":\"multiinfo\",\"resultcount\":1}", &env) != NULL,
          "a missing results array is refused (count 1, none present)");

    CHECK(strcmp(wubu_aur_type_name(WUBU_AUR_INFO), "info") == 0,
          "the types have readable names");
    CHECK(strcmp(wubu_aur_type_name((wubu_aur_type_t)99), "unknown") == 0,
          "and an unknown type has a name");

    if (failures == 0) printf("ALL WUBU_AUR TESTS PASSED\n");
    else printf("%d WUBU_AUR FAILURES\n", failures);
    return failures ? 1 : 0;
}
