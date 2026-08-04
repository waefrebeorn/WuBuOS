/*
 * wubu_runtime.h -- WUBURUNTIME: the runtime that fills the gap.
 *
 * The user's directive (2026-08-04, research/063): "the runtime that
 * fills the gap of all the object oriented runtime based operating
 * systems and allows them to have their own compilation space so we
 * don't have a huge disorganization issue."
 *
 * Every OO runtime (JVM, CLR, Smalltalk image, V8 isolate, Wasm
 * instance, C++ ABI) gets its OWN compilation space: a named,
 * versioned, isolated region (hive slot + namespace + personality)
 * where its ABI/image/heap lives. The OS *knows* the runtime exists
 * and provides the shared substrate instead of each runtime
 * re-implementing memory/threads/IO.
 *
 * The SNAPSHOT guarantee ("people aren't left in the dust"): every
 * space records compiler_ver + language_ver + created, so a space
 * from 2026 still loads in 2036.
 *
 * C11, self-contained (hive-backed slots).
 */
#ifndef WUBU_RUNTIME_H
#define WUBU_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include "../kernel/wubu_hive.h"

/* the compilation-space state (the amoeba membrane) */
typedef enum {
    WUBU_RT_COLD = 0,    /* registered, not started */
    WUBU_RT_WARM,        /* loading / warming */
    WUBU_RT_LIVE,        /* running */
    WUBU_RT_FROZEN       /* suspended / archived */
} wubu_rt_state_t;

/* ====================================================================
 * WAVE 3 -- THE RUNTIME PERSONALITIES (the gap filler)
 *
 * Every OO runtime speaks syscalls its own way. The personality is a
 * per-space dispatch table that maps a runtime's syscalls onto the
 * OS-native substrate, so the JVM doesn't re-implement files and the
 * Wasm instance doesn't re-implement threads.
 * ==================================================================== */

/* the syscalls a runtime can make (the shared substrate) */
typedef enum {
    WUBU_RT_SYS_OPEN = 0,
    WUBU_RT_SYS_READ,
    WUBU_RT_SYS_WRITE,
    WUBU_RT_SYS_CLOSE,
    WUBU_RT_SYS_HEAP_ALLOC,
    WUBU_RT_SYS_HEAP_FREE,
    WUBU_RT_SYS_EXIT,
    WUBU_RT_SYS_COUNT
} wubu_rt_syscall_t;

/* the sandbox refusal sentinel: a personality returns this when its
 * policy (e.g. WASI's namespace gate) refuses a syscall BEFORE the OS
 * sees it — distinct from an OS error (-1/-errno), so tests can tell
 * "refused by policy" from "admitted, then failed to resolve". */
#define WUBU_RT_SANDBOX_REFUSED (-10000)

/* a personality: the native dispatch table (the gap filler) */
typedef struct {
    const char *name;                      /* "posix" / "image" / "wasi" */
    int (*sys_open)(const char *path, int flags);
    int (*sys_read)(int fd, void *buf, size_t n);
    int (*sys_write)(int fd, const void *buf, size_t n);
    int (*sys_close)(int fd);
    void *(*sys_heap_alloc)(size_t n);
    void (*sys_heap_free)(void *p);
    void (*sys_exit)(int code);
} wubu_rt_personality_t;

/* one compilation space (the core abstraction of wuburuntime) */
typedef struct {
    uint64_t id;                 /* the hive slot id */
    char name[64];               /* "java-jvm-21" / "dotnet-clr-9" */
    char language[48];           /* the OO runtime this space serves */
    char compiler_ver[32];       /* the compiler that built it (snapshot) */
    char language_ver[32];       /* the language/runtime spec version */
    char abi_snapshot[96];       /* vtable layout / RTTI / GC hooks */
    char created[32];            /* the date (snapshot: 2026-08-04) */
    char namespace_path[64];     /* the 9P path (/n/java/) */
    uint64_t heap_cap;           /* the runtime's memory region cap */
    uint64_t heap_used;          /* current usage (ring-bounded) */
    wubu_rt_state_t state;
    const wubu_rt_personality_t *personality; /* wave 3: the gap filler */
} wubu_rt_space_t;

