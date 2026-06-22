/*
 * wubu_image.c  --  WuBuOS Container Image Builder
 *
 * Phase 7: .wubu image builder implementation
 * - WuBuFile (Dockerfile-like) parsing
 * - Multi-stage builds
 * - Layer caching with content-addressable storage
 * - Base images from Arch Linux packages
 * - Export to .wubu container format
 * - OCI image ref conversion
 */

#include "wubu_image.h"
#include "wubu_container.h"
#include "wubu_oci.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

/* -- Internal Helpers --------------------------------------------- */

static uint32_t wubu_crc32_internal(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/* SHA256 Implementation (public domain / FIPS 180-4) */
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

typedef struct {
    uint32_t h[8];
    uint64_t total_len;
    uint8_t buffer[64];
    size_t buffer_len;
} SHA256_CTX;

static void sha256_init(SHA256_CTX *ctx) {
    ctx->h[0] = 0x6a09e667;
    ctx->h[1] = 0xbb67ae85;
    ctx->h[2] = 0x3c6ef372;
    ctx->h[3] = 0xa54ff53a;
    ctx->h[4] = 0x510e527f;
    ctx->h[5] = 0x9b05688c;
    ctx->h[6] = 0x1f83d9ab;
    ctx->h[7] = 0x5be0cd19;
    ctx->total_len = 0;
    ctx->buffer_len = 0;
}

static void sha256_transform(SHA256_CTX *ctx, const uint8_t *data) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (data[i*4] << 24) | (data[i*4+1] << 16) | (data[i*4+2] << 8) | data[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25)) ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);
        uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15)) ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    
    uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];
    uint32_t e = ctx->h[4], f = ctx->h[5], g = ctx->h[6], h = ctx->h[7];
    
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + sha256_k[i] + w[i];
        uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    
    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

static void sha256_update(SHA256_CTX *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    ctx->total_len += len;
    
    while (len > 0) {
        size_t space = 64 - ctx->buffer_len;
        size_t take = len < space ? len : space;
        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += take;
        p += take;
        len -= take;
        
        if (ctx->buffer_len == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t *hash) {
    ctx->buffer[ctx->buffer_len++] = 0x80;
    if (ctx->buffer_len > 56) {
        while (ctx->buffer_len < 64) ctx->buffer[ctx->buffer_len++] = 0;
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    while (ctx->buffer_len < 56) ctx->buffer[ctx->buffer_len++] = 0;
    
    uint64_t bits = ctx->total_len * 8;
    for (int i = 0; i < 8; i++) {
        ctx->buffer[56 + i] = (bits >> (56 - i * 8)) & 0xFF;
    }
    sha256_transform(ctx, ctx->buffer);
    
    for (int i = 0; i < 8; i++) {
        hash[i] = (ctx->h[i] >> 24) & 0xFF;
        hash[i+8] = (ctx->h[i] >> 16) & 0xFF;
        hash[i+16] = (ctx->h[i] >> 8) & 0xFF;
        hash[i+24] = ctx->h[i] & 0xFF;
    }
}

static void sha256_digest(const void *data, size_t size, char *out_hex, size_t out_size) {
    if (!out_hex || out_size < 65) { if (out_hex && out_size > 0) out_hex[0] = '\0'; return; }
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++) {
        snprintf(out_hex + i * 2, out_size - i * 2, "%02x", hash[i]);
    }
    out_hex[64] = '\0';
}

static void sha256_file(const char *path, char *out_hex, size_t out_size) {
    if (!out_hex || out_size < 65) { if (out_hex && out_size > 0) out_hex[0] = '\0'; return; }
    FILE *f = fopen(path, "rb");
    if (!f) { memset(out_hex, 0, out_size < 65 ? out_size : 65); return; }

    SHA256_CTX ctx;
    sha256_init(&ctx);
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha256_update(&ctx, buf, n);
    }
    fclose(f);
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++) {
        snprintf(out_hex + i * 2, out_size - i * 2, "%02x", hash[i]);
    }
    out_hex[64] = '\0';
}

static void str_trim(char *s) {
    char *end;
    while (isspace(*s)) s++;
    if (!*s) return;
    end = s + strlen(s) - 1;
    while (end > s && isspace(*end)) end--;
    *(end + 1) = '\0';
}

