/*
 * wubu_oci.h  --  WuBuOS OCI (Open Container Initiative) Compatibility Layer
 *
 * Phase 7: OCI image spec v1.0+ and runtime spec v1.0+ compatibility
 * - Pull/push OCI images from registries (Docker Hub, GHCR, etc.)
 * - Convert between .wubu and OCI formats
 * - OCI runtime spec support (container config, hooks)
 * - Image manifest v2 schema 2 (multi-arch)
 * - Content-addressable blob storage (SHA256)
 * - Distribution spec (registry API)
 */

#ifndef WUBU_OCI_H
#define WUBU_OCI_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- Limits ------------------------------------------------------- */

#define OCI_MAX_LAYERS           128
#define OCI_MAX_ENV              128
#define OCI_MAX_PORTS            32
#define OCI_MAX_VOLUMES          32
#define OCI_MAX_DEVICES          16
#define OCI_MAX_MOUNTS           64
#define OCI_MAX_HOOKS            32
#define OCI_MAX_ANNOTATIONS      32
#define OCI_DIGEST_LEN           72      /* sha256:<64 hex> */
#define OCI_MAX_MEDIA_TYPE_LEN   128

/* -- Media Types -------------------------------------------------- */

#define OCI_MEDIA_TYPE_IMAGE_MANIFEST_V1      "application/vnd.oci.image.manifest.v1+json"
#define OCI_MEDIA_TYPE_IMAGE_MANIFEST_V2      "application/vnd.oci.image.manifest.v2+json"
#define OCI_MEDIA_TYPE_IMAGE_INDEX_V1         "application/vnd.oci.image.index.v1+json"
#define OCI_MEDIA_TYPE_IMAGE_CONFIG_V1        "application/vnd.oci.image.config.v1+json"
#define OCI_MEDIA_TYPE_LAYER_V1               "application/vnd.oci.image.layer.v1.tar"
#define OCI_MEDIA_TYPE_LAYER_V1_GZIP          "application/vnd.oci.image.layer.v1.tar+gzip"
#define OCI_MEDIA_TYPE_LAYER_V1_ZSTD          "application/vnd.oci.image.layer.v1.tar+zstd"
#define OCI_MEDIA_TYPE_EMPTY_JSON             "application/vnd.oci.empty.v1+json"

/* -- OCI Descriptor ----------------------------------------------- */

typedef struct {
    int              schema_version;        /* Always 2 for v2 */
    char             media_type[OCI_MAX_MEDIA_TYPE_LEN];
    uint64_t         size;
    char             digest[OCI_DIGEST_LEN];  /* sha256:<hex> */
} OciDescriptor;

/* -- OCI Image Config --------------------------------------------- */

typedef struct {
    char                created[64];           /* RFC3339 timestamp */
    char                architecture[32];
    char                os[32];
    char                os_version[32];
    char                os_features[64];
    char                variant[32];
    
    /* Config */
    char                entrypoint[16][256];
    int                 entrypoint_count;
    char                cmd[16][256];
    int                 cmd_count;
    char                working_dir[256];
    char                user[64];
    
    /* Env */
    char                env[128][256];
    int                 env_count;
    
    /* Exposed ports */
    int                 exposed_ports[32];
    int                 exposed_port_count;
    
    /* Volumes */
    char                volumes[32][256];
    int                 volume_count;
    
    /* Labels */
    char                labels[32][256];
    int                 label_count;
    
    /* Rootfs */
    struct {
        char type[32];
        char diff_ids[128][72];
        int  diff_id_count;
    } rootfs;
    
    int                 stop_signal;
} OciImageConfig;

/* -- OCI Image Manifest ------------------------------------------- */

typedef struct {
    int                 schema_version;
    char                media_type[OCI_MAX_MEDIA_TYPE_LEN];
    OciDescriptor       config;
    OciDescriptor       layers[OCI_MAX_LAYERS];
    int                 layer_count;
} OciImageManifest;

/* -- OCI Platform ------------------------------------------------- */

typedef struct {
    char architecture[32];
    char os[32];
    char os_version[32];
    char os_features[64];
    char variant[32];
} OciPlatform;

/* -- OCI Image Index (Multi-arch) --------------------------------- */

typedef struct {
    int                 schema_version;
    char                media_type[OCI_MAX_MEDIA_TYPE_LEN];
    OciDescriptor       manifests[16];
    OciPlatform         platforms[16];
    int                 manifest_count;
    char                annotations[32][2][128];
    int                 annotation_count;
} OciImageIndex;

/* -- Registry Auth ------------------------------------------------ */

typedef struct {
    char username[128];
    char password[128];
    char token[512];
} OciRegistryAuth;

/* -- OCI Runtime Spec --------------------------------------------- */

typedef struct {
    char path[256];
    int  argc;
    char args[32][256];
    int  envc;
    char env[32][256];
    int  timeout;
} OciHook;

typedef struct {
    char effective[32][64];
    int  effective_count;
    char bounding[32][64];
    int  bounding_count;
    char inheritable[32][64];
    int  inheritable_count;
    char permitted[32][64];
    int  permitted_count;
    char ambient[32][64];
    int  ambient_count;
} OciCapabilities;

