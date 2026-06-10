/*
 * wubu_editor.h — WuBuOS Code Editor (Notepad++ class)
 *
 * Cell 396: Full programmer's text editor.
 *
 * Features from Notepad++:
 *   - Tabbed multi-file editing
 *   - Syntax highlighting (C, HolyC, Python, Shell, Makefile, Config)
 *   - Find & Replace (with regex support)
 *   - Line numbers + gutter
 *   - Code folding ({  }  block collapse)
 *   - Word wrap toggle
 *   - Indent guides
 *   - Bookmarks per line
 *   - Auto-indent
 *   - Brace matching + highlight
 *   - File status (modified dot, read-only lock)
 *   - Encoding display (UTF-8, ASCII)
 *   - EOL display (LF, CRLF, CR)
 *   - Split view (horizontal/vertical)
 *   - Macro record/playback
 *   - Session save/restore
 *
 * WuBuOS additions:
 *   - HolyC syntax mode (ZealOS types: U0, I64, F64, Bool, U8...)
 *   - .wubu manifest mode (TOML-like)
 *   - Container-aware: open files from mounted container roots
 *   - GAAD-aware: split view uses golden ratio division
 */
#ifndef WUBU_EDITOR_H
#define WUBU_EDITOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Limits ──────────────────────────────────────────────────────── */

#define WUBU_ED_MAX_TABS       32
#define WUBU_ED_MAX_LINES      65536
#define WUBU_ED_MAX_LINE_LEN   4096
#define WUBU_ED_MAX_BOOKMARKS  64
#define WUBU_ED_MAX_UNDO       1024
#define WUBU_ED_FIND_LEN       256

/* ── Syntax Highlighting ─────────────────────────────────────────── */

typedef enum {
    SYNTAX_NONE     = 0,
    SYNTAX_C        = 1,
    SYNTAX_HOLYC    = 2,    /* ZealOS HolyC */
    SYNTAX_PYTHON   = 3,
    SYNTAX_SHELL    = 4,
    SYNTAX_MAKEFILE = 5,
    SYNTAX_TOML     = 6,    /* .wubu manifests */
    SYNTAX_MARKDOWN = 7,
    SYNTAX_JSON     = 8,
    SYNTAX_DIFF     = 9,
} WubuSyntax;

/* Token types for syntax coloring */
typedef enum {
    TOK_DEFAULT    = 0,
    TOK_KEYWORD    = 1,    /* Language keywords */
    TOK_TYPE       = 2,    /* Types (int, I64, F64) */
    TOK_STRING     = 3,    /* String literals */
    TOK_CHAR       = 4,    /* Char literals */
    TOK_NUMBER     = 5,    /* Numeric literals */
    TOK_COMMENT    = 6,    /* Single-line comment */
    TOK_COMMENT_ML = 7,    /* Multi-line comment */
    TOK_PREPROC    = 8,    /* Preprocessor directives */
    TOK_OPERATOR   = 9,    /* Operators */
    TOK_BRACKET    = 10,   /* {}, [], () */
    TOK_FUNC       = 11,   /* Function names */
    TOK_LABEL      = 12,   /* HolyC labels / goto targets */
    TOK_WHITESPACE = 13,
} WubuTokenKind;

/* ── Line Data ──────────────────────────────────────────────────── */

typedef struct {
    char     text[WUBU_ED_MAX_LINE_LEN];
    int      len;
    bool     folded;        /* This line is folded (hidden) */
    bool     fold_start;    /* This line starts a fold block */
    bool     bookmark;
    int      fold_level;    /* Nesting depth for indent guides */
} WubuEdLine;

/* ── Undo Action ─────────────────────────────────────────────────── */

typedef enum {
    UNDO_INSERT = 1,
    UNDO_DELETE = 2,
    UNDO_REPLACE = 3,
} WubuUndoKind;

typedef struct {
    WubuUndoKind kind;
    int          line;
    int          col;
    char         text[WUBU_ED_MAX_LINE_LEN];
    int          text_len;
} WubuUndo;

/* ── Editor Tab (one per open file) ─────────────────────────────── */

typedef struct {
    char          filename[512];
    char          title[64];       /* Tab title (filename or "Untitled") */
    bool          modified;        /* Unsaved changes? */
    bool          readonly;        /* File is read-only? */
    WubuSyntax    syntax;          /* Auto-detected syntax mode */

    WubuEdLine   *lines;           /* Line array (dynamically allocated) */
    int           n_lines;
    int           lines_capacity;

    int           cursor_line;     /* Cursor position */
    int           cursor_col;
    int           scroll_x;       /* Viewport scroll */
    int           scroll_y;
    int           sel_start_line;  /* Selection (-1 = no selection) */
    int           sel_start_col;
    int           sel_end_line;
    int           sel_end_col;

    WubuUndo      undo_stack[WUBU_ED_MAX_UNDO];
    int           undo_pos;
    int           undo_count;

    bool          word_wrap;
    bool          show_whitespace;
    int           tab_size;        /* Tab width (default 4) */
    bool          use_spaces;      /* Insert spaces instead of tabs */
} WubuEdTab;