static bool str_startswith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static char *str_dup(const char *s) {
    if (!s) return NULL;
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

/* -- WuBuFile Parser ---------------------------------------------- */

static int parse_instruction(const char *line, WubuInstruction *inst, int line_num) {
    char *trimmed = str_dup(line);
    str_trim(trimmed);
    
    if (!*trimmed || *trimmed == '#') {
        free(trimmed);
        return 0;  /* Skip empty/comment */
    }
    
    inst->line_num = line_num;
    inst->has_json_form = false;
    strcpy(inst->original, line);
    
    /* Parse instruction type */
    char *token = strtok(trimmed, " \t");
    if (!token) {
        free(trimmed);
        return -1;
    }
    
    /* Convert to uppercase for comparison */
    char upper[32];
    for (int i = 0; token[i] && i < 31; i++) upper[i] = toupper(token[i]);
    upper[31] = '\0';
    
    if (strcmp(upper, "FROM") == 0) inst->type = WUBU_INST_FROM;
    else if (strcmp(upper, "RUN") == 0) inst->type = WUBU_INST_RUN;
    else if (strcmp(upper, "COPY") == 0) inst->type = WUBU_INST_COPY;
    else if (strcmp(upper, "ADD") == 0) inst->type = WUBU_INST_ADD;
    else if (strcmp(upper, "CMD") == 0) inst->type = WUBU_INST_CMD;
    else if (strcmp(upper, "ENTRYPOINT") == 0) inst->type = WUBU_INST_ENTRYPOINT;
    else if (strcmp(upper, "ENV") == 0) inst->type = WUBU_INST_ENV;
    else if (strcmp(upper, "ARG") == 0) inst->type = WUBU_INST_ARG;
    else if (strcmp(upper, "WORKDIR") == 0) inst->type = WUBU_INST_WORKDIR;
    else if (strcmp(upper, "USER") == 0) inst->type = WUBU_INST_USER;
    else if (strcmp(upper, "EXPOSE") == 0) inst->type = WUBU_INST_EXPOSE;
    else if (strcmp(upper, "VOLUME") == 0) inst->type = WUBU_INST_VOLUME;
    else if (strcmp(upper, "LABEL") == 0) inst->type = WUBU_INST_LABEL;
    else if (strcmp(upper, "ONBUILD") == 0) inst->type = WUBU_INST_ONBUILD;
    else if (strcmp(upper, "HEALTHCHECK") == 0) inst->type = WUBU_INST_HEALTHCHECK;
    else if (strcmp(upper, "SHELL") == 0) inst->type = WUBU_INST_SHELL;
    else if (strcmp(upper, "MOUNT") == 0) inst->type = WUBU_INST_MOUNT;
    else if (strcmp(upper, "DEVICE") == 0) inst->type = WUBU_INST_DEVICE;
    else if (strcmp(upper, "SECURITY") == 0) inst->type = WUBU_INST_SECURITY;
    else if (strcmp(upper, "STOPSIGNAL") == 0) inst->type = WUBU_INST_STOP_SIGNAL;
    else if (strcmp(upper, "ARCH") == 0) inst->type = WUBU_INST_ARCH;
    else if (strcmp(upper, "OS") == 0) inst->type = WUBU_INST_OS;
    else {
        fprintf(stderr, "Line %d: Unknown instruction: %s\n", line_num, token);
        free(trimmed);
        return -1;
    }
    
    /* Get rest of line as args */
    char *rest = strtok(NULL, "");
    if (rest) {
        str_trim(rest);
        strncpy(inst->args, rest, WUBU_MAX_CMD_LEN - 1);
        inst->args[WUBU_MAX_CMD_LEN - 1] = '\0';
        
        /* Check for JSON form: ["cmd", "arg"] */
        if (rest[0] == '[' && rest[strlen(rest) - 1] == ']') {
            inst->has_json_form = true;
        }
    } else {
        inst->args[0] = '\0';
    }
    
    free(trimmed);
    return 1;
}

int wubu_parse_wubufile(const char *path, WubuBuildContext *ctx) {
    if (!path || !ctx) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        return -1;
    }
    
    strncpy(ctx->wubufile_path, path, WUBU_MAX_PATH - 1);
    
    char line[4096];
    int line_num = 0;
    int current_stage = 0;
    WubuStage *stage = &ctx->stages[0];
    memset(stage, 0, sizeof(WubuStage));
    stage->arch = WUBU_ARCH_X86_64;
    stage->os = WUBU_OS_LINUX;
    strcpy(stage->shell, "/bin/bash");
    
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        char *trimmed = str_dup(line);
        str_trim(trimmed);
        
        if (!*trimmed || *trimmed == '#') {
            free(trimmed);
            continue;
        }
        
        /* Handle line continuations */
        size_t len = strlen(trimmed);
        while (len > 0 && trimmed[len - 1] == '\\') {
            trimmed[len - 1] = '\0';
            char next_line[4096];
            if (!fgets(next_line, sizeof(next_line), f)) break;
            line_num++;
            char *next_trimmed = str_dup(next_line);
            str_trim(next_trimmed);
            char combined[8192];
            snprintf(combined, sizeof(combined), "%s%s", trimmed, next_trimmed);
            free(trimmed);
            free(next_trimmed);
            trimmed = str_dup(combined);
            len = strlen(trimmed);
        }
        
        WubuInstruction inst;
        int result = parse_instruction(trimmed, &inst, line_num);
        free(trimmed);
        
        if (result <= 0) {
            if (result < 0) { fclose(f); return -1; }
            continue;
        }
        
        /* Handle FROM - new stage */
        if (inst.type == WUBU_INST_FROM) {
            if (stage->inst_count > 0 || current_stage > 0) {
                current_stage++;
                if (current_stage >= WUBU_MAX_STAGES) {
                    fprintf(stderr, "Too many stages (max %d)\n", WUBU_MAX_STAGES);
                    fclose(f);
                    return -1;
                }
                stage = &ctx->stages[current_stage];
                memset(stage, 0, sizeof(WubuStage));
                stage->arch = WUBU_ARCH_X86_64;
                stage->os = WUBU_OS_LINUX;
                strcpy(stage->shell, "/bin/bash");
            }
            
            /* Parse FROM args: <image>[:tag] [AS <name>] */
            char *img = strtok(inst.args, " \t");
            if (img) {
                char *tag = strchr(img, ':');
                if (tag) {
                    *tag = '\0';
                    tag++;
                    strncpy(stage->base_tag, tag, 31);
                }
                strncpy(stage->base_image, img, 127);
                
                char *as = strtok(NULL, " \t");
                if (as && strcmp(as, "AS") == 0) {
                    as = strtok(NULL, " \t");
                    if (as) strncpy(stage->name, as, 63);
                }
            }
            continue;
        }
        
        /* Add instruction to current stage */
        if (stage->inst_count >= WUBU_MAX_INSTRUCTIONS) {
            fprintf(stderr, "Too many instructions in stage (max %d)\n", WUBU_MAX_INSTRUCTIONS);
            fclose(f);
            return -1;
        }
        inst.stage = current_stage;
        stage->insts[stage->inst_count++] = inst;
        
        /* Handle other instructions that affect stage config */
        switch (inst.type) {
            case WUBU_INST_ENV: {
                /* ENV KEY=VAL KEY2=VAL2 ... */
                char *kv = strtok(inst.args, " \t");
                while (kv && stage->env_count < WUBU_MAX_ENVS) {
                    strncpy(stage->envs[stage->env_count], kv, 127);
                    stage->env_count++;
                    kv = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_ARG: {
                char *arg = strtok(inst.args, " \t");
                while (arg && stage->arg_count < WUBU_MAX_ARGS) {
                    strncpy(stage->args[stage->arg_count], arg, 127);
                    stage->arg_count++;
                    arg = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_EXPOSE: {
                char *port = strtok(inst.args, " \t");
                while (port && stage->port_count < WUBU_MAX_PORTS) {
                    stage->ports[stage->port_count++] = atoi(port);
                    port = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_VOLUME: {
                char *vol = strtok(inst.args, " \t");
                while (vol && stage->volume_count < WUBU_MAX_VOLUMES) {
                    strncpy(stage->volumes[stage->volume_count], vol, 255);
                    stage->volume_count++;
                    vol = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_LABEL: {
                char *kv = strtok(inst.args, " \t");
                while (kv && stage->label_count < WUBU_MAX_LABELS) {
                    strncpy(stage->labels[stage->label_count], kv, 127);
                    stage->label_count++;
                    kv = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_WORKDIR: {
                str_trim(inst.args);
                strncpy(stage->workdir, inst.args, 255);
                break;
            }
            case WUBU_INST_USER: {
                str_trim(inst.args);
                char *colon = strchr(inst.args, ':');
                if (colon) {
                    *colon = '\0';
                    strncpy(stage->user, inst.args, 63);
                    strncpy(stage->group, colon + 1, 63);
                } else {
                    strncpy(stage->user, inst.args, 63);
                }
                break;
            }
            case WUBU_INST_ENTRYPOINT: {
                strncpy(stage->entrypoint, inst.args, WUBU_MAX_CMD_LEN - 1);
                break;
            }
            case WUBU_INST_CMD: {
                strncpy(stage->cmd, inst.args, WUBU_MAX_CMD_LEN - 1);
                break;
            }
            case WUBU_INST_SHELL: {
                str_trim(inst.args);
                strncpy(stage->shell, inst.args, 127);
                break;
            }
            case WUBU_INST_ARCH: {
                stage->arch = wubu_arch_from_string(inst.args);
                break;
            }
            case WUBU_INST_OS: {
                stage->os = wubu_os_from_string(inst.args);
                break;
            }
            default:
                break;
        }
    }
    
    fclose(f);
    ctx->stage_count = current_stage + 1;
    
    /* Set context path from WuBuFile location */
    char *slash = strrchr(ctx->wubufile_path, '/');
    if (slash) {
        *slash = '\0';
        strncpy(ctx->context_path, ctx->wubufile_path, WUBU_MAX_PATH - 1);
        *slash = '/';
    } else {
        strcpy(ctx->context_path, ".");
    }
    
    return 0;
}

int wubu_parse_wubufile_str(const char *content, WubuBuildContext *ctx) {
    if (!content || !ctx) return -1;
    /* Write to temp file and parse */
    char tmp_path[WUBU_MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubufile_%d", getpid());
    FILE *f = fopen(tmp_path, "w");
    if (!f) return -1;
    fputs(content, f);
    fclose(f);
    int ret = wubu_parse_wubufile(tmp_path, ctx);
    unlink(tmp_path);
    return ret;
}

/* -- Layer Cache -------------------------------------------------- */

#define CACHE_DIR "/var/cache/wubu/layers"

int wubu_layer_cache_get(const char *digest, void *out_data, size_t *out_size) {
    if (!digest || !out_data || !out_size) return -1;
    
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, digest);
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    fstat(fd, &st);
    *out_size = st.st_size;
    
    ssize_t n = read(fd, out_data, *out_size);
    close(fd);
    
    return n == (ssize_t)*out_size ? 0 : -1;
}

int wubu_layer_cache_put(const char *digest, const void *data, size_t size) {
    if (!digest || !data || size == 0) return -1;
    
    mkdir(CACHE_DIR, 0755);
    
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, digest);
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    ssize_t n = write(fd, data, size);
    close(fd);
    
    return n == (ssize_t)size ? 0 : -1;
}

bool wubu_layer_cache_exists(const char *digest) {
    if (!digest) return false;
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, digest);
    return access(path, F_OK) == 0;
}

/* -- Image Build -------------------------------------------------- */

static int create_layer_tar(const char *src_dir, const char *dest_tar, const char *instruction) {
    (void)instruction;
    char cmd[WUBU_MAX_PATH * 2 + 64];
    snprintf(cmd, sizeof(cmd), "cd '%s' && tar -cf '%s' . 2>/dev/null", src_dir, dest_tar);
    return system(cmd) == 0 ? 0 : -1;
}

int wubu_image_build(WubuBuildContext *ctx, WubuImageManifest *out_manifest) {
    if (!ctx || !out_manifest || ctx->stage_count == 0) return -1;
    
    memset(out_manifest, 0, sizeof(WubuImageManifest));
    out_manifest->arch = ctx->stages[0].arch;
    out_manifest->os = ctx->stages[0].os;
    out_manifest->created = time(NULL);
    out_manifest->stop_signal = 15;  /* SIGTERM */
    
    /* Generate image name/tag */
    if (ctx->output_tag[0]) {
        strncpy(out_manifest->name, ctx->output_tag, 127);
    } else {
        strcpy(out_manifest->name, "wubu-image");
    }
    strcpy(out_manifest->tag, "latest");
    
    /* Build each stage */
    char prev_layer_digest[WUBU_LAYER_DIGEST_LEN] = "";
    char build_root[WUBU_MAX_PATH];
    snprintf(build_root, sizeof(build_root), "/tmp/wubu_build_%d", getpid());
    mkdir(build_root, 0755);
    
    for (int si = 0; si < ctx->stage_count; si++) {
        WubuStage *stage = &ctx->stages[si];
        
        /* Create stage directory */
        char stage_dir[WUBU_MAX_PATH];
        snprintf(stage_dir, sizeof(stage_dir), "%s/stage%d", build_root, si);
        mkdir(stage_dir, 0755);
        
        /* Extract base image if FROM specified */
        if (stage->base_image[0]) {
            /* In production: extract base image layers */
            if (ctx->progress_cb) {
                ctx->progress_cb(stage->name[0] ? stage->name : "base", 
                                 "Extracting base image", si + 1, ctx->stage_count, ctx->progress_user_data);
            }
        }
        
        /* Execute instructions */
        for (int ii = 0; ii < stage->inst_count; ii++) {
            WubuInstruction *inst = &stage->insts[ii];
            
            if (ctx->progress_cb) {
                ctx->progress_cb(stage->name[0] ? stage->name : "base",
                                 inst->original, ii + 1, stage->inst_count, ctx->progress_user_data);
            }
            
            switch (inst->type) {
                case WUBU_INST_RUN: {
                    /* Execute command in stage directory */
                    char cmd[WUBU_MAX_PATH + WUBU_MAX_CMD_LEN + 16];
                    snprintf(cmd, sizeof(cmd), "cd '%s' && %s", stage_dir, inst->args);
                    if (system(cmd) != 0) {
                        fprintf(stderr, "RUN failed: %s\n", inst->args);
                    }
                    break;
                }
                case WUBU_INST_COPY: {
                    /* COPY src... dest */
                    /* Simplified: just parse and copy */
                    break;
                }
                case WUBU_INST_WORKDIR: {
                    if (stage->workdir[0]) {
                        mkdir(stage->workdir, 0755);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        
        /* Create layer from stage directory */
        char layer_tar[WUBU_MAX_PATH];
        snprintf(layer_tar, sizeof(layer_tar), "%s/layer%d.tar", build_root, si);
        
        if (create_layer_tar(stage_dir, layer_tar, "layer") == 0) {
            /* Compute layer digest */
            sha256_file(layer_tar, out_manifest->layers[out_manifest->layer_count].digest, WUBU_LAYER_DIGEST_LEN + 1);
            
            WubuLayer *layer = &out_manifest->layers[out_manifest->layer_count];
            strncpy(layer->parent_digest, prev_layer_digest, WUBU_LAYER_DIGEST_LEN - 1);
            struct stat st;
            stat(layer_tar, &st);
            layer->size = st.st_size;
            layer->created = time(NULL);
            strncpy(layer->instruction, "layer", WUBU_MAX_CMD_LEN - 1);
            
            strncpy(prev_layer_digest, layer->digest, WUBU_LAYER_DIGEST_LEN - 1);
            out_manifest->layer_count++;
            
            /* Cache layer */
            void *data = malloc(layer->size);
            if (data) {
                FILE *f = fopen(layer_tar, "rb");
                if (f) {
                    fread(data, 1, layer->size, f);
                    fclose(f);
                    wubu_layer_cache_put(layer->digest, data, layer->size);
                }
                free(data);
            }
        }
    }
    
    /* Compute image config */
    /* Set entrypoint/cmd from last stage */
    WubuStage *last = &ctx->stages[ctx->stage_count - 1];
    if (last->entrypoint[0]) strncpy(out_manifest->entrypoint, last->entrypoint, WUBU_MAX_CMD_LEN - 1);
    if (last->cmd[0]) strncpy(out_manifest->cmd, last->cmd, WUBU_MAX_CMD_LEN - 1);
    if (last->workdir[0]) strncpy(out_manifest->workdir, last->workdir, 255);
    if (last->user[0]) strncpy(out_manifest->user, last->user, 63);
    
    /* Copy envs, ports, volumes, labels */
    for (int i = 0; i < last->env_count && i < WUBU_MAX_ENVS; i++) {
        strncpy(out_manifest->envs[i], last->envs[i], 127);
    }
    out_manifest->env_count = last->env_count;
    
    for (int i = 0; i < last->port_count && i < WUBU_MAX_PORTS; i++) {
        out_manifest->ports[i] = last->ports[i];
    }
    out_manifest->port_count = last->port_count;
    
    for (int i = 0; i < last->volume_count && i < WUBU_MAX_VOLUMES; i++) {
        strncpy(out_manifest->volumes[i], last->volumes[i], 255);
    }
    out_manifest->volume_count = last->volume_count;
    
    for (int i = 0; i < last->label_count && i < WUBU_MAX_LABELS; i++) {
        strncpy(out_manifest->labels[i], last->labels[i], 127);
    }
    out_manifest->label_count = last->label_count;
    
    /* Compute image ID from layer digests */
    char combined[WUBU_MAX_LAYERS * 65];
    combined[0] = '\0';
    size_t combined_len = 0;
    for (int i = 0; i < out_manifest->layer_count; i++) {
        size_t dlen = strlen(out_manifest->layers[i].digest);
        if (combined_len + dlen >= sizeof(combined)) break;
        memcpy(combined + combined_len, out_manifest->layers[i].digest, dlen);
        combined_len += dlen;
        combined[combined_len] = '\0';
    }
    sha256_digest(combined, strlen(combined), out_manifest->image_id, WUBU_IMAGE_ID_LEN + 1);
    
    /* Cleanup */
    char rm_cmd[512];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", build_root);
    system(rm_cmd);
    
    return 0;
}

/* -- Export to .wubu ---------------------------------------------- */

int wubu_image_export_wubu(const WubuImageManifest *manifest, const char *output_path) {
    if (!manifest || !output_path) return -1;
    
    /* Serialize manifest to JSON-like format first */
    char manifest_json[65536];
    char *p = manifest_json;
    size_t remaining = sizeof(manifest_json);
    int n;
#define WUBU_JSON_APPEND(fmt, ...) do { \
    n = snprintf(p, remaining, fmt, ##__VA_ARGS__); \
    if (n < 0 || (size_t)n >= remaining) goto trunc; \
    p += n; remaining -= n; \
} while(0)
    WUBU_JSON_APPEND("{\"image_id\":\"%s\",", manifest->image_id);
    WUBU_JSON_APPEND("\"name\":\"%s\",", manifest->name);
    WUBU_JSON_APPEND("\"tag\":\"%s\",", manifest->tag);
    WUBU_JSON_APPEND("\"arch\":%d,", manifest->arch);
    WUBU_JSON_APPEND("\"os\":%d,", manifest->os);
    WUBU_JSON_APPEND("\"created\":%ld,", manifest->created);
    WUBU_JSON_APPEND("\"layer_count\":%d,", manifest->layer_count);
    WUBU_JSON_APPEND("\"entrypoint\":\"%s\",", manifest->entrypoint);
    WUBU_JSON_APPEND("\"cmd\":\"%s\",", manifest->cmd);
    WUBU_JSON_APPEND("\"workdir\":\"%s\",", manifest->workdir);
    WUBU_JSON_APPEND("\"user\":\"%s\"", manifest->user);
    WUBU_JSON_APPEND("}");
#undef WUBU_JSON_APPEND
    goto done;
trunc:
    strcpy(manifest_json, "{}");
done:
    
    /* Create .wubu container */
    WUBU_HEADER header = {0};
    memcpy(header.magic, WUBU_MAGIC, WUBU_MAGIC_SIZE);
    header.version_major = WUBU_VERSION_MAJOR;
    header.version_minor = WUBU_VERSION_MINOR;
    header.payload_type = WUBU_PAYLOAD_NATIVE_EXEC;
    header.arch = manifest->arch;
    header.flags = WUBU_FLAG_SANDBOXED;
    header.handler_id = 1;  /* WuBuOS native */
    header.os_persona = manifest->os;
    header.payload_size = strlen(manifest_json);
    header.meta_offset = 0;
    header.meta_size = 0;
    header.header_crc = 0;
    header.header_crc = wubu_crc32_internal(&header, WUBU_HEADER_SIZE);
    
    /* Write .wubu file */
    int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    write(fd, &header, WUBU_HEADER_SIZE);
    write(fd, manifest_json, strlen(manifest_json));
    close(fd);
    
    return 0;
}

/* -- Export to OCI ------------------------------------------------ */

int wubu_image_export_oci(const WubuImageManifest *manifest, const char *output_tar) {
    if (!manifest || !output_tar) return -1;
    
    /* Create OCI image layout */
    char tmp_dir[WUBU_MAX_PATH];
    snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/wubu_oci_%d", getpid());
    mkdir(tmp_dir, 0755);
    
    /* blobs/sha256/ directory */
    char blobs_dir[WUBU_MAX_PATH];
    snprintf(blobs_dir, sizeof(blobs_dir), "%s/blobs/sha256", tmp_dir);
    mkdir(blobs_dir, 0755);
    
    /* Write config blob */
    char config_json[16384];
    char *p = config_json;
    size_t remaining = sizeof(config_json);
    int n;
#define WUBU_CFG_APPEND(fmt, ...) do { \
    n = snprintf(p, remaining, fmt, ##__VA_ARGS__); \
    if (n < 0 || (size_t)n >= remaining) goto cfg_trunc; \
    p += n; remaining -= n; \
} while(0)
    WUBU_CFG_APPEND("{\"created\":\"%ld\",\"architecture\":\"%s\",\"os\":\"%s\",\"config\":{",
                    manifest->created, wubu_arch_name(manifest->arch), wubu_os_name(manifest->os));
    WUBU_CFG_APPEND("\"Entrypoint\":[\"%s\"],", manifest->entrypoint);
    WUBU_CFG_APPEND("\"Cmd\":[\"%s\"],", manifest->cmd);
    WUBU_CFG_APPEND("\"WorkingDir\":\"%s\",", manifest->workdir);
    WUBU_CFG_APPEND("\"User\":\"%s\"},", manifest->user);
    WUBU_CFG_APPEND("\"rootfs\":{\"type\":\"layers\",\"diff_ids\":[");
    for (int i = 0; i < manifest->layer_count; i++) {
        if (i) WUBU_CFG_APPEND(",");
        WUBU_CFG_APPEND("\"sha256:%s\"", manifest->layers[i].digest);
    }
    WUBU_CFG_APPEND("]}");
#undef WUBU_CFG_APPEND
    *p = '}'; p++; *p = '\0';
    goto cfg_done;
cfg_trunc:
    strcpy(config_json, "{}");
cfg_done:
    
    /* Compute config digest and write */
    char config_digest[65];
    sha256_digest(config_json, strlen(config_json), config_digest, sizeof(config_digest));
    
    char config_path[WUBU_MAX_PATH];
    snprintf(config_path, sizeof(config_path), "%s/%s", blobs_dir, config_digest);
    int fd = open(config_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, config_json, strlen(config_json));
        close(fd);
    }
    
    /* Write manifest */
    char manifest_json[32768];
    p = manifest_json;
    remaining = sizeof(manifest_json);
#define WUBU_MFEST_APPEND(fmt, ...) do { \
    n = snprintf(p, remaining, fmt, ##__VA_ARGS__); \
    if (n < 0 || (size_t)n >= remaining) goto mfest_trunc; \
    p += n; remaining -= n; \
} while(0)
    WUBU_MFEST_APPEND("{\"schemaVersion\":2,\"mediaType\":\"%s\",\"config\":{\"mediaType\":\"%s\",\"size\":%zu,\"digest\":\"sha256:%s\"},\"layers\":[",
                    OCI_MEDIA_TYPE_IMAGE_MANIFEST_V2, OCI_MEDIA_TYPE_IMAGE_CONFIG_V1, strlen(config_json), config_digest);
    for (int i = 0; i < manifest->layer_count; i++) {
        if (i) WUBU_MFEST_APPEND(",");
        WUBU_MFEST_APPEND("{\"mediaType\":\"%s\",\"size\":%lu,\"digest\":\"sha256:%s\"}",
                         OCI_MEDIA_TYPE_LAYER_V1_GZIP, manifest->layers[i].size, manifest->layers[i].digest);
    }
    WUBU_MFEST_APPEND("]}");
#undef WUBU_MFEST_APPEND
    *p = '\0';
    goto mfest_done;
mfest_trunc:
    strcpy(manifest_json, "{}");
mfest_done:
    
    /* Compute manifest digest */
    char manifest_digest[65];
    sha256_digest(manifest_json, strlen(manifest_json), manifest_digest, sizeof(manifest_digest));
    
    /* Write manifest to oci-layout */
    char oci_layout[256];
    snprintf(oci_layout, sizeof(oci_layout), "{\"imageLayoutVersion\":\"1.0.0\"}");
    char layout_path[WUBU_MAX_PATH];
    snprintf(layout_path, sizeof(layout_path), "%s/oci-layout", tmp_dir);
    fd = open(layout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, oci_layout, strlen(oci_layout)); close(fd); }
    
    /* index.json */
    char index_json[16384];
    p = index_json;
    remaining = sizeof(index_json);
    n = snprintf(p, remaining, "{\"schemaVersion\":2,\"mediaType\":\"%s\",\"manifests\":[{\"mediaType\":\"%s\",\"size\":%zu,\"digest\":\"sha256:%s\",\"platform\":{\"architecture\":\"%s\",\"os\":\"%s\"}}]}",
                 OCI_MEDIA_TYPE_IMAGE_INDEX_V1, OCI_MEDIA_TYPE_IMAGE_MANIFEST_V2,
                 strlen(manifest_json), manifest_digest,
                 wubu_arch_name(manifest->arch), wubu_os_name(manifest->os));
    if (n < 0 || (size_t)n >= remaining) { strcpy(index_json, "{}"); }
    
    char index_path[WUBU_MAX_PATH];
    snprintf(index_path, sizeof(index_path), "%s/index.json", tmp_dir);
    fd = open(index_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, index_json, strlen(index_json)); close(fd); }
    
    /* Create tar */
    char tar_cmd[WUBU_MAX_PATH * 2 + 64];
    snprintf(tar_cmd, sizeof(tar_cmd), "cd '%s' && tar -cf '%s' . 2>/dev/null", tmp_dir, output_tar);
    int ret = system(tar_cmd);
    
    /* Cleanup */
    char rm_cmd[512];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", tmp_dir);
    system(rm_cmd);
    return ret == 0 ? 0 : -1;
}

/* -- Import from .wubu -------------------------------------------- */

int wubu_image_import_wubu(const char *wubu_path, WubuImageManifest *out_manifest) {
    if (!wubu_path || !out_manifest) return -1;
    FILE *f = fopen(wubu_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = malloc(sz > 0 ? sz : 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, sz, f);
    fclose(f);
    WUBU_HEADER header;
    const void *payload;
    size_t payload_size;
    int ret = wubu_container_parse(buf, sz, &header, &payload, &payload_size);
    if (ret != 0 || !payload || payload_size == 0) { free(buf); return -1; }
    char *json = malloc(payload_size + 1);
    if (!json) { free(buf); return -1; }
    memcpy(json, payload, payload_size);
    json[payload_size] = 0;
    int rc = wubu_manifest_from_json(json, out_manifest);
    free(json);
    free(buf);
    return rc;
}

/* -- Import from OCI ---------------------------------------------- */

int wubu_image_import_oci(const char *oci_dir, WubuImageManifest *out_manifest) {
    if (!oci_dir || !out_manifest) return -1;
    char index_path[WUBU_MAX_PATH];
    snprintf(index_path, sizeof(index_path), "%s/index.json", oci_dir);
    FILE *f = fopen(index_path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    const char *digest_start = strstr(buf, "\"digest\"");
    char manifest_blob[128] = {0};
    if (digest_start) {
        const char *q = strchr(digest_start, '"');
        if (q) {
            const char *e = strchr(q + 1, '"');
            if (e) {
                size_t len = (size_t)(e - q - 1);
                if (len >= sizeof(manifest_blob)) len = sizeof(manifest_blob) - 1;
                memcpy(manifest_blob, q + 1, len);
            }
        }
    }
    free(buf);
    if (!manifest_blob[0]) return -1;
    char blob_path[WUBU_MAX_PATH];
    if (strncmp(manifest_blob, "sha256:", 7) == 0)
        snprintf(blob_path, sizeof(blob_path), "%s/blobs/sha256/%s", oci_dir, manifest_blob + 7);
    else
        snprintf(blob_path, sizeof(blob_path), "%s/blobs/sha256/%s", oci_dir, manifest_blob);
    f = fopen(blob_path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    OciImageManifest oci_manifest;
    int rc = oci_manifest_from_json(buf, &oci_manifest);
    free(buf);
    if (rc < 0) return rc;
    return oci_manifest_to_wubu(&oci_manifest, oci_dir, out_manifest);
}

/* -- Base Image Loaders ------------------------------------------- */

int wubu_image_load_base_arch(const char *pkg_name, const char *version, WubuStage *stage) {
    if (!pkg_name || !stage) return -1;
    
    /* Use pacman to get package info */
    char cmd[WUBU_MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd), "pacman -Sddp '%s' 2>/dev/null | head -1", pkg_name);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    
    char line[1024];
    if (fgets(line, sizeof(line), f)) {
        /* Parse package URL and download */
    }
    pclose(f);
    return 0;
}

int wubu_image_load_steam_runtime(WubuArch arch, WubuStage *stage) {
    if (!stage) return -1;
    
    /* Load Steam Runtime 2.0 "Soldier" preset */
    strcpy(stage->base_image, "steam-runtime-2.0");
    strcpy(stage->base_tag, "soldier");
    stage->arch = arch;
    stage->os = WUBU_OS_LINUX;
    
    return 0;
}

int wubu_image_load_proton(const char *proton_version, WubuStage *stage) {
    if (!stage) return -1;
    
    /* Load Proton-GE or Proton Experimental */
    strncpy(stage->base_image, "proton", 127);
    if (proton_version) strncpy(stage->base_tag, proton_version, 31);
    else strcpy(stage->base_tag, "GE");
    stage->arch = WUBU_ARCH_X86_64;
    stage->os = WUBU_OS_WINDOWS;  /* Windows persona */
    
    return 0;
}

/* -- Manifest Operations ------------------------------------------ */

int wubu_manifest_compute_id(WubuImageManifest *manifest) {
    if (!manifest) return -1;
    
    char combined[WUBU_MAX_LAYERS * 65];
    combined[0] = '\0';
    size_t combined_len = 0;
    for (int i = 0; i < manifest->layer_count; i++) {
        size_t dlen = strlen(manifest->layers[i].digest);
        if (combined_len + dlen >= sizeof(combined)) break;
        memcpy(combined + combined_len, manifest->layers[i].digest, dlen);
        combined_len += dlen;
        combined[combined_len] = '\0';
    }
    sha256_digest(combined, strlen(combined), manifest->image_id, WUBU_IMAGE_ID_LEN + 1);
    return 0;
}

int wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size) {
    if (!manifest || !out_json || out_size < 1024) return -1;
    
    char *p = out_json;
    size_t remaining = out_size;
    int n = snprintf(p, remaining, "{");
    p += n; remaining -= n;
    
    n = snprintf(p, remaining, "\"image_id\":\"%s\",", manifest->image_id);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"name\":\"%s\",", manifest->name);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"tag\":\"%s\",", manifest->tag);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"arch\":%d,", manifest->arch);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"os\":%d,", manifest->os);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"created\":%ld,", manifest->created);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"layer_count\":%d,", manifest->layer_count);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"entrypoint\":\"%s\",", manifest->entrypoint);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"cmd\":\"%s\",", manifest->cmd);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"workdir\":\"%s\",", manifest->workdir);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "\"user\":\"%s\"", manifest->user);
    p += n; remaining -= n;
    n = snprintf(p, remaining, "}");
    p += n; remaining -= n;
    
    return 0;
}

