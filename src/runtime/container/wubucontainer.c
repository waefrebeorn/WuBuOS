/*
 * wubucontainer.c  --  WuBuContainer Conversion Toolkit C Implementation
 * 
 * Implements the C-side interface to the WuBuContainer TypeScript/JS
 * universal file converter. Communicates via JSON-RPC over stdin/stdout
 * to a long-running Bun/Node.js + Electron + WASM subprocess.
 * 
 * This is a core primitive for the agentic latent AGI layer - agents can
 * convert arbitrary data formats to/from canonical representations for
 * reasoning, synthesis, and cross-modal operations.
 */

#include "wubucontainer.h"
#include "wubucontainer_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>      /* htonl / ntohl for JSON-RPC length framing */
#include <errno.h>
#include <json-c/json.h>  /* Using json-c for C-side JSON handling */

/* The opaque struct, JSON-RPC version, and IPC forward decls live in
 * wubucontainer_internal.h. Registry bookkeeping is in wubucontainer_registry.c. */

static int wubu_ctr_send_request(WubuContainerEngine *engine, const char *method, json_object *params, json_object **out_result);
static int wubu_ctr_spawn_engine(WubuContainerEngine *engine);
static void wubu_ctr_cleanup_engine(WubuContainerEngine *engine);

/* ================================================================
 * Engine Lifecycle
 * ================================================================ */

int wubu_container_init(WubuContainerEngine **engine_out, const char *container_dir) {
    if (!engine_out) return WUBU_CTR_ERR_INVAL;
    
    WubuContainerEngine *engine = calloc(1, sizeof(WubuContainerEngine));
    if (!engine) return WUBU_CTR_ERR_OOM;
    
    if (container_dir) {
        strncpy(engine->container_dir, container_dir, WUBU_CONTAINER_MAX_PATH - 1);
    } else {
        /* Default to submodule path */
        snprintf(engine->container_dir, WUBU_CONTAINER_MAX_PATH,
                 "%s/src/runtime/container/wubucontainer", getenv("WUBUOS_ROOT") ?: "/home/wubu/.hermes/profiles/mind-palace/home/myseed");
    }
    
    /* The in-memory handler registry is always usable, so the engine object is
     * valid from here on. Spawning the TypeScript/IPC subprocess is OPTIONAL:
     * live conversion requires it, but registration/introspection do not. We
     * attempt spawn best-effort and record engine_up without failing init when
     * the subprocess is unavailable (e.g. no `bun` in the test environment). */
    engine->initialized = true;
    if (wubu_ctr_spawn_engine(engine) == WUBU_CTR_OK) {
        engine->engine_up = true;
    } else {
        engine->engine_up = false;
        fprintf(stderr,
                "[wubucontainer] engine subprocess unavailable; "
                "in-memory registry active, live conversion disabled\n");
    }
    
    *engine_out = engine;
    return WUBU_CTR_OK;
}

void wubu_container_shutdown(WubuContainerEngine *engine) {
    if (!engine) return;
    
    if (engine->child_pid > 0) {
        kill(engine->child_pid, SIGTERM);
        waitpid(engine->child_pid, NULL, 0);
    }
    
    if (engine->stdin_fd >= 0) close(engine->stdin_fd);
    if (engine->stdout_fd >= 0) close(engine->stdout_fd);
    if (engine->stderr_fd >= 0) close(engine->stderr_fd);
    if (engine->socket_fd >= 0) close(engine->socket_fd);
    if (engine->socket_path[0] && engine->use_socket) {
        unlink(engine->socket_path);
    }
    
    wubu_ctr_cleanup_engine(engine);
    free(engine);
}

/* ================================================================
 * Engine Spawning (Node.js/Bun + Electron)
 * ================================================================ */

