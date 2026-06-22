/*
 * wubu_image.h  --  WuBuOS Container Image Builder
 *
 * Phase 7: .wubu image builder with:
 *   - WuBuFile (Dockerfile-like) declarative syntax
 *   - Multi-stage builds
 *   - Layer caching with content-addressable storage
 *   - Base images from Arch Linux packages, Steam Runtime, etc.
 *   - .wubu output with proper headers
 *   - OCI image ref conversion (pull/push)
 *   - Build context with .wubuignore
 */

#ifndef WUBU_IMAGE_H
#define WUBU_IMAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- Limits ------------------------------------------------------- */

#define WUBU_MAX_LAYERS          128
#define WUBU_MAX_STAGES          16
#define WUBU_MAX_INSTRUCTIONS    512
#define WUBU_MAX_ARGS            64
#define WUBU_MAX_ENVS            64
#define WUBU_MAX_PORTS           32
#define WUBU_MAX_VOLUMES         32
#define WUBU_MAX_LABELS          32
#define WUBU_MAX_DEVICES         16
#define WUBU_LAYER_DIGEST_LEN    64      /* SHA256 hex */
#define WUBU_IMAGE_ID_LEN        64      /* SHA256 hex */
#define WUBU_MAX_PATH            4096
#define WUBU_MAX_CMD_LEN         4096

/* -- Instruction Types -------------------------------------------- */

typedef enum {
    WUBU_INST_FROM        = 0,    /* FROM <image>[:tag] [AS <name>] */
    WUBU_INST_RUN         = 1,    /* RUN <command> */
    WUBU_INST_COPY        = 2,    /* COPY [--from=<stage>] <src>... <dest> */
    WUBU_INST_ADD         = 3,    /* ADD [--from=<stage>] <src>... <dest> (auto-extract) */
    WUBU_INST_CMD         = 4,    /* CMD ["executable", "param1", ...] */
    WUBU_INST_ENTRYPOINT  = 5,    /* ENTRYPOINT ["executable", "param1", ...] */
    WUBU_INST_ENV         = 6,    /* ENV <key>=<value> ... */
    WUBU_INST_ARG         = 7,    /* ARG <name>[=<default>] */
    WUBU_INST_WORKDIR     = 8,    /* WORKDIR <path> */
    WUBU_INST_USER        = 9,    /* USER <user>[:<group>] */
    WUBU_INST_EXPOSE      = 10,   /* EXPOSE <port>[/<protocol>] ... */
    WUBU_INST_VOLUME      = 11,   /* VOLUME ["/path1", "/path2", ...] */
    WUBU_INST_LABEL       = 12,   /* LABEL <key>=<value> ... */
    WUBU_INST_ONBUILD     = 13,   /* ONBUILD <INSTRUCTION> */
    WUBU_INST_HEALTHCHECK = 14,   /* HEALTHCHECK [OPTIONS] CMD */
    WUBU_INST_SHELL       = 15,   /* SHELL ["executable", "-c"] */
    WUBU_INST_MOUNT       = 16,   /* MOUNT [--type=bind] <src> <dest> [options] */
    WUBU_INST_DEVICE      = 17,   /* DEVICE <device>[:<mode>] */
    WUBU_INST_SECURITY    = 18,   /* SECURITY <opt>=<val> (seccomp/apparmor/selinux) */
    WUBU_INST_STOP_SIGNAL = 19,   /* STOPSIGNAL <signal> */
    WUBU_INST_ARCH        = 20,   /* ARCH <arch> */
    WUBU_INST_OS          = 21,   /* OS <os> */
} WubuInstType;

/* -- Architecture / OS -------------------------------------------- */

typedef enum {
    WUBU_ARCH_X86_64   = 0,
    WUBU_ARCH_AARCH64  = 1,
    WUBU_ARCH_RISCV64  = 2,
    WUBU_ARCH_WASM     = 3,
} WubuArch;

