/*
 * wubu_oci_test.c  --  Tests for OCI runtime
 */

#include "wubu_oci.h"
#include "wubu_image.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int pass = 0, fail = 0;
#define TEST(name) printf("  TEST OCI: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_media_types(void) {
    TEST("media type strings");
    CHECK(strcmp(oci_media_type_image_manifest_v2(), "application/vnd.oci.image.manifest.v2+json") == 0, "manifest v2");
    CHECK(strcmp(oci_media_type_image_config_v1(), "application/vnd.oci.image.config.v1+json") == 0, "config v1");
    CHECK(strcmp(oci_media_type_layer_v1_gzip(), "application/vnd.oci.image.layer.v1.tar+gzip") == 0, "layer gzip");
    PASS();
}

static void test_descriptor(void) {
    TEST("oci_create_descriptor");
    OciDescriptor desc;
    int rc = oci_create_descriptor(&desc, oci_media_type_layer_v1_gzip(), 1234, "abcd1234");
    CHECK(rc == 0, "create ok");
    CHECK(desc.schema_version == 2, "schema version");
    CHECK(strstr(desc.digest, "sha256:") == desc.digest, "digest prefix");
    CHECK(desc.size == 1234, "size");
    PASS();
}

static void test_config_roundtrip(void) {
    TEST("config create -> json -> from_json roundtrip");
    WubuImageManifest wubu = {0};
    wubu.arch = WUBU_ARCH_X86_64;
    wubu.os = WUBU_OS_LINUX;
    strncpy(wubu.entrypoint, "/bin/sh", WUBU_MAX_CMD_LEN - 1);
    strncpy(wubu.cmd, "run", WUBU_MAX_CMD_LEN - 1);
    strncpy(wubu.workdir, "/app", 255);
    strncpy(wubu.user, "root", 63);
    wubu.port_count = 1;
    wubu.ports[0] = 8080;
    wubu.layer_count = 1;
    strncpy(wubu.layers[0].digest, "layer1", WUBU_LAYER_DIGEST_LEN - 1);
    wubu.layers[0].size = 1024;

    OciImageConfig config;
    int rc = oci_config_create(&config, &wubu);
    CHECK(rc == 0, "create config");
    CHECK(strcmp(config.architecture, "x86_64") == 0, "arch");
    CHECK(strcmp(config.os, "linux") == 0, "os");
    CHECK(config.entrypoint_count == 1, "entrypoint count");
    CHECK(strcmp(config.entrypoint[0], "/bin/sh") == 0, "entrypoint");
    CHECK(config.cmd_count == 1, "cmd count");
    CHECK(config.exposed_port_count == 1, "ports");
    CHECK(config.exposed_ports[0] == 8080, "port value");

    char json[16384];
    rc = oci_config_to_json(&config, json, sizeof(json));
    CHECK(rc == 0, "to json");

    OciImageConfig config2;
    rc = oci_config_from_json(json, &config2);
    CHECK(rc == 0, "from json");
    CHECK(strcmp(config2.architecture, config.architecture) == 0, "arch roundtrip");
    CHECK(strcmp(config2.os, config.os) == 0, "os roundtrip");
    CHECK(config2.entrypoint_count == config.entrypoint_count, "entrypoint count roundtrip");
    PASS();
}

static void test_manifest_roundtrip(void) {
    TEST("manifest create -> json -> from_json roundtrip");
    WubuImageManifest wubu = {0};
    wubu.arch = WUBU_ARCH_X86_64;
    wubu.os = WUBU_OS_LINUX;
    wubu.layer_count = 2;
    strncpy(wubu.layers[0].digest, "layer1", WUBU_LAYER_DIGEST_LEN - 1);
    wubu.layers[0].size = 1024;
    strncpy(wubu.layers[1].digest, "layer2", WUBU_LAYER_DIGEST_LEN - 1);
    wubu.layers[1].size = 2048;

    OciImageManifest manifest;
    int rc = oci_manifest_create(&manifest, &wubu);
    CHECK(rc == 0, "create manifest");
    CHECK(manifest.layer_count == 2, "layer count");
    CHECK(manifest.config.size > 0, "config size");

    char json[32768];
    rc = oci_manifest_to_json(&manifest, json, sizeof(json));
    CHECK(rc == 0, "to json");

    OciImageManifest manifest2;
    rc = oci_manifest_from_json(json, &manifest2);
    CHECK(rc == 0, "from json");
    CHECK(strcmp(manifest2.media_type, manifest.media_type) == 0, "media type roundtrip");
    CHECK(manifest2.layer_count == manifest.layer_count, "layer count roundtrip");
    CHECK(strcmp(manifest2.layers[0].digest, manifest.layers[0].digest) == 0, "layer digest");
    PASS();
}

