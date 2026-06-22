/*
 * wubu_snapshot.h  --  WuBuOS Container Snapshot Manager
 *
 * Phase 7: Snapshotting with:
 *   - Copy-on-write (CoW) filesystem snapshots (overlayfs/btrfs/zfs)
 *   - Incremental snapshots with deduplication
 *   - Snapshot branching and tagging
 *   - Export/import snapshots as .wubu packages
 *   - Time-travel debugging (restore to any snapshot)
 *   - Automated snapshot policies (on commit, on timer, on signal)
 *   - Snapshot garbage collection with retention policies
 */

#ifndef WUBU_SNAPSHOT_H
#define WUBU_SNAPSHOT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- Limits ------------------------------------------------------- */

#define WUBU_MAX_SNAPSHOTS       1024
#define WUBU_MAX_BRANCHES        64
#define WUBU_MAX_TAGS            256
#define WUBU_MAX_RETENTION_RULES 32
#define WUBU_SNAPSHOT_ID_LEN     64      /* SHA256 hex */
#define WUBU_MAX_PATH            4096
#define WUBU_MAX_LABEL           128
#define WUBU_MAX_DESCRIPTION     1024

/* -- Snapshot Types ----------------------------------------------- */

typedef enum {
    WUBU_SNAP_TYPE_FULL      = 0,    /* Full filesystem snapshot */
    WUBU_SNAP_TYPE_INCREMENTAL = 1,  /* Incremental (diff from parent) */
    WUBU_SNAP_TYPE_DELTA     = 2,    /* Delta from previous snapshot */
    WUBU_SNAP_TYPE_BASE      = 3,    /* Base layer (immutable) */
} WubuSnapshotType;

/* -- Snapshot Status ---------------------------------------------- */

typedef enum {
    WUBU_SNAP_STATUS_CREATING  = 0,
    WUBU_SNAP_STATUS_READY     = 1,
    WUBU_SNAP_STATUS_MOUNTED   = 2,
    WUBU_SNAP_STATUS_ERROR     = 3,
    WUBU_SNAP_STATUS_DELETING  = 4,
    WUBU_SNAP_STATUS_MERGING   = 5,
} WubuSnapshotStatus;

/* -- Filesystem Types --------------------------------------------- */

typedef enum {
    WUBU_FS_OVERLAYFS   = 0,    /* overlayfs (default, works everywhere) */
    WUBU_FS_BTRFS       = 1,    /* btrfs native snapshots */
    WUBU_FS_ZFS         = 2,    /* ZFS native snapshots */
    WUBU_FS_LVM         = 3,    /* LVM thin provisioning */
    WUBU_FS_AUTO        = 4,    /* Auto-detect best available */
} WubuFsType;

/* -- Snapshot ----------------------------------------------------- */

typedef struct {
    char             id[WUBU_SNAPSHOT_ID_LEN];
    char             parent_id[WUBU_SNAPSHOT_ID_LEN];
    char             branch[WUBU_MAX_LABEL];
    char             tags[WUBU_MAX_TAGS][WUBU_MAX_LABEL];
    int              tag_count;
    
    WubuSnapshotType type;
    WubuSnapshotStatus status;
    WubuFsType       fs_type;
    
    char             container_id[WUBU_SNAPSHOT_ID_LEN];
    char             container_name[128];
    
    /* Filesystem info */
    char             lower_dir[WUBU_MAX_PATH];      /* Base layer */
    char             upper_dir[WUBU_MAX_PATH];      /* RW layer */
    char             work_dir[WUBU_MAX_PATH];       /* overlayfs work dir */
    char             merged_dir[WUBU_MAX_PATH];     /* Mount point */
    
    /* Size info */
    uint64_t         size_bytes;                    /* Total snapshot size */
    uint64_t         unique_bytes;                  /* Unique bytes (not in parent) */
    uint64_t         shared_bytes;                  /* Shared with parent/siblings */
    
    /* Metadata */
    time_t           created;
    time_t           modified;
    time_t           accessed;
    char             label[WUBU_MAX_LABEL];
    char             description[WUBU_MAX_DESCRIPTION];
    char             author[128];
    
    /* Config snapshot */
    char             config_json[4096];             /* Container config at snapshot time */
    
    /* References */
    int              ref_count;                     /* Mount/container references */
    bool             protected;                     /* Protected from GC */
    bool             read_only;                     /* Immutable snapshot */
} WubuSnapshot;

