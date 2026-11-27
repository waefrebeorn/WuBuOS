/*
 * file_assoc.h -- what opens this file?
 *
 * One owner for the answer. Explorer used to answer it inline with a chain of strcmp()s that only knew source
 * files, so double-clicking a .txt did nothing at all -- in a file manager, the single most ordinary action.
 *
 * The mapping is by extension and it is DELIBERATELY small: a type either has a handler we really ship, or it
 * has none and the Open-With resolver asks the user. Guessing a handler is how a file manager lies.
 */
#ifndef WUBU_FILE_ASSOC_H
#define WUBU_FILE_ASSOC_H

typedef enum {
    ASSOC_NONE = 0,   /* no handler: the caller must ask (Open With), never guess */
    ASSOC_NOTEPAD,    /* text: notepad_open(path) */
    ASSOC_WCODE,      /* source/IDE-owned: W-Code */
    ASSOC_WET         /* .WET applet: the applet host */
} AssocKind;

/* Extension of `path` (after the last '.', lowercased), or "" -- never NULL. */
const char *file_assoc_ext(const char *path);

/* Which handler owns this path. Pure: no I/O, no allocation. */
AssocKind file_assoc_kind(const char *path);

/* Stable name for a kind ("Notepad", "W-Code", ".WET applet", "" for none) -- for menus and messages. */
const char *file_assoc_label(AssocKind kind);

#endif /* WUBU_FILE_ASSOC_H */