int wubu_manifest_from_json(const char *json, WubuImageManifest *manifest) {
    if (!json || !manifest) return -1;
    memset(manifest, 0, sizeof(*manifest));

    const char *p = strstr(json, "\"image_id\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->image_id)) len = sizeof(manifest->image_id) - 1;
                    memcpy(manifest->image_id, start, len);
                    manifest->image_id[len] = '\0';
                }
            }
        }
    }

    p = strstr(json, "\"name\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->name)) len = sizeof(manifest->name) - 1;
                    memcpy(manifest->name, start, len);
                    manifest->name[len] = '\0';
                }
            }
        }
    }

    p = strstr(json, "\"tag\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->tag)) len = sizeof(manifest->tag) - 1;
                    memcpy(manifest->tag, start, len);
                    manifest->tag[len] = '\0';
                }
            }
        }
    }

    p = strstr(json, "\"arch\"");
    if (p) {
        while (*p && !isdigit((unsigned char)*p)) p++;
        if (*p) manifest->arch = (WubuArch)atoi(p);
    }

    p = strstr(json, "\"os\"");
    if (p) {
        while (*p && !isdigit((unsigned char)*p)) p++;
        if (*p) manifest->os = (WubuOS)atoi(p);
    }

    p = strstr(json, "\"created\"");
    if (p) {
        while (*p && !isdigit((unsigned char)*p) && *p != '-') p++;
        if (*p) manifest->created = (uint64_t)atol(p);
    }

    p = strstr(json, "\"layer_count\"");
    if (p) {
        while (*p && !isdigit((unsigned char)*p)) p++;
        if (*p) manifest->layer_count = atoi(p);
    }

    p = strstr(json, "\"entrypoint\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->entrypoint)) len = sizeof(manifest->entrypoint) - 1;
                    memcpy(manifest->entrypoint, start, len);
                    manifest->entrypoint[len] = '\0';
                }
            }
        }
    }

    p = strstr(json, "\"cmd\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->cmd)) len = sizeof(manifest->cmd) - 1;
                    memcpy(manifest->cmd, start, len);
                    manifest->cmd[len] = '\0';
                }
            }
        }
    }

    p = strstr(json, "\"workdir\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->workdir)) len = sizeof(manifest->workdir) - 1;
                    memcpy(manifest->workdir, start, len);
                    manifest->workdir[len] = '\0';
                }
            }
        }
    }

    p = strstr(json, "\"user\"");
    if (p) {
        const char *start = strchr(p, '"');
        if (start) {
            start = strchr(start + 1, '"');
            if (start) {
                start++;
                const char *end = strchr(start, '"');
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(manifest->user)) len = sizeof(manifest->user) - 1;
                    memcpy(manifest->user, start, len);
                    manifest->user[len] = '\0';
                }
            }
        }
    }

    return 0;
}

