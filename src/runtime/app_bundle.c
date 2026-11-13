/*
 * app_bundle.c -- see app_bundle.h.  Parsing is fixed-offset and explicit;
 * launching is delegated to the exec layer that already owns every format.
 */

#include "app_bundle.h"
#include "wubu_exec.h"
#include "wubu_container.h"          /* WUBU_PAYLOAD_TYPE */

#include <string.h>

#define WUAPP_HDR_SIZE   128
#define WUAPP_NEED_SIZE  64

struct wubu_app_bundle {
    const uint8_t *data;
    size_t         len;

    char     name[WUBU_APP_NAME_MAX];
    char     version[WUBU_APP_STR_MAX];
    uint32_t personality;
    uint32_t needs_count;
    uint32_t entry_off;
    uint32_t entry_len;
    size_t   needs_off;
};

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Copy a NUL-padded fixed-width field. Returns -1 when the field has no
 * terminator, which means the producer wrote past the field's meaning. */
static int rd_name(const uint8_t *src, size_t width, char *out)
{
    size_t n = 0;
    while (n < width && src[n]) n++;
    if (n == width) return -1;
    memcpy(out, src, n);
    out[n] = '\0';
    return 0;
}

/* Personalities this tree can actually start. Anything else in a bundle is
 * refused rather than passed to an exec path that will guess wrong. */
static bool personality_supported(uint32_t p)
{
    switch (p) {
    case 0x01:                 /* native      */
    case 0x02:                 /* HolyC       */
    case 0x03:                 /* C source    */
    case 0x10:                 /* Linux ELF   */
    case 0x11:                 /* Windows PE  */
    case 0x12:                 /* Mach-O      */
    case 0x20:                 /* DOS         */
    case 0x21:                 /* WASM        */
        return true;
    default:
        return false;
    }
}

const char *wubu_app_personality_name(uint32_t p)
{
    switch (p) {
    case 0x01: return "native";
    case 0x02: return "holyc";
    case 0x03: return "c-source";
    case 0x10: return "linux-elf";
    case 0x11: return "windows-pe";
    case 0x12: return "macos-macho";
    case 0x20: return "dos";
    case 0x21: return "wasm";
    default:   return "unknown";
    }
}

wubu_app_bundle_t *wubu_app_open(const uint8_t *data, size_t len)
{
    if (!data) return NULL;
    if (len < WUAPP_HDR_SIZE) return NULL;
    if (memcmp(data, WUBU_APP_MAGIC, 7) != 0 || data[7] != '\0')
        return NULL;

    if (rd_u32(data + 8) != WUBU_APP_VERSION) return NULL;

    uint32_t flags = rd_u32(data + 12);
    if (flags & ~(uint32_t)1) return NULL;      /* only bit 0 defined */

    uint32_t personality = rd_u32(data + 16);
    uint32_t needs       = rd_u32(data + 116);
    uint32_t eoff        = rd_u32(data + 120);
    uint32_t elen        = rd_u32(data + 124);

    if (needs > WUBU_APP_NEEDS_MAX) return NULL;

    wubu_app_bundle_t *a = calloc(1, sizeof(*a));
    if (!a) return NULL;

    a->data = data;
    a->len  = len;
    a->personality = personality;
    a->needs_count = needs;
    a->entry_off   = eoff;
    a->entry_len   = elen;

    if (rd_name(data + 20, WUBU_APP_NAME_MAX, a->name) != 0 ||
        rd_name(data + 84, WUBU_APP_STR_MAX, a->version) != 0) {
        free(a);
        return NULL;
    }

    a->needs_off = WUAPP_HDR_SIZE;

    /* The needs table and the entry payload must both fit whole. */
    size_t needs_end = a->needs_off + (size_t)needs * WUAPP_NEED_SIZE;
    if (needs_end > len) { free(a); return NULL; }

    if (elen) {
        /* A bundle claiming an entry must carry all of it, from where it says
         * to where it says it ends. */
        if ((size_t)eoff > len || elen > len - (size_t)eoff) {
            free(a);
            return NULL;
        }
    } else if (eoff != 0) {
        free(a);                               /* no entry, but one claimed */
        return NULL;
    }

    return a;
}

void wubu_app_close(wubu_app_bundle_t *a)
{
    free(a);                     /* the image itself is borrowed */
}

int wubu_app_info(const wubu_app_bundle_t *a, wubu_app_info_t *out)
{
    if (!a || !out) return -1;
    memset(out, 0, sizeof(*out));
    memcpy(out->name, a->name, sizeof(out->name));
    memcpy(out->version, a->version, sizeof(out->version));
    out->personality = a->personality;
    return 0;
}

int wubu_app_needs(const wubu_app_bundle_t *a, char out[][WUBU_APP_NAME_MAX], int cap)
{
    if (!a || !out || cap <= 0) return -1;
    int n = 0;
    for (uint32_t i = 0; i < a->needs_count && n < cap; i++) {
        const uint8_t *s = a->data + a->needs_off + (size_t)i * WUAPP_NEED_SIZE;
        if (rd_name(s, WUAPP_NEED_SIZE, out[n]) != 0) continue;   /* skip junk */
        n++;
    }
    return n;
}

const uint8_t *wubu_app_entry(const wubu_app_bundle_t *a, uint32_t *len)
{
    if (!a || a->entry_len == 0) return NULL;
    if (len) *len = a->entry_len;
    return a->data + a->entry_off;
}

int wubu_app_launch(const wubu_app_bundle_t *a)
{
    if (!a) return WUBU_EXEC_ERR_FMT;

    uint32_t elen = 0;
    const uint8_t *entry = wubu_app_entry(a, &elen);
    if (!entry) return WUBU_EXEC_ERR_FMT;       /* nothing to run */
    if (!personality_supported(a->personality))
        return WUBU_EXEC_ERR_FMT;               /* we cannot run this */

    /* Delegate: the exec layer owns every format and detects by content, so
     * the bundle only decides WHAT to hand over, never HOW to run it. */
    int64_t rc = wubu_exec(entry, (size_t)elen, a->name);
    return (int)rc;
}
