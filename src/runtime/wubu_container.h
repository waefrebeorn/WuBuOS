/*
 * wubu_container.h  --  WuBuOS Universal Container Format (.wubu)
 *
 * The .wubu format is a magic-header container: every file is .wubu,
 * but the header determines interpretation. Like ELF's magic, but
 * universal across executables, data, media, documents.
 *
 * Inspired by WuBuContainer (github.com/waefrebeorn/WuBuContainer):
 *   - Universal file handling via format-aware dispatch
 *   - Handler selection by header, not extension
 *
 * WuBuOS uses .wubu as:
 *   - Native executable format (payload_type = WUBU_EXEC)
 *   - Universal interpreter: header says what runs it
 *   - Linux Proton: payload_type = WUBU_LINUX_ELF → VSL
 *   - Windows Proton: payload_type = WUBU_PE → proton handler
 *   - Any format: handler_id selects the runtime
 *
 * Layout:
 *   [WUBU_MAGIC (8 bytes)]
 *   [WUBU_HEADER (64 bytes)]
 *   [PAYLOAD (variable)]
 */

#ifndef WUBUOS_CONTAINER_H
#define WUBUOS_CONTAINER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -- Magic -------------------------------------------------------- */

#define WUBU_MAGIC      "WUBU!\0\1\2"   /* 8 bytes */
#define WUBU_MAGIC_SIZE 8

/* -- Version ------------------------------------------------------ */

#define WUBU_VERSION_MAJOR  1
#define WUBU_VERSION_MINOR  0

/* -- Payload Types ------------------------------------------------ */

typedef enum {
    /* Native WuBuOS executable  --  JIT-compiled or AOT native */
    WUBU_PAYLOAD_NATIVE_EXEC   = 0x01,

    /* HolyC source  --  JIT compile and execute */
    WUBU_PAYLOAD_HOLYC_SRC    = 0x02,

    /* C source  --  compile via HolyC compiler and execute */
    WUBU_PAYLOAD_C_SRC        = 0x03,

    /* Linux ELF binary  --  run via Virtualization Layer (VSL) */
    WUBU_PAYLOAD_LINUX_ELF    = 0x10,

    /* Windows PE binary  --  run via Proton compatibility layer */
    WUBU_PAYLOAD_WIN_PE       = 0x11,

    /* macOS Mach-O  --  run via VSL/Darling */
    WUBU_PAYLOAD_MAC_MACHO    = 0x12,

    /* Shell script  --  run via VSL bash */
    WUBU_PAYLOAD_SHELL_SCRIPT = 0x20,

    /* Python script  --  run via VSL python3 */
    WUBU_PAYLOAD_PYTHON       = 0x21,

    /* WASM binary  --  run via wasm runtime */
    WUBU_PAYLOAD_WASM         = 0x30,

    /* Bytecode  --  interpreter selected by handler_id */
    WUBU_PAYLOAD_BYTECODE     = 0x40,

    /* Data/document  --  not executable, handler by MIME */
    WUBU_PAYLOAD_DATA         = 0x80,

    /* Embedded .wubu  --  container inside container */
    WUBU_PAYLOAD_NESTED_WUBU  = 0xFF,
} WUBU_PAYLOAD_TYPE;

/* -- Flags -------------------------------------------------------- */

#define WUBU_FLAG_COMPRESSED    0x0001   /* Payload is compressed (zlib) */
#define WUBU_FLAG_ENCRYPTED     0x0002   /* Payload is encrypted */
#define WUBU_FLAG_SANDBOXED     0x0004   /* Run in sandbox (VSL isolated) */
#define WUBU_FLAG_PERSIST_VSL   0x0008   /* Keep VSL alive after exec */
#define WUBU_FLAG_JIT_COMPILE   0x0010   /* JIT compile on load */
#define WUBU_FLAG_AOT_COMPILE   0x0020   /* AOT compile on install */
#define WUBU_FLAG_SHARED_LIB    0x0040   /* Shared library, not main */

/* -- Container Header (64 bytes) --------------------------------- */

typedef struct __attribute__((packed)) {
    /* Magic  --  must be WUBU_MAGIC */
    char         magic[8];

    /* Format version */
    uint8_t      version_major;
    uint8_t      version_minor;

    /* What the payload is */
    uint8_t      payload_type;     /* WUBU_PAYLOAD_TYPE */

    /* Architecture target */
    uint8_t      arch;             /* 0=x86_64, 1=aarch64, 2=riscv64 */

    /* Flags */
    uint16_t     flags;

    /* Handler ID  --  which interpreter/runtime to use
     * 0    = default for payload_type
     * 1    = WuBuOS native
     * 2    = HolyC JIT
     * 10   = VSL (Linux)
     * 11   = Proton (Windows)
     * 20   = Python 3
     * 21   = Node.js
     * 30   = WASM runtime
     * 0xFF = custom (see handler_name) */
    uint8_t      handler_id;

    /* OS personality for this container */
    uint8_t      os_persona;       /* 0=native, 1=linux, 2=win32, 3=macos */

    /* Payload size (bytes, not including header) */
    uint64_t     payload_size;

    /* Entry point offset within payload (for executables) */
    uint64_t     entry_offset;

    /* Container metadata offset (0 = none) */
    uint64_t     meta_offset;

    /* Container metadata size */
    uint32_t     meta_size;

    /* CRC32 of header (for integrity) */
    uint32_t     header_crc;

    /* Human-readable handler name (if handler_id = 0xFF) */
    char         handler_name[8];

    /* Reserved */
    uint8_t      reserved[8];
} WUBU_HEADER;