typedef enum {
    WUBU_OS_LINUX   = 0,
    WUBU_OS_WINDOWS = 1,
    WUBU_OS_MACOS   = 2,
    WUBU_OS_NATIVE  = 3,
} WubuOS;

/* -- Instruction -------------------------------------------------- */

typedef struct {
    WubuInstType     type;
    char             args[WUBU_MAX_CMD_LEN];    /* Raw arguments */
    char             original[WUBU_MAX_CMD_LEN]; /* Original line */
    int              line_num;
    bool             has_json_form;             /* true for [\"exe\", \"arg\"] form */
    int              stage;                     /* Build stage index */
} WubuInstruction;

/* -- Build Stage -------------------------------------------------- */

typedef struct {
    char             name[64];                  /* AS name */
    char             base_image[128];           /* FROM image */
    char             base_tag[32];
    WubuArch         arch;
    WubuOS           os;
    WubuInstruction  insts[WUBU_MAX_INSTRUCTIONS];
    int              inst_count;
    char             envs[WUBU_MAX_ENVS][128];  /* KEY=VALUE */
    int              env_count;
    char             args[WUBU_MAX_ARGS][128];  /* NAME=VALUE */
    int              arg_count;
    int              ports[WUBU_MAX_PORTS];
    int              port_count;
    char             volumes[WUBU_MAX_VOLUMES][256];
    int              volume_count;
    char             labels[WUBU_MAX_LABELS][128];
    int              label_count;
    char             entrypoint[WUBU_MAX_CMD_LEN];
    char             cmd[WUBU_MAX_CMD_LEN];
    char             workdir[256];
    char             user[64];
    char             group[64];
    char             shell[128];
    int              devices[WUBU_MAX_DEVICES];
    int              device_count;
    char             security_opts[256];
    int              stop_signal;
    bool             has_healthcheck;
    char             healthcheck_cmd[WUBU_MAX_CMD_LEN];
    int              healthcheck_interval;      /* seconds */
    int              healthcheck_timeout;
    int              healthcheck_retries;
    int              healthcheck_start_period;
} WubuStage;

/* -- Layer -------------------------------------------------------- */

typedef struct {
    char             digest[WUBU_LAYER_DIGEST_LEN];  /* SHA256 */
    char             parent_digest[WUBU_LAYER_DIGEST_LEN];
    uint64_t         size;
    time_t           created;
    char             instruction[WUBU_MAX_CMD_LEN];   /* Instruction that created this layer */
    /* Actual layer data stored in content-addressable store */
} WubuLayer;

/* -- Image Manifest ----------------------------------------------- */

typedef struct {
    char             image_id[WUBU_IMAGE_ID_LEN];
    char             name[128];
    char             tag[32];
    WubuArch         arch;
    WubuOS           os;
    WubuLayer        layers[WUBU_MAX_LAYERS];
    int              layer_count;
    char             config_digest[WUBU_LAYER_DIGEST_LEN];
    time_t           created;
    char             author[128];
    /* Runtime config */
    char             entrypoint[WUBU_MAX_CMD_LEN];
    char             cmd[WUBU_MAX_CMD_LEN];
    char             workdir[256];
    char             user[64];
    char             envs[WUBU_MAX_ENVS][128];
    int              env_count;
    int              ports[WUBU_MAX_PORTS];
    int              port_count;
    char             volumes[WUBU_MAX_VOLUMES][256];
    int              volume_count;
    char             labels[WUBU_MAX_LABELS][128];
    int              label_count;
    char             shell[128];
    int              stop_signal;
    char             security_opts[256];
} WubuImageManifest;

/* -- Build Context ------------------------------------------------ */