int wubu_manifest_save(const WubuImageManifest *manifest, const char *path) {
    if (!manifest || !path) return -1;
    
    char json[65536];
    if (wubu_manifest_to_json(manifest, json, sizeof(json)) < 0) return -1;
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, json, strlen(json));
    close(fd);
    return 0;
}

int wubu_manifest_load(const char *path, WubuImageManifest *manifest) {
    if (!path || !manifest) return -1;
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    fstat(fd, &st);
    char *buf = malloc(st.st_size + 1);
    if (!buf) { close(fd); return -1; }
    
    ssize_t n = read(fd, buf, st.st_size);
    close(fd);
    if (n != st.st_size) { free(buf); return -1; }
    buf[n] = '\0';
    
    int ret = wubu_manifest_from_json(buf, manifest);
    free(buf);
    return ret;
}

/* -- Tag Management ----------------------------------------------- */

#define TAG_DIR "/var/lib/wubu/tags"

int wubu_image_tag(const char *image_id, const char *tag) {
    if (!image_id || !tag) return -1;
    mkdir(TAG_DIR, 0755);
    
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", TAG_DIR, tag);
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, image_id, strlen(image_id));
    close(fd);
    return 0;
}

int wubu_image_untag(const char *tag) {
    if (!tag) return -1;
    
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", TAG_DIR, tag);
    return unlink(path);
}

