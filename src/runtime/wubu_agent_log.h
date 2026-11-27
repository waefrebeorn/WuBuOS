/* wubu_agent_log.h -- append-only, hash-chained record of what an agent DID.
 *
 * Own implementation (docs/GOAL-SLERM.json, D6). The benchmark graded this ABSENT:
 * the OS has an agent loop, a KV namespace and capability checks, but nothing that
 * answers "what did it do, and can I believe the answer afterwards?".
 *
 * Every record is chained to the previous one with BLAKE2b (our own, see
 * wubu_blake2b.h), so editing or deleting a middle record breaks verification at that
 * index instead of passing silently. That is the difference between a log and a claim.
 *
 * Format is one text line per record, chosen so an operator can read it with cat and
 * a tool can still verify the chain:
 *
 *   seq|ts_ms|action|verdict|detail|prev_hex|hash_hex
 */
#ifndef WUBU_AGENT_LOG_H
#define WUBU_AGENT_LOG_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_AGENT_ACTION_MAX 64
#define WUBU_AGENT_VERDICT_MAX 32
#define WUBU_AGENT_DETAIL_MAX 160
#define WUBU_AGENT_HASH 32
#define WUBU_AGENT_PATH_MAX 512

enum {
    WUBU_AGENT_LOG_OK         =  0,
    WUBU_AGENT_LOG_ARGS       = -1,
    WUBU_AGENT_LOG_IO         = -2,   /* cannot open/create/write the file */
    WUBU_AGENT_LOG_BROKEN     = -3,   /* the chain does not verify */
    WUBU_AGENT_LOG_FORMAT     = -4,   /* a line is not a record */
    WUBU_AGENT_LOG_FULL       = -5,   /* a field is longer than its limit */
    WUBU_AGENT_LOG_RESUME_SEQ = -6    /* an existing file has a gap or a bad tail */
};

typedef struct {
    char         path[WUBU_AGENT_PATH_MAX];
    uint8_t      prev[WUBU_AGENT_HASH];
    int          next_seq;
} WubuAgentLog;

typedef struct {
    int      count;
    int      bad_index;      /* -1 when the chain verified */
    int      gap;            /* 1 when a sequence number skipped */
} WubuAgentVerdict;

/* Open an existing log (verifying its chain and resuming the sequence) or create a new
 * one. Refuses a log whose chain is broken rather than appending to it. */
int wubu_agent_log_open(WubuAgentLog *log, const char *path, int *err);

/* Append one record. */
int wubu_agent_log_append(WubuAgentLog *log, const char *action, const char *verdict,
                          const char *detail, int *err);

/* Verify a log file without opening it for writing. */
int wubu_agent_log_verify(const char *path, WubuAgentVerdict *out, int *err);

const char *wubu_agent_log_reason(int err);

#endif