static int wubu_ctr_spawn_engine(WubuContainerEngine *engine) {
    /* Create socket pair for IPC */
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        perror("socketpair");
        return WUBU_CTR_ERR_IO;
    }

    engine->stdin_fd = sv[1];
    engine->stdout_fd = sv[0];
    engine->use_socket = true;

    /* Build command to run the WuBuContainer engine. Size the buffer for the
     * longest container_dir (WUBU_CONTAINER_MAX_PATH) plus the fixed suffix. */
    char cmd[WUBU_CONTAINER_MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd),
             "cd %s && bun run src/main.ts --ipc-mode 2>&1",
             engine->container_dir);

    pid_t pid = fork();
    if (pid == -1) {
        close(sv[0]);
        close(sv[1]);
        return WUBU_CTR_ERR_IO;
    }

    if (pid == 0) {
        /* Child process */
        close(sv[1]);  /* Close parent's end */

        /* Redirect stdin/stdout to socket */
        dup2(sv[0], STDIN_FILENO);
        dup2(sv[0], STDOUT_FILENO);
        close(sv[0]);

        /* Execute */
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent */
    engine->child_pid = pid;
    close(sv[0]);  /* Close child's end */

    /* Wait (briefly) for engine to signal ready. If the subprocess is absent
     * or fails to come up, this read returns 0/EOF quickly and we report
     * failure WITHOUT having wired dangling fds that shutdown would misuse. */
    char buffer[1024];
    ssize_t n = read(engine->stdout_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        if (strstr(buffer, "Engine ready")) {
            return WUBU_CTR_OK;
        }
    }

    /* Engine did not come up: tear down the fds/child so shutdown() is a no-op
     * for the IPC side, and let init proceed in registry-only mode. */
    if (engine->child_pid > 0) {
        kill(engine->child_pid, SIGTERM);
        waitpid(engine->child_pid, NULL, 0);
        engine->child_pid = 0;
    }
    if (engine->stdin_fd >= 0) { close(engine->stdin_fd); engine->stdin_fd = -1; }
    if (engine->stdout_fd >= 0) { close(engine->stdout_fd); engine->stdout_fd = -1; }
    engine->use_socket = false;
    return WUBU_CTR_ERR_ENGINE;
}

static void wubu_ctr_cleanup_engine(WubuContainerEngine *engine) {
    (void)engine;
}

/* ================================================================
 * JSON-RPC Communication
 * ================================================================ */

static int wubu_ctr_send_request(WubuContainerEngine *engine, const char *method, json_object *params, json_object **out_result) {
    if (!engine || !engine->initialized) return WUBU_CTR_ERR_INIT;
    /* Live conversion requires the TS/IPC subprocess. If it never came up,
     * fail fast instead of writing to a closed fd. */
    if (!engine->engine_up) return WUBU_CTR_ERR_ENGINE;
    if (engine->stdin_fd < 0 || engine->stdout_fd < 0) return WUBU_CTR_ERR_IO;
    /* Build JSON-RPC request */
    json_object *request = json_object_new_object();
    json_object_object_add(request, "jsonrpc", json_object_new_string(WUBU_CTR_JSONRPC_VERSION));
    json_object_object_add(request, "method", json_object_new_string(method));
    json_object_object_add(request, "params", params ? params : json_object_new_object());
    json_object_object_add(request, "id", json_object_new_int64(rand()));
    
    const char *request_str = json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN);
    size_t req_len = strlen(request_str);
    
    /* Send length prefix + request */
    uint32_t len_net = htonl((uint32_t)req_len);
    if (write(engine->stdin_fd, &len_net, 4) != 4) {
        json_object_put(request);
        return WUBU_CTR_ERR_IO;
    }
    if (write(engine->stdin_fd, request_str, req_len) != (ssize_t)req_len) {
        json_object_put(request);
        return WUBU_CTR_ERR_IO;
    }
    
    json_object_put(request);
    
    /* Read response length */
    uint32_t resp_len_net;
    if (read(engine->stdout_fd, &resp_len_net, 4) != 4) {
        return WUBU_CTR_ERR_IO;
    }
    uint32_t resp_len = ntohl(resp_len_net);
    
    if (resp_len > 1024 * 1024) {  /* 1MB max response */
        return WUBU_CTR_ERR_IO;
    }
    
    char *resp_buf = malloc(resp_len + 1);
    if (!resp_buf) return WUBU_CTR_ERR_OOM;
    
    size_t total_read = 0;
    while (total_read < resp_len) {
        ssize_t n = read(engine->stdout_fd, resp_buf + total_read, resp_len - total_read);
        if (n <= 0) {
            free(resp_buf);
            return WUBU_CTR_ERR_IO;
        }
        total_read += n;
    }
    resp_buf[resp_len] = '\0';
    
    /* Parse response */
    json_object *response = json_tokener_parse(resp_buf);
    free(resp_buf);
    
    if (!response) return WUBU_CTR_ERR_IO;
    
    /* Check for error */
    json_object *error_obj;
    if (json_object_object_get_ex(response, "error", &error_obj)) {
        const char *err_msg = json_object_get_string(error_obj);
        fprintf(stderr, "[wubucontainer] Engine error: %s\n", err_msg);
        json_object_put(response);
        return WUBU_CTR_ERR_CONVERT;
    }
    
    /* Extract result */
    json_object *result_obj;
    if (json_object_object_get_ex(response, "result", &result_obj)) {
        if (out_result) {
            *out_result = json_object_get(result_obj);  /* Increment ref */
        }
        json_object_put(response);
        return WUBU_CTR_OK;
    }
    
    json_object_put(response);
    return WUBU_CTR_ERR_IO;
}