int wubu_image_list(char images[][128], int max) {
    if (!images || max <= 0) return -1;
    
    DIR *d = opendir(TAG_DIR);
    if (!d) return 0;
    
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) && count < max) {
        if (ent->d_name[0] == '.') continue;
        strncpy(images[count], ent->d_name, 127);
        count++;
    }
    closedir(d);
    return count;
}

/* -- Image Removal / Prune ---------------------------------------- */

int wubu_image_remove(const char *image_id, bool force) {
    if (!image_id) return -1;
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", TAG_DIR, image_id);
    if (unlink(path) != 0) {
        if (force) {
            snprintf(path, sizeof(path), "/var/lib/wubu/images/%s.json", image_id);
            unlink(path);
            return 0;
        }
        return -1;
    }
    snprintf(path, sizeof(path), "/var/lib/wubu/images/%s.json", image_id);
    unlink(path);
    return 0;
}

int wubu_image_prune(void) {
    DIR *d = opendir("/var/lib/wubu/images");
    if (!d) return 0;
    struct dirent *ent;
    int pruned = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char tag_path[WUBU_MAX_PATH];
        snprintf(tag_path, sizeof(tag_path), "%s/%s", TAG_DIR, ent->d_name);
        if (access(tag_path, F_OK) != 0) {
            char img_path[WUBU_MAX_PATH];
            snprintf(img_path, sizeof(img_path), "/var/lib/wubu/images/%s", ent->d_name);
            unlink(img_path);
            pruned++;
        }
    }
    closedir(d);
    return pruned;
}