typedef struct {
    char entrypoint[32][256];
    int  entrypoint_count;
    char args[32][256];
    int  args_count;
    char env[128][256];
    int  env_count;
    char cwd[256];
    struct {
        int uid;
        int gid;
        char username[64];
        char group[64];
        int additional_gids[32];
        int additional_gid_count;
    } user;
    OciCapabilities capabilities;
    char rlimits[16][128];
    int rlimit_count;
    bool no_new_privs;
    int apparmor_profile;
    char selinux_label[128];
    char seccomp_profile[256];
    bool terminal;
    int console_size[2];
} OciProcess;

typedef struct {
    char path[256];
    bool readonly;
} OciRoot;

typedef struct {
    char destination[256];
    char type[32];
    char source[256];
    char options[32][128];
    int options_count;
} OciMount;

typedef struct {
    char type[16];
    int major;
    int minor;
    char permissions[16];
    char path[256];
    int uid;
    int gid;
} OciDevice;

typedef struct {
    char path[256];
    char type[32];
    int major;
    int minor;
    char permissions[16];
    int uid;
    int gid;
} OciRuntimeDevice;

typedef struct {
    char hostname[256];
    char domainname[256];
    char dns[32][64];
    int dns_count;
    char dns_search[32][64];
    int dns_search_count;
    char dns_option[32][64];
    int dns_option_count;
    char extra_hosts[32][128];
    int extra_hosts_count;
} OciNetwork;

typedef struct {
    int  cpu_shares;
    int64_t cpu_quota;
    int64_t cpu_period;
    int  cpu_rt_runtime;
    int  cpu_rt_period;
    char cpuset_cpus[128];
    char cpuset_mems[128];
    int64_t memory_limit;
    int64_t memory_swap_limit;
    int64_t memory_reservation;
    int64_t kernel_memory_limit;
    int  memory_swappiness;
    int64_t blkio_weight;
    char blkio_weight_device[16][128];
    int blkio_weight_device_count;
    char blkio_throttle_read[16][128];
    int blkio_throttle_read_count;
    char blkio_throttle_write[16][128];
    int blkio_throttle_write_count;
    int  pids_limit;
    int  oom_score_adj;
    bool oom_kill_disable;
} OciResources;

typedef struct {
    char type[32];
    char path[256];
} OciNamespace;

typedef struct {
    int container_id_offset;
    int size;
} OciUidGidMapping;

typedef struct {
    char path[256];
    bool readonly;
} OciMaskedPath;

typedef struct {
    char path[256];
} OciReadonlyPath;

typedef struct {
    int default_action;
    char architectures[16][32];
    int architecture_count;
    int flags;
    char listeners[16][64];
    int listener_count;
} OciSeccomp;

typedef struct {
    char oci_version[32];
    OciProcess process;
    OciRoot root;
    char hostname[256];
    OciMount mounts[64];
    int mount_count;
    OciHook hooks_prestart[32];
    int hook_prestart_count;
    OciHook hooks_poststart[32];
    int hook_poststart_count;
    OciHook hooks_poststop[32];
    int hook_poststop_count;
    OciNetwork network;
    OciRuntimeDevice devices[16];
    int device_count;
    OciResources resources;
    char linux_cgroups_path[256];
    OciNamespace linux_namespaces[16];
    int linux_namespace_count;
    OciUidGidMapping linux_uid_mappings[16];
    int linux_uid_mapping_count;
    OciUidGidMapping linux_gid_mappings[16];
    int linux_gid_mapping_count;
    char linux_sysctl[32][128];
    int linux_sysctl_count;
    OciMaskedPath linux_masked_paths[16];
    int linux_masked_path_count;
    OciReadonlyPath linux_readonly_paths[16];
    int linux_readonly_path_count;
    char linux_mount_label[256];
    char linux_process_label[256];
    OciSeccomp linux_seccomp;
} OciRuntimeSpec;

/* -- OCI Registry Client ------------------------------------------ */

typedef struct OciRegistryClient OciRegistryClient;

OciRegistryClient *oci_registry_client_new(const char *registry, const char *username, const char *password);
void oci_registry_client_free(OciRegistryClient *client);

int oci_registry_ping(OciRegistryClient *client);

int oci_registry_get_manifest(OciRegistryClient *client, const char *repo, const char *tag_or_digest,
                              OciImageManifest *out_manifest, char *out_raw_json, size_t raw_size);

int oci_registry_put_manifest(OciRegistryClient *client, const char *repo, const char *tag,
                              const OciImageManifest *manifest, const char *media_type);

int oci_registry_get_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          void *out_data, size_t *out_size);

int oci_registry_put_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          const void *data, size_t size);

int oci_registry_mount_blob(OciRegistryClient *client, const char *from_repo, const char *to_repo,
                            const char *digest);

int oci_registry_list_tags(OciRegistryClient *client, const char *repo, char tags[][128], int max);

int oci_registry_delete_manifest(OciRegistryClient *client, const char *repo, const char *digest);

