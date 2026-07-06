/*
 * oci_convert.c  --  Convert between .wubu and OCI formats
 * 
 * Extracted from wubu_oci.c (lines 1055-1307).
 */

#include "oci_internal.h"

/* -- OCI Image to Wubu ---------------------------------------------- */

int oci_image_to_wubu(const char *oci_dir, const char *wubu_output) {
    if (!oci_dir || !wubu_output) return -1;
    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/index.json", oci_dir);
    FILE *f = fopen(index_path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long index_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *index_json = malloc(index_size + 1);
    if (!index_json) { fclose(f); return -1; }
    fread(index_json, 1, index_size, f);
    index_json[index_size] = '\0';
    fclose(f);

    const char *manifest_ref = strstr(index_json, "\"digest\"");
    if (!manifest_ref) { free(index_json); return -1; }
    manifest_ref = strchr(manifest_ref, ':');
    if (!manifest_ref) { free(index_json); return -1; }
    const char *digest_start = strchr(manifest_ref, '"');
    if (!digest_start) { free(index_json); return -1; }
    digest_start++;
    const char *digest_end = strchr(digest_start, '"');
    if (!digest_end) { free(index_json); return -1; }
    char manifest_digest[128];
    size_t digest_len = (size_t)(digest_end - digest_start);
    if (digest_len >= sizeof(manifest_digest)) digest_len = sizeof(manifest_digest) - 1;
    memcpy(manifest_digest, digest_start, digest_len);
    manifest_digest[digest_len] = '\0';
    char blob_name[128] = {0};
    if (strncmp(manifest_digest, "sha256:", 7) == 0)
        memcpy(blob_name, manifest_digest + 7, digest_len - 7);
    else
        memcpy(blob_name, manifest_digest, digest_len);

    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/blobs/sha256/%s", oci_dir, blob_name);
    f = fopen(manifest_path, "rb");
    if (!f) { free(index_json); return -1; }
    fseek(f, 0, SEEK_END);
    long manifest_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *manifest_json = malloc(manifest_size + 1);
    if (!manifest_json) { fclose(f); free(index_json); return -1; }
    fread(manifest_json, 1, manifest_size, f);
    manifest_json[manifest_size] = '\0';
    fclose(f);

    OciImageManifest oci_manifest;
    if (oci_manifest_from_json(manifest_json, &oci_manifest) < 0) { free(manifest_json); free(index_json); return -1; }
    char config_blob[128] = {0};
    if (strncmp(oci_manifest.config.digest, "sha256:", 7) == 0)
        memcpy(config_blob, oci_manifest.config.digest + 7, strlen(oci_manifest.config.digest) - 7);
    else
        memcpy(config_blob, oci_manifest.config.digest, strlen(oci_manifest.config.digest));
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/blobs/sha256/%s", oci_dir, config_blob);
    char config_json[8192] = {0};
    f = fopen(config_path, "rb");
    if (f) {
        long n = fread(config_json, 1, sizeof(config_json) - 1, f);
        if (n > 0) config_json[n] = '\0';
        fclose(f);
    }

    WubuImageManifest wubu_manifest;
    if (oci_manifest_to_wubu(&oci_manifest, oci_dir, &wubu_manifest) < 0) { free(manifest_json); free(index_json); return -1; }
    int ret = wubu_image_export_wubu(&wubu_manifest, wubu_output);
    free(manifest_json);
    free(index_json);
    return ret;
}

/* -- OCI Manifest -> Wubu Manifest Conversion ----------------------- */

int oci_manifest_to_wubu(const OciImageManifest *oci_manifest, const char *oci_dir,
                         WubuImageManifest *wubu_manifest) {
    if (!oci_manifest || !wubu_manifest) return -1;

    char config_blob[128] = {0};
    if (strncmp(oci_manifest->config.digest, "sha256:", 7) == 0)
        memcpy(config_blob, oci_manifest->config.digest + 7, strlen(oci_manifest->config.digest) - 7);
    else
        memcpy(config_blob, oci_manifest->config.digest, strlen(oci_manifest->config.digest));

    char config_path[512];
    if (oci_dir) {
        snprintf(config_path, sizeof(config_path), "%s/blobs/sha256/%s", oci_dir, config_blob);
    } else {
        config_path[0] = '\0';
    }
    char config_json[8192] = {0};
    if (oci_dir && config_path[0]) {
        FILE *f = fopen(config_path, "rb");
        if (f) {
            long n = fread(config_json, 1, sizeof(config_json) - 1, f);
            if (n > 0) config_json[n] = '\0';
            fclose(f);
        }
    }

    memset(wubu_manifest, 0, sizeof(*wubu_manifest));
    OciImageConfig oci_config;
    oci_config_from_json(config_json, &oci_config);
    if (oci_config.entrypoint_count > 0) strncpy(wubu_manifest->entrypoint, oci_config.entrypoint[0], WUBU_MAX_CMD_LEN - 1);
    if (oci_config.cmd_count > 0) strncpy(wubu_manifest->cmd, oci_config.cmd[0], WUBU_MAX_CMD_LEN - 1);
    strncpy(wubu_manifest->workdir, oci_config.working_dir, sizeof(wubu_manifest->workdir) - 1);
    strncpy(wubu_manifest->user, oci_config.user, sizeof(wubu_manifest->user) - 1);
    for (int i = 0; i < oci_config.env_count && i < WUBU_MAX_ENVS; i++) strncpy(wubu_manifest->envs[i], oci_config.env[i], sizeof(wubu_manifest->envs[i]) - 1);
    wubu_manifest->env_count = oci_config.env_count;
    for (int i = 0; i < oci_config.exposed_port_count && i < WUBU_MAX_PORTS; i++) wubu_manifest->ports[i] = oci_config.exposed_ports[i];
    wubu_manifest->port_count = oci_config.exposed_port_count;
    wubu_manifest->layer_count = oci_manifest->layer_count;
    for (int i = 0; i < oci_manifest->layer_count && i < WUBU_MAX_LAYERS; i++) {
        wubu_manifest->layers[i].size = oci_manifest->layers[i].size;
        strncpy(wubu_manifest->layers[i].digest, oci_manifest->layers[i].digest, WUBU_LAYER_DIGEST_LEN - 1);
        wubu_manifest->layers[i].created = time(NULL);
    }
    wubu_manifest->arch = wubu_arch_from_string(oci_config.architecture);
    wubu_manifest->os = wubu_os_from_string(oci_config.os);
    wubu_manifest->created = atol(oci_config.created);
    wubu_manifest->stop_signal = oci_config.stop_signal;
    wubu_manifest_compute_id(wubu_manifest);
    return 0;
}

int oci_image_from_wubu(const char *wubu_path, const char *output_dir) {
    if (!wubu_path || !output_dir) return -1;

    WubuImageManifest manifest;
    if (wubu_image_import_wubu(wubu_path, &manifest) < 0) return -1;

    return oci_image_from_manifest(&manifest, output_dir);
}

int oci_image_from_manifest(const void *wubu_manifest_ptr, const char *output_dir) {
    if (!wubu_manifest_ptr || !output_dir) return -1;

    const WubuImageManifest *manifest = (const WubuImageManifest *)wubu_manifest_ptr;

    oci_ensure_oci_dirs();
    mkdir(output_dir, 0755);

    /* Create OCI config */
    OciImageConfig config;
    oci_config_create(&config, manifest);

    char config_json[16384];
    oci_config_to_json(&config, config_json, sizeof(config_json));

    char config_digest[65];
    sha256_digest(config_json, strlen(config_json), config_digest);

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/blobs/sha256/%s", output_dir, config_digest);
    mkdir("/var/lib/wubu/oci/blobs/sha256", 0755);

    int fd = open(config_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, config_json, strlen(config_json));
    close(fd);

    /* Create manifest */
    OciImageManifest oci_manifest;
    oci_manifest_create(&oci_manifest, manifest);

    char manifest_json[32768];
    oci_manifest_to_json(&oci_manifest, manifest_json, sizeof(manifest_json));

    char manifest_digest[65];
    sha256_digest(manifest_json, strlen(manifest_json), manifest_digest);

    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/blobs/sha256/%s", output_dir, manifest_digest);
    fd = open(manifest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, manifest_json, strlen(manifest_json));
    close(fd);

    /* Write oci-layout */
    char layout_json[256];
    snprintf(layout_json, sizeof(layout_json), "{\"imageLayoutVersion\":\"1.0.0\"}");

    char layout_path[1024];
    snprintf(layout_path, sizeof(layout_path), "%s/oci-layout", output_dir);
    fd = open(layout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, layout_json, strlen(layout_json)); close(fd); }

    /* Write index.json */
    OciImageIndex index;
    oci_index_create(&index);

    OciDescriptor desc = oci_manifest.config;
    OciPlatform platform = {0};
    strncpy(platform.architecture, wubu_arch_name(manifest->arch), 31);
    strncpy(platform.os, wubu_os_name(manifest->os), 31);

    oci_index_add_manifest(&index, &desc, &platform);

    char index_json[8192];
    oci_index_to_json(&index, index_json, sizeof(index_json));

    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.json", output_dir);
    fd = open(index_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, index_json, strlen(index_json)); close(fd); }

    /* Copy layer blobs using sendfile (no system() call) */
    for (int i = 0; i < manifest->layer_count; i++) {
        char src_path[1024];
        snprintf(src_path, sizeof(src_path), "/var/cache/wubu/layers/%s", manifest->layers[i].digest);

        char dst_path[1024];
        snprintf(dst_path, sizeof(dst_path), "%s/blobs/sha256/%s", output_dir, manifest->layers[i].digest);
        mkdir("/var/lib/wubu/oci/blobs/sha256", 0755);

        /* Open source file */
        int src_fd = open(src_path, O_RDONLY);
        if (src_fd < 0) {
            /* Source doesn't exist - continue (non-fatal) */
            continue;
        }

        /* Get file size */
        struct stat st;
        if (fstat(src_fd, &st) < 0) {
            close(src_fd);
            continue;
        }

        /* Open destination */
        int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dst_fd < 0) {
            close(src_fd);
            continue;
        }

        /* Copy using sendfile for efficiency */
        off_t offset = 0;
        ssize_t sent = sendfile(dst_fd, src_fd, &offset, st.st_size);
        close(src_fd);
        close(dst_fd);

        if (sent != st.st_size) {
            /* Incomplete copy - remove partial file */
            unlink(dst_path);
        }
    }

    return 0;
}