/* -- Inspect / History -------------------------------------------- */

int wubu_image_inspect(const char *image_ref, WubuImageManifest *out_manifest) {
    /* Try tag first */
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", TAG_DIR, image_ref);
    
    char image_id[65];
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        ssize_t rd = read(fd, image_id, sizeof(image_id) - 1);
        close(fd);
        if (rd <= 0) return -1;
        image_id[rd] = '\0';
        /* Strip trailing whitespace/newline */
        while (rd > 0 && (image_id[rd-1] == '\n' || image_id[rd-1] == '\r' || image_id[rd-1] == ' ')) {
            image_id[--rd] = '\0';
        }

        /* Look up manifest by image_id */
        snprintf(path, sizeof(path), "/var/lib/wubu/images/%s.json", image_id);
        return wubu_manifest_load(path, out_manifest);
    }
    
    return -1;
}

int wubu_image_history(const char *image_ref, WubuLayer layers[], int max_layers) {
    if (!image_ref || !layers || max_layers <= 0) return -1;
    
    WubuImageManifest manifest;
    if (wubu_image_inspect(image_ref, &manifest) < 0) return -1;
    
    int count = manifest.layer_count < max_layers ? manifest.layer_count : max_layers;
    for (int i = 0; i < count; i++) {
        layers[i] = manifest.layers[i];
    }
    return count;
}

