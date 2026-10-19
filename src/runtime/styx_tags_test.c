/* styx_tags_test.c -- 256 in-flight tags, out-of-order reply routing. */
#include "styx_tags.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;
#define T(n) do { printf("  TEST %-52s", n); } while (0)
#define OK() do { printf("✅\n"); pass++; } while (0)
#define NO(m) do { printf("❌ %s\n", m); fail++; } while (0)

int main(void)
{
    styx_tagtable_t tt;
    styx_tags_init(&tt);

    T("256 distinct tags allocate, the 257th fails");
    uint16_t tags[STYX_TAG_SLOTS];
    int ok = 1;
    for (int i = 0; i < STYX_TAG_SLOTS; i++)
        if (styx_tags_alloc(&tt, (uint32_t)(i + 1), &tags[i]) != 0) { ok = 0; break; }
    uint16_t extra;
    int full = (styx_tags_alloc(&tt, 999, &extra) == -1);
    if (ok && full && tt.live == STYX_TAG_SLOTS) OK(); else NO("allocation or exhaustion wrong");

    T("tags are unique");
    int dup = 0;
    for (int i = 0; i < STYX_TAG_SLOTS && !dup; i++)
        for (int j = i + 1; j < STYX_TAG_SLOTS; j++)
            if (tags[i] == tags[j]) { dup = 1; break; }
    if (!dup) OK(); else NO("duplicate tag handed out");

    T("out-of-order replies route to their own request");
    int routed = 1;
    uint32_t rid = 0;
    for (int k = 0; k < STYX_TAG_SLOTS; k++) {
        int i = (k * 97) % STYX_TAG_SLOTS;      /* scrambled reply order */
        if (styx_tags_lookup(&tt, tags[i], &rid) != 0 || rid != (uint32_t)(i + 1))
            routed = 0;
    }
    if (routed) OK(); else NO("a reply was routed to the wrong request");

    T("release frees exactly one slot and it can be reused");
    int before = tt.live;
    if (styx_tags_release(&tt, tags[5]) != 0) NO("release failed");
    else if (tt.live != before - 1) NO("live count wrong");
    else if (styx_tags_alloc(&tt, 4242, &extra) != 0) NO("reuse failed");
    else if (styx_tags_lookup(&tt, extra, &rid) != 0 || rid != 4242) NO("reused tag misrouted");
    else OK();

    T("lookup of an unknown tag fails");
    if (styx_tags_lookup(&tt, (uint16_t)0xFFFE, &rid) == -1) OK(); else NO("unknown tag resolved");

    printf("\n=== styx_tags: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
