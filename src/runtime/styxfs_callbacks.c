/*
 * styxfs_callbacks.c -- StyxFS 9P2000/Styx callback handlers.
 * Extracted from the monolithic styxfs.c. Depends on styxfs_internal.h for
 * the shared file-table + server helpers. C11, no god headers.
 */
#define _GNU_SOURCE
#include "styxfs.h"
#include "styxfs_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
/* -- Callbacks ------------------------------------------------ */

/* Helper: get styxfs_server_t from base */
styxfs_server_t *styxfs_get_server(styx_server_t *base) {
    /* base is the first member of styxfs_server_t */
    return (styxfs_server_t *)base;
}

/* Helper: resolve a fid to its path */
const char *styxfs_fid_to_path(styxfs_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid)
            return srv->open_files[i].path;
    }
    return "/";
}

/* Helper: build a full path from a base path and walk name */

int styxfs_attach_cb(styx_server_t *base, uint32_t fid, const char *aname) {
    styxfs_server_t *srv = styxfs_get_server(base);
    (void)aname;
    /* Attach: associate fid with root directory */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && 
            srv->open_files[i].qid_path == (uint64_t)fid) {
            /* Already allocated by base Styx */
            return 0;
        }
    }
    /* Allocate if not already done by base */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            srv->open_files[i].qid_path = fid;
            srv->open_files[i].qid_type = STX_QTDIR;
            srv->open_files[i].qid_version = 1;
            strcpy(srv->open_files[i].path, "/");
            return 0;
        }
    }
    return -1;
}

int styxfs_walk_cb(styx_server_t *base, uint32_t fid, uint32_t newfid,
                    const char **wname, int nwname,
                    styx_qid_t *qids, int *nwqid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    *nwqid = 0;
    
    if (nwname == 0) {
        /* Clone fid: newfid gets same path */
        const char *src_path = styxfs_fid_to_path(srv, fid);
        for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
            if (!srv->open_files[i].in_use) {
                memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
                srv->open_files[i].in_use = 1;
                srv->open_files[i].qid_path = newfid;
                srv->open_files[i].qid_type = STX_QTDIR;
                srv->open_files[i].qid_version = 1;
                strncpy(srv->open_files[i].path, src_path, STYXFS_MAX_PATH - 1);
                qids[0].type = STX_QTDIR;
                qids[0].version = 1;
                qids[0].path = (uint64_t)newfid;
                *nwqid = 1;
                return 0;
            }
        }
        return -1;
    }
    
    char cur_path_buf[STYXFS_MAX_PATH];
    strncpy(cur_path_buf, styxfs_fid_to_path(srv, fid), STYXFS_MAX_PATH - 1);
    
    for (int i = 0; i < nwname && i < 16; i++) {
        char new_path[STYXFS_MAX_PATH];
        build_path(new_path, sizeof(new_path), cur_path_buf, wname[i]);
        
        /* Check if this path exists (is a mount point or under one) */
        int exists = path_is_mounted(srv, new_path);
        
        /* Also check if it's the root or a direct mount */
        if (!exists) {
            for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
                if (strcmp(m->path, new_path) == 0) { exists = 1; break; }
            }
        }
        /* Root always exists */
        if (strcmp(new_path, "/") == 0) exists = 1;
        
        if (!exists) {
            /* Path doesn't exist  --  stop walk here */
            return 0;
        }
        
        /* Set QID for this step */
        qids[i].type = path_is_mounted(srv, new_path) ? STX_QTDIR : STX_QTFILE;
        qids[i].version = 1;
        qids[i].path = (uint64_t)(*nwqid + 1);
        (*nwqid)++;
        
        /* Update current path for next component */
        strncpy(cur_path_buf, new_path, STYXFS_MAX_PATH - 1);
    }
    
    /* Allocate newfid if walk succeeded */
    if (*nwqid > 0 && newfid != fid) {
        for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
            if (!srv->open_files[i].in_use) {
                memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
                srv->open_files[i].in_use = 1;
                srv->open_files[i].qid_path = newfid;
                srv->open_files[i].qid_type = (nwname == (*nwqid)) ? STX_QTDIR : STX_QTFILE;
                srv->open_files[i].qid_version = 1;
                /* Build final path */
                char final_path[STYXFS_MAX_PATH];
                strncpy(final_path, styxfs_fid_to_path(srv, fid), STYXFS_MAX_PATH - 1);
                for (int j = 0; j < nwname && j < *nwqid; j++) {
                    char tmp[STYXFS_MAX_PATH];
                    build_path(tmp, sizeof(tmp), final_path, wname[j]);
                    strncpy(final_path, tmp, STYXFS_MAX_PATH - 1);
                }
                strncpy(srv->open_files[i].path, final_path, STYXFS_MAX_PATH - 1);
                break;
            }
        }
    }
    
    return 0;
}