/* -- Branch ------------------------------------------------------- */

typedef struct {
    char             name[WUBU_MAX_LABEL];
    char             head_snapshot_id[WUBU_SNAPSHOT_ID_LEN];
    char             base_snapshot_id[WUBU_SNAPSHOT_ID_LEN];
    time_t           created;
    time_t           updated;
    int              snapshot_count;
    bool             protected;
} WubuBranch;

/* -- Tag ---------------------------------------------------------- */

typedef struct {
    char             name[WUBU_MAX_LABEL];
    char             snapshot_id[WUBU_SNAPSHOT_ID_LEN];
    time_t           created;
    char             message[256];
    bool             annotated;                     /* Annotated tag (like git) */
} WubuTag;

/* -- Retention Policy --------------------------------------------- */

typedef enum {
    WUBU_RETENTION_KEEP_LAST_N        = 0,    /* Keep last N snapshots */
    WUBU_RETENTION_KEEP_DAYS          = 1,    /* Keep snapshots younger than N days */
    WUBU_RETENTION_KEEP_TAGGED        = 2,    /* Keep tagged snapshots */
    WUBU_RETENTION_KEEP_BRANCH_HEAD   = 3,    /* Keep branch heads */
    WUBU_RETENTION_KEEP_PROTECTED     = 4,    /* Keep protected snapshots */
    WUBU_RETENTION_MAX_SIZE           = 5,    /* Keep total size under N bytes */
} WubuRetentionType;

typedef struct {
    WubuRetentionType  type;
    int                value;                   /* N for LAST_N, days for KEEP_DAYS, bytes for MAX_SIZE */
    char               branch[WUBU_MAX_LABEL];  /* Empty = all branches */
    bool               enabled;
} WubuRetentionRule;

/* -- Snapshot Manager --------------------------------------------- */

typedef struct {
    char             root_path[WUBU_MAX_PATH];          /* Snapshot store root */
    WubuFsType       fs_type;
    
    WubuSnapshot     snapshots[WUBU_MAX_SNAPSHOTS];
    int              snapshot_count;
    
    WubuBranch       branches[WUBU_MAX_BRANCHES];
    int              branch_count;
    
    WubuTag          tags[WUBU_MAX_TAGS];
    int              tag_count;
    
    WubuRetentionRule retention_rules[WUBU_MAX_RETENTION_RULES];
    int              retention_rule_count;
    
    /* Auto-snapshot triggers */
    bool             auto_snapshot_on_commit;   /* Snapshot after container commit */
    bool             auto_snapshot_on_stop;     /* Snapshot on container stop */
    bool             auto_snapshot_on_signal;   /* Snapshot on SIGUSR1 */
    int              auto_snapshot_interval;    /* Periodic snapshot interval (seconds) */
    
    /* GC */
    bool             auto_gc;
    int              gc_interval;               /* GC interval (seconds) */
    uint64_t         max_store_size;            /* Max store size (0 = unlimited) */
    
    /* Progress callback */
    void (*progress_cb)(const char *operation, int current, int total, void *user_data);
    void             *progress_user_data;
} WubuSnapshotManager;

/* -- Public API --------------------------------------------------- */

/* Manager lifecycle */
int  wubu_snapshot_manager_init(const char *root_path, WubuSnapshotManager *mgr);
int  wubu_snapshot_manager_shutdown(WubuSnapshotManager *mgr);
int  wubu_snapshot_manager_set_fs_type(WubuSnapshotManager *mgr, WubuFsType fs_type);

