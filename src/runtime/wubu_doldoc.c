/*
 * wubu_doldoc.c -- DolDoc: the TempleOS document format.
 *
 * See wubu_doldoc.h.  Every read is bounds-checked against `len`; there is no
 * path through this parser that reads past the end of the document.
 */
#include "wubu_doldoc.h"

#include <string.h>

const char *wubu_doldoc_type_name(wubu_doldoc_type_t t)
{
    static const char *names[WUBU_DOLDOC_TYPE_COUNT] = {
        "text", "header", "paragraph", "link",
        "image", "list", "code", "table"
    };
    if (t < 0 || t >= WUBU_DOLDOC_TYPE_COUNT) return "unknown";
    return names[t];
}

const char *wubu_doldoc_check_header(const wubu_doldoc_header_t *h)
{
    if (!h) return "no header";
    /* The magic is compared with memcmp, not strcmp: it is four arbitrary
     * bytes and need not be NUL-terminated. */
    if (memcmp(h->magic, WUBU_DOLDOC_MAGIC, 4) != 0)
        return "a bad magic (not a DolDoc document)";
    /* A FUTURE version is rejected rather than half-understood: silently
     * misreading a newer format is worse than refusing it. */
    if (h->version != WUBU_DOLDOC_VERSION)
        return "an unknown DolDoc version";
    return NULL;
}

/* A bounds-checked little-endian u32.  Returns false when it does not fit. */
static bool read_u32(const uint8_t *data, size_t len, size_t *pos, uint32_t *out)
{
    if (*pos + 4 > len) return false;
    *out = (uint32_t)data[*pos] |
           ((uint32_t)data[*pos + 1] << 8) |
           ((uint32_t)data[*pos + 2] << 16) |
           ((uint32_t)data[*pos + 3] << 24);
    *pos += 4;
    return true;
}

const char *wubu_doldoc_parse(const uint8_t *data, size_t len,
                              wubu_doldoc_node_t *nodes, uint32_t max,
                              uint32_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!data) return "no data";
    if (!nodes || max == 0) return "no output array";
    if (len < WUBU_DOLDOC_HEADER_SIZE) return "a truncated header";

    wubu_doldoc_header_t h;
    memcpy(h.magic, data, 4);
    h.version    = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                   ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    h.node_count = (uint32_t)data[8] | ((uint32_t)data[9] << 8) |
                   ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
    h.flags      = (uint32_t)data[12] | ((uint32_t)data[13] << 8) |
                   ((uint32_t)data[14] << 16) | ((uint32_t)data[15] << 24);

    const char *e = wubu_doldoc_check_header(&h);
    if (e) return e;

    /* The caller's array is BOUNDED.  A document needing more nodes than the
     * array can hold is refused rather than overflowing it. */
    if (h.node_count > max)
        return "the document needs more nodes than the array can hold";

    size_t pos = WUBU_DOLDOC_HEADER_SIZE;
    for (uint32_t i = 0; i < h.node_count; i++) {
        wubu_doldoc_node_t *n = &nodes[i];
        memset(n, 0, sizeof(*n));

        uint32_t type, flags, text_len, child_count, target_len;
        if (!read_u32(data, len, &pos, &type)) return "a truncated node type";
        /* An UNKNOWN TYPE is rejected, not skipped: skipping silently drops
         * content, and guessing renders the wrong thing. */
        if (type >= WUBU_DOLDOC_TYPE_COUNT) return "an unknown node type";
        if (!read_u32(data, len, &pos, &flags)) return "a truncated node flags";

        if (!read_u32(data, len, &pos, &text_len)) return "a truncated text length";
        /* A length field beyond the buffer is a REJECTION, never a read past
         * the end.  The cap also stops an absurd length from being trusted. */
        if (text_len >= WUBU_DOLDOC_TEXT_MAX) return "a text length beyond the maximum";
        if (pos + text_len > len) return "the text runs past the end of the document";
        memcpy(n->text, data + pos, text_len);
        n->text[text_len] = '\0';
        n->text_len = text_len;
        pos += text_len;

        if (!read_u32(data, len, &pos, &child_count))
            return "a truncated child count";
        /* DEPTH IS CAPPED: a document cannot exhaust the stack. */
        if (child_count > WUBU_DOLDOC_DEPTH_MAX)
            return "a nesting depth beyond the maximum";

        if (!read_u32(data, len, &pos, &target_len)) return "a truncated target length";
        if (target_len >= WUBU_DOLDOC_TEXT_MAX) return "a target length beyond the maximum";
        if (pos + target_len > len) return "the target runs past the end of the document";
        memcpy(n->target, data + pos, target_len);
        n->target[target_len] = '\0';
        n->target_len = target_len;
        pos += target_len;

        n->type = type;
        n->flags = flags;
        n->child_count = child_count;

        /* A LINK node with no target is a link to nowhere. */
        if (type == WUBU_DOLDOC_LINK && target_len == 0)
            return "a link node with an empty target";
    }

    /* Trailing bytes are tolerated (a document may carry a footer), but the
     * node count must not have UNDERSTATED what is present. */
    if (out_n) *out_n = h.node_count;
    return NULL;
}

bool wubu_doldoc_is_usable_link(const wubu_doldoc_node_t *n)
{
    if (!n) return false;
    return n->type == WUBU_DOLDOC_LINK && n->target_len > 0;
}