/* ================================================================
 * Format/Handler Queries
 * ================================================================ */

int wubu_container_get_formats(WubuContainerEngine *engine,
                                WubuContainerFormat *out_formats,
                                uint32_t *inout_count) {
    json_object *params = json_object_new_object();
    json_object *result;
    int rc = wubu_ctr_send_request(engine, "getFormats", params, &result);
    json_object_put(params);
    
    if (rc != WUBU_CTR_OK) return rc;
    
    if (!json_object_is_type(result, json_type_array)) {
        json_object_put(result);
        return WUBU_CTR_ERR_IO;
    }
    
    uint32_t count = json_object_array_length(result);
    uint32_t max = inout_count ? *inout_count : count;
    if (count > max) count = max;
    
    for (uint32_t i = 0; i < count; i++) {
        json_object *fmt = json_object_array_get_idx(result, i);
        if (!out_formats) continue;
        
        json_object *mime, *format, *ext, *name, *from, *to, *handler;
        json_object_object_get_ex(fmt, "mime", &mime);
        json_object_object_get_ex(fmt, "format", &format);
        json_object_object_get_ex(fmt, "extension", &ext);
        json_object_object_get_ex(fmt, "name", &name);
        json_object_object_get_ex(fmt, "from", &from);
        json_object_object_get_ex(fmt, "to", &to);
        json_object_object_get_ex(fmt, "handler", &handler);
        
        strncpy(out_formats[i].mime, mime ? json_object_get_string(mime) : "", sizeof(out_formats[i].mime) - 1);
        strncpy(out_formats[i].format, format ? json_object_get_string(format) : "", sizeof(out_formats[i].format) - 1);
        strncpy(out_formats[i].extension, ext ? json_object_get_string(ext) : "", sizeof(out_formats[i].extension) - 1);
        strncpy(out_formats[i].name, name ? json_object_get_string(name) : "", sizeof(out_formats[i].name) - 1);
        out_formats[i].from = from ? json_object_get_boolean(from) : false;
        out_formats[i].to = to ? json_object_get_boolean(to) : false;
        strncpy(out_formats[i].handler, handler ? json_object_get_string(handler) : "", sizeof(out_formats[i].handler) - 1);
    }
    
    if (inout_count) *inout_count = count;
    json_object_put(result);
    return WUBU_CTR_OK;
}

