/*
 * fw_fsproto.c  --  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL / EFI_FILE_PROTOCOL
 *                   bound to a fw_volume.
 *
 * Read-only: Write/Delete/SetInfo return EFI_WRITE_PROTECTED rather than
 * pretending to succeed. A boot loader only needs Open/Read/SetPosition/
 * GetInfo, all of which are real here.
 */

#include "fw.h"

#define MAX_FILES 16

typedef struct {
    EFI_FILE_PROTOCOL proto;      /* must be first                     */
    int        used;
    fw_volume *vol;
    char       path[256];
    uint8_t   *data;              /* whole-file cache                  */
    uint64_t   size;
    uint64_t   pos;
    size_t     pages;
    int        is_dir;
} fw_file;

typedef struct {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL proto;
    fw_volume *vol;
} fw_fs;

static fw_file g_files[MAX_FILES];
static fw_fs   g_fs[8];
static int     g_nfs;

static fw_file *file_alloc(void) {
    for (int i = 0; i < MAX_FILES; i++)
        if (!g_files[i].used) { fw_memset(&g_files[i], 0, sizeof(fw_file)); return &g_files[i]; }
    return NULL;
}

/* CHAR16 path -> ASCII, normalising separators. */
static void path16_to_ascii(const CHAR16 *in, char *out, size_t max) {
    size_t n = 0;
    for (; in && *in && n + 1 < max; in++) {
        char c = (*in < 0x80) ? (char)*in : '_';
        out[n++] = (c == '/') ? '\\' : c;
    }
    out[n] = 0;
}

static void path_join(const char *base, const char *rel, char *out, size_t max) {
    if (rel[0] == '\\') { 
        size_t n = 0;
        while (rel[n] && n + 1 < max) { out[n] = rel[n]; n++; }
        out[n] = 0;
        return;
    }
    size_t n = 0;
    while (base[n] && n + 1 < max) { out[n] = base[n]; n++; }
    if (n && out[n - 1] != '\\' && n + 1 < max) out[n++] = '\\';
    for (size_t i = 0; rel[i] && n + 1 < max; i++) out[n++] = rel[i];
    out[n] = 0;
}

static EFI_STATUS EFIAPI file_close(EFI_FILE_PROTOCOL *This);
static EFI_STATUS EFIAPI file_open(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **New,
                                   CHAR16 *Name, UINT64 Mode, UINT64 Attr);

