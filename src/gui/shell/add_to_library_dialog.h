/*
 * add_to_library_dialog.h -- "Add folders to <Library>" (AddSuggestedFoldersToLibraryDialog parity).
 *
 * Windows offers the folders it can see when a library is empty or thin. WuBuOS's libraries module
 * already answers WHAT could be added (wubu_library_suggest: existing folders not yet included); this
 * is only the card that lets the user pick. It is built on the shared ChoiceDialog, so it has no
 * geometry, hit-test or key handling of its own to get wrong.
 *
 * C11.
 */
#ifndef ADD_TO_LIBRARY_DIALOG_H
#define ADD_TO_LIBRARY_DIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "choice_dialog.h"
#include "../../runtime/wubu_libraries.h"

#define ATL_PATH_MAX 256

typedef struct {
    bool            open;
    WubuLibraryId   library;
    ChoiceDialog   *card;        /* owned by this dialog; non-NULL while open */
    WubuLibraries   libs;        /* the snapshot the choices apply to */
    char            status[160]; /* what the last accept did, for a toast */
    int             added;
} AddToLibraryState;

/* Open the card for a library. Returns false when there is nothing to suggest (the caller should say
 * so rather than show an empty card) or the library does not exist. */
bool add_to_library_open(WubuLibraryId id, int fb_w, int fb_h);

/* Apply the ticked rows: adds each folder, saves the config ONCE, and records a status line.
 * Returns the number of folders added (0 when nothing was ticked). */
int add_to_library_apply(void);

void add_to_library_close(void);
bool add_to_library_is_open(void);
const AddToLibraryState *add_to_library_state(void);
const char *add_to_library_status(void);

/* Input, routed by wm_input while the card is up. click returns 1 when accepted (the caller toasts the
 * status), 2 when cancelled, 0 otherwise. */
int  add_to_library_click(int x, int y, int fb_w, int fb_h);
bool add_to_library_key(uint32_t key, int fb_w, int fb_h);
void add_to_library_render(uint32_t *fb, int fb_w, int fb_h);

#endif