#define WUBU_HEADER_SIZE  64

/* -- Architecture IDs -------------------------------------------- */

#define WUBU_ARCH_X86_64    0
#define WUBU_ARCH_AARCH64   1
#define WUBU_ARCH_RISCV64   2
#define WUBU_ARCH_WASM      3

/* -- OS Persona IDs ---------------------------------------------- */

#define WUBU_OS_NATIVE      0
#define WUBU_OS_LINUX       1
#define WUBU_OS_WIN32       2
#define WUBU_OS_MACOS       3

/* -- Container Metadata (optional JSON-like KV pairs) ------------ */

typedef struct {
    char     key[32];
    char     value[64];
} WUBU_META_ENTRY;

#define WUBU_META_MAX_ENTRIES  16

typedef struct {
    uint32_t           n_entries;
    WUBU_META_ENTRY    entries[WUBU_META_MAX_ENTRIES];
} WUBU_METADATA;

/* -- Container API ----------------------------------------------- */

/*
 * Create a .wubu container.
 * header: pre-filled header (magic/crc filled automatically)
 * payload: raw payload data
 * payload_size: size of payload
 * out_buf: output buffer (must be >= payload_size + WUBU_HEADER_SIZE)
 * out_size: receives actual output size
 * Returns 0 on success.
 */
int wubu_container_create(const WUBU_HEADER *header,
                          const void *payload, size_t payload_size,
                          void *out_buf, size_t out_buf_size,
                          size_t *out_size);

/*
 * Parse a .wubu container from buffer.
 * Validates magic, version, CRC.
 * Returns 0 on success, -1 on error.
 */
int wubu_container_parse(const void *data, size_t data_size,
                         WUBU_HEADER *out_header,
                         const void **out_payload, size_t *out_payload_size);

/*
 * Validate container integrity (magic + CRC).
 * Returns 0 if valid.
 */
int wubu_container_validate(const void *data, size_t data_size);

/*
 * Read metadata from container (if present).
 * Returns number of entries read, or -1 on error.
 */
int wubu_container_read_meta(const void *data, size_t data_size,
                             WUBU_METADATA *out_meta);

/* CRC32 is in wubu_crypto.h */

/*
 * Detect payload type from raw data (for auto-detection).
 * Reads ELF magic, PE magic, shebang, etc.
 */
WUBU_PAYLOAD_TYPE wubu_detect_payload_type(const void *data, size_t size);

/*
 * Create a simple native executable container.
 * Convenience wrapper for common case.
 */
int wubu_container_native_exec(const void *code, size_t code_size,
                               uint64_t entry_offset,
                               void *out_buf, size_t out_buf_size,
                               size_t *out_size);

/*
 * Create a VSL (Linux) container.
 * Wraps a Linux ELF for execution via Virtualization Layer.
 */
int wubu_container_linux_elf(const void *elf_data, size_t elf_size,
                             void *out_buf, size_t out_buf_size,
                             size_t *out_size);

/*
 * Launch a Windows binary through the Proton/container path (the SteamOS
 * strategy: Windows runs in a sandbox, never via an NT-kernel reimpl).
 *
 * Behavior (real, no stubs):
 *   1. Detect payload type (PE vs ELF vs .wubu) via wubu_detect_payload_type.
 *   2. Create a cgroup v2 isolation sandbox (wubu_ct_cgroup_create) so the
 *      foreign process is resource-bounded -- the Pressure-Vessel analog.
 *   3. Route PE -> wubu_proton_exec (real Proton PE loader -> VSL).
 *      Route ELF -> wubu_container_linux_elf (VSL Linux persona).
 *      Route .wubu -> wubu_container_create (native/.wubu dispatch).
 * Returns a VSL/container process id on success, or -1 on error.
 *
 * Note: deep bwrap filesystem isolation (wubu_ct_bwrap_*) is layered on by
 * the caller/session; this fn owns detection + cgroup + the exec route.
 */
int wubu_launch_windows(const void *data, size_t size, const char *cmdline);

#endif /* WUBUOS_CONTAINER_H */