static EFI_STATUS EFIAPI file_read(EFI_FILE_PROTOCOL *This, UINTN *size, VOID *buf) {
    fw_file *f = (fw_file *)This;
    if (!f || !f->used || !size) return EFI_INVALID_PARAMETER;
    if (f->is_dir) { *size = 0; return EFI_SUCCESS; }   /* dir enumeration unsupported */
    uint64_t left = (f->pos < f->size) ? (f->size - f->pos) : 0;
    UINTN n = (*size < left) ? *size : (UINTN)left;
    if (n && buf) fw_memcpy(buf, f->data + f->pos, n);
    f->pos += n;
    *size = n;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_write(EFI_FILE_PROTOCOL *This, UINTN *size, VOID *buf) {
    (void)This; (void)buf;
    if (size) *size = 0;
    return EFI_WRITE_PROTECTED;
}

static EFI_STATUS EFIAPI file_getpos(EFI_FILE_PROTOCOL *This, UINT64 *pos) {
    fw_file *f = (fw_file *)This;
    if (!f || !f->used || !pos) return EFI_INVALID_PARAMETER;
    *pos = f->pos;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_setpos(EFI_FILE_PROTOCOL *This, UINT64 pos) {
    fw_file *f = (fw_file *)This;
    if (!f || !f->used) return EFI_INVALID_PARAMETER;
    if (pos == 0xFFFFFFFFFFFFFFFFULL) { f->pos = f->size; return EFI_SUCCESS; }
    if (pos > f->size) return EFI_UNSUPPORTED;
    f->pos = pos;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_getinfo(EFI_FILE_PROTOCOL *This, EFI_GUID *type,
                                      UINTN *size, VOID *buf) {
    fw_file *f = (fw_file *)This;
    if (!f || !f->used || !type || !size) return EFI_INVALID_PARAMETER;
    if (fw_memcmp(type, &gEfiFileInfoGuid, sizeof(EFI_GUID)) != 0) return EFI_UNSUPPORTED;

    /* File name = last component of the stored path. */
    const char *base = f->path;
    for (const char *p = f->path; *p; p++) if (*p == '\\') base = p + 1;
    size_t nlen = fw_strlen(base);

    UINTN need = sizeof(EFI_FILE_INFO) + (nlen + 1) * sizeof(CHAR16);
    if (*size < need || !buf) { *size = need; return EFI_BUFFER_TOO_SMALL; }

    EFI_FILE_INFO *fi = (EFI_FILE_INFO *)buf;
    fw_memset(fi, 0, need);
    fi->Size         = need;
    fi->FileSize     = f->size;
    fi->PhysicalSize = f->size;
    fi->Attribute    = f->is_dir ? EFI_FILE_DIRECTORY : EFI_FILE_READ_ONLY;
    fw_rtc_read(&fi->CreateTime);
    fi->LastAccessTime   = fi->CreateTime;
    fi->ModificationTime = fi->CreateTime;
    for (size_t i = 0; i < nlen; i++) fi->FileName[i] = (CHAR16)base[i];
    fi->FileName[nlen] = 0;
    *size = need;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_setinfo(EFI_FILE_PROTOCOL *This, EFI_GUID *t, UINTN s, VOID *b) {
    (void)This; (void)t; (void)s; (void)b;
    return EFI_WRITE_PROTECTED;
}

static EFI_STATUS EFIAPI file_flush(EFI_FILE_PROTOCOL *This) { (void)This; return EFI_SUCCESS; }
static EFI_STATUS EFIAPI file_delete(EFI_FILE_PROTOCOL *This) {
    file_close(This);
    return EFI_WRITE_PROTECTED;
}

static void file_init_vtable(fw_file *f) {
    f->proto.Revision    = 0x10000;
    f->proto.Open        = file_open;
    f->proto.Close       = file_close;
    f->proto.Delete      = file_delete;
    f->proto.Read        = file_read;
    f->proto.Write       = file_write;
    f->proto.GetPosition = file_getpos;
    f->proto.SetPosition = file_setpos;
    f->proto.GetInfo     = file_getinfo;
    f->proto.SetInfo     = file_setinfo;
    f->proto.Flush       = file_flush;
}

static EFI_STATUS EFIAPI file_close(EFI_FILE_PROTOCOL *This) {
    fw_file *f = (fw_file *)This;
    if (!f || !f->used) return EFI_INVALID_PARAMETER;
    if (f->data && f->pages) fw_free_pages(f->data, f->pages);
    f->used = 0;
    f->data = NULL;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_open(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **New,
                                   CHAR16 *Name, UINT64 Mode, UINT64 Attr) {
    fw_file *parent = (fw_file *)This;
    (void)Attr;
    if (!parent || !parent->used || !New || !Name) return EFI_INVALID_PARAMETER;
    if (Mode & (EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE)) return EFI_WRITE_PROTECTED;

    char rel[256], full[256];
    path16_to_ascii(Name, rel, sizeof(rel));
    if (fw_strcmp(rel, ".") == 0) { *New = This; return EFI_SUCCESS; }
    path_join(parent->path, rel, full, sizeof(full));

    uint64_t sz = 0;
    uint32_t attr = 0;
    if (fw_vol_stat(parent->vol, full, &sz, &attr) != 0) return EFI_NOT_FOUND;

    fw_file *f = file_alloc();
    if (!f) return EFI_OUT_OF_RESOURCES;
    file_init_vtable(f);
    f->used   = 1;
    f->vol    = parent->vol;
    f->is_dir = (attr & 0x10) ? 1 : 0;
    f->size   = sz;
    f->pos    = 0;
    for (size_t i = 0; i < sizeof(f->path) && (f->path[i] = full[i]); i++) { }
    f->path[sizeof(f->path) - 1] = 0;

    if (!f->is_dir) {
        void *data = NULL;
        uint64_t got = 0;
        if (fw_vol_read_file(parent->vol, full, &data, &got) != 0) { f->used = 0; return EFI_DEVICE_ERROR; }
        f->data  = (uint8_t *)data;
        f->size  = got;
        f->pages = (size_t)((got + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
        if (f->pages == 0) f->pages = 1;
    }
    *New = &f->proto;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI fs_open_volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
                                        EFI_FILE_PROTOCOL **root) {
    fw_fs *fs = (fw_fs *)This;
    if (!fs || !root) return EFI_INVALID_PARAMETER;
    fw_file *f = file_alloc();
    if (!f) return EFI_OUT_OF_RESOURCES;
    file_init_vtable(f);
    f->used   = 1;
    f->vol    = fs->vol;
    f->is_dir = 1;
    f->path[0] = '\\';
    f->path[1] = 0;
    *root = &f->proto;
    return EFI_SUCCESS;
}

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fw_fsproto_create(fw_volume *v) {
    if (g_nfs >= 8 || !v) return NULL;
    fw_fs *fs = &g_fs[g_nfs++];
    fs->proto.Revision   = 0x10000;
    fs->proto.OpenVolume = fs_open_volume;
    fs->vol = v;
    return &fs->proto;
}
