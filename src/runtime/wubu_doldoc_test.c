/*
 * wubu_doldoc_test.c -- the DolDoc document parser.
 *
 * The load-bearing rules: a bad magic is a different format (never guessed), a
 * length field beyond the document is a REJECTION rather than a read past the
 * end, an unknown node type is refused rather than skipped, and a link with no
 * target is a link to nowhere.
 */
#include "wubu_doldoc.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* Build a document body into `buf`.  Returns the length. */
static size_t put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)(v >> 24);
    return 4;
}

static size_t make_doc(uint8_t *buf, uint32_t version, uint32_t node_count,
                       const void *body, size_t body_len)
{
    size_t o = 0;
    memcpy(buf, WUBU_DOLDOC_MAGIC, 4);
    o = 4;
    o += put_u32(buf + o, version);
    o += put_u32(buf + o, node_count);
    o += put_u32(buf + o, 0);            /* flags */
    if (body && body_len) {
        memcpy(buf + o, body, body_len);
        o += body_len;
    }
    return o;
}

/* One node: type, flags, text_len, text, child_count, target_len, target */
static size_t put_node(uint8_t *p, uint32_t type, uint32_t flags,
                       const char *text, uint32_t children, const char *target)
{
    size_t o = 0;
    o += put_u32(p + o, type);
    o += put_u32(p + o, flags);
    uint32_t tl = text ? (uint32_t)strlen(text) : 0;
    o += put_u32(p + o, tl);
    if (tl) { memcpy(p + o, text, tl); o += tl; }
    o += put_u32(p + o, children);
    uint32_t gl = target ? (uint32_t)strlen(target) : 0;
    o += put_u32(p + o, gl);
    if (gl) { memcpy(p + o, target, gl); o += gl; }
    return o;
}

