/*
 * wubu_cap_audit.c -- denial ring for capability resolution. See the header.
 */
#include "wubu_cap_audit.h"

#include <pthread.h>
#include <string.h>

static wubu_cap_audit_rec_t g_ring[WUBU_CAP_AUDIT_CAPACITY];
static size_t   g_head;    /* next write position */
static size_t   g_count;   /* retained records, <= capacity */
static uint64_t g_seq;     /* total records ever written */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void wubu_cap_audit_reset(void)
{
    pthread_mutex_lock(&g_lock);
    memset(g_ring, 0, sizeof g_ring);
    g_head = g_count = 0;
    g_seq = 0;
    pthread_mutex_unlock(&g_lock);
}

void wubu_cap_audit_record(int32_t pid, uint16_t kind, uint64_t requested,
                           uint8_t check)
{
    pthread_mutex_lock(&g_lock);
    wubu_cap_audit_rec_t *r = &g_ring[g_head];
    r->pid = pid;
    r->kind = kind;
    r->check = check;
    r->_pad = 0;
    r->requested = requested;
    r->seq = ++g_seq;
    g_head = (g_head + 1) % WUBU_CAP_AUDIT_CAPACITY;
    if (g_count < WUBU_CAP_AUDIT_CAPACITY) g_count++;
    pthread_mutex_unlock(&g_lock);
}

size_t wubu_cap_audit_count(void)
{
    pthread_mutex_lock(&g_lock);
    size_t n = g_count;
    pthread_mutex_unlock(&g_lock);
    return n;
}

uint64_t wubu_cap_audit_total(void)
{
    pthread_mutex_lock(&g_lock);
    uint64_t n = g_seq;
    pthread_mutex_unlock(&g_lock);
    return n;
}

const wubu_cap_audit_rec_t *wubu_cap_audit_at(size_t i)
{
    /* The returned pointer is only stable while no other thread records; the
     * ring is append-only for readers that snapshot immediately, which is how
     * the tests and the diagnostic export use it. */
    pthread_mutex_lock(&g_lock);
    const wubu_cap_audit_rec_t *out = NULL;
    if (i < g_count) {
        size_t start = (g_head + WUBU_CAP_AUDIT_CAPACITY - g_count) % WUBU_CAP_AUDIT_CAPACITY;
        out = &g_ring[(start + i) % WUBU_CAP_AUDIT_CAPACITY];
    }
    pthread_mutex_unlock(&g_lock);
    return out;
}