/* ── Find/Replace State ──────────────────────────────────────────── */

typedef struct {
    char    find_text[WUBU_ED_FIND_LEN];
    char    replace_text[WUBU_ED_FIND_LEN];
    bool    case_sensitive;
    bool    whole_word;
    bool    use_regex;
    bool    wrap_around;
    int     last_found_line;
    int     last_found_col;
} WubuEdFind;

/* ── Complete Editor State ───────────────────────────────────────── */

typedef struct {
    WubuEdTab     tabs[WUBU_ED_MAX_TABS];
    int           n_tabs;
    int           active_tab;      /* Currently visible tab */

    WubuEdFind    find;
    bool          find_visible;    /* Find dialog showing? */
    bool          replace_visible; /* Replace dialog? */

    bool          show_line_nums;
    bool          show_indent_guides;
    bool          show_fold_markers;
    int           gutter_width;    /* Pixels for line number gutter */

    /* Split view */
    bool          split;
    bool          split_vertical;  /* true=vertical, false=horizontal */
    int           split_tab;       /* Tab index in second pane */
    double        split_ratio;     /* GAAD: default 1/φ for golden split */

    /* Macro recording */
    bool          macro_recording;
    bool          macro_playing;
    
    /* Clipboard */
    char         *clipboard;
    size_t        clipboard_size;
} WubuEditor;

/* ── Editor API ─────────────────────────────────────────────────── */

/* Create/destroy editor */
WubuEditor *wubu_ed_create(void);
void        wubu_ed_destroy(WubuEditor *ed);

/* File operations */
int  wubu_ed_open(WubuEditor *ed, const char *filename);
int  wubu_ed_save(WubuEditor *ed);
int  wubu_ed_save_as(WubuEditor *ed, const char *filename);
void wubu_ed_new_file(WubuEditor *ed);
int  wubu_ed_close_tab(WubuEditor *ed, int tab_idx);

/* Tab management */
void wubu_ed_switch_tab(WubuEditor *ed, int tab_idx);
int  wubu_ed_active_tab(WubuEditor *ed);
int  wubu_ed_tab_count(WubuEditor *ed);
WubuEdTab *wubu_ed_current_tab(WubuEditor *ed);

/* Text editing */
void wubu_ed_insert_char(WubuEditor *ed, char ch);
void wubu_ed_insert_newline(WubuEditor *ed);
void wubu_ed_delete_char(WubuEditor *ed);     /* Backspace */
void wubu_ed_delete_forward(WubuEditor *ed);   /* Delete */
void wubu_ed_insert_text(WubuEditor *ed, const char *text);

/* Undo/Redo */
void wubu_ed_undo(WubuEditor *ed);
void wubu_ed_redo(WubuEditor *ed);
bool wubu_ed_can_undo(WubuEditor *ed);
bool wubu_ed_can_redo(WubuEditor *ed);

/* Selection */
void wubu_ed_select_all(WubuEditor *ed);
void wubu_ed_cut(WubuEditor *ed);
void wubu_ed_copy(WubuEditor *ed);
void wubu_ed_paste(WubuEditor *ed);

/* Find/Replace */
int  wubu_ed_find_next(WubuEditor *ed);
int  wubu_ed_find_prev(WubuEditor *ed);
int  wubu_ed_replace_next(WubuEditor *ed);
int  wubu_ed_replace_all(WubuEditor *ed);

/* Code folding */
void wubu_ed_fold_toggle(WubuEditor *ed, int line);
void wubu_ed_fold_all(WubuEditor *ed);
void wubu_ed_unfold_all(WubuEditor *ed);

/* Bookmarks */
void wubu_ed_bookmark_toggle(WubuEditor *ed, int line);
int  wubu_ed_bookmark_next(WubuEditor *ed);
int  wubu_ed_bookmark_prev(WubuEditor *ed);

/* Syntax detection */
WubuSyntax wubu_ed_detect_syntax(const char *filename);

/* View toggles */
void wubu_ed_toggle_line_nums(WubuEditor *ed);
void wubu_ed_toggle_word_wrap(WubuEditor *ed);
void wubu_ed_toggle_split(WubuEditor *ed);
void wubu_ed_toggle_whitespace(WubuEditor *ed);

/* Macro */
void wubu_ed_macro_start(WubuEditor *ed);
void wubu_ed_macro_stop(WubuEditor *ed);
void wubu_ed_macro_play(WubuEditor *ed);

/* Session persistence */
int  wubu_ed_session_save(WubuEditor *ed, const char *path);
int  wubu_ed_session_load(WubuEditor *ed, const char *path);

#endif /* WUBU_EDITOR_H */