/* -- Push/Pull (stubs for registry) ------------------------------- */

int wubu_image_push(const char *image_ref, const char *registry, const char *auth) {
    if (!image_ref || !registry) return -1;
    WubuImageManifest manifest;
    if (wubu_image_inspect(image_ref, &manifest) < 0) return -1;
    char manifest_json[65536];
    if (wubu_manifest_to_json(&manifest, manifest_json, sizeof(manifest_json)) < 0) return -1;
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "/tmp/wubu_push_manifest_%d.json", getpid());
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fwrite(manifest_json, 1, strlen(manifest_json), f);
    fclose(f);
    char url[WUBU_MAX_PATH];
    snprintf(url, sizeof(url), "https://%s/v2/wubu/images/manifests/%s", registry, image_ref);
    char cmd[WUBU_MAX_PATH * 2 + 128];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -X PUT -H 'Content-Type: application/vnd.oci.image.manifest.v2+json' --data-binary @%s '%s' >/dev/null 2>&1", tmp, url);
    int ret = system(cmd);
    unlink(tmp);
    for (int i = 0; i < manifest.layer_count && ret == 0; i++) {
        char layer_path[WUBU_MAX_PATH];
        snprintf(layer_path, sizeof(layer_path), "/var/cache/wubu/layers/%s", manifest.layers[i].digest);
        if (access(layer_path, F_OK) != 0) continue;
        char blob_url[WUBU_MAX_PATH];
        snprintf(blob_url, sizeof(blob_url), "https://%s/v2/wubu/blobs/%s", registry, manifest.layers[i].digest);
        snprintf(cmd, sizeof(cmd), "curl -sS -L -X PUT --data-binary @%s '%s' >/dev/null 2>&1", layer_path, blob_url);
        if (system(cmd) != 0) { ret = -1; break; }
    }
    return ret == 0 ? 0 : -1;
}

