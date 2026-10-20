/*
 * test_wubu_audit.c -- asserts the hash-chained kernel audit log contract
 * (SECURITY #775): chain integrity, tamper detection, ring-wrap semantics.
 * Hosted build of two freestanding kernel modules (audit + sha256).
 */
#include "wubu_audit.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

static int hash_eq(const uint8_t a[32], const uint8_t b[32])
{
    return memcmp(a, b, 32) == 0;
}

int main(void)
{
    /* ---- empty log ---- */
    wubu_audit_reset();
    CHECK(wubu_audit_count() == 0);
    CHECK(wubu_audit_at(0) == NULL);
    uint8_t head[32];
    wubu_audit_head(head);
    uint8_t zeros[32];
    memset(zeros, 0, sizeof(zeros));
    CHECK(hash_eq(head, zeros));
    CHECK(wubu_audit_verify() == 0);        /* empty is intact */

    /* ---- records land, chain links ---- */
    CHECK(wubu_audit_record(WUBU_AUDIT_SECURITY, 1, 100, "sb policy installed") == 1);
    CHECK(wubu_audit_record(WUBU_AUDIT_TASK, 7, 110, "task bonzi spawned") == 2);
    CHECK(wubu_audit_record(WUBU_AUDIT_IRQ, 0, 120, "irq storm masked") == 3);
    CHECK(wubu_audit_count() == 3);
    const wubu_audit_rec_t *r0 = wubu_audit_at(0);
    const wubu_audit_rec_t *r1 = wubu_audit_at(1);
    const wubu_audit_rec_t *r2 = wubu_audit_at(2);
    CHECK(r0 && r1 && r2);
    CHECK(r0->seq == 1 && r1->seq == 2 && r2->seq == 3);
    CHECK(r1->pid == 7 && r1->event == WUBU_AUDIT_TASK);
    CHECK(strcmp(r0->msg, "sb policy installed") == 0);
    /* the chain: each record's prev_hash is its predecessor's hash */
    CHECK(hash_eq(r1->prev_hash, r0->hash));
    CHECK(hash_eq(r2->prev_hash, r1->hash));
    CHECK(!hash_eq(r0->hash, r1->hash));    /* distinct hashes */
    /* the head is the newest record's hash */
    wubu_audit_head(head);
    CHECK(hash_eq(head, r2->hash));
    CHECK(wubu_audit_verify() == 0);

    /* ---- tamper detection: alter a payload, the chain reports it ---- */
    wubu_audit_rec_t *evil = (wubu_audit_rec_t *)r1;   /* white-box tamper */
    char was[WUBU_AUDIT_MSG_SZ];
    memcpy(was, evil->msg, sizeof(was));
    evil->msg[0] = 'X';
    CHECK(wubu_audit_verify() == -1);
    memcpy(evil->msg, was, sizeof(was));
    CHECK(wubu_audit_verify() == 0);        /* restored => intact again */
    /* tamper with the hash itself (a forged chain link) */
    uint8_t was_hash[32];
    memcpy(was_hash, evil->hash, sizeof(was_hash));
    evil->hash[0] ^= 0xFF;
    CHECK(wubu_audit_verify() == -1);       /* the successor link breaks */
    memcpy(evil->hash, was_hash, sizeof(was_hash));
    CHECK(wubu_audit_verify() == 0);

    /* ---- msg truncation + bad args ---- */
    char longmsg[WUBU_AUDIT_MSG_SZ + 32];
    memset(longmsg, 'a', sizeof(longmsg) - 1);
    longmsg[sizeof(longmsg) - 1] = 0;
    CHECK(wubu_audit_record(WUBU_AUDIT_GENERIC, 0, 1, longmsg) == 4);
    const wubu_audit_rec_t *r3 = wubu_audit_at(3);
    CHECK(r3 && r3->msg[WUBU_AUDIT_MSG_SZ - 1] == 0);
    CHECK(strlen(r3->msg) == WUBU_AUDIT_MSG_SZ - 1);
    CHECK(wubu_audit_record(WUBU_AUDIT_GENERIC, 0, 1, NULL) == 0);

    /* ---- ring wrap: the chain continues across eviction ---- */
    for (uint64_t i = 0; i < WUBU_AUDIT_CAP + 40; i++)
        wubu_audit_record(WUBU_AUDIT_HW, (uint32_t)i, i, "wrap fill");
    CHECK(wubu_audit_count() == WUBU_AUDIT_CAP);
    CHECK(wubu_audit_verify() == 0);       /* every retained link checks */
    const wubu_audit_rec_t *oldest = wubu_audit_at(0);
    const wubu_audit_rec_t *newest = wubu_audit_at(WUBU_AUDIT_CAP - 1);
    CHECK(oldest && newest);
    CHECK(oldest->seq == 44 + 1);           /* evicted the first 44 */
    CHECK(newest->seq == 4 + WUBU_AUDIT_CAP + 40);
    wubu_audit_head(head);
    CHECK(hash_eq(head, newest->hash));

    /* ---- reset restores a virgin log ---- */
    wubu_audit_reset();
    CHECK(wubu_audit_count() == 0);
    CHECK(wubu_audit_record(WUBU_AUDIT_SECURITY, 1, 100, "again") == 1);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
