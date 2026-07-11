/* wubu_editor_macro.c -- WuBuOS editor: keyboard macro record/playback.
 * Extracted from wubu_editor.c (separable leaf). Self-contained: uses the
 * public editor insert API (wubu_ed_insert_char / wubu_ed_insert_newline).
 * C11, minimal includes.
 */
#include "wubu_editor.h"

#include <stdlib.h>
#include <string.h>

static void macro_record(WubuEditor *ed, char ch) {
    if (!ed || !ed->macro_recording) return;
    if (ed->macro_len + 1 >= ed->macro_size) {
        size_t new_size = ed->macro_size ? ed->macro_size * 2 : 256;
        char *new_buf = (char *)realloc(ed->macro_buf, new_size);
        if (!new_buf) return;
        ed->macro_buf = new_buf;
        ed->macro_size = new_size;
    }
    ed->macro_buf[ed->macro_len++] = ch;
}

void wubu_ed_macro_start(WubuEditor *ed) {
    if (!ed) return;
    ed->macro_recording = true;
    ed->macro_len = 0;
}

void wubu_ed_macro_stop(WubuEditor *ed) {
    if (ed) ed->macro_recording = false;
}

void wubu_ed_macro_play(WubuEditor *ed) {
    if (!ed || !ed->macro_buf || ed->macro_len == 0) return;
    ed->macro_playing = true;
    for (size_t i = 0; i < ed->macro_len; i++) {
        char ch = ed->macro_buf[i];
        if (ch == '\n') wubu_ed_insert_newline(ed);
        else wubu_ed_insert_char(ed, ch);
    }
    ed->macro_playing = false;
}