int styxfs_open_cb(styx_server_t *base, uint32_t fid, int mode,
                    styx_qid_t *qid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            srv->open_files[i].mode = mode;
            qid->type = srv->open_files[i].qid_type;
            qid->version = srv->open_files[i].qid_version;
            qid->path = srv->open_files[i].qid_path;
            return 0;
        }
    }
    return -1;
}

int styxfs_create_cb(styx_server_t *base, uint32_t fid, const char *name,
                      uint32_t perm, int mode, styx_qid_t *qid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    (void)mode;
    
    if (srv->readonly) return -1;
    
    const char *parent_path = styxfs_fid_to_path(srv, fid);
    char new_path[STYXFS_MAX_PATH];
    build_path(new_path, sizeof(new_path), parent_path, name);
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            srv->open_files[i].qid_path = styxfs_next_qid_path(srv);
            srv->open_files[i].qid_type = (perm & 0x80000000) ? STX_QTDIR : STX_QTFILE;
            srv->open_files[i].qid_version = 1;
            strncpy(srv->open_files[i].path, new_path, STYXFS_MAX_PATH - 1);
            srv->open_files[i].mode = 1;
            
            qid->type = srv->open_files[i].qid_type;
            qid->version = 1;
            qid->path = srv->open_files[i].qid_path;
            return 0;
        }
    }
    return -1;
}

int styxfs_read_cb(styx_server_t *base, uint32_t fid,
                    uint64_t offset, uint32_t count,
                    uint8_t *data, uint32_t *nread) {
    styxfs_server_t *srv = styxfs_get_server(base);
    *nread = 0;
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            if (srv->open_files[i].qid_type == STX_QTDIR) {
                /* Read directory: return stat entries for children */
                const char *path = srv->open_files[i].path;
                
                /* Simple directory listing: iterate mounts */
                uint32_t buf_off = 0;
                uint64_t dir_offset = offset;
                int entry_count = 0;
                
                for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
                    size_t plen = strlen(path);
                    if (strcmp(path, "/") == 0) {
                        /* Root: list top-level mount points */
                        const char *slash = strchr(m->path + 1, '/');
                        int name_len = slash ? (int)(slash - (m->path + 1)) : (int)strlen(m->path + 1);
                        
                        /* Skip entries before offset */
                        if ((uint64_t)entry_count < dir_offset) { entry_count++; continue; }
                        
                        /* Build stat entry */
                        char entry_name[256];
                        strncpy(entry_name, m->path + 1, (size_t)name_len);
                        entry_name[name_len] = '\0';
                        
                        /* Skip duplicates */
                        int dup = 0;
                        for (styxfs_mount_t *p = srv->mounts; p != m; p = p->next) {
                            const char *pslash = strchr(p->path + 1, '/');
                            int plen2 = pslash ? (int)(pslash - (p->path + 1)) : (int)strlen(p->path + 1);
                            if (plen2 == name_len && strncmp(p->path + 1, entry_name, (size_t)name_len) == 0) {
                                dup = 1; break;
                            }
                        }
                        if (dup) { entry_count++; continue; }
                        
                        /* Check if we have room */
                        uint32_t need = (uint32_t)(2 + 4 + 8 + 4 + 4 + 4 + 8 + 2 + strlen(entry_name));
                        if (buf_off + need > count) break;
                        
                        /* Build 9P stat structure */
                        styx_dir_t dir;
                        memset(&dir, 0, sizeof(dir));
                        dir.type = 0;
                        dir.dev = 0;
                        dir.qid.type = STX_QTDIR;
                        dir.qid.version = 1;
                        dir.qid.path = (uint64_t)(entry_count + 100);
                        dir.mode = 040755;
                        dir.atime = (uint32_t)time(NULL);
                        dir.mtime = dir.atime;
                        dir.length = 0;
                        strncpy(dir.name, entry_name, sizeof(dir.name) - 1);
                        strcpy(dir.uid, "wubu");
                        strcpy(dir.gid, "wubu");
                        strcpy(dir.muid, "wubu");
                        
                        /* Pack stat into data buffer */
                        uint32_t dir_size_pos = buf_off;
                        buf_off += 2; /* size field, filled later */
                        uint32_t dir_start = buf_off;
                        styx_put16(data + buf_off, dir.type); buf_off += 2;
                        styx_put32(data + buf_off, dir.dev); buf_off += 4;
                        data[buf_off++] = dir.qid.type;
                        styx_put32(data + buf_off, dir.qid.version); buf_off += 4;
                        styx_put64(data + buf_off, dir.qid.path); buf_off += 8;
                        styx_put32(data + buf_off, dir.mode); buf_off += 4;
                        styx_put32(data + buf_off, dir.atime); buf_off += 4;
                        styx_put32(data + buf_off, dir.mtime); buf_off += 4;
                        styx_put64(data + buf_off, dir.length); buf_off += 8;
                        buf_off += styx_putstr(data + buf_off, dir.name);
                        buf_off += styx_putstr(data + buf_off, dir.uid);
                        buf_off += styx_putstr(data + buf_off, dir.gid);
                        buf_off += styx_putstr(data + buf_off, dir.muid);
                        
                        /* Fill in stat size */
                        uint32_t dir_total = buf_off - dir_start;
                        styx_put16(data + dir_size_pos, (uint16_t)dir_total);
                        
                        entry_count++;
                    }
                }
                
                *nread = buf_off;
            } else {
                /* Read file data */
                if (srv->open_files[i].container_payload && srv->open_files[i].payload_size > 0) {
                    uint64_t file_size = (uint64_t)srv->open_files[i].payload_size;
                    if (offset >= file_size) { *nread = 0; return 0; }
                    uint64_t avail = file_size - offset;
                    uint32_t to_read = (count < avail) ? count : (uint32_t)avail;
                    if (data && to_read > 0) {
                        memcpy(data, srv->open_files[i].container_payload + offset, to_read);
                    }
                    *nread = to_read;
                } else {
                    *nread = 0;
                }
            }
            return 0;
        }
    }
    return -1;
}

