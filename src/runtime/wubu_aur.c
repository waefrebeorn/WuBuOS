/*
 * wubu_aur.c -- the AUR RPC client core.
 *
 * See wubu_aur.h.  The JSON is handled by the tree's existing parser; this
 * module adds the URL rules and the envelope contract.
 */
#include "wubu_aur.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_json.h"

const char *wubu_aur_type_name(wubu_aur_type_t t)
{
    switch (t) {
    case WUBU_AUR_INFO:        return "info";
    case WUBU_AUR_MULTIINFO:   return "multiinfo";
    case WUBU_AUR_SEARCH:      return "search";
    case WUBU_AUR_MULTISEARCH: return "multisearch";
    default:                   return "unknown";
    }
}

static bool url_safe(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9'))
        return true;
    /* RFC 3986 unreserved: - _ . ~ */
    return c == '-' || c == '_' || c == '.' || c == '~';
}

const char *wubu_aur_urlencode(const char *in, char *out, size_t n)
{
    if (!in) return "no input";
    if (!out || n == 0) return "no output buffer";

    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        if (url_safe(*p)) {
            if (o + 2 > n) return "the encoded string does not fit";
            out[o++] = (char)*p;
        } else {
            /* %XX, always with a leading zero: a bare %2 would be read as a
             * malformed escape rather than as a byte value. */
            if (o + 4 > n) return "the encoded string does not fit";
            static const char hex[] = "0123456789ABCDEF";
            out[o++] = '%';
            out[o++] = hex[*p >> 4];
            out[o++] = hex[*p & 0x0F];
        }
    }
    out[o] = '\0';
    return NULL;
}

/* Count the comma-separated names. */
static uint32_t count_names(const char *names)
{
    if (!names || !*names) return 0;
    uint32_t n = 1;
    for (const char *p = names; *p; p++) if (*p == ',') n++;
    return n;
}

uint32_t wubu_aur_split_count(const char *names)
{
    uint32_t total = count_names(names);
    if (total == 0) return 0;
    return (total + WUBU_AUR_MAX_NAMES - 1) / WUBU_AUR_MAX_NAMES;
}

const char *wubu_aur_build_info(char *out, size_t n, const char *names,
                                wubu_aur_type_t type)
{
    if (!out || n == 0) return "no output buffer";
    if (!names || !*names) return "an empty package list";
    if (type != WUBU_AUR_INFO && type != WUBU_AUR_MULTIINFO)
        return "not an info request";

    uint32_t total = count_names(names);
    if (total == 0) return "an empty package list";
    /* The AUR caps a multiinfo request.  Truncating would silently report the
     * tail of the list as ABSENT, so a long list is an error and the CALLER
     * must split it. */
    if (total > WUBU_AUR_MAX_NAMES)
        return "too many names for one request; split the list";

    char enc[WUBU_AUR_URL_MAX];
    const char *e = wubu_aur_urlencode(names, enc, sizeof(enc));
    if (e) return e;

    int w = snprintf(out, n, "https://aur.archlinux.org/rpc/v5/%s?arg[]=%s",
                     wubu_aur_type_name(type), enc);
    if (w < 0 || (size_t)w >= n) return "the URL does not fit";
    return NULL;
}

const char *wubu_aur_build_search(char *out, size_t n, const char *pattern,
                                  wubu_aur_by_t by)
{
    if (!out || n == 0) return "no output buffer";
    if (!pattern || !*pattern) return "an empty search pattern";

    const char *by_name;
    switch (by) {
    case WUBU_AUR_BY_NAME:        by_name = "name";        break;
    case WUBU_AUR_BY_NAME_DESC:   by_name = "name-desc";   break;
    case WUBU_AUR_BY_MAINTAINER:  by_name = "maintainer";  break;
    case WUBU_AUR_BY_DEPENDS:     by_name = "depends";     break;
    case WUBU_AUR_BY_KEYWORDS:    by_name = "keywords";    break;
    default:                      return "an unknown search field";
    }

    char enc[WUBU_AUR_URL_MAX];
    const char *e = wubu_aur_urlencode(pattern, enc, sizeof(enc));
    if (e) return e;

    int w = snprintf(out, n,
                     "https://aur.archlinux.org/rpc/v5/search/%s?by=%s&arg=%s",
                     enc, by_name, enc);
    if (w < 0 || (size_t)w >= n) return "the URL does not fit";
    return NULL;
}

/* -- the envelope --------------------------------------------------------- */

static bool json_is(JsonToken *t, JsonType type)
{
    return t && t->type == type;
}

const char *wubu_aur_check_envelope(const char *json, wubu_aur_envelope_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!json) return "no body";
    if (!*json) return "an empty body";
    if (!out) return "no output";

    const char *p = json;
    JsonToken *root = json_parse_value(&p);
    if (!root || !json_is(root, JSON_TYPE_OBJECT)) return "the body is not an object";

    /* -- the response type decides everything else -- */
    JsonToken *type = json_find(root, "type");
    if (!json_is(type, JSON_TYPE_STRING)) return "the response has no type";

    strncpy(out->type, type->str_value ? type->str_value : "", sizeof(out->type) - 1);
    out->type[sizeof(out->type) - 1] = '\0';

    if (strcmp(out->type, "error") == 0) {
        /* An error response is surfaced, never parsed as results. */
        out->is_error = true;
        JsonToken *msg = json_find(root, "error");
        if (json_is(msg, JSON_TYPE_STRING) && msg->str_value)
            strncpy(out->error, msg->str_value, WUBU_AUR_ERR_MAX - 1);
        out->valid = true;
        json_free(root);
        return NULL;
    }

    if (strcmp(out->type, "multiinfo") != 0 &&
        strcmp(out->type, "search") != 0 &&
        strcmp(out->type, "multisearch") != 0) {
        json_free(root);
        return "an unknown response type";
    }

    JsonToken *rc = json_find(root, "resultcount");
    if (!json_is(rc, JSON_TYPE_NUMBER) || rc->int_value < 0) {
        json_free(root);
        return "the response has no valid resultcount";
    }
    out->resultcount = (uint32_t)rc->int_value;

    /* The results actually present. */
    JsonToken *results = json_find(root, "results");
    out->n_results = json_is(results, JSON_TYPE_ARRAY) ? results->child_count : 0;

    /* A mismatch is a TRUNCATED BODY.  Trusting the count would let a partial
     * answer look complete -- the worst failure mode for a package lookup. */
    if (out->n_results != (int)out->resultcount) {
        json_free(root);
        return "resultcount disagrees with the results present (a truncated body)";
    }

    out->valid = true;
    json_free(root);
    return NULL;
}
