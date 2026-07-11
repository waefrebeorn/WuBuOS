/*
 * wubucontainer.h  --  WuBuContainer Conversion Toolkit C Interface
 * 
 * This provides the C-side interface to the WuBuContainer TypeScript/JS
 * universal file converter (https://github.com/waefrebeorn/WuBuContainer).
 * 
 * The actual conversion runs in a Node.js/Bun + Electron + WASM runtime.
 * This header defines the IPC contract and C API for WuBuOS integration.
 * 
 * Key capability: converts ANY format to ANY other format via intermediate
 * format graph traversal (e.g., AVI → PDF, SVG → EXE, WAV → MIDI, etc.)
 * 
 * Agentic Latent AGI Layer: This is a core primitive for the agentic OS -
 * agents can convert arbitrary data formats to/from canonical representations
 * for reasoning, synthesis, and cross-modal operations.
 */

#ifndef WUBU_CONTAINER_H
#define WUBU_CONTAINER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Constants
 * ================================================================ */

#define WUBU_CONTAINER_MAX_PATH     4096
#define WUBU_CONTAINER_MAX_MIME     256
#define WUBU_CONTAINER_MAX_FORMATS  512
#define WUBU_CONTAINER_MAX_HANDLERS 64
#define WUBU_CONTAINER_MAX_CONVERSION_DEPTH 16

/* Error codes */
typedef enum {
    WUBU_CTR_OK            = 0,
    WUBU_CTR_ERR_INIT      = -1,  /* Engine not initialized */
    WUBU_CTR_ERR_IO        = -2,  /* File I/O error */
    WUBU_CTR_ERR_FORMAT    = -3,  /* Unsupported format */
    WUBU_CTR_ERR_CONVERT   = -4,  /* Conversion failed */
    WUBU_CTR_ERR_NO_ROUTE  = -5,  /* No conversion path found */
    WUBU_CTR_ERR_DEAD_END  = -6,  /* Hit dead end in graph */
    WUBU_CTR_ERR_TIMEOUT   = -7,  /* Conversion timed out */
    WUBU_CTR_ERR_OOM       = -8,  /* Out of memory */
    WUBU_CTR_ERR_INVAL     = -9,  /* Invalid argument */
    WUBU_CTR_ERR_ENGINE    = -10, /* Engine crash/restart needed */
} WubuContainerError;

/* ================================================================
 * Data Structures
 * ================================================================ */

/* Format descriptor - mirrors TypeScript FileFormat */
typedef struct {
    char mime[WUBU_CONTAINER_MAX_MIME];      /* e.g., "image/png" */
    char format[64];                          /* e.g., "PNG" */
    char extension[32];                       /* e.g., ".png" */
    char name[128];                           /* Human-readable name */
    bool from;                                /* Can be input */
    bool to;                                  /* Can be output */
    char handler[64];                         /* Handler name that provides this */
} WubuContainerFormat;

/* Handler descriptor - mirrors TypeScript FormatHandler */
typedef struct {
    char name[64];                            /* e.g., "FFmpeg", "ImageMagick" */
    uint32_t format_count;
    WubuContainerFormat formats[WUBU_CONTAINER_MAX_FORMATS];
    bool ready;
    bool supports_any_input;                  /* Can accept arbitrary binary */
} WubuContainerHandler;

/* Conversion path node */
typedef struct {
    WubuContainerFormat format;
    WubuContainerHandler *handler;
} WubuContainerPathNode;

/* Full conversion path */
typedef struct {
    uint32_t node_count;
    WubuContainerPathNode nodes[WUBU_CONTAINER_MAX_CONVERSION_DEPTH];
} WubuContainerPath;

/* Conversion request */
typedef struct {
    const char *input_path;
    const char *output_path;
    const char *input_mime;      /* Optional: override detection */
    const char *output_mime;     /* Optional: override detection */
    const char *input_format;    /* Optional: override detection */
    const char *output_format;   /* Optional: override detection */
    uint32_t timeout_ms;         /* 0 = default (30s) */
    bool simple_mode;            /* true = group by format, false = by handler */
} WubuContainerConvertRequest;

/* Conversion result */
typedef struct {
    int error_code;
    char error_message[512];
    WubuContainerPath path_used;
    uint64_t duration_ms;
    size_t input_bytes;
    size_t output_bytes;
} WubuContainerConvertResult;