/* the registry config */
typedef struct {
    size_t max_spaces;           /* ring capacity (default 16) */
    uint64_t default_heap_cap;   /* per-space heap cap (default 1<<30) */
} wubu_rt_cfg_t;

typedef struct wubu_runtime wubu_runtime_t;

/* O1: init the registry over a hive. Returns NULL on error. */
wubu_runtime_t *wubu_runtime_init(wubu_hive_t *hive,
                                  const wubu_rt_cfg_t *cfg);

/* O2: free the registry (the hive stays caller-owned). */
void wubu_runtime_free(wubu_runtime_t *rt);

/* O3: CREATE a compilation space. The snapshot fields (compiler_ver,
 * language_ver, created) are recorded at creation — the "not left in
 * the dust" guarantee. Returns the space id, or 0 on error. */
uint64_t wubu_runtime_create(wubu_runtime_t *rt,
                             const char *name,
                             const char *language,
                             const char *compiler_ver,
                             const char *language_ver,
                             const char *abi_snapshot,
                             const char *namespace_path);

/* O4: FIND a space by id. Returns NULL if not present. */
wubu_rt_space_t *wubu_runtime_find(wubu_runtime_t *rt, uint64_t id);

/* O5: FIND a space by name. Returns NULL if not present. */
wubu_rt_space_t *wubu_runtime_find_name(wubu_runtime_t *rt,
                                        const char *name);

/* O6: SET the state (cold -> warm -> live -> frozen). */
int wubu_runtime_set_state(wubu_runtime_t *rt, uint64_t id,
                           wubu_rt_state_t state);

/* O7: account heap usage (ring-bounded: returns -1 if over cap). */
int wubu_runtime_touch_heap(wubu_runtime_t *rt, uint64_t id,
                            int64_t delta);

/* O8: DESTROY a space (the ring recycles it). Returns 0. */
int wubu_runtime_destroy(wubu_runtime_t *rt, uint64_t id);

/* O9: count live spaces. */
size_t wubu_runtime_count(const wubu_runtime_t *rt);

/* built-in personalities (the gap filler, real and native) */
extern const wubu_rt_personality_t wubu_rt_personality_posix;
extern const wubu_rt_personality_t wubu_rt_personality_image; /* Smalltalk */
extern const wubu_rt_personality_t wubu_rt_personality_wasi;  /* sandboxed */

/* W1: attach a personality to a space by kind name ("posix"/"image"/"wasi").
 * Returns 0 on success, -1 on unknown kind or missing space. */
int wubu_runtime_set_personality(wubu_runtime_t *rt, uint64_t id,
                                 const char *kind);

/* W2: dispatch one syscall through a space's personality. Returns the
 * personality's result, or -1 if the space has no personality (cold). */
int64_t wubu_runtime_call(wubu_runtime_t *rt, uint64_t id,
                          wubu_rt_syscall_t sys, int64_t a1,
                          int64_t a2, int64_t a3);

/* W3: enumerate the live spaces (the broker / -spaces view).
 * cb(space, user) returns 0 to continue, nonzero to stop. */
void wubu_runtime_list(const wubu_runtime_t *rt,
                       int (*cb)(const wubu_rt_space_t *, void *),
                       void *user);

/* W4: PERSISTENCE — the "nothing left in the dust" guarantee made
 * real: save the whole registry (every space + its snapshot) to a
 * file so spaces survive process exit, reboots, and compiler
 * versions. Returns 0 on success. */
int wubu_runtime_save(const wubu_runtime_t *rt, const char *path);

/* W5: load a previously-saved registry into a fresh runtime.
 * Returns 0 on success (missing/corrupt file -> -1, registry empty). */
int wubu_runtime_load(wubu_runtime_t *rt, const char *path);

#endif /* WUBU_RUNTIME_H */
