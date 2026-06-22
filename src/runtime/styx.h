/*
 * styx.h  --  Styx/9P2000 Protocol (Inferno OS wire format)
 * 
 * Styx is the file protocol used by Inferno OS (derived from Plan 9's 9P2000).
 * Everything is a file: data, devices, networks, services.
 * Fixed-size 24-byte header + variable-length data.
 * 
 * For WuBuOS, Styx provides:
 *   - Universal .wubu namespace (mount points as files)
 *   - VSL container management (process namespace)
 *   - Device access (VBE framebuffer, input, audio as files)
 *   - Package management (Flatpak-style repos as filesystems)
 * 
 * Reference: inferno-os/Inferno/Include/styx.h
 * License: MIT (WuBuOS additions), GPL (derived from Inferno/Plan 9)
 */
#ifndef WUBU_STYX_H
#define WUBU_STYX_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* -- Protocol Constants ------------------------------------------- */

#define STYX_MAX_MSG       (64 * 1024)  /* 64KB max message */
#define STYX_HEADER_SIZE   24            /* Fixed header + type + tag */
#define STYX_MAX_FNAME     256           /* Max filename length */
#define STYX_MAX_WRITE     (8 * 1024)    /* Max write chunk */
#define STYX_MAX_DATA      (8 * 1024)    /* Max read data per Tread/Rread */

/* -- Styx Message Types ------------------------------------------- */

/* T-messages (client → server) */
#define STX_TVERSION  100  /* Negotiate protocol version */
#define STX_TAUTH     102  /* Authentication */
#define STX_TATTACH   104  /* Attach to filesystem */
#define STX_TERROR    106  /* Error (legacy, use Rerror) */
#define STX_TFLUSH    108  /* Flush pending request */
#define STX_TWALK     110  /* Walk directory tree */
#define STX_TOPEN     112  /* Open a file */
#define STX_TCREATE   114  /* Create a file */
#define STX_TREAD     116  /* Read from a file */
#define STX_TWRITE    118  /* Write to a file */
#define STX_TCLUNK    120  /* Close a file */
#define STX_TREMOVE   122  /* Remove a file */
#define STX_TSTAT     124  /* Get file attributes */
#define STX_TWSTAT    126  /* Set file attributes */

/* R-messages (server → client)  --  type + 1 */
#define STX_RVERSION  101
#define STX_RAUTH     103
#define STX_RATTACH   105
#define STX_RERROR    107
#define STX_RFLUSH    109
#define STX_RWALK     111
#define STX_ROPEN     113
#define STX_RCREATE   115
#define STX_RREAD     117
#define STX_RWRITE    119
#define STX_RCLUNK    121
#define STX_RREMOVE   123
#define STX_RSTAT     125
#define STX_RWSTAT    127

/* -- Flags for Topen/Tcreate -------------------------------------- */

#define STX_OREAD   0  /* Open for reading */
#define STX_OWRITE  1  /* Open for writing */
#define STX_ORDWR   2  /* Open for reading and writing */
#define STX_OEXEC   3  /* Open for execution */
#define STX_OTRUNC  16 /* Truncate on open */
#define STX_ORCLOSE 32 /* Remove on close */
#define STX_OEXCL   64 /* Exclusive use */

/* -- QID Types (file type bits) ----------------------------------- */

#define STX_QTDIR   0x80  /* Directory */
#define STX_QTAPPEND 0x40 /* Append-only */
#define STX_QTEXCL  0x20  /* Exclusive use */
#define STX_QTMOUNT 0x10  /* Mount point */
#define STX_QTAUTH  0x08  /* Authentication file */
#define STX_QTTMP   0x04  /* Temporary file */
#define STX_QTFILE  0x00  /* Plain file */

/* -- File Permissions (mode bits, 9P2000) --------------------------- */

#define STX_DMDIR     0x80000000
#define STX_DMAPPEND  0x40000000
#define STX_DMEXCL    0x20000000
#define STX_DMMOUNT   0x10000000
#define STX_DMAUTH    0x08000000
#define STX_DMTMP     0x04000000

/* -- Data Structures ---------------------------------------------- */

