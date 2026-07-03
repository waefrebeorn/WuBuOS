/* edr_internal.h  --  WuBuOS EDR Engine Internal Header
 *
 * Private struct definitions shared across EDR submodules.
 * NOT part of the public API (wubu_edr.h is the public face).
 *
 * Module overview:
 *   edr_core.c      — lifecycle, module system, alert buffer, FNV-1a, worker thread
 *   edr_proc_pin.c  — NETLINK_CONNECTOR + cn_proc (process fork/exec/exit pin)
 *   edr_fanotify.c  — fanotify + inotify telemetry (file ops, timestomp, script capture)
 *   edr_poller.c    — periodic /proc scans (unix sockets, driver load, tasks)
 *
 * Architecture mirrors heavener's six-layer design adapted to Linux telemetry.
 */

#ifndef WUBU_EDR_INTERNAL_H
#define WUBU_EDR_INTERNAL_H

#define _GNU_SOURCE

#include "wubu_edr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/fanotify.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/audit.h>
#include <linux/limits.h>
#include <linux/cn_proc.h>
#include <dirent.h>

/* ================================================================
 * Constants
 * ================================================================ */

#define EDR_MAX_EVENTS          65536
#define EDR_BATCH_SIZE          256
#define EDR_CALL_STACK_DEPTH    32
#define EDR_SLIDING_WINDOW      32

/* ================================================================
 * Event Header — packed for zero-copy queue transfer
 * ================================================================ */

typedef struct __attribute__((packed)) {
    uint16_t    version;
    uint16_t    type;
    uint32_t    size;
    uint64_t    timestamp;
    uint32_t    pid;
    uint32_t    tid;
    uint32_t    extra_pid;
    uint32_t    u32;
    uint64_t    u64a;
    uint64_t    u64b;
    uint16_t    var_count;
} EdrEventHeader;

/* ================================================================
 * Process Model (full definition — opaque in public header)
 * ================================================================ */

struct EdrProcessInfo {
    uint64_t    uid;
    uint32_t    pid;
    uint32_t    ppid;
    uint32_t    creator_pid;
    char        name[EDR_MAX_FILENAME];
    char        cmdline[EDR_MAX_CMDLINE];
    char        exe_path[EDR_MAX_PATH];
    uint64_t    create_time;
    char        integrity[16];
    uint32_t    session_id;
    char        user_sid[64];
    char        cwd[EDR_MAX_PATH];
    /* file enrichment */
    char        sha256[65];
    bool        is_signed;
    char        cert_issuer[128];
    float       entropy;
    /* injection tracking */
    bool        has_injected_thread;
    uint32_t    injected_by_pid;
    bool        was_opened_cross_process;
    uint32_t    opened_by_pid;
    bool        opened_for_vm_read;
    bool        opened_for_vm_write;
    /* ancestry */
    struct EdrProcessInfo *model_parent;
    /* extension values */
    int         ext_count;
    struct { uint32_t key; char val[64]; } ext[16];
    /* thread tracking */
    int         thread_count;
    uint32_t    tids[64];   /* last 64 threads */
};

/* ================================================================
 * Global Process State
 * ================================================================ */

#define EDR_MAX_PROCESSES       8192
extern struct EdrProcessInfo g_processes[EDR_MAX_PROCESSES];
extern int g_process_count;
extern uint64_t g_next_uid;
extern pthread_rwlock_t g_proc_lock;

struct EdrProcessInfo *edr_proc_find(uint32_t pid);
struct EdrProcessInfo *edr_proc_find_or_add(uint32_t pid, uint32_t ppid);

/* ================================================================
 * Lock-free Event Queue (MPSC)
 * ================================================================ */

typedef struct EdrEvent {
    EdrEventHeader header;
    /* inline variable data follows */
} EdrEvent;

#define EDR_EVENT_DATA(ev) ((void*)((uint8_t*)(ev) + sizeof(EdrEventHeader)))

typedef struct {
    EdrEvent *buffer[EDR_MAX_EVENTS];
    volatile uint64_t head;
    volatile uint64_t tail;
    uint64_t dropped;
} EdrEventQueue;

extern EdrEventQueue g_queue;

bool edr_queue_push(EdrEvent *ev);
EdrEvent *edr_queue_pop(void);

/* ================================================================
 * Modifier Engine — sliding file window per process
 * ================================================================ */

typedef struct {
    uint32_t pid;
    char     files[EDR_SLIDING_WINDOW][EDR_MAX_FILENAME];
    int      count;
    int      cursor;
} EdrFileWindow;

extern EdrFileWindow g_file_windows[EDR_MAX_PROCESSES];
extern int g_file_window_count;

/* ================================================================
 * Alert Buffer
 * ================================================================ */

#define EDR_ALERT_CAPACITY 1024
extern EdrAlert g_alerts[EDR_ALERT_CAPACITY];
extern int g_alert_count;

uint64_t fnv1a(const char *key);
void edr_alert_push(const char *rule, const char *sev,
                     uint32_t pid, const char *module,
                     const char *desc);

/* ================================================================
 * Module Globals
 * ================================================================ */

extern EdrModule *g_modules[EDR_MAX_MODULES];
extern int g_module_count;
extern EdrModule *g_active_module;

/* ================================================================
 * Worker Thread
 * ================================================================ */

extern pthread_t g_worker;
extern volatile bool g_running;

/* ================================================================
 * Telemetry Sources — per-pin file descriptors
 * ================================================================ */

extern int g_fanotify_fd;
extern int g_inotify_fd;
extern int g_proc_connector_fd;  /* NETLINK_CONNECTOR socket */
extern int g_netfilter_fd;       /* NETLINK_NETFILTER socket */
extern int g_udev_fd;            /* NETLINK_KOBJECT_UEVENT socket */

/* ================================================================
 * Telemetry Source API (implemented in submodules)
 * ================================================================ */

/* edr_fanotify.c */
int  edr_fanotify_start(void);
void edr_fanotify_poll(void);
void edr_fanotify_stop(void);

/* edr_proc_pin.c */
int  edr_proc_pin_start(void);
void edr_proc_pin_poll(void);
void edr_proc_pin_stop(void);

/* edr_poller.c */
int  edr_poller_start(void);
void edr_poller_scan(void);
void edr_poller_stop(void);

#endif /* WUBU_EDR_INTERNAL_H */