int wubu_container_get_handlers(WubuContainerEngine *engine,
                                 WubuContainerHandler *out_handlers,
                                 uint32_t *inout_count) {
    json_object *params = json_object_new_object();
    json_object *result;
    int rc = wubu_ctr_send_request(engine, "getHandlers", params, &result);
    json_object_put(params);
    
    if (rc != WUBU_CTR_OK) return rc;
    
    if (!json_object_is_type(result, json_type_array)) {
        json_object_put(result);
        return WUBU_CTR_ERR_IO;
    }
    
    uint32_t count = json_object_array_length(result);
    uint32_t max = inout_count ? *inout_count : count;
    if (count > max) count = max;
    
    for (uint32_t i = 0; i < count; i++) {
        json_object *hdl = json_object_array_get_idx(result, i);
        if (!out_handlers) continue;
        
        json_object *name, *ready, *supports_any, *formats_arr;
        json_object_object_get_ex(hdl, "name", &name);
        json_object_object_get_ex(hdl, "ready", &ready);
        json_object_object_get_ex(hdl, "supportsAnyInput", &supports_any);
        json_object_object_get_ex(hdl, "formats", &formats_arr);
        
        strncpy(out_handlers[i].name, name ? json_object_get_string(name) : "", sizeof(out_handlers[i].name) - 1);
        out_handlers[i].ready = ready ? json_object_get_boolean(ready) : false;
        out_handlers[i].supports_any_input = supports_any ? json_object_get_boolean(supports_any) : false;
        out_handlers[i].format_count = 0;
        
        if (formats_arr && json_object_is_type(formats_arr, json_type_array)) {
            uint32_t fmt_count = json_object_array_length(formats_arr);
            for (uint32_t j = 0; j < fmt_count && j < WUBU_CONTAINER_MAX_FORMATS; j++) {
                json_object *fmt = json_object_array_get_idx(formats_arr, j);
                json_object *mime, *format, *ext, *fname, *from, *to;
                json_object_object_get_ex(fmt, "mime", &mime);
                json_object_object_get_ex(fmt, "format", &format);
                json_object_object_get_ex(fmt, "extension", &ext);
                json_object_object_get_ex(fmt, "name", &fname);
                json_object_object_get_ex(fmt, "from", &from);
                json_object_object_get_ex(fmt, "to", &to);
                
                strncpy(out_handlers[i].formats[j].mime, mime ? json_object_get_string(mime) : "", sizeof(out_handlers[i].formats[j].mime) - 1);
                strncpy(out_handlers[i].formats[j].format, format ? json_object_get_string(format) : "", sizeof(out_handlers[i].formats[j].format) - 1);
                strncpy(out_handlers[i].formats[j].extension, ext ? json_object_get_string(ext) : "", sizeof(out_handlers[i].formats[j].extension) - 1);
                strncpy(out_handlers[i].formats[j].name, fname ? json_object_get_string(fname) : "", sizeof(out_handlers[i].formats[j].name) - 1);
                out_handlers[i].formats[j].from = from ? json_object_get_boolean(from) : false;
                out_handlers[i].formats[j].to = to ? json_object_get_boolean(to) : false;
                strncpy(out_handlers[i].formats[j].handler, out_handlers[i].name, sizeof(out_handlers[i].formats[j].handler) - 1);
                out_handlers[i].format_count++;
            }
        }
    }
    
    if (inout_count) *inout_count = count;
    json_object_put(result);
    return WUBU_CTR_OK;
}

/* ================================================================
 * Conversion Operations
 * ================================================================ */