/* QID  --  unique file identifier */
typedef struct {
    uint8_t  type;    /* QTDIR, QTAPPEND, etc. */
    uint32_t version; /* Version (incremented on change) */
    uint64_t path;    /* Unique file identifier */
} styx_qid_t;

/* Dir  --  directory entry (stat/wstat) */
typedef struct {
    uint16_t size;    /* Total size of this Dir record */
    uint16_t type;    /* Server type */
    uint32_t dev;     /* Server subtype */
    styx_qid_t qid;   /* File QID */
    uint32_t mode;    /* Permissions + flags */
    uint32_t atime;   /* Last access time */
    uint32_t mtime;   /* Last modification time */
    uint64_t length;  /* File length */
    char     name[STYX_MAX_FNAME];  /* File name */
    char     uid[STYX_MAX_FNAME];   /* Owner name */
    char     gid[STYX_MAX_FNAME];   /* Group name */
    char     muid[STYX_MAX_FNAME];  /* Last modifier */
} styx_dir_t;

/* Styx message (raw wire format, 24-byte header + payload) */
typedef struct {
    uint32_t size;    /* Total message size (header + data) */
    uint8_t  type;    /* Message type (TVERSION..TWSTAT) */
    uint16_t tag;     /* Transaction tag (echoed in response) */
    uint8_t  data[STYX_MAX_MSG - 7];  /* Variable-length payload */
} styx_msg_t;

/* -- Connection / Fid State --------------------------------------- */

#define STYX_MAX_FIDS 128

/* File identifier state */
typedef struct styx_fid {
    int      in_use;
    uint32_t fid;              /* Fid number */
    uint32_t open_fid;         /* Opened fid (if open) */
    int      open_mode;        /* OREAD/OWRITE/ORDWR */
    uint64_t offset;           /* Current read/write offset */
    styx_qid_t qid;            /* QID of opened file */
    void    *file_state;       /* Per-fid file state (server-specific) */
} styx_fid_t;

/* Styx server state */
typedef struct styx_server {
    styx_fid_t fids[STYX_MAX_FIDS];
    uint16_t next_tag;
    uint32_t msize;            /* Maximum message size negotiated */
    char     version[16];      /* Protocol version string */
    int      connected;
    
    /* Callbacks  --  file system operations */
    int  (*attach)(struct styx_server *srv, uint32_t fid, const char *aname);
    int  (*walk) (struct styx_server *srv, uint32_t fid,
                  uint32_t newfid, const char **wname, int nwname,
                  styx_qid_t *qids, int *nwqid);
    int  (*open) (struct styx_server *srv, uint32_t fid, int mode,
                  styx_qid_t *qid);
    int  (*read) (struct styx_server *srv, uint32_t fid,
                  uint64_t offset, uint32_t count,
                  uint8_t *data, uint32_t *nread);
    int  (*write)(struct styx_server *srv, uint32_t fid,
                  uint64_t offset, uint32_t count,
                  const uint8_t *data, uint32_t *nwritten);
    int  (*clunk)(struct styx_server *srv, uint32_t fid);
    int  (*remove)(struct styx_server *srv, uint32_t fid);
    int  (*stat) (struct styx_server *srv, uint32_t fid,
                  styx_dir_t *dir);
    int  (*wstat)(struct styx_server *srv, uint32_t fid,
                  const styx_dir_t *dir);
    
    /* Server-specific data */
    void *user_data;
} styx_server_t;

/* -- API Functions ------------------------------------------------ */

/* Initialize a Styx server with default state */
void styx_init(styx_server_t *srv);

/* Process an incoming Styx message and produce a response */
int styx_serve(styx_server_t *srv,
               const uint8_t *inbuf, uint32_t inlen,
               uint8_t *outbuf, uint32_t *outlen);

/* Create a Styx error response */
void styx_error(uint16_t tag, const char *errmsg,
                uint8_t *outbuf, uint32_t *outlen);

/* -- Fid Management (public API) ---------------------------------- */

