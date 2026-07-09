/*
 * wubu_image_ops.c  --  WuBuOS Image Tag/Remove/Inspect/Push/Pull
 *
 * Extracted from wubu_image.c (2026-07-06): image lifecycle operations:
 * tagging, removal, pruning, inspection, history, push/pull.
 *
 * C11 only. Depends on wubu_image.h for types, wubu_image_internal.h
 * for string helpers/crypto, wubu_image_manifest.h for manifest I/O,
 * and wubu_oci.h for OCI manifest conversion.
 */

#include "wubu_image_internal.h"
#include "wubu_image_manifest.h"
#include "wubu_oci.h"
#include "wubu_spawn.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================ */
/* Tag Management                                                    */
/* ================================================================ */

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

/* ================================================================ */
/* Image Removal / Prune                                             */
/* ================================================================ */

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

/* ================================================================ */
/* Inspect / History                                                 */
/* ================================================================ */

int wubu_image_inspect(const char *image_ref, WubuImageManifest *out_manifest) {
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", TAG_DIR, image_ref);

    char image_id[65];
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        ssize_t rd = read(fd, image_id, sizeof(image_id) - 1);
        close(fd);
        if (rd <= 0) return -1;
        image_id[rd] = '\0';
        while (rd > 0 && (image_id[rd-1] == '\n' || image_id[rd-1] == '\r' || image_id[rd-1] == ' '))
            image_id[--rd] = '\0';

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
    for (int i = 0; i < count; i++)
        layers[i] = manifest.layers[i];
    return count;
}

/* ================================================================ */
/* Push/Pull (stubs for registry)                                    */
/* ================================================================ */

int wubu_image_push(const char *image_ref, const char *registry, const char *auth) {
    (void)auth;
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
    char *argv[] = {
        "curl", "-sS", "-L", "-X", "PUT",
        "-H", "Content-Type: application/vnd.oci.image.manifest.v2+json",
        "--data-binary", tmp, url, (char *)NULL
    };
    int ret = wubu_run_program("curl", argv, true);
    unlink(tmp);
    for (int i = 0; i < manifest.layer_count && ret == 0; i++) {
        char layer_path[WUBU_MAX_PATH];
        snprintf(layer_path, sizeof(layer_path), "/var/cache/wubu/layers/%s", manifest.layers[i].digest);
        if (access(layer_path, F_OK) != 0) continue;
        char blob_url[WUBU_MAX_PATH];
        snprintf(blob_url, sizeof(blob_url), "https://%s/v2/wubu/blobs/%s", registry, manifest.layers[i].digest);
        char *argv2[] = {
            "curl", "-sS", "-L", "-X", "PUT",
            "--data-binary", layer_path, blob_url, (char *)NULL
        };
        if (wubu_run_program("curl", argv2, true) != 0) { ret = -1; break; }
    }
    return ret == 0 ? 0 : -1;
}

int wubu_image_pull(const char *image_ref, const char *registry, const char *auth, WubuImageManifest *out_manifest) {
    (void)auth;
    if (!image_ref || !registry || !out_manifest) return -1;
    char url[WUBU_MAX_PATH];
    snprintf(url, sizeof(url), "https://%s/v2/wubu/images/manifests/%s", registry, image_ref);
    char *argv[] = {
        "curl", "-sS", "-L",
        "-H", "Accept: application/vnd.oci.image.manifest.v2+json",
        url, "-o", "/tmp/wubu_pull_manifest.json", (char *)NULL
    };
    if (wubu_run_program("curl", argv, true) != 0) return -1;
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
        char *argv3[] = {
            "curl", "-sS", "-L", "-o", layer_path, url, (char *)NULL
        };
        if (wubu_run_program("curl", argv3, true) != 0) {
            char blob_dir[WUBU_MAX_PATH];
            snprintf(blob_dir, sizeof(blob_dir), "/var/lib/wubu/oci/blobs/sha256");
            mkdir(blob_dir, 0755);
            char alt_path[WUBU_MAX_PATH + 64];
            snprintf(alt_path, sizeof(alt_path), "%s/%s", blob_dir, blob_name);
            char *argv4[] = {
                "curl", "-sS", "-L", "-o", alt_path, url, (char *)NULL
            };
            wubu_run_program("curl", argv4, true);
        }
    }
    free(buf);
    return 0;
}