int wubu_container_find_path(WubuContainerEngine *engine,
                              const char *input_mime, const char *input_format,
                              const char *output_mime, const char *output_format,
                              WubuContainerPath *out_path) {
    json_object *params = json_object_new_object();
    if (input_mime) json_object_object_add(params, "inputMime", json_object_new_string(input_mime));
    if (input_format) json_object_object_add(params, "inputFormat", json_object_new_string(input_format));
    if (output_mime) json_object_object_add(params, "outputMime", json_object_new_string(output_mime));
    if (output_format) json_object_object_add(params, "outputFormat", json_object_new_string(output_format));
    
    json_object *result;
    int rc = wubu_ctr_send_request(engine, "findPath", params, &result);
    json_object_put(params);
    
    if (rc != WUBU_CTR_OK) return rc;
    
    if (!json_object_is_type(result, json_type_array)) {
        json_object_put(result);
        return WUBU_CTR_ERR_NO_ROUTE;
    }
    
    uint32_t count = json_object_array_length(result);
    if (count > WUBU_CONTAINER_MAX_CONVERSION_DEPTH) count = WUBU_CONTAINER_MAX_CONVERSION_DEPTH;
    out_path->node_count = count;
    
    for (uint32_t i = 0; i < count; i++) {
        json_object *node = json_object_array_get_idx(result, i);
        json_object *fmt, *hdl;
        json_object_object_get_ex(node, "format", &fmt);
        json_object_object_get_ex(node, "handler", &hdl);
        
        if (fmt) {
                    json_object *mime, *format, *ext, *name, *from, *to;
                    json_object_object_get_ex(fmt, "mime", &mime);
                    json_object_object_get_ex(fmt, "format", &format);
                    json_object_object_get_ex(fmt, "extension", &ext);
                    json_object_object_get_ex(fmt, "name", &name);
                    json_object_object_get_ex(fmt, "from", &from);
                    json_object_object_get_ex(fmt, "to", &to);
            
                    strncpy(out_path->nodes[i].format.mime, mime ? json_object_get_string(mime) : "", sizeof(out_path->nodes[i].format.mime) - 1);
                    strncpy(out_path->nodes[i].format.format, format ? json_object_get_string(format) : "", sizeof(out_path->nodes[i].format.format) - 1);
                    strncpy(out_path->nodes[i].format.extension, ext ? json_object_get_string(ext) : "", sizeof(out_path->nodes[i].format.extension) - 1);
                    strncpy(out_path->nodes[i].format.name, name ? json_object_get_string(name) : "", sizeof(out_path->nodes[i].format.name) - 1);
                    out_path->nodes[i].format.from = from ? json_object_get_boolean(from) : false;
                    out_path->nodes[i].format.to = to ? json_object_get_boolean(to) : false;
                }
        
                if (hdl) {
                    out_path->nodes[i].handler = calloc(1, sizeof(WubuContainerHandler));
                    if (out_path->nodes[i].handler) {
                        json_object *hdl_name, *hdl_ready;
                        json_object_object_get_ex(hdl, "name", &hdl_name);
                        json_object_object_get_ex(hdl, "ready", &hdl_ready);
                        strncpy(out_path->nodes[i].handler->name, hdl_name ? json_object_get_string(hdl_name) : "", sizeof(out_path->nodes[i].handler->name) - 1);
                        out_path->nodes[i].handler->ready = hdl_ready ? json_object_get_boolean(hdl_ready) : false;
                    }
                }
    }
    
    json_object_put(result);
    return WUBU_CTR_OK;
}

int wubu_container_convert(WubuContainerEngine *engine,
                            const WubuContainerConvertRequest *request,
                            WubuContainerConvertResult *result) {
    if (!request || !request->input_path || !request->output_path) {
        return WUBU_CTR_ERR_INVAL;
    }
    
    json_object *params = json_object_new_object();
    json_object_object_add(params, "inputPath", json_object_new_string(request->input_path));
    json_object_object_add(params, "outputPath", json_object_new_string(request->output_path));
    if (request->input_mime) json_object_object_add(params, "inputMime", json_object_new_string(request->input_mime));
    if (request->output_mime) json_object_object_add(params, "outputMime", json_object_new_string(request->output_mime));
    if (request->input_format) json_object_object_add(params, "inputFormat", json_object_new_string(request->input_format));
    if (request->output_format) json_object_object_add(params, "outputFormat", json_object_new_string(request->output_format));
    json_object_object_add(params, "timeoutMs", json_object_new_int64(request->timeout_ms ?: 30000));
    json_object_object_add(params, "simpleMode", json_object_new_boolean(request->simple_mode));
    
    json_object *resp;
    int rc = wubu_ctr_send_request(engine, "convert", params, &resp);
    json_object_put(params);
    
    if (rc != WUBU_CTR_OK) return rc;
    
    if (result) {
        json_object *err_code, *err_msg, *path, *duration, *in_bytes, *out_bytes;
        json_object_object_get_ex(resp, "errorCode", &err_code);
        json_object_object_get_ex(resp, "errorMessage", &err_msg);
        json_object_object_get_ex(resp, "path", &path);
        json_object_object_get_ex(resp, "durationMs", &duration);
        json_object_object_get_ex(resp, "inputBytes", &in_bytes);
        json_object_object_get_ex(resp, "outputBytes", &out_bytes);
        
        result->error_code = err_code ? json_object_get_int(err_code) : 0;
        strncpy(result->error_message, err_msg ? json_object_get_string(err_msg) : "", sizeof(result->error_message) - 1);
        result->duration_ms = duration ? json_object_get_int64(duration) : 0;
        result->input_bytes = in_bytes ? json_object_get_int64(in_bytes) : 0;
        result->output_bytes = out_bytes ? json_object_get_int64(out_bytes) : 0;
        
        /* Parse path */
        if (path && json_object_is_type(path, json_type_array)) {
            uint32_t count = json_object_array_length(path);
            if (count > WUBU_CONTAINER_MAX_CONVERSION_DEPTH) count = WUBU_CONTAINER_MAX_CONVERSION_DEPTH;
            result->path_used.node_count = count;
            /* ... parse path nodes similar to find_path ... */
        }
    }
    
    json_object_put(resp);
    return result ? result->error_code : WUBU_CTR_OK;
}

