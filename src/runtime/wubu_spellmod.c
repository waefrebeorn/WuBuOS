/*
 * wubu_spellmod.c -- see wubu_spellmod.h for why this is the compiled form of
 * a `.spell` rather than a competing format.
 *
 * The parser is deliberately boring: fixed offsets, explicit little-endian
 * reads, one bounds check per field. Nothing here trusts the file's own counts
 * without first proving they fit inside the buffer.
 */

#include "wubu_spellmod.h"

#include <string.h>

#define WUSPELL_HDR_SIZE 128
#define WUSPELL_STR_SIZE 64
#define WUSPELL_EXPORT_SIZE 72     /* name[64] + offset(4) + size(4) */

struct wubu_spellmod {
    const uint8_t *data;
    size_t         len;

    char     name[WUBU_SPELLMOD_NAME_MAX];
    char     version[WUBU_SPELLMOD_STR_MAX];
    uint32_t provides_count;
    uint32_t needs_count;
    uint32_t export_count;
    uint32_t code_len;
    size_t   provides_off;
    size_t   needs_off;
    size_t   exports_off;
    size_t   code_off;
};

/* ---- little-endian readers ------------------------------------------- */

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Copy a fixed-width, NUL-padded name into `out` (which is exactly `width`
 * bytes). Returns -1 if the field contains no terminator at all, which means
 * the producer wrote an over-long name and we cannot trust its length. */
static int rd_name(const uint8_t *src, size_t width, char *out)
{
    size_t n = 0;
    while (n < width && src[n]) n++;
    if (n == width) return -1;          /* unterminated */
    memcpy(out, src, n);
    out[n] = '\0';
    return 0;
}

/* ---- parsing ---------------------------------------------------------- */

wubu_spellmod_t *wubu_spellmod_open(const uint8_t *data, size_t len)
{
    if (!data) return NULL;

    /* A file shorter than the header cannot be this format. */
    if (len < WUSPELL_HDR_SIZE) return NULL;
    if (memcmp(data, WUBU_SPELLMOD_MAGIC, 7) != 0 || data[7] != '\0')
        return NULL;

    uint32_t version = rd_u32(data + 8);
    if (version != WUBU_SPELLMOD_VERSION) return NULL;

    /* flags at 12; only bit 0 (has_code) is defined so far. Anything else set
     * is a format we do not understand -- refuse rather than misinterpret. */
    uint32_t flags = rd_u32(data + 12);
    if (flags & ~(uint32_t)1) return NULL;

    uint32_t np = rd_u32(data + 112);
    uint32_t nn = rd_u32(data + 116);
    uint32_t ne = rd_u32(data + 120);
    uint32_t cl = rd_u32(data + 124);

    if (np > WUBU_SPELLMOD_LIST_MAX || nn > WUBU_SPELLMOD_LIST_MAX ||
        ne > WUBU_SPELLMOD_EXPORT_MAX) return NULL;

    wubu_spellmod_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;

    m->data = data;
    m->len  = len;
    m->provides_count = np;
    m->needs_count    = nn;
    m->export_count   = ne;
    m->code_len       = cl;

    if (rd_name(data + 16, WUBU_SPELLMOD_NAME_MAX, m->name) != 0 ||
        rd_name(data + 80, WUBU_SPELLMOD_STR_MAX,  m->version) != 0) {
        free(m);
        return NULL;
    }

    m->provides_off = WUSPELL_HDR_SIZE;
    m->needs_off    = m->provides_off + (size_t)np * WUSPELL_STR_SIZE;
    m->exports_off  = m->needs_off + (size_t)nn * WUSPELL_STR_SIZE;
    m->code_off     = m->exports_off + (size_t)ne * WUSPELL_EXPORT_SIZE;

    /* Every region must fit whole.  Overflow-safe because each count is already
     * capped and the multipliers are small. */
    size_t end = m->code_off + cl;
    if (end > len) { free(m); return NULL; }
    if (!(flags & 1) && cl != 0) { free(m); return NULL; }   /* claimed none */

    return m;
}

void wubu_spellmod_close(wubu_spellmod_t *m)
{
    free(m);                        /* the image itself is borrowed, not owned */
}

int wubu_spellmod_info(const wubu_spellmod_t *m, wubu_spellmod_info_t *out)
{
    if (!m || !out) return -1;
    memset(out, 0, sizeof(*out));
    memcpy(out->name, m->name, sizeof(out->name));
    memcpy(out->version, m->version, sizeof(out->version));

    for (uint32_t i = 0; i < m->provides_count; i++) {
        const uint8_t *s = m->data + m->provides_off + (size_t)i * WUSPELL_STR_SIZE;
        if (rd_name(s, WUSPELL_STR_SIZE, out->provides[i]) != 0) return -1;
    }
    out->provides_count = m->provides_count;

    for (uint32_t i = 0; i < m->needs_count; i++) {
        const uint8_t *s = m->data + m->needs_off + (size_t)i * WUSPELL_STR_SIZE;
        if (rd_name(s, WUSPELL_STR_SIZE, out->needs[i]) != 0) return -1;
    }
    out->needs_count = m->needs_count;
    return 0;
}

int wubu_spellmod_export(const wubu_spellmod_t *m, const char *name,
                         uint32_t *offset, uint32_t *size)
{
    if (!m || !name || !*name) return -1;
    if (offset) *offset = 0;
    if (size)   *size   = 0;

    for (uint32_t i = 0; i < m->export_count; i++) {
        const uint8_t *e = m->data + m->exports_off + (size_t)i * WUSPELL_EXPORT_SIZE;
        char ename[WUSPELL_STR_SIZE];
        if (rd_name(e, WUSPELL_STR_SIZE, ename) != 0) continue;   /* skip junk */
        if (strcmp(ename, name) != 0) continue;

            uint32_t off = rd_u32(e + 64);
            uint32_t sz  = rd_u32(e + 68);

            /* An export must point INSIDE the code blob.  This is the check that
             * stops a lying module from sending a caller to a wild address. */
            if (m->code_len > 0 &&
                (off > m->code_len || sz > m->code_len - off)) return -1;

            if (offset) *offset = off;
            if (size)   *size   = sz;
            return 0;
    }
    return -1;
}

const uint8_t *wubu_spellmod_code(const wubu_spellmod_t *m, uint32_t *len)
{
    if (!m) return NULL;
    if (len) *len = m->code_len;
    return m->code_len ? m->data + m->code_off : NULL;
}

bool wubu_spellmod_provides(const wubu_spellmod_t *m, const char *capability)
{
    if (!m || !capability || !*capability) return false;
    for (uint32_t i = 0; i < m->provides_count; i++) {
        const uint8_t *s = m->data + m->provides_off + (size_t)i * WUSPELL_STR_SIZE;
        char c[WUSPELL_STR_SIZE];
        if (rd_name(s, WUSPELL_STR_SIZE, c) != 0) continue;
        if (strcmp(c, capability) == 0) return true;
    }
    return false;
}