int wubu_image_pull(const char *image_ref, const char *registry, const char *auth, WubuImageManifest *out_manifest) {
    if (!image_ref || !registry || !out_manifest) return -1;
    char url[WUBU_MAX_PATH];
    snprintf(url, sizeof(url), "https://%s/v2/wubu/images/manifests/%s", registry, image_ref);
    char cmd[WUBU_MAX_PATH * 2 + 128];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -H 'Accept: application/vnd.oci.image.manifest.v2+json' '%s' > /tmp/wubu_pull_manifest.json", url);
    if (system(cmd) != 0) return -1;
    FILE *f = fopen("/tmp/wubu_pull_manifest.json", "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    unlink("/tmp/wubu_pull_manifest.json");
    OciImageManifest oci_manifest;
    int rc = oci_manifest_from_json(buf, &oci_manifest);
    if (rc < 0) { free(buf); return rc; }
    rc = oci_manifest_to_wubu(&oci_manifest, NULL, out_manifest);
    if (rc < 0) { free(buf); return rc; }
    for (int i = 0; i < out_manifest->layer_count; i++) {
        const char *digest = out_manifest->layers[i].digest;
        const char *blob_name = strncmp(digest, "sha256:", 7) == 0 ? digest + 7 : digest;
        snprintf(url, sizeof(url), "https://%s/v2/wubu/blobs/%s", registry, digest);
        char layer_path[WUBU_MAX_PATH];
        snprintf(layer_path, sizeof(layer_path), "/var/cache/wubu/layers/%s", blob_name);
        snprintf(cmd, sizeof(cmd), "curl -sS -L -o '%s' '%s' >/dev/null 2>&1", layer_path, url);
        if (system(cmd) != 0) {
            char blob_dir[WUBU_MAX_PATH];
            snprintf(blob_dir, sizeof(blob_dir), "/var/lib/wubu/oci/blobs/sha256");
            mkdir(blob_dir, 0755);
            snprintf(cmd, sizeof(cmd), "curl -sS -L -o '%s/%s' '%s' >/dev/null 2>&1", blob_dir, blob_name, url);
            system(cmd);
        }
    }
    free(buf);
    return 0;
}

/* -- Helpers ------------------------------------------------------ */

const char *wubu_arch_name(WubuArch arch) {
    static const char *names[] = {"x86_64", "aarch64", "riscv64", "wasm"};
    if (arch >= 0 && arch < 4) return names[arch];
    return "unknown";
}

const char *wubu_os_name(WubuOS os) {
    static const char *names[] = {"linux", "windows", "macos", "native"};
    if (os >= 0 && os < 4) return names[os];
    return "unknown";
}

WubuArch wubu_arch_from_string(const char *str) {
    if (!str) return WUBU_ARCH_X86_64;
    if (strcmp(str, "x86_64") == 0 || strcmp(str, "amd64") == 0) return WUBU_ARCH_X86_64;
    if (strcmp(str, "aarch64") == 0 || strcmp(str, "arm64") == 0) return WUBU_ARCH_AARCH64;
    if (strcmp(str, "riscv64") == 0) return WUBU_ARCH_RISCV64;
    if (strcmp(str, "wasm") == 0) return WUBU_ARCH_WASM;
    return WUBU_ARCH_X86_64;
}

WubuOS wubu_os_from_string(const char *str) {
    if (!str) return WUBU_OS_LINUX;
    if (strcmp(str, "linux") == 0) return WUBU_OS_LINUX;
    if (strcmp(str, "windows") == 0) return WUBU_OS_WINDOWS;
    if (strcmp(str, "macos") == 0) return WUBU_OS_MACOS;
    if (strcmp(str, "native") == 0) return WUBU_OS_NATIVE;
    return WUBU_OS_LINUX;
}

void wubu_build_context_free(WubuBuildContext *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->stage_count; i++) {
        WubuStage *s = &ctx->stages[i];
        /* Nothing to free for now */
    }
    ctx->stage_count = 0;
}
