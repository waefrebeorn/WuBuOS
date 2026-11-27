/* wubu_clip.c -- see wubu_clip.h. */
#include "wubu_clip.h"
#include <string.h>

void wubu_clip_init(WubuClip *c) {
    if (!c) return;
    memset(c, 0, sizeof *c);
}

int wubu_clip_mime_ok(const char *mime) {
    if (!mime) return 0;
    size_t n = strlen(mime);
    if (n == 0 || n >= WUBU_CLIP_MIME_MAX) return 0;
    const char *slash = strchr(mime, '/');
    if (!slash || slash == mime || slash[1] == '\0') return 0;
    if (strchr(slash + 1, '/')) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)mime[i];
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                 ch == '/' || ch == '-' || ch == '+' || ch == '.' || ch == '_';
        if (!ok) return 0;
    }
    return 1;
}

int wubu_clip_set(WubuClip *c, const char *mime, const void *data, size_t len) {
    if (!c || !mime) return WUBU_CLIP_ARGS;
    if (!data && len) return WUBU_CLIP_ARGS;
    if (!wubu_clip_mime_ok(mime)) return WUBU_CLIP_BAD_MIME;

    for (int i = 0; i < c->count; i++) {
        if (strcmp(c->items[i].mime, mime) == 0) {
            c->items[i].data = data;          /* same type: newer bytes replace older */
            c->items[i].len = len;
            c->items[i].seq = c->next_seq++;
            c->generation++;
            return WUBU_CLIP_OK;
        }
    }
    if (c->count == WUBU_CLIP_MAX_ITEMS) return WUBU_CLIP_FULL;
    WubuClipItem *it = &c->items[c->count++];
    memset(it, 0, sizeof *it);
    strncpy(it->mime, mime, sizeof it->mime - 1);
    it->data = data;
    it->len = len;
    it->seq = c->next_seq++;
    c->generation++;
    return WUBU_CLIP_OK;
}

const WubuClipItem *wubu_clip_get(const WubuClip *c, const char *mime) {
    if (!c || !mime) return NULL;
    for (int i = 0; i < c->count; i++)
        if (strcmp(c->items[i].mime, mime) == 0) return &c->items[i];
    return NULL;
}

const WubuClipItem *wubu_clip_best(const WubuClip *c, const char *const *prefs, int nprefs) {
    if (!c || !prefs) return NULL;
    for (int p = 0; p < nprefs; p++) {
        const WubuClipItem *hit = wubu_clip_get(c, prefs[p]);
        if (hit) return hit;
    }
    return NULL;
}

int wubu_clip_count(const WubuClip *c) { return c ? c->count : 0; }

void wubu_clip_clear(WubuClip *c) {
    if (!c) return;
    memset(c->items, 0, sizeof c->items);
    c->count = 0;
    c->generation++;
}
