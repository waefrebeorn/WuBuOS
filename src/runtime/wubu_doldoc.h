/*
 * wubu_doldoc.h -- DolDoc: the TempleOS document format.
 *
 * DolDoc is TempleOS's document format: a fixed header followed by a flat list
 * of typed nodes (text, header, paragraph, link, image, list, code, table).
 * It is parsed by the TempleOS shell and by the "MySeed" document viewer, and a
 * MALFORMED document must be rejected rather than rendered, because the parser
 * runs with the user's privileges on untrusted input.
 *
 * The rules, all of them about refusing bad input:
 *
 *   * The MAGIC must match exactly ("DolD").  A wrong magic is a different
 *     format, and guessing lets a non-DolDoc file be parsed as one.
 *   * The VERSION must be known.  A future version is rejected rather than
 *     half-understood.
 *   * The document must not be TRUNCATED.  Every length field is checked
 *     against the bytes actually present; a length beyond the buffer is a
 *     rejection, never a read past the end.
 *   * The node count must be CONSISTENT with the data.  A count larger than
 *     the bytes can supply is truncation.
 *   * A node TYPE must be known.  An unknown type is rejected, not skipped or
 *     guessed -- a skipped node silently drops content.
 *   * A LINK node must have a TARGET.  An empty target is a link to nowhere.
 *   * Nesting DEPTH is capped, so a document cannot exhaust the stack.
 *   * The caller's node array is BOUNDED, and a document needing more nodes
 *     than that is refused rather than overflowing.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_DOLDOC_H
#define WUBU_DOLDOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_DOLDOC_MAGIC     "DolD"
#define WUBU_DOLDOC_VERSION   1u
/* the header size -- named ..._SIZE because WUBU_DOLDOC_HEADER is an enum
 * value for the header NODE TYPE, and a macro with that name would clobber it */
#define WUBU_DOLDOC_HEADER_SIZE 16u     /* magic[4] | version | node_count | flags */
#define WUBU_DOLDOC_TEXT_MAX  4096u
#define WUBU_DOLDOC_DEPTH_MAX 64u

typedef enum {
    WUBU_DOLDOC_TEXT = 0,
    WUBU_DOLDOC_HEADER,
    WUBU_DOLDOC_PARAGRAPH,
    WUBU_DOLDOC_LINK,
    WUBU_DOLDOC_IMAGE,
    WUBU_DOLDOC_LIST,
    WUBU_DOLDOC_CODE,
    WUBU_DOLDOC_TABLE,
    WUBU_DOLDOC_TYPE_COUNT
} wubu_doldoc_type_t;

const char *wubu_doldoc_type_name(wubu_doldoc_type_t t);

typedef struct {
    char     magic[4];
    uint32_t version;
    uint32_t node_count;
    uint32_t flags;
} wubu_doldoc_header_t;

typedef struct {
    uint32_t type;              /* wubu_doldoc_type_t */
    uint32_t flags;
    char     text[WUBU_DOLDOC_TEXT_MAX];
    uint32_t text_len;
    char     target[WUBU_DOLDOC_TEXT_MAX];
    uint32_t target_len;
    uint32_t child_count;
} wubu_doldoc_node_t;

/* Validate a header.  Returns NULL on success, else a static reason. */
const char *wubu_doldoc_check_header(const wubu_doldoc_header_t *h);

/* Parse a document into the caller's array.  Returns NULL on success, else a
 * static reason.  *out_n receives the node count. */
const char *wubu_doldoc_parse(const uint8_t *data, size_t len,
                              wubu_doldoc_node_t *nodes, uint32_t max,
                              uint32_t *out_n);

/* Is this a link node with a target? */
bool wubu_doldoc_is_usable_link(const wubu_doldoc_node_t *n);

#endif /* WUBU_DOLDOC_H */
