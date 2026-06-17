/**
 * wubu_deploy_test.c - Unit tests for WuBuOS Multi-Target Deployment
 */

#include "wubu_deploy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_STR_CONTAINS(haystack, needle, msg) \
    do { \
        if (strstr((haystack), (needle)) == NULL) { \
            fprintf(stderr, "FAIL: %s - expected to contain '%s' (%s:%d)\n", msg, (needle), __FILE__, __LINE__); \
            return false; \
        } \
    } while (0)

static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(fn) \
    do { \
        printf("Running %s... ", #fn); \
        fflush(stdout); \
        if (fn()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            printf("FAIL\n"); \
            tests_failed++; \
        } \
    } while (0)

/* Test default configs */
bool test_default_configs(void) {
    wubu_baremetal_config_t bm;
    wubu_wsl2_config_t wsl;
    wubu_oci_config_t oci;
    wubu_macos_config_t mac;

    wubu_deploy_get_default_baremetal(&bm);
    TEST_ASSERT(bm.kernel_path != NULL, "baremetal kernel_path");
    TEST_ASSERT(bm.wubu_binary != NULL, "baremetal wubu_binary");
    TEST_ASSERT(bm.output_iso != NULL, "baremetal output_iso");

    wubu_deploy_get_default_wsl2(&wsl);
    TEST_ASSERT(wsl.distro_name != NULL, "wsl2 distro_name");
    TEST_ASSERT(wsl.wubu_binary != NULL, "wsl2 wubu_binary");
    TEST_ASSERT(wsl.output_tar != NULL, "wsl2 output_tar");
    TEST_ASSERT(wsl.systemd == true, "wsl2 systemd enabled");

    wubu_deploy_get_default_oci(&oci);
    TEST_ASSERT(oci.image_name != NULL, "oci image_name");
    TEST_ASSERT(oci.wubu_binary != NULL, "oci wubu_binary");
    TEST_ASSERT(oci.base_image != NULL, "oci base_image");
    TEST_ASSERT(oci.env_vars != NULL, "oci env_vars");

    wubu_deploy_get_default_macos(&mac);
    TEST_ASSERT(mac.app_bundle_id != NULL, "macos app_bundle_id");
    TEST_ASSERT(mac.wubu_binary != NULL, "macos wubu_binary");
    TEST_ASSERT(mac.output_app != NULL, "macos output_app");
    TEST_ASSERT(mac.vm_memory_mb > 0, "macos vm_memory_mb");
    TEST_ASSERT(mac.vm_cpus > 0, "macos vm_cpus");
    TEST_ASSERT(mac.gui_enabled == true, "macos gui_enabled");
    TEST_ASSERT(mac.rosetta == true, "macos rosetta");

    return true;
}

/* Test validation */
bool test_validation(void) {
    wubu_baremetal_config_t bm = {0};
    TEST_ASSERT(!wubu_deploy_validate_baremetal(&bm), "baremetal empty invalid");
    bm.kernel_path = "/kernel";
    bm.wubu_binary = "/wubu";
    bm.output_iso = "/out.iso";
    TEST_ASSERT(wubu_deploy_validate_baremetal(&bm), "baremetal valid");

    wubu_wsl2_config_t wsl = {0};
    TEST_ASSERT(!wubu_deploy_validate_wsl2(&wsl), "wsl2 empty invalid");
    wsl.distro_name = "Test";
    wsl.wubu_binary = "/wubu";
    wsl.output_tar = "/out.tar.gz";
    TEST_ASSERT(wubu_deploy_validate_wsl2(&wsl), "wsl2 valid");

    wubu_oci_config_t oci = {0};
    TEST_ASSERT(!wubu_deploy_validate_oci(&oci), "oci empty invalid");
    oci.image_name = "test:latest";
    oci.wubu_binary = "/wubu";
    oci.context_dir = ".";
    TEST_ASSERT(wubu_deploy_validate_oci(&oci), "oci valid");

    wubu_macos_config_t mac = {0};
    TEST_ASSERT(!wubu_deploy_validate_macos(&mac), "macos empty invalid");
    mac.app_bundle_id = "com.test.app";
    mac.wubu_binary = "/wubu";
    mac.output_app = "/Test.app";
    TEST_ASSERT(wubu_deploy_validate_macos(&mac), "macos valid");

    return true;
}

/* Test limine config generation */
bool test_limine_generation(void) {
    const char* test_path = "/tmp/test_limine.conf";
    const char* cmdline = "quiet loglevel=3 test_param=value";
    
    TEST_ASSERT(wubu_deploy_generate_limine_conf(test_path, cmdline), "generate limine");
    
    FILE* f = fopen(test_path, "r");
    TEST_ASSERT(f != NULL, "limine file exists");
    
    char content[4096];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    
    TEST_ASSERT_STR_CONTAINS(content, "timeout: 5", "timeout present");
    TEST_ASSERT_STR_CONTAINS(content, "default_entry: WuBuOS", "default_entry present");
    TEST_ASSERT_STR_CONTAINS(content, "protocol: linux", "linux protocol present");
    TEST_ASSERT_STR_CONTAINS(content, cmdline, "kernel cmdline present");
    TEST_ASSERT_STR_CONTAINS(content, "Reboot", "reboot entry present");
    TEST_ASSERT_STR_CONTAINS(content, "Power Off", "poweroff entry present");
    
    unlink(test_path);
    return true;
}

/* Test wsl.conf generation */
bool test_wsl_conf_generation(void) {
    const char* test_path = "/tmp/test_wsl.conf";
    
    TEST_ASSERT(wubu_deploy_generate_wsl_conf(test_path, true), "generate wsl.conf systemd");
    
    FILE* f = fopen(test_path, "r");
    TEST_ASSERT(f != NULL, "wsl.conf file exists");
    
    char content[2048];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    
    TEST_ASSERT_STR_CONTAINS(content, "systemd=true", "systemd=true present");
    TEST_ASSERT_STR_CONTAINS(content, "generateResolvConf=true", "resolvconf present");
    TEST_ASSERT_STR_CONTAINS(content, "default=wubu", "default user present");
    
    unlink(test_path);
    
    TEST_ASSERT(wubu_deploy_generate_wsl_conf(test_path, false), "generate wsl.conf no systemd");
    f = fopen(test_path, "r");
    n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    TEST_ASSERT_STR_CONTAINS(content, "systemd=false", "systemd=false present");
    
    unlink(test_path);
    return true;
}

/* Test Dockerfile generation */
bool test_dockerfile_generation(void) {
    const char* test_path = "/tmp/test_Dockerfile";
    
    wubu_oci_config_t config = {0};
    config.image_name = "wubuos:test";
    config.base_image = "scratch";
    config.entrypoint = "/usr/bin/wubu";
    config.env_vars = (const char*[]) { "TEST=value", "FOO=bar", NULL };
    config.ports = (const char*[]) { "8080/tcp", NULL };
    config.volumes = (const char*[]) { "/data", NULL };
    
    TEST_ASSERT(wubu_deploy_generate_dockerfile(&config, test_path), "generate Dockerfile scratch");
    
    FILE* f = fopen(test_path, "r");
    TEST_ASSERT(f != NULL, "Dockerfile exists");
    
    char content[8192];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    
    TEST_ASSERT_STR_CONTAINS(content, "FROM scratch", "scratch base present");
    TEST_ASSERT_STR_CONTAINS(content, "COPY rootfs/", "copy rootfs present");
    TEST_ASSERT_STR_CONTAINS(content, "ENV TEST=value", "env var present");
    TEST_ASSERT_STR_CONTAINS(content, "EXPOSE 8080/tcp", "expose port present");
    TEST_ASSERT_STR_CONTAINS(content, "VOLUME [\"/data\"]", "volume present");
    TEST_ASSERT_STR_CONTAINS(content, "ENTRYPOINT [\"/usr/bin/wubu\"]", "entrypoint present");
    TEST_ASSERT_STR_CONTAINS(content, "USER 1000:1000", "non-root user present");
    
    unlink(test_path);
    
    /* Test alpine base */
    config.base_image = "alpine:latest";
    TEST_ASSERT(wubu_deploy_generate_dockerfile(&config, test_path), "generate Dockerfile alpine");
    f = fopen(test_path, "r");
    n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    TEST_ASSERT_STR_CONTAINS(content, "apk add", "apk add present");
    TEST_ASSERT_STR_CONTAINS(content, "libwayland-client", "wayland dep present");
    
    unlink(test_path);
    
    /* Test debian base */
    config.base_image = "debian:bookworm-slim";
    TEST_ASSERT(wubu_deploy_generate_dockerfile(&config, test_path), "generate Dockerfile debian");
    f = fopen(test_path, "r");
    n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    TEST_ASSERT_STR_CONTAINS(content, "apt-get install", "apt-get install present");
    TEST_ASSERT_STR_CONTAINS(content, "libwayland-client0", "wayland dep present");
    
    unlink(test_path);
    return true;
}

/* Test macOS entitlements generation */
bool test_entitlements_generation(void) {
    const char* test_path = "/tmp/test.entitlements";
    
    wubu_macos_config_t config = {0};
    config.rosetta = true;
    
    TEST_ASSERT(wubu_deploy_generate_entitlements(&config, test_path), "generate entitlements rosetta");
    
    FILE* f = fopen(test_path, "r");
    TEST_ASSERT(f != NULL, "entitlements file exists");
    
    char content[4096];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    
    TEST_ASSERT_STR_CONTAINS(content, "com.apple.security.hypervisor", "hypervisor entitlement");
    TEST_ASSERT_STR_CONTAINS(content, "com.apple.security.vm.networking", "vm networking");
    TEST_ASSERT_STR_CONTAINS(content, "allow-dyld-environment-variables", "rosetta entitlement");
    TEST_ASSERT_STR_CONTAINS(content, "allow-unsigned-executable-memory", "rosetta memory");
    
    unlink(test_path);
    
    /* Without rosetta */
    config.rosetta = false;
    TEST_ASSERT(wubu_deploy_generate_entitlements(&config, test_path), "generate entitlements no rosetta");
    f = fopen(test_path, "r");
    n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    TEST_ASSERT(strstr(content, "allow-dyld-environment-variables") == NULL, "no rosetta entitlement");
    
    unlink(test_path);
    return true;
}

/* Test macOS Info.plist generation */
bool test_infoplist_generation(void) {
    const char* test_path = "/tmp/test_Info.plist";
    
    wubu_macos_config_t config = {0};
    config.app_bundle_id = "com.test.wubuos";
    config.app_name = "TestWuBuOS";
    
    TEST_ASSERT(wubu_deploy_generate_infoplist(&config, test_path), "generate Info.plist");
    
    FILE* f = fopen(test_path, "r");
    TEST_ASSERT(f != NULL, "Info.plist file exists");
    
    char content[4096];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    
    TEST_ASSERT_STR_CONTAINS(content, "com.test.wubuos", "bundle ID present");
    TEST_ASSERT_STR_CONTAINS(content, "TestWuBuOS", "app name present");
    TEST_ASSERT_STR_CONTAINS(content, "CFBundleExecutable", "executable key present");
    TEST_ASSERT_STR_CONTAINS(content, "wubu-macos", "executable name present");
    TEST_ASSERT_STR_CONTAINS(content, "LSMinimumSystemVersion", "min version present");
    TEST_ASSERT_STR_CONTAINS(content, "13.0", "macOS 13 min version");
    
    unlink(test_path);
    return true;
}

/* Test rootfs creation (basic structure) */
bool test_rootfs_creation(void) {
    const char* test_rootfs = "/tmp/test_wubuos_rootfs";
    const char* test_binary = "/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/hosted/wubu";
    
    /* Remove any existing */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_rootfs);
    system(cmd);
    
    /* Create - but we need the binary to exist */
    if (access(test_binary, F_OK) != 0) {
        printf("SKIP: wubu binary not found\n");
        return true;
    }
    
    TEST_ASSERT(wubu_deploy_create_rootfs(test_rootfs, test_binary), "create rootfs");
    
    /* Check essential directories */
    const char* dirs[] = {
        "bin", "sbin", "etc", "proc", "sys", "dev", "run", "tmp",
        "var/log", "home/wubu", "usr/bin", "usr/lib", "lib", "lib64"
    };
    for (int i = 0; i < (int)(sizeof(dirs)/sizeof(dirs[0])); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", test_rootfs, dirs[i]);
        struct stat st;
        TEST_ASSERT(stat(path, &st) == 0 && S_ISDIR(st.st_mode), dirs[i]);
    }
    
    /* Check essential files */
    const char* files[] = {
        "etc/passwd", "etc/group", "etc/hostname", "etc/hosts",
        "etc/resolv.conf", "etc/fstab", "etc/profile", "etc/bashrc",
        "init", "usr/bin/wubu", "etc/wsl.conf"
    };
    for (int i = 0; i < (int)(sizeof(files)/sizeof(files[0])); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", test_rootfs, files[i]);
        struct stat st;
        TEST_ASSERT(stat(path, &st) == 0 && S_ISREG(st.st_mode), files[i]);
    }
    
    /* Check passwd content */
    FILE* f = fopen("/tmp/test_wubuos_rootfs/etc/passwd", "r");
    char content[1024];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    TEST_ASSERT_STR_CONTAINS(content, "root:x:0:0", "root user");
    TEST_ASSERT_STR_CONTAINS(content, "wubu:x:1000:1000", "wubu user");
    
    /* Check init script executable */
    struct stat st;
    stat("/tmp/test_wubuos_rootfs/init", &st);
    TEST_ASSERT(st.st_mode & S_IXUSR, "init executable");
    
    /* Cleanup */
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_rootfs);
    system(cmd);
    
    return true;
}

/* Test deployment init/shutdown */
bool test_deploy_lifecycle(void) {
    TEST_ASSERT(wubu_deploy_init(), "deploy init");
    wubu_deploy_shutdown();
    return true;
}

/* Main */
int main(void) {
    printf("=== WuBuOS Deployment Tests ===\n\n");
    
    RUN_TEST(test_deploy_lifecycle);
    RUN_TEST(test_default_configs);
    RUN_TEST(test_validation);
    RUN_TEST(test_limine_generation);
    RUN_TEST(test_wsl_conf_generation);
    RUN_TEST(test_dockerfile_generation);
    RUN_TEST(test_entitlements_generation);
    RUN_TEST(test_infoplist_generation);
    RUN_TEST(test_rootfs_creation);
    
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}