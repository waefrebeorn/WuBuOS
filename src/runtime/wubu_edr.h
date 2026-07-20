/* wubu_edr.h  --  WuBuOS EDR Engine Public API */

#ifndef WUBU_EDR_H
#define WUBU_EDR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define EDR_MAX_FILENAME    256
#define EDR_MAX_CMDLINE     4096
#define EDR_MAX_PATH        4096
#define EDR_MAX_MODULES     16
#define EDR_MAX_RULESETS    64

/* System paths (used by core and telemetry modules) */
#define EDR_ALERT_PATH          "/edr/alerts"
#define EDR_REPLAY_PATH         "/edr/replay"
#define EDR_CONFIG_PATH         "/edr/config"
#define EDR_MODEL_PATH          "/edr/models"
#define EDR_RULES_PATH          "/edr/rules"

/* Event types (matching heavener taxonomy) */
typedef uint16_t EdrEventType;
enum {
    EDR_EV_PROCESS_CREATE        = 1,
    EDR_EV_PROCESS_EXIT          = 2,
    EDR_EV_THREAD_CREATE         = 3,
    EDR_EV_IMAGE_LOAD            = 4,
    EDR_EV_FILE_CREATE           = 5,
    EDR_EV_FILE_WRITE            = 6,
    EDR_EV_FILE_DELETE           = 7,
    EDR_EV_FILE_RENAME           = 8,
    EDR_EV_REG_CREATE_KEY        = 9,
    EDR_EV_REG_SET_VALUE         = 10,
    EDR_EV_REG_DELETE_KEY        = 11,
    EDR_EV_REG_DELETE_VALUE      = 12,
    EDR_EV_NETWORK_CONNECT       = 13,
    EDR_EV_DNS_QUERY             = 14,
    EDR_EV_DRIVER_LOAD           = 15,
    EDR_EV_LDAP_QUERY            = 16,
    EDR_EV_WMI_OPERATION         = 17,
    EDR_EV_USER_ACCOUNT_CREATED  = 18,
    EDR_EV_SCHEDULED_TASK_CREATED= 19,
    EDR_EV_SCRIPT_EXECUTION      = 20,
    EDR_EV_BEHAVIORAL_INDICATOR  = 21,
    EDR_EV_PROCESS_HANDLE_ACCESS = 22,
    EDR_EV_ETW_TI                = 23,
    EDR_EV_NAMED_PIPE_CREATE     = 24,
    EDR_EV_FILE_SET_BASIC_INFO   = 25,
    /* AGI / UI-automation action: every synthetic mouse/keyboard/type the
     * agent issues is logged here so a human can search and replay exactly
     * what the operating system did on the agent's behalf. Mirrors the same
     * reporting methodology as process/file/network events. */
    EDR_EV_AGENT_ACTION          = 26,
};

/* Agent (UI-automation) action sub-types carried in an EDR_EV_AGENT_ACTION
 * event's u32 field. */
enum {
    EDR_AGENT_MOUSE_MOVE = 1,
    EDR_AGENT_MOUSE_DOWN = 2,
    EDR_AGENT_MOUSE_UP   = 3,
    EDR_AGENT_KEY        = 4,
    EDR_AGENT_TYPE       = 5,
};

/* Alert */
typedef struct {
    char     id[17];          /* hex FNV-1a */
    char     rule_name[128];
    char     severity[16];    /* malicious / suspicious / info */
    uint32_t pid;
    char     module[32];
    char     description[512];
    char     process_chain[2048];
    uint64_t timestamp;
} EdrAlert;

/* Process info */
typedef struct EdrProcessInfo EdrProcessInfo;

/* Module interface */
typedef struct EdrModule {
    const char *name;
    const char *version;
    uint32_t caps;
    int  (*init)(struct EdrModule *self, const char *config_path);
    void (*shutdown)(struct EdrModule *self);
    int  (*scan_file)(struct EdrModule *self, const uint8_t *data,
                      size_t len, const char *path,
                      char *verdict, size_t vlen);
    void (*on_event)(struct EdrModule *self,
                     const void *hdr, const void *data,
                     const EdrProcessInfo *proc);
    int  (*drain_alerts)(struct EdrModule *self, EdrAlert *out, int max);
} EdrModule;

/* Lifecycle */
int  edr_start(void);
void edr_stop(void);

/* Module management */
void edr_register_module(EdrModule *mod);
int  edr_switch_module(const char *name);

/* Replay */
int  edr_replay(const char *json_path);

/* Query */
int  edr_get_alerts(EdrAlert *buf, int max);
int  edr_get_process_count(void);
const EdrProcessInfo *edr_get_process(uint32_t pid);

/* ================================================================
 * Agent (AGI) transparency -- the "giant toggle" + event logging
 * ================================================================ */

/* Master analytics switch. When disabled, NO agent/UI-automation events are
 * recorded (and, by policy, the user forfeits debug-report / bug-fix
 * eligibility because they are no longer part of the analytic corpus).
 * Persisted to EDR_CONFIG_PATH "/analytics" so the choice survives reboot. */
void edr_analytics_set_enabled(bool on);
bool edr_analytics_enabled(void);

/* Generic typed event push: allocates an EdrEvent (header + inline detail),
 * fills the header with a fresh timestamp + the caller's pid, and enqueues it
 * on the same lock-free queue every other telemetry source uses. Honors the
 * analytics toggle. Returns 0 on success, -1 if disabled or queue full. */
int  edr_log_event(uint16_t type, uint32_t pid, uint32_t extra_pid,
                   uint64_t u64a, uint64_t u64b, uint32_t u32,
                   const char *detail);

/* Convenience for the UI-automation layer: log one agent action. `action` is
 * an EDR_AGENT_* sub-type; x/y/btn/key describe the input; `detail` is a
 * human-readable summary (e.g. "click at 100,200"). */
int  edr_log_agent_action(uint16_t action, int x, int y, int btn,
                           uint32_t key, const char *detail);

/* Total agent events logged since process start (for tests / audit). */
uint64_t edr_agent_events_logged(void);

/* ================================================================
 * Read snapshot -- the disclosure surface
 * ================================================================
 * Non-destructive copy of the most recent events from the live ring so a
 * dashboard can render / search / tail exactly what the OS + agents did,
 * without disturbing the queue the behavioural modules drain. This is the
 * user-facing half of the "disclosure is the security model" principle. */

/* Opaque view of one event, safe for callers to read (no struct internals). */
typedef struct {
    uint16_t type;        /* EdrEventType */
    uint32_t pid;
    uint32_t extra_pid;
    uint64_t ts_ns;       /* nanosecond timestamp */
    uint64_t u64a, u64b;
    uint32_t u32;
    char     detail[256]; /* human summary (inline) */
} EdrEventView;

/* Copy the newest `max` events into `out` (oldest-first). Returns the count
 * actually copied. A negative `min_type`/`max_type` pair filters by type;
 * pass 0/0 to fetch all. This is a SNAPSHOT -- it never removes events. */
int edr_recent_events(EdrEventView *out, int max, int min_type, int max_type);

/* Human-readable name for an event type / agent sub-type (for the UI). */
const char *edr_event_type_name(uint16_t type);
const char *edr_agent_action_name(uint16_t action);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_EDR_H */