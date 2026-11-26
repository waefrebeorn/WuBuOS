/*
 * wubu_libraries.h -- Explorer libraries: one named collection built from several real folders.
 *
 * Windows' libraries are the reason "AddSuggestedFoldersToLibraryDialog" exists: Documents is not a
 * folder, it is a LIST of folders, and Windows offers to add the ones it can see. WuBuOS had only a
 * folder tree, so a user with photos on a second disk had no way to say so.
 *
 * Rules (the same ones the firmware/mirror/user surfaces follow):
 *   - every folder here is a real path read from the machine (XDG user dirs, the home directory);
 *   - a folder that does not exist is KEPT and reported missing -- Windows does not silently drop a
 *     library folder on an unplugged disk either, and a disappearing entry is a bug the user cannot see;
 *   - the user's additions live in one small text file (~/.config/wubu/libraries.conf), not in memory.
 *
 * C11.
 */
#ifndef WUBU_LIBRARIES_H
#define WUBU_LIBRARIES_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_LIB_COUNT      4
#define WUBU_LIB_MAX_FOLDERS 8
#define WUBU_LIB_PATH_MAX   256
#define WUBU_LIB_NAME_MAX   32

typedef enum {
    WUBU_LIB_DOCUMENTS = 0,
    WUBU_LIB_PICTURES  = 1,
    WUBU_LIB_MUSIC     = 2,
    WUBU_LIB_VIDEOS    = 3
} WubuLibraryId;

typedef struct {
    char path[WUBU_LIB_PATH_MAX];
    bool exists;                 /* measured when loaded; a missing folder stays in the list */
    bool from_default;           /* came from XDG/home, not invented by the user */
} WubuLibraryFolder;

typedef struct {
    WubuLibraryId id;
    char name[WUBU_LIB_NAME_MAX];
    WubuLibraryFolder folders[WUBU_LIB_MAX_FOLDERS];
    int  count;
} WubuLibrary;

typedef struct {
    WubuLibrary lib[WUBU_LIB_COUNT];
    int loaded;
} WubuLibraries;

/* $WUBU_LIBRARIES_CONF overrides the config file (the gate writes its own). */
const char *wubu_libraries_conf_path(void);

/* Fill every library: defaults resolved from $HOME (and XDG_*_DIR when set), then the config file. */
void wubu_libraries_load(WubuLibraries *libs);

/* Add a folder to a library. Returns 0 on success, -1 when the library is full or the path is blank;
 * 1 when the folder was already there (adding twice is not an error, it is a no-op). */
int wubu_library_add(WubuLibraries *libs, WubuLibraryId id, const char *path);

/* Remove a folder. Returns 0 when removed, 1 when it was not in the library. */
int wubu_library_remove(WubuLibraries *libs, WubuLibraryId id, const char *path);

/* Write the libraries back to the config file. Only USER folders are written: the defaults are
 * re-derived on load, so a moved home directory does not leave a dead path behind. */
int wubu_libraries_save(const WubuLibraries *libs);

/* Folders the user could plausibly want in this library but has not added: the library's own default
 * siblings that exist on disk. Fills `out` with up to `max` paths, returns the count. This is what the
 * "add suggested folders" dialog offers -- measured, never invented. */
int wubu_library_suggest(const WubuLibraries *libs, WubuLibraryId id,
                         char out[][WUBU_LIB_PATH_MAX], int max);

/* One item in a library's merged view. `source` names the folder it came from, so a file that exists
 * in two folders is shown ONCE and the user can still tell where this copy lives. */
typedef struct {
    char name[128];
    char path[WUBU_LIB_PATH_MAX * 2];
    char source[WUBU_LIB_NAME_MAX];
    bool is_dir;
    long size;
    long modified;      /* mtime, so the list can show a date like every other folder */
} WubuLibraryItem;

/* The library as one list: every folder's children, merged, de-duplicated by name, sorted by name
 * (directories first, as Explorer does). `max` bounds the result. Returns the count, or -1 when the
 * library does not exist. Folders that are missing contribute nothing -- they are not an error here,
 * the library page already says they are missing. */
int wubu_library_list(const WubuLibraries *libs, WubuLibraryId id,
                      WubuLibraryItem *out, int max);

WubuLibrary *wubu_library_get(WubuLibraries *libs, WubuLibraryId id);
const char  *wubu_library_name(WubuLibraryId id);

#endif