int styxfs_write_cb(styx_server_t *base, uint32_t fid,
                     uint64_t offset, uint32_t count,
                     const uint8_t *data, uint32_t *nwritten) {
    styxfs_server_t *srv = styxfs_get_server(base);
    *nwritten = 0;
    
    if (srv->readonly) return -1;
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            /* Simple write: extend buffer if needed */
            uint64_t end = offset + count;
            if (end > (uint64_t)srv->open_files[i].write_buf_size) {
                size_t new_size = (size_t)(end + 4096);
                uint8_t *new_buf = (uint8_t *)realloc(srv->open_files[i].write_buf, new_size);
                if (!new_buf) return -1;
                srv->open_files[i].write_buf = new_buf;
                srv->open_files[i].write_buf_size = new_size;
            }
            if (data && count > 0) {
                memcpy(srv->open_files[i].write_buf + offset, data, count);
            }
            srv->open_files[i].write_offset = (size_t)end;
            *nwritten = count;
            return 0;
        }
    }
    return -1;
}

int styxfs_clunk_cb(styx_server_t *base, uint32_t fid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            if (srv->open_files[i].container_payload) free(srv->open_files[i].container_payload);
            if (srv->open_files[i].write_buf) free(srv->open_files[i].write_buf);
            srv->open_files[i].in_use = 0;
            return 0;
        }
    }
    return 0;
}

int styxfs_remove_cb(styx_server_t *base, uint32_t fid) {
    styxfs_server_t *srv = styxfs_get_server(base);
    if (srv->readonly) return -1;
    return styxfs_clunk_cb(base, fid);
}