styx_fid_t *styx_fid_alloc(styx_server_t *srv, uint32_t fid);
styx_fid_t *styx_fid_lookup(styx_server_t *srv, uint32_t fid);
void styx_fid_free(styx_server_t *srv, uint32_t fid);

/* -- Message Construction Helpers --------------------------------- */

void styx_build_tversion(uint8_t *buf, uint32_t *len,
                          uint32_t msize, const char *version);
void styx_build_tattach(uint8_t *buf, uint32_t *len,
                         uint16_t tag, uint32_t fid,
                         uint32_t afid, const char *aname);
void styx_build_twalk(uint8_t *buf, uint32_t *len,
                       uint16_t tag, uint32_t fid,
                       uint32_t newfid, const char **wname, int nwname);
void styx_build_topen(uint8_t *buf, uint32_t *len,
                       uint16_t tag, uint32_t fid, int mode);
void styx_build_tread(uint8_t *buf, uint32_t *len,
                       uint16_t tag, uint32_t fid,
                       uint64_t offset, uint32_t count);
void styx_build_twrite(uint8_t *buf, uint32_t *len,
                        uint16_t tag, uint32_t fid,
                        uint64_t offset, uint32_t count,
                        const uint8_t *data);
void styx_build_tclunk(uint8_t *buf, uint32_t *len,
                        uint16_t tag, uint32_t fid);
void styx_build_tstat(uint8_t *buf, uint32_t *len,
                       uint16_t tag, uint32_t fid);

/* -- Response Parsing Helpers ------------------------------------- */

int styx_parse_version(const uint8_t *buf, uint32_t len,
                        uint32_t *msize, char *version);
int styx_parse_attach(const uint8_t *buf, uint32_t len,
                       uint32_t *fid, uint32_t *afid, char *aname);
int styx_parse_walk(const uint8_t *buf, uint32_t len,
                     uint32_t *fid, uint32_t *newfid,
                     char wnames[][STYX_MAX_FNAME], int *nwname);
int styx_parse_open(const uint8_t *buf, uint32_t len,
                     uint32_t *fid, int *mode);
int styx_parse_read(const uint8_t *buf, uint32_t len,
                     uint32_t *fid, uint64_t *offset, uint32_t *count);
int styx_parse_write(const uint8_t *buf, uint32_t len,
                      uint32_t *fid, uint64_t *offset,
                      uint32_t *count, const uint8_t **data);
int styx_parse_clunk(const uint8_t *buf, uint32_t len, uint32_t *fid);
int styx_parse_stat(const uint8_t *buf, uint32_t len, uint32_t *fid);

/* -- Helper Utilities --------------------------------------------- */

/* Pack/unpack little-endian 16-bit */
static inline void styx_put16(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}
static inline uint16_t styx_get16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/* Pack/unpack little-endian 32-bit */
static inline void styx_put32(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}
static inline uint32_t styx_get32(const uint8_t *buf) {
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* Pack/unpack little-endian 64-bit */
static inline void styx_put64(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)(val & 0xFF);
        val >>= 8;
    }
}
static inline uint64_t styx_get64(const uint8_t *buf) {
    uint64_t val = 0;
    for (int i = 7; i >= 0; i--) {
        val <<= 8;
        val |= buf[i];
    }
    return val;
}

/* Copy string with null-termination, return bytes written */
static inline int styx_putstr(uint8_t *buf, const char *s) {
    uint16_t n = (uint16_t)(s ? strlen(s) : 0);
    if (n > STYX_MAX_FNAME - 1) n = STYX_MAX_FNAME - 1;
    styx_put16(buf, n);
    if (n > 0) memcpy(buf + 2, s, n);
    return 2 + n;
}

/* Read string; returns pointer past string or NULL on error */
static inline const uint8_t *styx_getstr(const uint8_t *buf,
                                          char *out, int outsz) {
    uint16_t n = styx_get16(buf);
    if (n >= (uint16_t)outsz) n = (uint16_t)(outsz - 1);
    memcpy(out, buf + 2, n);
    out[n] = '\0';
    return buf + 2 + n;
}

/* Get message name for debugging */
const char *styx_msg_name(uint8_t type);

#endif /* WUBU_STYX_H */