/* -- Media Type Helpers ------------------------------------------- */

const char *oci_media_type_image_manifest_v1(void);
const char *oci_media_type_image_manifest_v2(void);
const char *oci_media_type_image_index_v1(void);
const char *oci_media_type_image_config_v1(void);
const char *oci_media_type_layer_v1(void);
const char *oci_media_type_layer_v1_gzip(void);
const char *oci_media_type_layer_v1_zstd(void);
const char *oci_media_type_empty_json(void);

/* -- Descriptor Operations ---------------------------------------- */

int oci_create_descriptor(OciDescriptor *desc, const char *media_type, uint64_t size, const char *sha256_digest);

/* -- OCI Image Config --------------------------------------------- */

int oci_config_create(OciImageConfig *config, const void *wubu_manifest);

int oci_config_to_json(const OciImageConfig *config, char *out_json, size_t out_size);
int oci_config_from_json(const char *json, OciImageConfig *config);
int oci_config_compute_digest(const OciImageConfig *config, char *out_digest, size_t out_size);

/* -- OCI Image Manifest ------------------------------------------- */

int oci_manifest_create(OciImageManifest *manifest, const void *wubu_manifest);
int oci_manifest_to_json(const OciImageManifest *manifest, char *out_json, size_t out_size);
int oci_manifest_from_json(const char *json, OciImageManifest *manifest);
int oci_manifest_compute_digest(const OciImageManifest *manifest, char *out_digest, size_t out_size);

/* -- OCI Image Index ---------------------------------------------- */

int oci_index_create(OciImageIndex *index);
int oci_index_add_manifest(OciImageIndex *index, const OciDescriptor *desc, const OciPlatform *platform);
int oci_index_to_json(const OciImageIndex *index, char *out_json, size_t out_size);
int oci_index_from_json(const char *json, OciImageIndex *index);

/* -- Blob Store --------------------------------------------------- */

int oci_blob_store_init(const char *root_path);
int oci_blob_put(const char *root_path, const char *digest, const void *data, size_t size);
int oci_blob_get(const char *root_path, const char *digest, void *out_data, size_t *out_size);
bool oci_blob_exists(const char *root_path, const char *digest);

/* -- Convert .wubu <-> OCI ---------------------------------------- */

#include "wubu_image.h"

int oci_image_from_wubu(const char *wubu_path, const char *output_dir);
int oci_image_from_manifest(const void *wubu_manifest_ptr, const char *output_dir);
int oci_image_to_wubu(const char *oci_dir, const char *wubu_output);

/* -- OCI Manifest -> Wubu Manifest Conversion ---------------------- */

int oci_manifest_to_wubu(const OciImageManifest *oci_manifest, const char *oci_dir,
                         WubuImageManifest *wubu_manifest);

/* -- Registry Operations ------------------------------------------ */

int oci_registry_ping(OciRegistryClient *client);
int oci_registry_get_manifest(OciRegistryClient *client, const char *repo, const char *tag_or_digest,
                              OciImageManifest *out_manifest, char *out_raw_json, size_t raw_size);
int oci_registry_put_manifest(OciRegistryClient *client, const char *repo, const char *tag,
                              const OciImageManifest *manifest, const char *media_type);
int oci_registry_get_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          void *out_data, size_t *out_size);
int oci_registry_put_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          const void *data, size_t size);
int oci_registry_mount_blob(OciRegistryClient *client, const char *from_repo, const char *to_repo,
                            const char *digest);
int oci_registry_list_tags(OciRegistryClient *client, const char *repo, char tags[][128], int max);
int oci_registry_delete_manifest(OciRegistryClient *client, const char *repo, const char *digest);

/* -- Runtime Spec Support ----------------------------------------- */

int oci_runtime_spec_create(OciRuntimeSpec *spec, const void *manifest);
void oci_runtime_spec_free(OciRuntimeSpec *spec);
int oci_runtime_spec_to_json(const OciRuntimeSpec *spec, char *out_json, size_t out_size);
int oci_runtime_spec_from_json(const char *json, OciRuntimeSpec *spec);
int oci_runtime_spec_validate(const OciRuntimeSpec *spec);

/* -- Hooks -------------------------------------------------------- */

int oci_hook_create(OciHook *hook, const char *path, const char *args[], int argc,
                    const char *env[], int envc, int timeout);
void oci_hook_free(OciHook *hook);

/* -- Cleanup ------------------------------------------------------ */

int oci_cleanup_old_layers(const char *root_path, time_t max_age_days, bool dry_run);
int oci_gc_unreferenced_blobs(const char *root_path, bool dry_run);

/* -- Helpers ------------------------------------------------------ */

const char *oci_media_type_image_manifest_v1(void);
const char *oci_media_type_image_manifest_v2(void);
const char *oci_media_type_image_index_v1(void);
const char *oci_media_type_image_config_v1(void);
const char *oci_media_type_layer_v1(void);
const char *oci_media_type_layer_v1_gzip(void);
const char *oci_media_type_layer_v1_zstd(void);
const char *oci_media_type_empty_json(void);

#endif /* WUBU_OCI_H */