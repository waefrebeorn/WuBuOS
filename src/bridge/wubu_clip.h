/* wubu_clip.h -- a clipboard that carries TYPES, not one host string.
 *
 * Own implementation (docs/GOAL-SLERM.json, D4 stage A). The bridge's clipboard is a
 * single character string (`bridge_clipboard_set`), which is why copying from a rich
 * application loses everything except its text, and why "paste as image" cannot exist:
 * there is nothing to ask for by name.
 *
 * This is the data model: several representations of ONE clipboard item, each with a MIME
 * type, plus the preference rule a paste operation needs ("give me the richest thing you
 * have that I can accept"). The host transport stays in bridge.c; this file decides what
 * the data IS.
 *
 * No allocation: entries point at caller-owned memory. That is deliberate — a clipboard
 * that copies megabytes on every keystroke of a selection is its own performance bug.
 */
#ifndef WUBU_CLIP_H
#define WUBU_CLIP_H

#include <stddef.h>

#define WUBU_CLIP_MAX_ITEMS 8
#define WUBU_CLIP_MIME_MAX 64

enum {
    WUBU_CLIP_OK        =  0,
    WUBU_CLIP_ARGS      = -1,   /* null pointers, or data==NULL with a non-zero length */
    WUBU_CLIP_BAD_MIME  = -2,   /* not "type/subtype" with a sane character set */
    WUBU_CLIP_FULL      = -3    /* WUBU_CLIP_MAX_ITEMS distinct types already present */
};

typedef struct {
    char        mime[WUBU_CLIP_MIME_MAX];
    const void *data;
    size_t      len;
    unsigned    seq;            /* set order, so eviction is oldest-first */
} WubuClipItem;

typedef struct {
    WubuClipItem items[WUBU_CLIP_MAX_ITEMS];
    int          count;
    unsigned     next_seq;
    unsigned     generation;    /* bumped whenever the content changes */
} WubuClip;

void wubu_clip_init(WubuClip *c);

/* Replace the representation for `mime` (or add it). The same MIME twice is one entry:
 * the newer bytes win, because a paste must not offer two versions of "the" text. */
int wubu_clip_set(WubuClip *c, const char *mime, const void *data, size_t len);

/* Exact lookup. */
const WubuClipItem *wubu_clip_get(const WubuClip *c, const char *mime);

/* First type from `prefs` that the clipboard actually holds. This is the function a paste
 * handler calls, so preference order is data, not an if-chain in every application. */
const WubuClipItem *wubu_clip_best(const WubuClip *c, const char *const *prefs, int nprefs);

int wubu_clip_count(const WubuClip *c);

/* Empty the clipboard and bump the generation, so a watcher can tell "cleared" from
 * "never set". */
void wubu_clip_clear(WubuClip *c);

/* 1 when `mime` is "type/subtype" using the characters a MIME type may contain. */
int wubu_clip_mime_ok(const char *mime);

const char *wubu_clip_reason(int code);

#endif