typedef struct {
    char             context_path[WUBU_MAX_PATH];
    char             wubufile_path[WUBU_MAX_PATH];
    char             ignore_path[WUBU_MAX_PATH];
    char             ignore_patterns[128][256];
    int              ignore_count;
    /* Parsed stages */
    WubuStage        stages[WUBU_MAX_STAGES];
    int              stage_count;
    int              target_stage;          /* -1 = all */
    /* Build args */
    char             build_args[WUBU_MAX_ARGS][128];
    int              build_arg_count;
    /* Cache */
    char             cache_dir[WUBU_MAX_PATH];
    bool             no_cache;
    bool             pull_base;
    /* Output */
    char             output_path[WUBU_MAX_PATH];
    char             output_tag[128];
    /* Network */
    char             network_mode[64];      /* host, bridge, none, container:<name> */
    /* Progress callback */
    void (*progress_cb)(const char *stage, const char *instruction, int current, int total, void *user_data);
    void             *progress_user_data;
} WubuBuildContext;

/* -- Public API --------------------------------------------------- */

/* Parse WuBuFile */
int  wubu_parse_wubufile(const char *path, WubuBuildContext *ctx);
int  wubu_parse_wubufile_str(const char *content, WubuBuildContext *ctx);

/* Build image from context */
int  wubu_image_build(WubuBuildContext *ctx, WubuImageManifest *out_manifest);

/* Export image as .wubu container */
int  wubu_image_export_wubu(const WubuImageManifest *manifest, const char *output_path);

/* Export image as OCI tarball */
int  wubu_image_export_oci(const WubuImageManifest *manifest, const char *output_tar);

/* Import image from .wubu container */
int  wubu_image_import_wubu(const char *wubu_path, WubuImageManifest *out_manifest);

/* Import image from OCI tarball */
int  wubu_image_import_oci(const char *oci_tar, WubuImageManifest *out_manifest);

/* Load base image from package manager (pacman) */
int  wubu_image_load_base_arch(const char *pkg_name, const char *version, WubuStage *stage);

/* Load Steam Runtime base image */
int  wubu_image_load_steam_runtime(WubuArch arch, WubuStage *stage);

/* Load Proton base image */
int  wubu_image_load_proton(const char *proton_version, WubuStage *stage);

/* Layer cache operations */
int  wubu_layer_cache_get(const char *digest, void *out_data, size_t *out_size);
int  wubu_layer_cache_put(const char *digest, const void *data, size_t size);
bool wubu_layer_cache_exists(const char *digest);

/* Image manifest operations */
int  wubu_manifest_compute_id(WubuImageManifest *manifest);  /* Computes image_id from layers */
int  wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size);
int  wubu_manifest_from_json(const char *json, WubuImageManifest *manifest);
int  wubu_manifest_save(const WubuImageManifest *manifest, const char *path);
int  wubu_manifest_load(const char *path, WubuImageManifest *manifest);

/* Tag management */
int  wubu_image_tag(const char *image_id, const char *tag);
int  wubu_image_untag(const char *tag);
int  wubu_image_list(char images[][128], int max);

/* Remove image and layers (garbage collection) */
int  wubu_image_remove(const char *image_id, bool force);
int  wubu_image_prune(void);    /* Remove dangling layers */

/* Inspect */
int  wubu_image_inspect(const char *image_ref, WubuImageManifest *out_manifest);
int  wubu_image_history(const char *image_ref, WubuLayer layers[], int max_layers);

/* Push/Pull to/from registry (stub for now) */
int  wubu_image_push(const char *image_ref, const char *registry, const char *auth);
int  wubu_image_pull(const char *image_ref, const char *registry, const char *auth, WubuImageManifest *out_manifest);

/* Helpers */
const char *wubu_arch_name(WubuArch arch);
const char *wubu_os_name(WubuOS os);
WubuArch wubu_arch_from_string(const char *str);
WubuOS wubu_os_from_string(const char *str);

/* Cleanup */
void wubu_build_context_free(WubuBuildContext *ctx);

#endif /* WUBU_IMAGE_H */
