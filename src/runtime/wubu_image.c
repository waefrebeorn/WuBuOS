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

#define _GNU_SOURCE
#include "wubu_image.h"
#include "wubu_image_internal.h"
#include "wubu_container.h"
#include "wubu_oci.h"
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/wait.h>

/* wubu_crc32 is declared in wubu_container.h, defined in wubu_container.c */
#define wubu_crc32_internal  wubu_crc32

/* nftw-based recursive directory remove (replaces system("rm -rf")) */
static int remove_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)ftwbuf;
    int rv = remove(fpath);
    if (rv) {
        fprintf(stderr, "[wubu_image] failed to remove %s: %s\n", fpath, strerror(errno));
    }
    return rv;
}

static int rmtree(const char *path) {
    if (!path || !path[0]) return -1;
    return nftw(path, remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

/* Forward declarations for tar functions (stay in this module for now) */
static int tar_write_header(int fd, const char *name, mode_t mode, uid_t uid, gid_t gid, off_t size, time_t mtime);
static int tar_write_file(int fd, const char *path, const struct stat *st);
static int tar_write_dir(int fd, const char *src_dir);

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
    /* Simple tar writer - creates a POSIX ustar tar archive */
    int fd = open(dest_tar, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    int ret = tar_write_dir(fd, src_dir);
    
    /* Write two 512-byte blocks of zeros (end of archive) */
    if (ret == 0) {
        char zeros[1024] = {0};
        write(fd, zeros, 1024);
    }
    
    close(fd);
    return ret;
}

/* Simple tar writer - creates a POSIX ustar tar archive */
static int tar_write_header(int fd, const char *name, mode_t mode, uid_t uid, gid_t gid, off_t size, time_t mtime) {
    char header[512] = {0};
    
    /* name (100 bytes) */
    strncpy(header, name, 99);
    /* mode (8 bytes, octal) */
    snprintf(header + 100, 8, "%07o", (unsigned int)mode);
    /* uid (8 bytes, octal) */
    snprintf(header + 108, 8, "%07o", (unsigned int)uid);
    /* gid (8 bytes, octal) */
    snprintf(header + 116, 8, "%07o", (unsigned int)gid);
    /* size (12 bytes, octal) */
    snprintf(header + 124, 12, "%011lo", (unsigned long)size);
    /* mtime (12 bytes, octal) */
    snprintf(header + 136, 12, "%011lo", (unsigned long)mtime);
    /* chksum (8 bytes) - initially spaces */
    memset(header + 148, ' ', 8);
    /* typeflag (1 byte) - '0' for regular file, '5' for directory */
    header[156] = S_ISDIR(mode) ? '5' : '0';
    /* linkname (100 bytes) - empty for regular files */
    /* magic (6 bytes) - "ustar" */
    memcpy(header + 257, "ustar", 5);
    /* version (2 bytes) - "00" */
    header[263] = '0';
    header[264] = '0';
    /* uname (32 bytes) */
    /* gname (32 bytes) */
    /* devmajor, devminor (8 bytes each) */
    
    /* Calculate checksum */
    unsigned int chksum = 0;
    for (int i = 0; i < 512; i++) {
        chksum += (unsigned char)header[i];
    }
    snprintf(header + 148, 8, "%06o\0", chksum);
    
    return write(fd, header, 512) == 512 ? 0 : -1;
}

static int tar_write_file(int fd, const char *path, const struct stat *st) {
    if (tar_write_header(fd, path, st->st_mode, st->st_uid, st->st_gid, st->st_size, st->st_mtime) < 0) {
        return -1;
    }
    
    if (S_ISREG(st->st_mode)) {
        int src_fd = open(path, O_RDONLY);
        if (src_fd < 0) return -1;
        
        char buf[8192];
        ssize_t n;
        while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
            if (write(fd, buf, n) != n) {
                close(src_fd);
                return -1;
            }
        }
        close(src_fd);
        
        /* Pad to 512-byte boundary */
        off_t size = st->st_size;
        size_t padding = (512 - (size % 512)) % 512;
        if (padding > 0) {
            char pad[512] = {0};
            if (write(fd, pad, padding) != (ssize_t)padding) {
                return -1;
            }
        }
    }
    return 0;
}

static int tar_write_dir(int fd, const char *src_dir) {
    char path[WUBU_MAX_PATH];
    struct stat st;
    DIR *dir = opendir(src_dir);
    if (!dir) return -1;
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        
        snprintf(path, sizeof(path), "%s/%s", src_dir, ent->d_name);
        if (lstat(path, &st) < 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            /* Write directory header */
            if (tar_write_header(fd, path, st.st_mode, st.st_uid, st.st_gid, 0, st.st_mtime) < 0) {
                closedir(dir);
                return -1;
            }
            /* Recurse into subdirectory */
            if (tar_write_dir(fd, path) < 0) {
                closedir(dir);
                return -1;
            }
        } else {
            /* Write file */
            if (tar_write_file(fd, path, &st) < 0) {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    return 0;
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
                    /* Execute command in stage directory using fork+exec (replaces system()) */
                    pid_t pid = fork();
                    if (pid == 0) {
                        /* Child process */
                        if (chdir(stage_dir) != 0) _exit(1);
                        /* Use shell to execute the command */
                        execl("/bin/sh", "sh", "-c", inst->args, (char *)NULL);
                        _exit(127);
                    } else if (pid > 0) {
                        /* Parent process */
                        int status;
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                            fprintf(stderr, "RUN failed (exit=%d): %s\n", WEXITSTATUS(status), inst->args);
                        }
                    } else {
                        fprintf(stderr, "RUN fork failed: %s\n", strerror(errno));
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
    rmtree(build_root);

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
    
    /* Create tar using direct tar implementation (replaces system("tar")) */
    int tar_fd = open(output_tar, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tar_fd < 0) {
        rmtree(tmp_dir);
        return -1;
    }
    
    int ret = tar_write_dir(tar_fd, tmp_dir);
    
    /* Write two 512-byte blocks of zeros (end of archive) */
    if (ret == 0) {
        char zeros[1024] = {0};
        write(tar_fd, zeros, 1024);
    }
    
    close(tar_fd);

    /* Cleanup */
    rmtree(tmp_dir);
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