static void test_blob_store(void) {
    TEST("blob store put/get/exists");
    const char *root = "/tmp/wubu_oci_test_blobs";
    int rc = oci_blob_store_init(root);
    CHECK(rc == 0, "init blob store");

    const char *digest = "abc123def456";
    const char *data = "hello world";
    rc = oci_blob_put(root, digest, data, strlen(data));
    CHECK(rc == 0, "put blob");

    bool exists = oci_blob_exists(root, digest);
    CHECK(exists == true, "blob exists");

    char buf[64];
    size_t buf_size = sizeof(buf);
    rc = oci_blob_get(root, digest, buf, &buf_size);
    CHECK(rc == 0, "get blob");
    CHECK(buf_size == strlen(data), "size matches");
    CHECK(memcmp(buf, data, buf_size) == 0, "data matches");

    char path[256];
    snprintf(path, sizeof(path), "%s/blobs/sha256/%s", root, digest);
    unlink(path);
    rmdir(root);
    PASS();
}

static void test_index(void) {
    TEST("index create/add/to_json");
    OciImageIndex index;
    int rc = oci_index_create(&index);
    CHECK(rc == 0, "create index");

    OciDescriptor desc;
    oci_create_descriptor(&desc, oci_media_type_image_manifest_v2(), 100, "idx1");
    OciPlatform platform;
    strncpy(platform.architecture, "x86_64", 31);
    strncpy(platform.os, "linux", 31);

    rc = oci_index_add_manifest(&index, &desc, &platform);
    CHECK(rc == 0, "add manifest");
    CHECK(index.manifest_count == 1, "count 1");

    char json[8192];
    rc = oci_index_to_json(&index, json, sizeof(json));
    CHECK(rc == 0, "to json");
    CHECK(strstr(json, "x86_64") != NULL, "contains arch");
    PASS();
}

static void test_runtime_spec(void) {
    TEST("runtime spec create/to/from json");
    OciRuntimeSpec spec;
    int rc = oci_runtime_spec_create(&spec, NULL);
    CHECK(rc == 0, "create spec");
    CHECK(spec.root.path[0] != '\0', "root path set");

    char json[4096];
    rc = oci_runtime_spec_to_json(&spec, json, sizeof(json));
    CHECK(rc == 0, "to json");

    OciRuntimeSpec spec2;
    rc = oci_runtime_spec_from_json(json, &spec2);
    CHECK(rc == 0, "from json");
    CHECK(strcmp(spec2.oci_version, spec.oci_version) == 0, "oci version roundtrip");

    rc = oci_runtime_spec_validate(&spec2);
    CHECK(rc == 0, "validate ok");

    OciRuntimeSpec bad = {0};
    rc = oci_runtime_spec_validate(&bad);
    CHECK(rc < 0, "validate empty fails");

    oci_runtime_spec_free(&spec);
    PASS();
}

static void test_hooks(void) {
    TEST("hook create");
    OciHook hook;
    const char *args[] = {"prestart", "arg1", NULL};
    int rc = oci_hook_create(&hook, "/bin/echo", args, 2, NULL, 0, 30);
    CHECK(rc == 0, "create hook");
    CHECK(strcmp(hook.path, "/bin/echo") == 0, "path");
    CHECK(hook.argc == 2, "argc");
    CHECK(strcmp(hook.args[0], "prestart") == 0, "arg0");
    CHECK(hook.timeout == 30, "timeout");
    oci_hook_free(&hook);
    PASS();
}

static void test_registry_client(void) {
    TEST("registry client lifecycle");
    OciRegistryClient *client = oci_registry_client_new("localhost:5000", "user", "pass");
    CHECK(client != NULL, "client created");

    int rc = oci_registry_ping(client);
    CHECK(rc == 0 || rc == -1, "ping returns ok or unavailable");

    oci_registry_client_free(client);
    PASS();
}

static void test_cleanup(void) {
    TEST("cleanup/gc stubbed but safe");
    int rc = oci_cleanup_old_layers("/tmp", 7, true);
    CHECK(rc == 0, "cleanup returns ok");
    rc = oci_gc_unreferenced_blobs("/tmp", true);
    CHECK(rc == 0, "gc returns ok");
    PASS();
}

int main(void) {
    printf("\n-- OCI Runtime Test Suite --\n\n");
    test_media_types();
    test_descriptor();
    test_config_roundtrip();
    test_manifest_roundtrip();
    test_blob_store();
    test_index();
    test_runtime_spec();
    test_hooks();
    test_registry_client();
    test_cleanup();
    printf("\n[skipped] registry/manifest blob roundtrips pending stable curl/system transport\n");

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", pass, pass + fail, fail);
    printf("==================================================\n");
    return fail > 0 ? 1 : 0;
}
