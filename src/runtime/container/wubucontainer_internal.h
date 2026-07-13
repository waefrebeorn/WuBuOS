/*
 * wubucontainer_internal.h -- Internal (non-public) definitions for the
 * WuBuContainer C engine.
 *
 * This header COMPLETES the opaque `struct WubuContainerEngine` declared as an
 * incomplete type in wubucontainer.h. Only translation units inside the
 * container subsystem include this file; external callers see only the opaque
 * typedef and the public API. This keeps the struct definition out of the
 * public god-header and lets the registry module and the IPC module share the
 * engine layout without exposing it.
 *
 * Minimal includes: just the public header (for the typedef + constants) and
 * <stdbool.h>. No json-c, no system IPC headers here -- those belong only to
 * the IPC module (wubucontainer.c).
 */
#ifndef WUBU_CONTAINER_INTERNAL_H
#define WUBU_CONTAINER_INTERNAL_H

#include "wubucontainer.h"
#include <stdbool.h>
#include <sys/types.h>   /* pid_t for the engine child process */

/* JSON-RPC protocol version shared by the IPC module. */
#define WUBU_CTR_JSONRPC_VERSION "2.0"

/* Full engine layout (opaque to public callers). */
struct WubuContainerEngine {
    pid_t child_pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    char socket_path[WUBU_CONTAINER_MAX_PATH];
    int socket_fd;
    bool use_socket;
    bool initialized;     /* engine object allocated + registry usable */
    bool engine_up;       /* TS/IPC subprocess connected and ready */
    char container_dir[WUBU_CONTAINER_MAX_PATH];
    /* Registry of agentic-layer custom handlers registered at runtime. */
    WubuContainerHandler custom_handlers[WUBU_CONTAINER_MAX_HANDLERS];
    int custom_handler_count;
};

#endif /* WUBU_CONTAINER_INTERNAL_H */