int styxfs_stat_cb(styx_server_t *base, uint32_t fid,
                    styx_dir_t *dir) {
    styxfs_server_t *srv = styxfs_get_server(base);
    
    memset(dir, 0, sizeof(*dir));
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            dir->type = 0;
            dir->dev = 0;
            dir->qid.type = srv->open_files[i].qid_type;
            dir->qid.version = srv->open_files[i].qid_version;
            dir->qid.path = srv->open_files[i].qid_path;
            
            if (srv->open_files[i].qid_type == STX_QTDIR) {
                dir->mode = 040755;
                dir->length = 0;
            } else {
                dir->mode = 0100644;
                dir->length = (uint64_t)srv->open_files[i].payload_size;
                if (srv->open_files[i].write_buf) {
                    dir->length = (uint64_t)srv->open_files[i].write_offset;
                }
            }
            
            dir->atime = (uint32_t)time(NULL);
            dir->mtime = dir->atime;
            
            /* Extract name from path */
            const char *path = srv->open_files[i].path;
            const char *last_slash = strrchr(path, '/');
            strncpy(dir->name, last_slash ? last_slash + 1 : path, sizeof(dir->name) - 1);
            strcpy(dir->uid, "wubu");
            strcpy(dir->gid, "wubu");
            strcpy(dir->muid, "wubu");

            /* Initialize metadata fields for future wstat */
            if (srv->open_files[i].file_mode == 0) {
                srv->open_files[i].file_mode = (uint16_t)(srv->open_files[i].is_dir ? 040755 : 0100644);
            }
            if (srv->open_files[i].mtime == 0) {
                srv->open_files[i].mtime = dir->atime;
            }
            if (srv->open_files[i].atime == 0) {
                srv->open_files[i].atime = dir->atime;
            }

            return 0;
        }
    }
    return -1;
}

int styxfs_wstat_cb(styx_server_t *base, uint32_t fid,
                     const styx_dir_t *dir) {
    styxfs_server_t *srv = styxfs_get_server(base);
    if (srv->readonly) return -1;

    /* Find the open file by fid (qid_path) */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            /* Apply stat changes from the incoming dir structure */

            /* Update mode if set */
            if (dir->mode != 0) {
                /* Extract permission bits (lower 12 bits) and type bits */
                uint16_t new_perms = (uint16_t)(dir->mode & 0xFFF);
                uint16_t type_bits = (uint16_t)(dir->mode & 0xF000);
                /* Preserve the type bits from the existing mode, update permissions */
                if (srv->open_files[i].is_dir) {
                    srv->open_files[i].file_mode = (uint16_t)(040000 | (new_perms & 0777));
                } else {
                    srv->open_files[i].file_mode = (uint16_t)(0100000 | (new_perms & 0777));
                }
                /* If the type bits changed, update is_dir */
                if ((dir->mode & STX_DMDIR) && !srv->open_files[i].is_dir) {
                    srv->open_files[i].is_dir = true;
                }
            }

            /* Update name if provided (rename) */
            if (dir->name[0] != '\0' && strcmp(dir->name, ".") != 0 &&
                strcmp(dir->name, "..") != 0) {
                /* Extract the directory portion of the path */
                char *path_copy = strdup(srv->open_files[i].path);
                if (path_copy) {
                    char *last_slash = strrchr(path_copy, '/');
                    if (last_slash) {
                        /* Replace the filename portion */
                        *(last_slash + 1) = '\0';
                        size_t remaining = sizeof(srv->open_files[i].path) -
                                           (size_t)(last_slash - path_copy) - 1;
                        strncat(path_copy, dir->name, remaining - 1);
                        strncpy(srv->open_files[i].path, path_copy,
                                sizeof(srv->open_files[i].path) - 1);
                    } else {
                        /* No slash — just replace the whole name */
                        strncpy(srv->open_files[i].path, dir->name,
                                sizeof(srv->open_files[i].path) - 1);
                    }
                    free(path_copy);
                }
            }

            /* Update length if provided (truncate) */
            if (dir->length != 0 || (dir->mode & STX_OTRUNC)) {
                srv->open_files[i].payload_size = (size_t)dir->length;
                srv->open_files[i].write_offset = (size_t)dir->length;
            }

            /* Update mtime/atime */
            uint32_t now = (uint32_t)time(NULL);
            if (dir->mtime != 0) {
                srv->open_files[i].mtime = dir->mtime;
            } else {
                srv->open_files[i].mtime = now;
            }
            if (dir->atime != 0) {
                srv->open_files[i].atime = dir->atime;
            } else {
                srv->open_files[i].atime = now;
            }

            /* Bump qid version to indicate change */
            srv->open_files[i].qid_version++;

            return 0;
        }
    }
    return -1; /* fid not found */
}

/* -- Helper Utilities ----------------------------------------- */



int styxfs_build_dirent(styxfs_server_t *srv, const char *path,
                         uint8_t *buf, uint32_t buf_size, uint32_t *out_size,
                         uint64_t offset, uint32_t count) {
    (void)srv; (void)path; (void)buf; (void)buf_size;
    (void)offset; (void)count;
    if (out_size) *out_size = 0;
    return 0;
}