int main(void)
{
    printf("=== test_wubu_doldoc ===\n");
    wubu_doldoc_header_t h;
    memcpy(h.magic, WUBU_DOLDOC_MAGIC, 4);
    h.version = WUBU_DOLDOC_VERSION;
    h.node_count = 1;
    h.flags = 0;
    CHECK(wubu_doldoc_check_header(&h) == NULL, "a normal header is valid");

    memcpy(h.magic, "DolX", 4);
    CHECK(strcmp(wubu_doldoc_check_header(&h),
                 "a bad magic (not a DolDoc document)") == 0,
          "a WRONG MAGIC is refused (never guessed)");
    memcpy(h.magic, WUBU_DOLDOC_MAGIC, 4);

    h.version = 99;
    CHECK(strcmp(wubu_doldoc_check_header(&h),
                 "an unknown DolDoc version") == 0,
          "a FUTURE version is refused rather than half-understood");
    h.version = WUBU_DOLDOC_VERSION;
    CHECK(wubu_doldoc_check_header(NULL) != NULL, "a null header is refused");

    /* -- a valid document -- */
    {
        uint8_t body[512], doc[1024];
        size_t o = 0;
        o += put_node(body + o, WUBU_DOLDOC_HEADER, 0, "Title", 0, NULL);
        o += put_node(body + o, WUBU_DOLDOC_PARAGRAPH, 0, "Hello", 0, NULL);
        o += put_node(body + o, WUBU_DOLDOC_LINK, 0, "click", 0, "http://x");
        size_t dl = make_doc(doc, 1, 3, body, o);

        wubu_doldoc_node_t nodes[8];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, dl, nodes, 8, &n) == NULL, "a document parses");
        CHECK(n == 3, "three nodes");
        CHECK(nodes[0].type == WUBU_DOLDOC_HEADER &&
              strcmp(nodes[0].text, "Title") == 0, "the header node");
        CHECK(nodes[2].type == WUBU_DOLDOC_LINK &&
              strcmp(nodes[2].target, "http://x") == 0, "the link target");
        CHECK(wubu_doldoc_is_usable_link(&nodes[2]), "and it is a usable link");
        CHECK(!wubu_doldoc_is_usable_link(&nodes[0]), "a header is not a link");
        CHECK(strcmp(wubu_doldoc_type_name(WUBU_DOLDOC_LINK), "link") == 0,
              "the node types have names");
        CHECK(strcmp(wubu_doldoc_type_name((wubu_doldoc_type_t)99), "unknown") == 0,
              "and an unknown type has a name");
    }

    /* -- truncation at every point is refused -- */
    {
        uint8_t body[256], doc[1024];
        size_t o = put_node(body, WUBU_DOLDOC_TEXT, 0, "some text", 0, NULL);
        size_t dl = make_doc(doc, 1, 1, body, o);

        wubu_doldoc_node_t nodes[4];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, WUBU_DOLDOC_HEADER - 1, nodes, 4, &n) != NULL,
              "a truncated HEADER is refused");
        CHECK(wubu_doldoc_parse(doc, dl - 1, nodes, 4, &n) != NULL,
              "a document cut by one byte is refused");
        CHECK(wubu_doldoc_parse(doc, 0, nodes, 4, &n) != NULL,
              "an empty document is refused");
        CHECK(wubu_doldoc_parse(NULL, dl, nodes, 4, &n) != NULL,
              "a null buffer is refused");
        CHECK(wubu_doldoc_parse(doc, dl, NULL, 0, &n) != NULL,
              "a null node array is refused");
        CHECK(n == 0, "and nothing is reported as parsed");
    }

    /* -- a length field beyond the document is a REJECTION -- */
    {
        uint8_t doc[64];
        size_t o = 0;
        memcpy(doc, WUBU_DOLDOC_MAGIC, 4); o = 4;
        o += put_u32(doc + o, 1);            /* version */
        o += put_u32(doc + o, 1);            /* node count */
        o += put_u32(doc + o, 0);            /* flags */
        o += put_u32(doc + o, WUBU_DOLDOC_TEXT);   /* type */
        o += put_u32(doc + o, 0);            /* flags */
        o += put_u32(doc + o, 100000);       /* a LIE about the text length */
        wubu_doldoc_node_t nodes[4];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, o, nodes, 4, &n) != NULL,
              "a text length past the end is refused, never read");
    }

    /* -- an unknown node type is refused, not skipped -- */
    {
        uint8_t body[64], doc[256];
        size_t o = put_node(body, 77, 0, "x", 0, NULL);
        size_t dl = make_doc(doc, 1, 1, body, o);
        wubu_doldoc_node_t nodes[4];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, dl, nodes, 4, &n) != NULL,
              "an UNKNOWN node type is refused (skipping would drop content)");
    }

    /* -- a link with no target -- */
    {
        uint8_t body[64], doc[256];
        size_t o = put_node(body, WUBU_DOLDOC_LINK, 0, "click", 0, NULL);
        size_t dl = make_doc(doc, 1, 1, body, o);
        wubu_doldoc_node_t nodes[4];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, dl, nodes, 4, &n) != NULL,
              "a LINK with an EMPTY TARGET is refused");
    }

    /* -- nesting depth is capped -- */
    {
        uint8_t body[64], doc[256];
        size_t o = put_node(body, WUBU_DOLDOC_LIST, 0, "x",
                            WUBU_DOLDOC_DEPTH_MAX + 1, NULL);
        size_t dl = make_doc(doc, 1, 1, body, o);
        wubu_doldoc_node_t nodes[4];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, dl, nodes, 4, &n) != NULL,
              "a nesting depth beyond the maximum is refused");
    }

    /* -- the caller's array is bounded -- */
    {
        uint8_t body[256], doc[512];
        size_t o = 0;
        for (int i = 0; i < 5; i++)
            o += put_node(body + o, WUBU_DOLDOC_TEXT, 0, "x", 0, NULL);
        size_t dl = make_doc(doc, 1, 5, body, o);
        wubu_doldoc_node_t nodes[2];
        uint32_t n = 0;
        CHECK(wubu_doldoc_parse(doc, dl, nodes, 2, &n) != NULL,
              "a document needing more nodes than the array is refused");
    }

    if (failures == 0) printf("ALL WUBU_DOLDOC TESTS PASSED\n");
    else printf("%d WUBU_DOLDOC FAILURES\n", failures);
    return failures ? 1 : 0;
}