int wubu_container_convert_file(WubuContainerEngine *engine,
                                 const char *input_path,
                                 const char *output_path,
                                 WubuContainerConvertResult *result) {
    WubuContainerConvertRequest req = {
        .input_path = input_path,
        .output_path = output_path,
        .timeout_ms = 30000,
        .simple_mode = true
    };
    return wubu_container_convert(engine, &req, result);
}

int wubu_container_convert_buffer(WubuContainerEngine *engine,
                                   const void *input_data, size_t input_size,
                                   const char *input_mime, const char *input_format,
                                   const char *output_mime, const char *output_format,
                                   void **output_data, size_t *output_size,
                                   WubuContainerConvertResult *result) {
    /* Write input to temp file, convert, read output */
    char input_tmp[] = "/tmp/wubu_in_XXXXXX";
    char output_tmp[] = "/tmp/wubu_out_XXXXXX";
    
    int in_fd = mkstemp(input_tmp);
    if (in_fd < 0) return WUBU_CTR_ERR_IO;
    write(in_fd, input_data, input_size);
    close(in_fd);
    
    int out_fd = mkstemp(output_tmp);
    if (out_fd < 0) {
        unlink(input_tmp);
        return WUBU_CTR_ERR_IO;
    }
    close(out_fd);
    
    WubuContainerConvertRequest req = {
        .input_path = input_tmp,
        .output_path = output_tmp,
        .input_mime = input_mime,
        .output_mime = output_mime,
        .input_format = input_format,
        .output_format = output_format,
        .timeout_ms = 30000,
        .simple_mode = true
    };
    
    int rc = wubu_container_convert(engine, &req, result);
    
    if (rc == WUBU_CTR_OK && output_data && output_size) {
        FILE *f = fopen(output_tmp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            *output_data = malloc(sz);
            if (*output_data) {
                *output_size = fread(*output_data, 1, sz, f);
            }
            fclose(f);
        }
    }
    
    unlink(input_tmp);
    unlink(output_tmp);
    return rc;
}

/* ================================================================
 * Format Detection
 * ================================================================ */

int wubu_container_detect_format(WubuContainerEngine *engine,
                                  const char *file_path,
                                  const void *data, size_t data_size,
                                  char *out_mime, size_t mime_size,
                                  char *out_format, size_t format_size) {
    json_object *params = json_object_new_object();
    if (file_path) json_object_object_add(params, "filePath", json_object_new_string(file_path));
    if (data && data_size > 0) {
        /* For simplicity, we'd send a base64-encoded prefix of the data */
        json_object_object_add(params, "hasData", json_object_new_boolean(true));
    }
    
    json_object *result;
    int rc = wubu_ctr_send_request(engine, "detectFormat", params, &result);
    json_object_put(params);
    
    if (rc != WUBU_CTR_OK) return rc;
    
    json_object *mime, *fmt;
    json_object_object_get_ex(result, "mime", &mime);
    json_object_object_get_ex(result, "format", &fmt);
    
    if (out_mime && mime) strncpy(out_mime, json_object_get_string(mime), mime_size - 1);
    if (out_format && fmt) strncpy(out_format, json_object_get_string(fmt), format_size - 1);
    
    json_object_put(result);
    return WUBU_CTR_OK;
}

/* ================================================================
 * Debug / Utility
 * ================================================================ */