/* Engine state (opaque) */
typedef struct WubuContainerEngine WubuContainerEngine;

/* ================================================================
 * Public API
 * ================================================================ */

/* Initialize the conversion engine.
 * 
 * Spawns the Node.js/Bun + Electron + WASM subprocess.
 * Loads format handlers from the WuBuContainer submodule.
 * 
 * @param engine_out    Output engine handle
 * @param container_dir Path to WuBuContainer submodule (or installed location)
 * @return 0 on success, negative error code on failure
 */
int wubu_container_init(WubuContainerEngine **engine_out, const char *container_dir);

/* Shutdown and cleanup */
void wubu_container_shutdown(WubuContainerEngine *engine);

/* Get all supported formats (aggregated from all handlers) */
int wubu_container_get_formats(WubuContainerEngine *engine,
                                WubuContainerFormat *out_formats,
                                uint32_t *inout_count);

/* Get all handlers */
int wubu_container_get_handlers(WubuContainerEngine *engine,
                                 WubuContainerHandler *out_handlers,
                                 uint32_t *inout_count);

/* Find conversion path from input format to output format */
int wubu_container_find_path(WubuContainerEngine *engine,
                              const char *input_mime, const char *input_format,
                              const char *output_mime, const char *output_format,
                              WubuContainerPath *out_path);

/* Execute a conversion */
int wubu_container_convert(WubuContainerEngine *engine,
                            const WubuContainerConvertRequest *request,
                            WubuContainerConvertResult *result);

/* Convenience: convert file by paths (auto-detect formats) */
int wubu_container_convert_file(WubuContainerEngine *engine,
                                 const char *input_path,
                                 const char *output_path,
                                 WubuContainerConvertResult *result);

/* Convenience: convert memory buffer */
int wubu_container_convert_buffer(WubuContainerEngine *engine,
                                   const void *input_data, size_t input_size,
                                   const char *input_mime, const char *input_format,
                                   const char *output_mime, const char *output_format,
                                   void **output_data, size_t *output_size,
                                   WubuContainerConvertResult *result);

/* Format detection from file extension or magic bytes */
int wubu_container_detect_format(WubuContainerEngine *engine,
                                  const char *file_path,
                                  const void *data, size_t data_size,
                                  char *out_mime, size_t mime_size,
                                  char *out_format, size_t format_size);

/* Print supported format cache (for debugging) */
void wubu_container_print_cache(WubuContainerEngine *engine);

/* Register custom format handler (for agentic layer extensions) */
int wubu_container_register_handler(WubuContainerEngine *engine,
                                     const WubuContainerHandler *handler);

/* Introspection accessors (engine state is opaque) */
int wubu_container_registered_count(const WubuContainerEngine *engine);
const char *wubu_container_registered_name(const WubuContainerEngine *engine, int idx);

/* ================================================================
 * High-level Agentic API
 * ================================================================ */

/* Convert to canonical representation for reasoning
 * 
 * The agentic latent AGI layer uses canonical forms:
 * - Images → PNG (lossless) or JPEG (perceptual)
 * - Audio → WAV (raw PCM) or OPUS (compressed)
 * - Video → WebM/VP9 or MP4/H.264
 * - Documents → Markdown or JSON
 * - Archives → TAR
 * - Executables → ELF analysis JSON
 * - 3D models → glTF
 * - Fonts → TTF/OTF
 * - etc.
 */
int wubu_container_to_canonical(WubuContainerEngine *engine,
                                 const char *input_path,
                                 const char *canonical_output_dir,
                                 const char *media_type);  /* "image", "audio", "video", "doc", "archive", "model", "font", "exec" */

/* Convert from canonical representation to target format */
int wubu_container_from_canonical(WubuContainerEngine *engine,
                                   const char *canonical_input_path,
                                   const char *target_format,
                                   const char *output_path);

/* Batch convert multiple files */
typedef struct {
    const char **input_paths;
    uint32_t input_count;
    const char *output_dir;
    const char *target_format;  /* NULL = auto-select canonical */
    bool parallel;
} WubuContainerBatchRequest;

int wubu_container_batch_convert(WubuContainerEngine *engine,
                                  const WubuContainerBatchRequest *request,
                                  WubuContainerConvertResult *results);  /* Array of size input_count */

#ifdef __cplusplus
}
#endif

#endif /* WUBU_CONTAINER_H */