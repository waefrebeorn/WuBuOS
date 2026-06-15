/*
 * wubu_exec.h  --  WuBuOS Universal Executable Dispatcher
 *
 * WuBuOS doesn't care about file extensions. It reads the header,
 * detects the format, and dispatches to the right handler.
 *
 * Supported formats (auto-detected):
 *   - .wubu containers (native WuBuOS format)
 *   - ELF binaries (Linux) → VSL
 *   - PE executables (Windows) → Proton
 *   - Mach-O (macOS) → VSL/Darling
 *   - Shell scripts (#!) → VSL
 *   - Python scripts → VSL python3
 *   - WASM → wasm runtime
 *   - HolyC source → JIT compile + execute
 *   - C source → compile + execute
 *
 * This is WuBuOS's answer to Linux's binfmt_misc  --  but universal.
 * One exec path. Any format. Zero configuration.
 */

#ifndef WUBUOS_EXEC_H
#define WUBUOS_EXEC_H

#include "wubu_container.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -- Exec Result -------------------------------------------------- */

typedef enum {
    WUBU_EXEC_OK       =  0,
    WUBU_EXEC_ERR_FMT  = -1,   /* Unknown/unsupported format */
    WUBU_EXEC_ERR_LOAD = -2,   /* Failed to load file */
    WUBU_EXEC_ERR_HDR  = -3,   /* Invalid .wubu header */
    WUBU_EXEC_ERR_MEM  = -4,   /* Out of memory */
    WUBU_EXEC_ERR_VSL  = -5,   /* VSL initialization failed */
    WUBU_EXEC_ERR_JIT  = -6,   /* JIT compilation failed */
    WUBU_EXEC_ERR_PERM = -7,   /* Permission denied */
} WUBU_EXEC_RESULT;

/* -- Exec Context ------------------------------------------------- */

typedef struct {
    /* File data */
    const char  *filename;       /* For error messages */
    const void  *file_data;
    size_t       file_size;

    /* Detected format */
    WUBU_PAYLOAD_TYPE payload_type;
    bool              is_wubu;

    /* .wubu header (if applicable) */
    WUBU_HEADER  container;

    /* Execution state */
    int64_t      exit_code;
    bool         vsl_active;     /* VSL was started for this exec */
} WUBU_EXEC_CTX;

/* -- API: Universal Exec ------------------------------------------ */

/*
 * Execute a file from memory.
 * Auto-detects format, dispatches to appropriate handler.
 * Returns exit code (0 = success).
 */
int64_t wubu_exec(const void *data, size_t size, const char *filename);

/*
 * Execute a .wubu container.
 * Parses header, dispatches by payload_type.
 */
int64_t wubu_exec_container(const WUBU_HEADER *hdr,
                            const void *payload, size_t payload_size);

/*
 * Execute a Linux ELF via VSL (Virtualization Substrate Layer).
 * The VSL is WuBuOS's "Proton"  --  a lightweight Linux VM
 * that runs Linux binaries with near-native performance.
 */
int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size);

/*
 * Execute a Windows PE via Proton compatibility layer.
 */
int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size);

/*
 * Execute a HolyC source file  --  JIT compile and run.
 */
int64_t wubu_exec_holyc(const char *source, size_t source_size);

/*
 * Execute a C source file  --  compile via HolyC compiler and run.
 */
int64_t wubu_exec_c(const char *source, size_t source_size);

/*
 * Execute a shell script via VSL.
 */
int64_t wubu_exec_shell(const char *script, size_t script_size);

/*
 * Execute a Python script via VSL.
 */
int64_t wubu_exec_python(const char *script, size_t script_size);

/* -- VSL (Virtualization Substrate Layer) ------------------------- */

/*
 * Initialize the VSL  --  WuBuOS's lightweight Linux VM.
 * This is the "Proton" layer: a minimal Linux environment
 * that runs Linux/Windows/macOS binaries.
 *
 * Architecture:
 *   - Ring-0 WuBuOS owns the hardware
 *   - VSL runs Linux in a lightweight VM (not full virtualization)
 *   - Linux drivers (Vulkan, CUDA, networking) are accessible
 *   - Near-native performance via direct hardware passthrough
 *
 * Returns 0 on success.
 */
int wubu_vsl_init(void);

/*
 * Shutdown VSL.
 */
void wubu_vsl_shutdown(void);

/*
 * Check if VSL is active.
 */
bool wubu_vsl_active(void);

/*
 * Run a command inside VSL.
 * Returns exit code.
 */
int64_t wubu_vsl_run(const char *cmd);

/* -- Format Detection --------------------------------------------- */

/*
 * Detect format from raw file data.
 * Returns the payload type and whether it's a .wubu container.
 */
WUBU_PAYLOAD_TYPE wubu_detect_format(const void *data, size_t size,
                                     bool *is_wubu);

/*
 * Get human-readable name for a payload type.
 */
const char *wubu_payload_name(WUBU_PAYLOAD_TYPE type);

/*
 * Get human-readable name for an exec result.
 */
const char *wubu_exec_result_str(WUBU_EXEC_RESULT result);

#endif /* WUBUOS_EXEC_H */
