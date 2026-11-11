/*
 * wubu_container_universal.h -- the WuBuContainer, slerm'd live.
 *
 * The seed is the WuBuContainer ethos (the convert.to.it internals the user
 * pointed at): one thing that accepts ANY source and yields what you need,
 * just works, and never leaves you hanging.  SLERMed into the OS this becomes
 * a UNIVERSAL CONTAINER: any of the container sources the OS already knows
 * (Arch, OCI, snapshot, ram, a host directory, a disk image) is presented
 * through ONE uniform, mountable, readable interface.  The word "universal"
 * is a real promise, not a slogan: new kinds are REGISTERED, and a kind that
 * does not exist is refused rather than half-served.
 *
 * The backends are not re-implemented here (that would duplicate the existing
 * container machinery).  A kind adapter BINDS the uniform interface to the
 * in-tree code for that source; this module is the usurping uniform layer.
 *
 * C11, libc only.  In the container runtime, `src/runtime/container/`.
 */

#ifndef WUBU_CONTAINER_UNIVERSAL_H
#define WUBU_CONTAINER_UNIVERSAL_H

#include <stddef.h>
#include <stdint.h>

#define WCU_KIND_LEN   24
#define WCU_PATH_LEN   256
#define WCU_MAX_KINDS  32
#define WCU_MAX_CHILDREN 128

/* A handle to one mounted universal container.  Opaque to the caller: the
 * methods below are the whole interface, so any backend is interchangeable. */
typedef struct wubu_container_universal wubu_container_universal_t;

/* A kind adapter implements the uniform ops over one source type.  This is the
 * extensibility point that makes the container universal: to support a new
 * source you register one adapter, and every existing caller gains it. */
typedef struct {
    const char *kind;                          /* "ram", "dir", "oci", ... */
    int  (*open)(const char *source, wubu_container_universal_t **out);
    void (*close)(wubu_container_universal_t *c);
    int  (*mount)(wubu_container_universal_t *c, const char *path);
    int  (*stat)(wubu_container_universal_t *c, const char *path,
                 uint64_t *size, uint32_t *is_dir);
    int  (*list)(wubu_container_universal_t *c, const char *dir,
                 char out[][WCU_PATH_LEN], int cap);
    int  (*read)(wubu_container_universal_t *c, const char *path,
                 uint8_t *buf, uint32_t size);
    int  (*put)(wubu_container_universal_t *c, const char *path,
                const uint8_t *data, uint32_t size);
} wubu_container_adapter_t;

/* Adapter helpers.  An adapter's open() calls wu_container_handle_new() to
 * build its handle (the closing/op sizing is the module's), and every op
 * recovers its private state with wu_container_backend().  This keeps the
 * handle opaque even to its own backend. */
int   wu_container_handle_new(const char *kind, void *backend,
                              wubu_container_universal_t **out);
void *wu_container_backend(wubu_container_universal_t *c);

/* REGISTER a kind so it can be opened.  Refuses a NULL/empty kind or one that
 * is already registered.  Returns 0, or -1. */
int wu_container_register(const wubu_container_adapter_t *adapter);

/* The registered kind names -- proof of universality, easy to list. */
int wu_container_kinds(char names[][WCU_KIND_LEN], int cap);

/* OPEN a universal container of `kind` over `source`.  Returns 0 and fills
 * *out on success; -1 for an unknown kind, a NULL source, or a backend that
 * cannot open the source.  An unknown kind is an ERROR, never a fallback. */
int wu_container_open(const char *kind, const char *source,
                      wubu_container_universal_t **out);
void wu_container_close(wubu_container_universal_t *c);

const char *wu_container_kind(wubu_container_universal_t *c);

/* The uniform ops.  Return 0 on success, -1 on error. */
int wu_container_mount(wubu_container_universal_t *c, const char *path);
int wu_container_stat(wubu_container_universal_t *c, const char *path,
                      uint64_t *size, uint32_t *is_dir);
int wu_container_list(wubu_container_universal_t *c, const char *dir,
                      char out[][WCU_PATH_LEN], int cap);
int wu_container_read(wubu_container_universal_t *c, const char *path,
                      uint8_t *buf, uint32_t size);
int wu_container_put(wubu_container_universal_t *c, const char *path,
                     const uint8_t *data, uint32_t size);

#endif /* WUBU_CONTAINER_UNIVERSAL_H */