/* Snapshot operations */
int  wubu_snapshot_create(WubuSnapshotManager *mgr, const char *container_id, const char *base_snapshot_id,
                          const char *branch, const char *label, const char *description,
                          WubuSnapshot **out_snapshot);
int  wubu_snapshot_create_incremental(WubuSnapshotManager *mgr, const char *parent_id,
                                      const char *branch, const char *label,
                                      WubuSnapshot **out_snapshot);
int  wubu_snapshot_delete(WubuSnapshotManager *mgr, const char *snapshot_id, bool force);
int  wubu_snapshot_mount(WubuSnapshotManager *mgr, const char *snapshot_id, const char *mount_point, bool read_only);
int  wubu_snapshot_unmount(WubuSnapshotManager *mgr, const char *snapshot_id);
int  wubu_snapshot_list(WubuSnapshotManager *mgr, WubuSnapshot *out_snapshots, int max,
                        const char *branch_filter, bool include_hidden);

/* Branch operations */
int  wubu_branch_create(WubuSnapshotManager *mgr, const char *name, const char *base_snapshot_id);
int  wubu_branch_delete(WubuSnapshotManager *mgr, const char *name, bool force);
int  wubu_branch_switch(WubuSnapshotManager *mgr, const char *name);
int  wubu_branch_merge(WubuSnapshotManager *mgr, const char *from_branch, const char *into_branch,
                       const char *merge_message, WubuSnapshot **out_snapshot);
int  wubu_branch_list(WubuSnapshotManager *mgr, WubuBranch *out_branches, int max);

/* Tag operations */
int  wubu_tag_create(WubuSnapshotManager *mgr, const char *name, const char *snapshot_id,
                     const char *message, bool annotated);
int  wubu_tag_delete(WubuSnapshotManager *mgr, const char *name);
int  wubu_tag_list(WubuSnapshotManager *mgr, WubuTag *out_tags, int max);

/* Export/Import */
int  wubu_snapshot_export(const WubuSnapshotManager *mgr, const char *snapshot_id,
                          const char *output_path, bool include_config);
int  wubu_snapshot_import(WubuSnapshotManager *mgr, const char *input_path,
                          const char *branch, const char *tag,
                          WubuSnapshot **out_snapshot);

/* Diff */
int  wubu_snapshot_diff(WubuSnapshotManager *mgr, const char *snapshot_id1,
                        const char *snapshot_id2, char *out_diff, size_t out_size);

/* Rollback / Restore */
int  wubu_snapshot_rollback(WubuSnapshotManager *mgr, const char *snapshot_id);
int  wubu_snapshot_restore_as_new(WubuSnapshotManager *mgr, const char *snapshot_id,
                                  const char *new_container_name, char *out_container_id);

/* Garbage Collection */
int  wubu_snapshot_gc(WubuSnapshotManager *mgr);
int  wubu_snapshot_gc_add_rule(WubuSnapshotManager *mgr, const WubuRetentionRule *rule);
int  wubu_snapshot_gc_remove_rule(WubuSnapshotManager *mgr, int rule_index);
int  wubu_snapshot_gc_list_rules(WubuSnapshotManager *mgr, WubuRetentionRule *out_rules, int max);

/* Inspect */
int  wubu_snapshot_inspect(const WubuSnapshotManager *mgr, const char *snapshot_id,
                           WubuSnapshot *out_snapshot) ;
int  wubu_snapshot_tree(const WubuSnapshotManager *mgr, const char *snapshot_id,
                        char tree[][256], int max_depth);

/* Helpers */
const char *wubu_snapshot_status_str(WubuSnapshotStatus status);
const char *wubu_snapshot_type_str(WubuSnapshotType type);
const char *wubu_fs_type_str(WubuFsType type);

/* Cleanup */
void wubu_snapshot_manager_free(WubuSnapshotManager *mgr);

#endif /* WUBU_SNAPSHOT_H */