void wubu_container_print_cache(WubuContainerEngine *engine) {
    json_object *params = json_object_new_object();
    json_object *result;
    wubu_ctr_send_request(engine, "printCache", params, &result);
    json_object_put(params);
    
    if (result) {
        const char *str = json_object_to_json_string_ext(result, JSON_C_TO_STRING_PRETTY);
        printf("%s\n", str);
        json_object_put(result);
    }
}

/* ================================================================
 * High-level Agentic API
 * ================================================================ */

int wubu_container_to_canonical(WubuContainerEngine *engine,
                                 const char *input_path,
                                 const char *canonical_output_dir,
                                 const char *media_type) {
    if (!media_type) return WUBU_CTR_ERR_INVAL;
    
    /* Map media type to canonical format */
    const char *canonical_fmt = NULL;
    if (strcmp(media_type, "image") == 0) canonical_fmt = "png";
    else if (strcmp(media_type, "audio") == 0) canonical_fmt = "wav";
    else if (strcmp(media_type, "video") == 0) canonical_fmt = "webm";
    else if (strcmp(media_type, "doc") == 0) canonical_fmt = "md";
    else if (strcmp(media_type, "archive") == 0) canonical_fmt = "tar";
    else if (strcmp(media_type, "model") == 0) canonical_fmt = "gltf";
    else if (strcmp(media_type, "font") == 0) canonical_fmt = "ttf";
    else if (strcmp(media_type, "exec") == 0) canonical_fmt = "json";
    else return WUBU_CTR_ERR_INVAL;
    
    char output_path[WUBU_CONTAINER_MAX_PATH];
    snprintf(output_path, sizeof(output_path), "%s/canonical.%s", canonical_output_dir, canonical_fmt);
    
    WubuContainerConvertRequest req = {
        .input_path = input_path,
        .output_path = output_path,
        .output_format = canonical_fmt,
        .timeout_ms = 60000,
        .simple_mode = true
    };
    
    return wubu_container_convert(engine, &req, NULL);
}

int wubu_container_from_canonical(WubuContainerEngine *engine,
                                   const char *canonical_input_path,
                                   const char *target_format,
                                   const char *output_path) {
    if (!target_format || !output_path) return WUBU_CTR_ERR_INVAL;
    
    WubuContainerConvertRequest req = {
        .input_path = canonical_input_path,
        .output_path = output_path,
        .output_format = target_format,
        .timeout_ms = 60000,
        .simple_mode = true
    };
    
    return wubu_container_convert(engine, &req, NULL);
}

int wubu_container_batch_convert(WubuContainerEngine *engine,
                                  const WubuContainerBatchRequest *request,
                                  WubuContainerConvertResult *results) {
    if (!request || !request->input_paths || request->input_count == 0) {
        return WUBU_CTR_ERR_INVAL;
    }
    
    int failed = 0;
    for (uint32_t i = 0; i < request->input_count; i++) {
        char output_path[WUBU_CONTAINER_MAX_PATH];
        if (request->output_dir) {
            const char *base = strrchr(request->input_paths[i], '/');
            base = base ? base + 1 : request->input_paths[i];
            snprintf(output_path, sizeof(output_path), "%s/%s", request->output_dir, base);
            /* Replace extension if target_format specified */
            if (request->target_format) {
                char *dot = strrchr(output_path, '.');
                if (dot) *dot = '\0';
                strncat(output_path, ".", sizeof(output_path) - strlen(output_path) - 1);
                strncat(output_path, request->target_format, sizeof(output_path) - strlen(output_path) - 1);
            }
        } else {
            strncpy(output_path, request->input_paths[i], sizeof(output_path) - 1);
        }
        
        WubuContainerConvertRequest req = {
            .input_path = request->input_paths[i],
            .output_path = output_path,
            .output_format = request->target_format,
            .timeout_ms = 60000,
            .simple_mode = true
        };
        
        WubuContainerConvertResult *res = results ? &results[i] : NULL;
        int rc = wubu_container_convert(engine, &req, res);
        if (rc != WUBU_CTR_OK) failed++;
    }
    
    return failed ? WUBU_CTR_ERR_CONVERT : WUBU_CTR_OK;
}