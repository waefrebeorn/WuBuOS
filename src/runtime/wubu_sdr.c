/*
 * wubu_sdr.c -- Steam Datagram Relay: routing tickets and virtual routes.
 *
 * See wubu_sdr.h.
 */
#include "wubu_sdr.h"

#include <string.h>

/* -- a tiny base64 (no dependency, and the alphabet is fixed) -------------- */

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* FNV-1a over the canonical bytes -- a ticket must be tamper-evident. */
static uint32_t ticket_checksum(const wubu_sdr_ticket_t *t)
{
    const unsigned char *p = (const unsigned char *)t;
    uint32_t h = 2166136261u;
    /* Copy the FIELDS into a zeroed struct: a raw memcpy would carry the
     * source's padding bytes, and padding is unspecified -- the checksum
     * would then differ between encode and decode for an identical ticket. */
    wubu_sdr_ticket_t c;
    memset(&c, 0, sizeof(c));
    c.pop_id = t->pop_id;
    c.src = t->src;
    c.dst = t->dst;
    c.issued_at = t->issued_at;
    c.expires_at = t->expires_at;
    p = (const unsigned char *)&c;
    for (size_t i = 0; i < sizeof(c); i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

const char *wubu_sdr_ticket_validate(const wubu_sdr_ticket_t *t)
{
    if (!t) return "no ticket";
    if (t->pop_id == 0) return "a ticket must name a POP";
    if (t->src.vip == 0 || t->dst.vip == 0) return "a virtual address is zero";
    if (t->src.vport == 0 || t->dst.vport == 0) return "a virtual port is zero";
    if (t->expires_at < t->issued_at) return "the ticket expires before it is issued";
    if (t->expires_at - t->issued_at > WUBU_SDR_MAX_TICKET_WINDOW)
        return "the ticket window is longer than the maximum";
    return NULL;
}

long wubu_sdr_ticket_encode(const wubu_sdr_ticket_t *t, char *out, size_t n)
{
    if (!t || !out) return -1;
    if (wubu_sdr_ticket_validate(t) != NULL) return -1;

    /* canonical little-endian payload + a 4-byte checksum */
    unsigned char payload[44];
    memset(payload, 0, sizeof(payload));
    uint32_t pop = t->pop_id;
    payload[0] = (unsigned char)pop;
    payload[1] = (unsigned char)(pop >> 8);
    payload[2] = (unsigned char)(pop >> 16);
    payload[3] = (unsigned char)(pop >> 24);
    payload[4] = (unsigned char)t->src.vip;
    payload[5] = (unsigned char)(t->src.vip >> 8);
    payload[6] = (unsigned char)(t->src.vip >> 16);
    payload[7] = (unsigned char)(t->src.vip >> 24);
    payload[8] = (unsigned char)t->src.vport;
    payload[9] = (unsigned char)(t->src.vport >> 8);
    payload[10] = (unsigned char)t->dst.vip;
    payload[11] = (unsigned char)(t->dst.vip >> 8);
    payload[12] = (unsigned char)(t->dst.vip >> 16);
    payload[13] = (unsigned char)(t->dst.vip >> 24);
    payload[14] = (unsigned char)t->dst.vport;
    payload[15] = (unsigned char)(t->dst.vport >> 8);
    for (int i = 0; i < 8; i++) {
        payload[16 + i] = (unsigned char)(t->issued_at >> (i * 8));
        payload[24 + i] = (unsigned char)(t->expires_at >> (i * 8));
    }
    uint32_t sum = ticket_checksum(t);
    payload[32] = (unsigned char)sum;
    payload[33] = (unsigned char)(sum >> 8);
    payload[34] = (unsigned char)(sum >> 16);
    payload[35] = (unsigned char)(sum >> 24);

    /* 36 bytes -> 48 base64 chars (no padding needed: 36 is a multiple of 3) */
    size_t need = 48;
    if (n < need + 1) return -1;
    size_t o = 0;
    for (size_t i = 0; i < 36; i += 3) {
        uint32_t v = (uint32_t)payload[i] |
                     ((uint32_t)payload[i + 1] << 8) |
                     ((uint32_t)payload[i + 2] << 16);
        out[o++] = B64[(v >> 0) & 63];
        out[o++] = B64[(v >> 6) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = B64[(v >> 18) & 63];
    }
    out[o] = '\0';
    return (long)o;
}

bool wubu_sdr_ticket_decode(const char *in, wubu_sdr_ticket_t *out)
{
    if (!in || !out) return false;

    size_t len = strlen(in);
    if (len != 48) return false;             /* a different length is garbage */

    unsigned char payload[36];
    memset(payload, 0, sizeof(payload));
    size_t p = 0;
    for (size_t i = 0; i < len; i += 4) {
        int a = b64_value(in[i]);
        int b = b64_value(in[i + 1]);
        int c = b64_value(in[i + 2]);
        int d = b64_value(in[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return false;
        uint32_t v = (uint32_t)a | ((uint32_t)b << 6) |
                     ((uint32_t)c << 12) | ((uint32_t)d << 18);
        payload[p++] = (unsigned char)(v & 0xFF);
        payload[p++] = (unsigned char)((v >> 8) & 0xFF);
        payload[p++] = (unsigned char)((v >> 16) & 0xFF);
    }

    uint32_t sum = (uint32_t)payload[32] | ((uint32_t)payload[33] << 8) |
                   ((uint32_t)payload[34] << 16) | ((uint32_t)payload[35] << 24);

    memset(out, 0, sizeof(*out));
    out->pop_id = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                  ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
    out->src.vip = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) |
                   ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
    out->src.vport = (uint16_t)((uint32_t)payload[8] | ((uint32_t)payload[9] << 8));
    out->dst.vip = (uint32_t)payload[10] | ((uint32_t)payload[11] << 8) |
                   ((uint32_t)payload[12] << 16) | ((uint32_t)payload[13] << 24);
    out->dst.vport = (uint16_t)((uint32_t)payload[14] | ((uint32_t)payload[15] << 8));
    for (int i = 0; i < 8; i++) {
        out->issued_at |= (uint64_t)payload[16 + i] << (i * 8);
        out->expires_at |= (uint64_t)payload[24 + i] << (i * 8);
    }

    /* The checksum covers the WHOLE struct, and the struct was zeroed before
     * the fields were filled, so it must match exactly. */
    if (ticket_checksum(out) != sum) return false;
    /* A well-formed ticket must also be structurally valid. */
    if (wubu_sdr_ticket_validate(out) != NULL) return false;
    return true;
}

bool wubu_sdr_ticket_is_valid(const wubu_sdr_ticket_t *t, uint64_t now)
{
    if (!t || wubu_sdr_ticket_validate(t) != NULL) return false;
    if (now < t->issued_at) return false;      /* not yet valid */
    if (now >= t->expires_at) return false;    /* expired */
    return true;
}

bool wubu_sdr_ticket_matches(const wubu_sdr_ticket_t *t,
                             const wubu_sdr_addr_t *src,
                             const wubu_sdr_addr_t *dst)
{
    if (!t || !src || !dst) return false;
    return t->src.vip == src->vip && t->src.vport == src->vport &&
           t->dst.vip == dst->vip && t->dst.vport == dst->vport;
}

/* -- the route table ------------------------------------------------------ */

void wubu_sdr_table_init(wubu_sdr_table_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

int wubu_sdr_table_add(wubu_sdr_table_t *t, const wubu_sdr_route_t *r)
{
    if (!t || !r) return -1;
    if (t->count >= WUBU_SDR_MAX_ROUTES) return -1;
    /* A virtual address may be claimed ONCE.  A duplicate is a hijack attempt,
     * not an update -- an update goes through set_ready. */
    if (wubu_sdr_table_find(t, &r->addr) != NULL) return -1;
    t->routes[t->count++] = *r;
    return 0;
}

const wubu_sdr_route_t *wubu_sdr_table_find(const wubu_sdr_table_t *t,
                                            const wubu_sdr_addr_t *a)
{
    if (!t || !a) return NULL;
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->routes[i].addr.vip == a->vip && t->routes[i].addr.vport == a->vport)
            return &t->routes[i];
    }
    return NULL;
}

int wubu_sdr_table_set_ready(wubu_sdr_table_t *t,
                             const wubu_sdr_addr_t *a, bool ready)
{
    if (!t || !a) return -1;
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->routes[i].addr.vip == a->vip &&
            t->routes[i].addr.vport == a->vport) {
            t->routes[i].ready = ready;
            return 0;
        }
    }
    return -1;
}

uint32_t wubu_sdr_table_ready_count(const wubu_sdr_table_t *t)
{
    if (!t) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < t->count; i++) if (t->routes[i].ready) n++;
    return n;
}
