<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Public API (header-sourced)
> Generated 2026-11-18 05:20 UTC -- prototypes extracted from the headers = the real interface contracts.

> 4721 prototypes across the tree.

## `src/apps/apps.h`

- `app_scan_wet_dir(const char *dir)`
- `app_wet_count(void)`
- `terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)`
- `terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods)`
- `terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind)`
- `launch_file_manager(void)`
- `launch_settings(void)`
- `launch_editor(void)`

## `src/apps/calc.h`

- `calc_open(void)`
- `calc_init(void)`
- `calc_shutdown(void)`

## `src/apps/explorer.h`

- `explorer_open(void)`
- `explorer_init(void)`
- `explorer_shutdown(void)`

## `src/apps/guest_file.h`

- `guest_file_read(const char *name, uint8_t *buf, uint32_t cap, uint32_t *len)`
- `guest_file_write(const char *name, const uint8_t *data, uint32_t len)`
- `guest_file_flush(void)`
- `guest_file_delete(const char *name)`
- `guest_file_list(char out[][64], int max_len, int cap)`
- `guest_file_children(const char *path, guest_child_cb cb, void *user)`
- `guest_file_is_dir(const char *name)`
- `guest_file_rmdir(const char *name)`
- `guest_file_rename(const char *old_name, const char *new_name)`
- `guest_file_stat(const char *name, uint32_t *size, bool *is_dir)`

## `src/apps/notepad.h`

- `notepad_save(const char *name)`
- `notepad_reset(void)`
- `notepad_line_count(void)`
- `notepad_cursor_line(void)`
- `notepad_cursor_col(void)`
- `notepad_modified(void)`

## `src/apps/repl.h`

- `repl_input_char(REPLState *r, char c)`
- `repl_input_backspace(REPLState *r)`
- `repl_submit(REPLState *r)`

## `src/apps/terminal.h`

- `terminal_open(void)`
- `terminal_init(void)`
- `terminal_shutdown(void)`
- `terminal_poll(void)`

## `src/apps/wubu_canvas.h`

- `wubu_cv_layer_set_opacity(WubuCanvas *cv, int idx, uint8_t opacity)`
- `wubu_cv_layer_set_blend(WubuCanvas *cv, int idx, WubuBlendMode blend)`
- `wubu_cv_layer_set_visible(WubuCanvas *cv, int idx, bool visible)`
- `wubu_cv_layer_set_locked(WubuCanvas *cv, int idx, bool locked)`
- `wubu_cv_load_png(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_gif(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_bmp(WubuCanvas *cv, const char *path)`
- `wubu_cv_load_ppm(WubuCanvas *cv, const char *path)`
- `wubu_cv_zoom_out(WubuCanvas *cv)`
- `wubu_cv_zoom_fit(WubuCanvas *cv)`
- `wubu_cv_pan(WubuCanvas *cv, int dx, int dy)`

## `src/apps/wubu_canvas_internal.h`

- `wubu_cv__undo_push(WubuCanvas *cv)`

## `src/apps/wubu_editor.h`

- `wubu_ed_delete_forward(WubuEditor *ed)`
- `wubu_ed_insert_text(WubuEditor *ed, const char *text)`

## `src/apps/wubu_image_codec_internal.h`

- `png_unfilter(uint8_t *raw, int w, int h, int channels)`

## `src/apps/taskmgr/taskmgr.h`

- `taskmgr_has_session(const DosGuiWindow *win)`

## `src/apps/calc/calc_internal.h`

- `calc_apply_op(int op, double a, double b, bool *err)`
- `calc_apply_func(int func, double x, bool *err)`

## `src/apps/control/control.h`

- `control_create(void)`
- `control_destroy(ControlState *ctrl)`
- `control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl)`
- `control_launch(void)`
- `control_set_tab(ControlState *ctrl, int tab)`
- `control_set_theme(int theme_id)`

## `src/apps/wubupad/src/agent.h`

- `agent_serve(Agent *a, void *in, void *out)`

## `src/apps/wubupad/src/buffer.h`

- `buf_line_col_of(const Buf *b, size_t pos, size_t *line, size_t *col)`

## `src/apps/wubupad/src/complete.h`

- `doc_symbols_free(DocSymbol *syms, size_t n)`

## `src/apps/wubupad/src/diff.h`

- `diff_free(Diff *d)`

## `src/apps/wubupad/src/doc.h`

- `doc_free(Doc *d)`
- `doc_replace(Doc *d, size_t from, size_t to, const char *text)`
- `doc_get_colmode(const Doc *d)`
- `doc_set_colmode(Doc *d, int on)`
- `doc_get_rect(const Doc *d, size_t *r0, size_t *c0, size_t *r1, size_t *c1)`
- `doc_set_rect(Doc *d, size_t r0, size_t c0, size_t r1, size_t c1)`

## `src/apps/wubupad/src/docs.h`

- `docs_free(Docs *s)`
- `docs_open(Docs *s, const char *path, const char *text, const char *lang)`
- `docs_load_file(Docs *s, const char *path, const char *lang)`

## `src/apps/wubupad/src/json.h`

- `j_free(JVal *v)`
- `j_obj_put(JVal *obj, const char *key, JVal *val)`
- `j_type(const JVal *v)`
- `j_as_bool(const JVal *v)`
- `j_as_num(const JVal *v)`
- `j_len(const JVal *v)`

## `src/apps/wubupad/src/lex.h`

- `lex_free(Lex *l)`
- `lex_run(Lex *l, const char *text, size_t len, LexSpan *out, size_t cap)`
- `lex_folds(const char *text, size_t len, LexFold *out, size_t cap)`
- `lex_symbols(const char *text, size_t len, LexSym *out, size_t cap)`

## `src/apps/wubupad/src/search.h`

- `regex_free(Regex *r)`

## `src/apps/wubupad/src/transform.h`

- `transform_case(Doc *d, int which)`
- `transform_sort_lines(Doc *d, int which)`

## `src/apps/wubupad/src/treeview/treeview.h`

- `treeview_free(TreeView *t)`
- `treeview_apply_git(TreeView *t, const char *porcelain)`

## `src/apps/wubupad/src/palette/palette.h`

- `palette_feed(Palette *p, char ch, int key, void *arg)`

## `src/apps/wubupad/src/snippet/snippet.h`

- `snippet_add(SnippetEngine *e, const char *trigger, const char *body)`
- `snippet_next(SnippetEngine *e, size_t *out_from, size_t *out_to)`

## `src/apps/wubupad/src/command/command.h`

- `command_register(CommandRegistry *r, const char *name, command_fn fn)`
- `command_run(CommandRegistry *r, const char *name, void *arg)`
- `command_enumerate(const CommandRegistry *r, command_enum_fn cb, void *ctx)`

## `src/apps/wubupad/src/minimap/minimap.h`

- `minimap_free(Minimap *m)`
- `minimap_update(Minimap *m, const char *text)`

## `src/apps/wubupad/src/autoindent/autoindent.h`

- `autoindent_continued(const char *line)`

## `src/apps/wubupad/src/mdpreview/mdpreview.h`

- `mdpreview_render_buf(const char *md, const char *title, char *out, size_t cap)`

## `src/apps/wubupad/src/pkgmgr/pkgmgr.h`

- `pkgmgr_enable(PackageManager *m, const char *name)`

## `src/apps/wubupad/src/fuzzy/fuzzy.h`

- `fuzzy_score(const char *query, const char *cand)`

## `src/apps/wubupad/src/ui/ui.h`

- `ui_find(UI *ui, const char *pattern, int regex, int icase)`
- `ui_toggle_theme(UI *ui)`
- `ui_theme_dark(const UI *ui)`
- `ui_set_docs(UI *ui, Docs *docs)`
- `ui_set_macro(UI *ui, UIMacro *m)`
- `ui_toggle_macro(UI *ui)`
- `ui_replay_macro(UI *ui)`
- `ui_case_convert(UI *ui, int which)`
- `ui_complete(UI *ui)`
- `ui_toggle_fold(UI *ui, size_t a, size_t b)`
- `ui_is_folded(const UI *ui, size_t line)`
- `ui_unfold_all(UI *ui)`
- `ui_fold_current_block(UI *ui)`
- `ui_symbols(const UI *ui, DocSymbol **out, size_t *n)`
- `ui_open_palette(UI *ui)`
- `ui_palette_render(const UI *ui, char *buf, size_t bufn)`
- `ui_render(UI *ui, const char *lang)`
- `ui_run(UI *ui, const char *lang)`
- `ui_apply(UI *ui, char ch, int key)`
- `ui_step(UI *ui, const char *lang)`
- `ui_capture(UI *ui, unsigned char **rgba, int *w, int *h)`

## `src/apps/wubupad/src/ui/ui_find.h`

- `ui_find_count(const UIFind *f)`
- `ui_find_active(const UIFind *f, size_t *start, size_t *end)`
- `ui_find_next(UIFind *f)`
- `ui_find_prev(UIFind *f)`

## `src/apps/wubupad/src/ui/ui_gfx.h`

- `ui_gfx_capture(void *st, unsigned char **rgba, int *w, int *h)`

## `src/apps/wubupad/src/ui/ui_headless.h`

- `ui_headless_queue_key(UI *ui, const char *ch, int key)`
- `ui__hl_queue(void *bstate, const char *ch, int key)`

## `src/apps/wubupad/src/ui/ui_macro.h`

- `ui_macro_free(UIMacro *m)`
- `ui_macro_record_start(UIMacro *m)`
- `ui_macro_record_stop(UIMacro *m)`
- `ui_macro_recording(const UIMacro *m)`

## `src/apps/wubupad/src/ui/ui_theme.h`

- `ui_theme_load(UITheme *t)`
- `ui_theme_save(const UITheme *t)`

## `src/apps/wubupad/src/ui/ui_tty.h`

- `ui_tty_enable_raw(UI *ui)`

## `src/apps/wubuoffice/apps/wubushow/show.h`

- `wubushow_free(wubushow_pres *p)`
- `wubushow_slide_count(const wubushow_pres *p)`
- `wubushow_slide_get(const wubushow_pres *p, int idx, const char **out_title, const char **out_body)`

## `src/apps/wubuoffice/apps/wubushow/show_read.h`

- `wubushow_read(const char *path, wubushow_pres **out)`

## `src/apps/wubuoffice/apps/wubupad_bridge/wubupad_bridge.h`

- `wubupad_load(const char *path, char **out, size_t *out_len)`

## `src/apps/wubuoffice/apps/wubunote/note_controller.h`

- `nctrl_prompt_commit(NState *st, char **out_path)`
- `nctrl_handle(NState *st, const TuiKey *k, char **out_path)`

## `src/apps/wubuoffice/apps/wubuview/controller.h`

- `vctrl_handle(VState *st, const TuiKey *k)`

## `src/apps/wubuoffice/apps/wubucell/cell.h`

- `wubucell_free(wubucell_book *b)`
- `wubucell_cell_s(wubucell_book *b, int sheet, int col, int row, const char *text)`
- `wubucell_cell_sx(wubucell_book *b, int sheet, int col, int row, const char *text, int style)`
- `wubucell_cell_n(wubucell_book *b, int sheet, int col, int row, double num)`
- `wubucell_cell_nx(wubucell_book *b, int sheet, int col, int row, double num, int style)`
- `wubucell_cell_f(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached)`
- `wubucell_cell_fx(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached, int style)`
- `wubucell_merge(wubucell_book *b, int sheet, int c0, int r0, int c1, int r1)`
- `wubucell_merge_count(const wubucell_book *b, int sheet)`
- `wubucell_col_width_set(wubucell_book *b, int sheet, int col, double w)`
- `wubucell_col_width_get(const wubucell_book *b, int sheet, int col)`
- `wubucell_use_shared_strings(wubucell_book *b, int enable)`
- `wubucell_sheet_count(const wubucell_book *b)`
- `wubucell_sheet_dims(const wubucell_book *b, int sheet, int *max_col, int *max_row)`

## `src/apps/wubuoffice/apps/wubucell/cell_csv.h`

- `wubucell_read_csv(const char *path, char delim, wubucell_book **out)`
- `wubucell_write_csv(const wubucell_book *b, int sheet, char delim, const char *path)`

## `src/apps/wubuoffice/apps/wubucell/cell_read.h`

- `wubucell_read(const char *path, wubucell_book **out)`

## `src/apps/wubuoffice/apps/wubucell/style.h`

- `wubucell_style_free(wubucell_style *s)`
- `wubucell_style_fill(wubucell_style *s, const char *color)`
- `wubucell_style_border(wubucell_style *s, const char *style, const char *color)`
- `wubucell_style_numfmt(wubucell_style *s, const char *code)`

## `src/apps/wubuoffice/apps/wubuodf/odf.h`

- `wubuodf_read_odt(const char *path, dm_doc *out)`
- `wubuodf_read_ods(const char *path, wubucell_book **out)`
- `wubuodf_read_odp(const char *path, wubushow_pres **out)`
- `wubuodf_write_fodt(const dm_doc *d, const char *path)`
- `wubuodf_write_fods(const wubucell_book *b, const char *path)`
- `wubuodf_write_fodp(const wubushow_pres *p, const char *path)`
- `wubuodf_read_fodt(const char *path, dm_doc *out)`
- `wubuodf_read_fods(const char *path, wubucell_book **out)`
- `wubuodf_read_fodp(const char *path, wubushow_pres **out)`

## `src/apps/wubuoffice/apps/wubuodf/odf_body.h`

- `wubuodf_emit_text_body(odf_sbuf *b, const dm_doc *d)`
- `wubuodf_emit_sheet_body(odf_sbuf *b, const wubucell_book *bk)`
- `wubuodf_emit_pres_body(odf_sbuf *b, const wubushow_pres *pres)`
- `wubuodf_parse_text_xml(const uint8_t *bytes, size_t len, dm_doc *out)`
- `wubuodf_parse_sheet_xml(const uint8_t *bytes, size_t len, wubucell_book *bk)`
- `wubuodf_parse_pres_xml(const uint8_t *bytes, size_t len, wubushow_pres *pres)`

## `src/apps/wubuoffice/apps/wubuword/assemble.h`

- `wubuword_assemble(const char *outpath, const void *doc_xml, size_t doc_len)`
- `wubuword_assemble_doc(const char *outpath, wubuword_doc *doc)`

## `src/apps/wubuoffice/apps/wubuword/word.h`

- `wubuword_free(wubuword_doc *d)`
- `wubuword_para(wubuword_doc *d, const char *style, int bold, const char *text)`
- `wubuword_list_begin(wubuword_doc *d, const char *kind)`
- `wubuword_list_item(wubuword_doc *d, const char *text)`
- `wubuword_list_end(wubuword_doc *d)`
- `wubuword_used_lists(const wubuword_doc *d)`
- `wubuword_table_begin(wubuword_doc *d)`
- `wubuword_row(wubuword_doc *d)`
- `wubuword_cell(wubuword_doc *d, int bold, const char *text)`
- `wubuword_table_end(wubuword_doc *d)`

## `src/apps/wubuoffice/apps/wubuedit/docmodel.h`

- `wubuedit_docmodel_parse(const uint8_t *xml, size_t len, dm_doc *out)`
- `wubuedit_docmodel_add_para(dm_doc *d, const char *style, int bold, const char *text)`
- `wubuedit_docmodel_add_table(dm_doc *d, dm_para **cells, size_t rows, size_t cols)`

## `src/apps/wubuoffice/apps/wubuedit/edit.h`

- `wubuedit_roundtrip(const char *in_path, const char *out_path)`

## `src/apps/wubuoffice/apps/wuburead/readers.h`

- `wuburead_epub(const char *path, dm_doc *out)`

## `src/apps/wubuoffice/apps/wubupdf/pdf.h`

- `wubupdf_write(const dm_doc *doc, const char *outpath)`

## `src/apps/wubuoffice/apps/wubupdf/pdf_internal.h`

- `pdf_text_width(const char *winansi, size_t len, double size, int bold)`

## `src/apps/wubuoffice/apps/wubudoc/doc_md.h`

- `wubudoc_read_md(const char *path, wubuword_doc **out)`

## `src/apps/wubuoffice/apps/wubudoc/epub.h`

- `wubudoc_write_epub(const dm_doc *d, const char *path)`

## `src/apps/wubuoffice/apps/wubudoc/model_json.h`

- `wubudoc_write_doc_json(const dm_doc *d, const char *path)`
- `wubudoc_write_book_json(const wubucell_book *b, const char *path)`
- `wubudoc_write_pres_json(const wubushow_pres *p, const char *path)`

## `src/apps/wubuoffice/apps/wubuconv/conv_bridge.h`

- `wubuconv_model_to_text(const wubumodel_doc *m, dm_doc *out)`

## `src/apps/wubuoffice/apps/wubuconv/conv_map.h`

- `wubuconv_convert(const char *in_path, const char *out_path)`

## `src/apps/wubuoffice/apps/wubuos/autocomp.h`

- `autocomp_destroy(AutoComp *ac)`
- `autocomp_open(AutoComp *ac, const void *doc)`
- `autocomp_accept(AutoComp *ac, void *doc)`

## `src/apps/wubuoffice/apps/wubuos/bkmk.h`

- `bkmk_jump(BkMk *b, int from, int dir)`

## `src/apps/wubuoffice/apps/wubuos/codefold.h`

- `codefold_toggle_block(CodeFold *cf, const void *doc, int caret_line)`

## `src/apps/wubuoffice/apps/wubuos/dialog.h`

- `dialog_destroy(Dialog *d)`
- `dialog_key(Dialog *d, int key, const char *ch)`

## `src/apps/wubuoffice/apps/wubuos/doccmd.h`

- `doccmd_insert_qr(wubumodel_doc *doc, const char *text)`
- `doccmd_insert_link_url(wubumodel_doc *doc, const char *url)`
- `doccmd_insert_pagebreak(wubumodel_doc *doc)`
- `doccmd_insert_sectionbreak(wubumodel_doc *doc)`
- `doccmd_insert_header(wubumodel_doc *doc)`
- `doccmd_insert_footer(wubumodel_doc *doc)`
- `doccmd_insert_comment(wubumodel_doc *doc)`
- `doccmd_insert_trackchange(wubumodel_doc *doc)`
- `doccmd_insert_field(wubumodel_doc *doc)`
- `doccmd_insert_script_field(wubumodel_doc *doc, const char *expr)`
- `doccmd_a11y_check(wubumodel_doc *doc, a11y_report *out)`

## `src/apps/wubuoffice/apps/wubuos/findbar.h`

- `findbar_next(FindBar *fb, const void *doc, size_t from)`

## `src/apps/wubuoffice/apps/wubuos/gotoline.h`

- `gotoline_destroy(GotoLine *g)`
- `gotoline_key(GotoLine *g, int key)`
- `gotoline_commit(const GotoLine *g)`

## `src/apps/wubuoffice/apps/wubuos/hive.h`

- `hive_free(Hive *h)`

## `src/apps/wubuoffice/apps/wubuos/macro.h`

- `macro_record(Macro *m, int opcode, unsigned char ch)`
- `macro_set_name(Macro *m, const char *name)`
- `macro_save(const char *path)`
- `macro_load(const char *path)`

## `src/apps/wubuoffice/apps/wubuos/palette.h`

- `palette_destroy(Palette *p)`
- `palette_add(Palette *p, const char *label, int cmd_id)`

## `src/apps/wubuoffice/apps/wubuos/plugin.h`

- `wuos_plugins_free(WuOSPluginMgr *m)`
- `wuos_plugins_count(const WuOSPluginMgr *m)`
- `wuos_plugin_count(void)`
- `wuos_plugin_load_path(const char *so)`

## `src/apps/wubuoffice/apps/wubuos/toast.h`

- `toast_destroy(Toasts *t)`

## `src/apps/wubuoffice/apps/wubuos/view_doc_internal.h`

- `doc_move_para(DocV *e, int dx)`

## `src/apps/wubuoffice/apps/wubuos/wuos.h`

- `wuos_editor_find_stats(WuView *v, int *active, int *total)`
- `wuos_editor_spell(WuView *v, const char *word)`
- `wuos_doc_insert_link_url(WuView *v, const char *url)`
- `wuos_doc_insert_image_alt(WuView *v, const char *alt)`
- `wuos_doc_insert_qr(WuView *v, const char *text)`
- `cellv_test_col_x(WuView *v, int c)`
- `wuos_cell_editing(WuView *v)`
- `wuos_cell_value(WuView *v, char *out, int outn)`
- `wuos_cell_kind(WuView *v)`
- `wuos_cell_formula(WuView *v, char *out, int outn)`

## `src/apps/wubuoffice/apps/wubuos/wuos_font.h`

- `wuos_font_init(void)`
- `wuos_font_quit(void)`
- `wuos_font_scan(void)`
- `wuos_font_family_count(void)`

## `src/apps/wubuoffice/apps/wubuos/wuos_motion.h`

- `wuos_ease_linear(float t)`
- `wuos_ease_out_quad(float t)`
- `wuos_ease_in_out_cubic(float t)`
- `wuos_ease_out_back(float t)`
- `wuos_tween_start(WuosTween *tw, float from, float to, float dur, int kind)`
- `wuos_tween_advance(WuosTween *tw, float dt)`
- `wuos_tween_value(const WuosTween *tw)`
- `wuos_tween_done(const WuosTween *tw)`

## `src/apps/wubuoffice/apps/wubuos/wuos_theme.h`

- `wubusettings_dark(const struct WubuSettings *s)`

## `src/apps/wubuoffice/apps/wubuos/wuos_toolbar.h`

- `wuos_tb_init(const struct HiveToolbar *tb)`
- `wuos_tb_at(int x)`
- `wuos_tb_cmd_to_key(int cmd)`

## `src/apps/wubuoffice/src/wubuformula/ast.h`

- `ast_free(ast *n)`

## `src/apps/wubuoffice/src/wubuformula/funcs.h`

- `wubu_func_lookup(const char *name)`
- `wubu_formula_register_all(void)`

## `src/apps/wubuoffice/src/wubuformula/lexer.h`

- `wubu_lexer_init(wubu_lexer *L, const char *src, size_t len)`

## `src/apps/wubuoffice/src/wubuformula/value.h`

- `wubuval_set_num(wubuval *v, double x)`
- `wubuval_set_str(wubuval *v, const char *s)`
- `wubuval_set_bool(wubuval *v, int b)`
- `wubuval_set_empty(wubuval *v)`
- `wubuval_set_err(wubuval *v, wubuval_err e)`
- `wubuval_free(wubuval *v)`
- `wubuval_copy(wubuval *dst, const wubuval *src)`

## `src/apps/wubuoffice/src/wubuformula/value_util.h`

- `wubu_to_num(const wubuval *v, int *ok)`
- `wubu_match_criteria(const wubuval *v, const wubuval *crit)`

## `src/apps/wubuoffice/src/wubuformula/functions/registry.h`

- `wubu_formula_register_all(void)`
- `wubu_register_logic(wubu_func_registrar reg)`
- `wubu_register_math(wubu_func_registrar reg)`
- `wubu_register_text(wubu_func_registrar reg)`
- `wubu_register_lookup(wubu_func_registrar reg)`
- `wubu_register_datefin(wubu_func_registrar reg)`
- `wubu_register_stat(wubu_func_registrar reg)`

## `src/apps/wubuoffice/src/wubucfb/cfb_write.h`

- `wubucfb_writer_add(wubucfb_writer *w, const char *name, const void *data, size_t len)`
- `wubucfb_writer_finish(wubucfb_writer *w, uint8_t **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubua11yannounce/a11yannounce.h`

- `a11y_announce_destroy(A11yAnnounce *a)`

## `src/apps/wubuoffice/src/wubuform/form.h`

- `form_destroy(Form *f)`
- `form_add(Form *f, const char *name, FormType type, const char *value)`
- `form_set_value(Form *f, const char *name, const char *value)`
- `form_type(const Form *f, const char *name)`
- `form_count(const Form *f)`

## `src/apps/wubuoffice/src/wubuheading/heading.h`

- `heading_destroy(Heading *h)`

## `src/apps/wubuoffice/src/wubuconnector/wubuconnector.h`

- `wubuconnector_destroy(wubuconnector *c)`
- `wubuconnector_count(const wubuconnector *c)`

## `src/apps/wubuoffice/src/wuburender/wuburender.h`

- `wurender_destroy(Wurender *r)`

## `src/apps/wubuoffice/src/wubuwatermark/watermark.h`

- `watermark_destroy(Watermark *w)`
- `watermark_set_text(Watermark *w, const char *text)`
- `watermark_set_angle(Watermark *w, int degrees)`
- `watermark_set_opacity(Watermark *w, float opacity)`
- `watermark_set_enabled(Watermark *w, int on)`
- `watermark_angle(const Watermark *w)`
- `watermark_opacity(const Watermark *w)`
- `watermark_enabled(const Watermark *w)`

## `src/apps/wubuoffice/src/wubufreeze/wubufreeze.h`

- `wubufreeze_visible_row(const wubufreeze *f, int row)`
- `wubufreeze_visible_col(const wubufreeze *f, int col)`
- `wubufreeze_frozen_rows(const wubufreeze *f)`
- `wubufreeze_frozen_cols(const wubufreeze *f)`

## `src/apps/wubuoffice/src/wubudiff/wubudiff.h`

- `wubudiff_text(const char *a, const char *b, wubudiff_hunk *out, int cap)`

## `src/apps/wubuoffice/src/wubulang/lang.h`

- `lang_destroy(LangMap *m)`
- `lang_set(LangMap *m, uint64_t node_id, const char *tag)`
- `lang_count(const LangMap *m)`
- `lang_id_at(const LangMap *m, int i)`

## `src/apps/wubuoffice/src/wubutransition/wubutransition.h`

- `wubutransition_init(wubutransition *t)`
- `wubutransition_set(wubutransition *t, wubutr_type type, double speed, int advance, double delay)`
- `wubutransition_progress(const wubutransition *t, double elapsed)`

## `src/apps/wubuoffice/src/wubuxml/xml.h`

- `wubuxml_open(wubuxml_writer *w, const char *name)`
- `wubuxml_set_attr(wubuxml_writer *w, const char *key, const char *val)`
- `wubuxml_close(wubuxml_writer *w)`

## `src/apps/wubuoffice/src/wububasic/wububasic.h`

- `wububasic_destroy(wububasic *b)`
- `wububasic_run(wububasic *b)`

## `src/apps/wubuoffice/src/wubuspell/spell.h`

- `spell_load(SpellDict *d, const char *path)`
- `spell_check(SpellDict *d, const char *word)`
- `spell_suggest(SpellDict *d, const char *word, char **out, int max)`
- `spell_scan(SpellDict *d, const char *utf8, SpellError *out, int max)`
- `spell_seed_english(SpellDict *d)`

## `src/apps/wubuoffice/src/wubuindex/wubuindex.h`

- `wubuindex_feed_page(wubuindex *ix, const char *page_text, int page)`
- `wubuindex_destroy(wubuindex *ix)`

## `src/apps/wubuoffice/src/wuburedact/redact.h`

- `redact_destroy(Redact *r)`

## `src/apps/wubuoffice/src/wubunote/edit.h`

- `edit_load(EditBuf *b, const char *text)`
- `edit_line_count(const EditBuf *b)`
- `edit_dirty(const EditBuf *b)`
- `edit_cursor_end(EditBuf *b)`
- `edit_new_line(EditBuf *b)`
- `edit_backspace(EditBuf *b)`
- `edit_delete(EditBuf *b)`
- `edit_tab(EditBuf *b)`
- `edit_arrow_left(EditBuf *b)`
- `edit_arrow_right(EditBuf *b)`
- `edit_arrow_up(EditBuf *b)`
- `edit_arrow_down(EditBuf *b)`
- `edit_find_next(EditBuf *b, const char *needle)`

## `src/apps/wubuoffice/src/wubusettings/settings.h`

- `wubusettings_load(WubuSettings *s, const char *path)`
- `wubusettings_save(const WubuSettings *s, const char *path)`
- `wubusettings_set_zoom(WubuSettings *s, double z)`
- `wubusettings_dark(const WubuSettings *s)`
- `wubusettings_set_dark(WubuSettings *s, int dark)`
- `wubusettings_autosave_ms(const WubuSettings *s)`
- `wubusettings_set_autosave_ms(WubuSettings *s, int ms)`
- `wubusettings_set_language(WubuSettings *s, const char *lang)`
- `wubusettings_font_size(const WubuSettings *s)`
- `wubusettings_set_font_size(WubuSettings *s, int px)`
- `wubusettings_word_wrap(const WubuSettings *s)`
- `wubusettings_set_word_wrap(WubuSettings *s, int on)`
- `wubusettings_tab_width(const WubuSettings *s)`
- `wubusettings_set_tab_width(WubuSettings *s, int w)`
- `wubusettings_first_run(const WubuSettings *s)`
- `wubusettings_set_first_run(WubuSettings *s, int on)`
- `wubusettings_set_font_family(WubuSettings *s, const char *family)`
- `wubusettings_recents_count(const WubuSettings *s)`
- `wubusettings_add_recent(WubuSettings *s, const char *path)`

## `src/apps/wubuoffice/src/wubusort/wubusort.h`

- `wubusort_numcmp(const char *a, const char *b)`

## `src/apps/wubuoffice/src/wubuzip/bit.h`

- `wubuzip_bit_init(wubuzip_bitreader *b, const uint8_t *data, size_t len)`
- `wubuzip_bit_get(wubuzip_bitreader *b, int n)`

## `src/apps/wubuoffice/src/wubuzip/bitw.h`

- `wubuzip_bitw_init(wubuzip_bitwriter *w)`
- `wubuzip_bitw_free(wubuzip_bitwriter *w)`
- `wubuzip_bitw_align(wubuzip_bitwriter *w)`
- `wubuzip_bitw_finish(wubuzip_bitwriter *w, uint8_t **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubuzip/block.h`

- `wubuzip_block_decode(void *br, const void *lh, const void *dh, void *out)`
- `wubuzip_block_dynamic(void *br, void *lh, void *dh, void *out)`

## `src/apps/wubuoffice/src/wubuzip/crc.h`

- `wubuzip_crc32(const void *data, size_t len)`

## `src/apps/wubuoffice/src/wubuzip/deflate.h`

- `wubuzip_deflate(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubuzip/fixed.h`

- `wubuzip_fixed_litlen(uint8_t out[288])`
- `wubuzip_fixed_dist(uint8_t out[32])`

## `src/apps/wubuoffice/src/wubuzip/huffman.h`

- `wubuzip_huff_build(wubuzip_huff *h, const uint8_t *lengths, int n)`
- `wubuzip_huff_decode(void *br, const wubuzip_huff *h)`

## `src/apps/wubuoffice/src/wubuzip/limitcode.h`

- `wubuzip_limit_lengths(const uint32_t *freqs, int n, int maxlen, uint8_t *len)`

## `src/apps/wubuoffice/src/wubuzip/lz77.h`

- `wubuzip_lz_parse(const uint8_t *in, size_t in_len, wubuzip_lz **out)`

## `src/apps/wubuoffice/src/wubuzip/reader.h`

- `wubuzip_extract(const wubuzip_archive *z, size_t i, uint8_t **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubuzip/zip.h`

- `wubuzip_add(wubuzip_writer *z, const char *name, const void *data, uint32_t size)`
- `wubuzip_add_deflated(wubuzip_writer *z, const char *name, const void *data, uint32_t size)`
- `wubuzip_finalize(wubuzip_writer *z)`

## `src/apps/wubuoffice/src/wubujson/json.h`

- `j_free(JVal *v)`
- `j_obj_put(JVal *obj, const char *key, JVal *val)`
- `j_type(const JVal *v)`
- `j_as_bool(const JVal *v)`
- `j_as_num(const JVal *v)`
- `j_len(const JVal *v)`

## `src/apps/wubuoffice/src/wubumodel/load_async.h`

- `load_async_destroy(load_async *la)`
- `load_async_join(load_async *la)`

## `src/apps/wubuoffice/src/wubumodel/model.h`

- `wubumodel_node_set_text(wubumodel_node *n, const char *utf8)`
- `wubumodel_node_apply_named_style(wubumodel_node *n, const char *name)`
- `wubumodel_node_set_note(wubumodel_node *n, const char *body)`
- `wubumodel_doc_notes(const wubumodel_doc *doc, const char ***out)`
- `wubumodel_node_set_link(wubumodel_node *n, const char *target)`
- `wubumodel_node_set_image(wubumodel_node *n, const uint8_t *rgba, int w, int h)`
- `wubumodel_node_float_side(const wubumodel_node *n)`
- `wubumodel_node_float_wrap(const wubumodel_node *n)`
- `wubumodel_node_float_w(const wubumodel_node *n)`
- `wubumodel_node_float_h(const wubumodel_node *n)`
- `wubumodel_node_set_field(wubumodel_node *n, const char *f)`
- `wubumodel_node_set_tc(wubumodel_node *n, int t)`
- `wubumodel_node_tc(const wubumodel_node *n)`
- `wubumodel_node_set_break(wubumodel_node *n, int b)`
- `wubumodel_node_break(const wubumodel_node *n)`
- `wubumodel_on_change(wubumodel_doc *doc, wubumodel_change_cb cb, void *user)`
- `wubumodel_emit_change(wubumodel_doc *doc, wubumodel_id node)`
- `wubumodel_write_docx(const wubumodel_doc *doc, const char *path)`
- `wubumodel_load_docx(const char *path, wubumodel_doc **out)`
- `wubumodel_write_odt(const wubumodel_doc *doc, const char *path)`
- `wubumodel_load_odt(const char *path, wubumodel_doc **out)`
- `wubumodel_load_rtf(const char *path, wubumodel_doc **out)`
- `wubumodel_load_epub(const char *path, wubumodel_doc **out)`

## `src/apps/wubuoffice/src/wubuanimation/wubuanimation.h`

- `wubuanimation_destroy(wubuanimation *a)`
- `wubuanimation_add(wubuanimation *a, const char *target, wubuan_type type, double dur, double delay, int repeat)`
- `wubuanimation_count(const wubuanimation *a)`
- `wubuanimation_progress(const wubuanimation *a, const char *target, double t)`

## `src/apps/wubuoffice/src/wubusvg/agent.h`

- `svgagent_free(SvgAgent *a)`
- `svgagent_serve(SvgAgent *a, FILE *in, FILE *out)`

## `src/apps/wubuoffice/src/wubusvg/wubusvg.h`

- `svg_free(SvgDoc *doc)`
- `svg_count_tag(const SvgNode *n, const char *tag)`
- `svg_append_child(SvgNode *parent, SvgNode *kid)`
- `svg_insert_child(SvgNode *parent, size_t i, SvgNode *kid)`
- `svg_find_all(const SvgNode *root, const char *path, SvgNode **out, size_t maxout)`
- `svg_set_attr_path(SvgNode *root, const char *path, const char *key, const char *val)`
- `svg_remove_path(SvgNode *root, const char *path)`

## `src/apps/wubuoffice/src/wubufmtpaint/fmtpaint.h`

- `fmtpaint_destroy(FmtPaint *f)`
- `fmtpaint_pick(FmtPaint *f, const void *src)`
- `fmtpaint_apply(FmtPaint *f, void *dst)`
- `fmtpaint_clear(FmtPaint *f)`

## `src/apps/wubuoffice/src/wubu3d/wubu3d.h`

- `wubu3d_destroy(wubu3d *m)`
- `wubu3d_rotate(wubu3d *m, float ax, float ay)`

## `src/apps/wubuoffice/src/wububase/wububase.h`

- `wububase_utf8_decode(const char *s, uint32_t *cp)`
- `wububase_buf_init(Buf *b)`
- `wububase_buf_free(Buf *b)`

## `src/apps/wubuoffice/src/wubugrammar/wubugrammar.h`

- `wubugrammar_check(const char *text, wubugrammar_finding *out, int cap)`

## `src/apps/wubuoffice/src/gpu/gpu_blas.h`

- `gpu_gemmT(const float *A, const float *B, float *C, int M, int K, int N)`
- `gpu_gemmNT(const float *A, const float *B, float *C, int M, int K, int N)`

## `src/apps/wubuoffice/src/wubunesttab/nesttab.h`

- `nesttab_depth(const void *node)`

## `src/apps/wubuoffice/src/wubuaislot/aislot.h`

- `aislot_destroy(AiSlot *s)`
- `aislot_set_provider(AiSlot *s, AiProviderFn fn, void *user)`
- `aislot_has_custom_provider(const AiSlot *s)`

## `src/apps/wubuoffice/src/wubudyslexia/dyslexia.h`

- `dyslexia_destroy(Dyslexia *d)`
- `dyslexia_set_enabled(Dyslexia *d, int on)`
- `dyslexia_enabled(const Dyslexia *d)`
- `dyslexia_set_face(Dyslexia *d, const char *face)`

## `src/apps/wubuoffice/src/wubuscenario/wubuscenario.h`

- `wubuscenario_destroy(wubuscenario *s)`

## `src/apps/wubuoffice/src/wubufont/odttf.h`

- `wubufont_odttf_guid_from_string(const char *guid, uint8_t out[16])`

## `src/apps/wubuoffice/src/wubufont/wubufont.h`

- `font_free(Font *f)`
- `font_find_table(const Font *f, uint32_t tag, size_t *offset, size_t *length)`
- `font_cmap(const Font *f, uint32_t codepoint)`
- `font_advance(const Font *f, uint32_t codepoint)`
- `font_data_size(const Font *f)`

## `src/apps/wubuoffice/src/wubupivot/wubupivot.h`

- `wubupiv_get(const wubupiv *p, const char *rowkey, const char *colkey, double *v)`
- `wubupiv_free(wubupiv *p)`

## `src/apps/wubuoffice/src/wubudropcap/wubudropcap.h`

- `wubudropcap_init(wubudropcap *d, int lines)`
- `wubudropcap_enable(wubudropcap *d, int lines)`
- `wubudropcap_disable(wubudropcap *d)`
- `wubudropcap_lines(const wubudropcap *d)`

## `src/apps/wubuoffice/src/wubucite/cite.h`

- `cite_destroy(Cite *c)`
- `cite_count(const Cite *c)`

## `src/apps/wubuoffice/src/wubutui/draw.h`

- `tui_text(TuiScreen *s, size_t x, size_t y, const char *str, uint8_t attr)`
- `tui_wrap_free(char **lines, size_t count)`
- `tui_scrollbar_thumb(size_t h, size_t total, size_t visible, size_t scroll)`
- `tui_scrollbar_scroll_at(size_t h, size_t total, size_t visible, size_t row)`
- `tui_button_width(const char *label)`
- `tui_button(TuiScreen *s, size_t x, size_t y, const char *label, uint8_t attr)`
- `tui_tabbar_layout(TuiTab *tabs, size_t n, size_t x0)`
- `tui_tabbar(TuiScreen *s, size_t y, TuiTab *tabs, size_t n, size_t x0)`
- `tui_tabbar_hit(TuiTab *tabs, size_t n, size_t px, size_t py, size_t bar_y)`

## `src/apps/wubuoffice/src/wubutui/input.h`

- `tui_key_decode(const char *buf, size_t len, TuiKey *out)`
- `tui_key_state_init(TuiKeyState *st)`
- `tui_key_state_free(TuiKeyState *st)`

## `src/apps/wubuoffice/src/wubutui/screen.h`

- `tui_screen_free(TuiScreen *s)`
- `tui_screen_width(const TuiScreen *s)`
- `tui_screen_height(const TuiScreen *s)`
- `tui_screen_put(TuiScreen *s, size_t x, size_t y, char ch, uint8_t attr)`
- `tui_screen_copy(TuiScreen *dst, const TuiScreen *src)`

## `src/apps/wubuoffice/src/wubutui/term.h`

- `tui_term_present(TuiTerm *t, const TuiScreen *s)`
- `tui_term_read(TuiTerm *t, char *buf, size_t cap)`

## `src/apps/wubuoffice/src/wubuhash/hash.h`

- `sha256_destroy(Sha256 *s)`
- `sha256_update(Sha256 *s, const void *data, size_t len)`

## `src/apps/wubuoffice/src/wubuimage/png.h`

- `png_free(PngImage *im)`
- `png_width(const PngImage *im)`
- `png_height(const PngImage *im)`

## `src/apps/wubuoffice/src/wubushape/shape.h`

- `shape_reorder(const char *text, ShapeDir base, char *out, size_t outcap)`
- `shape_is_rtl_codepoint(unsigned long cp)`

## `src/apps/wubuoffice/src/wubunotebookbar/wubunotebookbar.h`

- `wubunotebookbar_destroy(wubunotebookbar *n)`
- `wubunotebookbar_add(wubunotebookbar *n, const char *name)`
- `wubunotebookbar_count(const wubunotebookbar *n)`
- `wubunotebookbar_set_active(wubunotebookbar *n, size_t i)`
- `wubunotebookbar_active(const wubunotebookbar *n)`

## `src/apps/wubuoffice/src/wubusmartart/wubusmartart.h`

- `wubusmartart_destroy(wubusmartart *s)`
- `wubusmartart_set_layout(wubusmartart *s, wubusa_layout layout)`
- `wubusmartart_layout(const wubusmartart *s)`
- `wubusmartart_add_node(wubusmartart *s, const char *text)`
- `wubusmartart_count(const wubusmartart *s)`

## `src/apps/wubuoffice/src/wubumailexport/wubumailexport.h`

- `wubumailexport_free(wubumailexport *m)`

## `src/apps/wubuoffice/src/wubugallery/wubugallery.h`

- `wubugallery_destroy(wubugallery *g)`

## `src/apps/wubuoffice/src/wubucol/col.h`

- `col_destroy(Col *c)`
- `col_resolved(const Col *c, int tid)`
- `col_reply_count(const Col *c, int tid)`

## `src/apps/wubuoffice/src/wubutoc/toc.h`

- `toc_free(Toc *t)`
- `toc_count(const Toc *t)`
- `toc_level(const Toc *t, int i)`
- `toc_page(const Toc *t, int i)`

## `src/apps/wubuoffice/src/wubuhyperlink/wubuhyperlink.h`

- `wubuhyperlink_destroy(wubuhyperlink *h)`

## `src/apps/wubuoffice/src/wubuexp/exp.h`

- `wubuexp_pdf_geometry(const wubulayout_doc *L, const char *out)`
- `wubuexp_pdf(const wubulayout_doc *L, const char *out)`

## `src/apps/wubuoffice/src/wubucrdt/crdt.h`

- `crdt_destroy(Crdt *c)`
- `crdt_delete(Crdt *c, int pos)`
- `crdt_merge(Crdt *dst, const Crdt *src)`

## `src/apps/wubuoffice/src/wubupdfform/pdfform.h`

- `pdfform_build(const Form *form, uint8_t **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubua11y/a11y.h`

- `wubua11y_palette_aa(a11y_report *out)`

## `src/apps/wubuoffice/src/wubuscope/scope.h`

- `scope_destroy(ScopeMap *m)`
- `scope_count(const ScopeMap *m)`
- `scope_id_at(const ScopeMap *m, int i)`

## `src/apps/wubuoffice/src/wubusolver/wubusolver.h`

- `wubusolver_solve(const double *A, int n, const double *b, double *x)`
- `wubusolver_inv(const double *A, int n, double *inv)`

## `src/apps/wubuoffice/src/wubuoxml/docx_text.h`

- `wubuoxml_docx_text(const uint8_t *xml, size_t len, char **out)`
- `wubuoxml_pptx_text(const uint8_t *xml, size_t len, char **out)`

## `src/apps/wubuoffice/src/wubuoxml/extract.h`

- `wubuoxml_pptx_text(const uint8_t *xml, size_t len, char **out)`

## `src/apps/wubuoffice/src/wubuoxml/package.h`

- `wubuoxml_add_default_type(wubuoxml_package *p, const char *ext, const char *ct)`
- `wubuoxml_add_override(wubuoxml_package *p, const char *part, const char *ct)`
- `wubuoxml_add_relationship(wubuoxml_package *p, const char *source, const char *target, const char *rtype)`
- `wubuoxml_add_part(wubuoxml_package *p, const char *path, const void *data, size_t size)`
- `wubuoxml_finalize(wubuoxml_package *p)`

## `src/apps/wubuoffice/src/wubugridline/wubugridline.h`

- `wubugridline_init(wubugridline *g)`
- `wubugridline_toggle(wubugridline *g)`
- `wubugridline_set(wubugridline *g, int show, int print)`

## `src/apps/wubuoffice/src/wubumasterdoc/wubumasterdoc.h`

- `wubumasterdoc_destroy(wubumasterdoc *m)`

## `src/apps/wubuoffice/src/wubusandbox/sandbox.h`

- `sandbox_destroy(Sandbox *s)`

## `src/apps/wubuoffice/src/wubulayout/ublayout.h`

- `wubulayout_destroy(wubulayout_doc *L)`
- `wubulayout_invalidate(wubulayout_doc *L, void *block)`
- `wubulayout_box_count(const wubulayout_doc *L, int page)`

## `src/apps/wubuoffice/src/wubueqnum/eqnum.h`

- `eqnum_destroy(EqNum *e)`
- `eqnum_count(EqNum *e)`

## `src/apps/wubuoffice/src/wuburuler/wuburuler.h`

- `wuburuler_init(wuburuler *r, double pw, double ph)`
- `wuburuler_set_margins(wuburuler *r, double l, double rgt, double t, double b)`

## `src/apps/wubuoffice/src/wubuvars/vars.h`

- `vars_destroy(Vars *v)`
- `vars_count(const Vars *v)`

## `src/apps/wubuoffice/src/wubuautosave/autosave.h`

- `wubuautosave_destroy(Autosave *a)`
- `wubuautosave_tick(Autosave *a, const wubumodel_doc *doc)`
- `wubuautosave_clear(Autosave *a)`
- `wubuautosave_has_recovery(const char *doc_path)`
- `wubuautosave_recover(const char *doc_path, wubumodel_doc **out)`
- `wubuautosave_discard_recovery(const char *doc_path)`

## `src/apps/wubuoffice/src/wubufocus/focus.h`

- `focus_destroy(Focus *f)`
- `focus_set_color(Focus *f, unsigned char r, unsigned char g, unsigned char b, unsigned char a)`
- `focus_set_width(Focus *f, int px)`
- `focus_set_enabled(Focus *f, int on)`
- `focus_color(const Focus *f)`
- `focus_width(const Focus *f)`
- `focus_enabled(const Focus *f)`

## `src/apps/wubuoffice/src/wubucaption/caption.h`

- `caption_destroy(CaptionMap *m)`
- `caption_set(CaptionMap *m, uint64_t node_id, const char *text)`
- `caption_count(const CaptionMap *m)`
- `caption_id_at(const CaptionMap *m, int i)`

## `src/apps/wubuoffice/src/wubucsv/csv.h`

- `csv_destroy(Csv *c)`

## `src/apps/wubuoffice/src/wubuchart/chart.h`

- `chart_set_subtitle_formula(Chart *c, const char *formula)`

## `src/apps/wubuoffice/src/wubumasterslide/wubumasterslide.h`

- `wubumasterslide_destroy(wubumasterslide *m)`
- `wubumasterslide_resolve(const wubumasterslide *m, wubums_palette *out)`

## `src/apps/wubuoffice/src/wubusync/sync.h`

- `sync_get(Sync *s, const char *key, char **out_blob, size_t *out_len)`
- `sync_lock(Sync *s, const char *key, int pid, const char *site)`

## `src/apps/wubuoffice/src/wubudoc/agent.h`

- `doc_agent_serve(DocSession *s, FILE *in, FILE *out)`

## `src/apps/wubuoffice/src/wubudoc/b64.h`

- `b64_encode(const uint8_t *in, size_t n, char *out)`

## `src/apps/wubuoffice/src/wubudoc/wubudoc.h`

- `doc_open(DocSession *s, const char *path)`
- `doc_ingest_bytes(DocSession *s, const char *type, const uint8_t *data, size_t len)`
- `doc_open_mem(DocSession *s, const uint8_t *data, size_t len)`
- `doc_open_auto(DocSession *s, const uint8_t *data, size_t len)`
- `doc_set_model(DocSession *s, long id, void *model)`
- `doc_add_media(DocSession *s, long id, const char *name, const uint8_t *data, size_t len)`
- `doc_create(DocSession *s, long id, const char *format, const char *out_path)`
- `doc_agent_serve(DocSession *s, FILE *in, FILE *out)`

## `src/apps/wubuoffice/src/wubuencrypt/wubuencrypt.h`

- `wubuencrypt_pkcs7_pad(const unsigned char *in, size_t n, unsigned char *out)`
- `wubuencrypt_pkcs7_unpad(const unsigned char *in, size_t padded_len, unsigned char *out)`

## `src/apps/wubuoffice/src/wubuhistory/history.h`

- `history_destroy(History *h)`

## `src/apps/wubuoffice/src/wubuthesaurus/wubuthesaurus.h`

- `wubuthesaurus_destroy(wubuthesaurus *t)`

## `src/apps/wubuoffice/src/wubuexp_png/exp_png.h`

- `exp_png_write(const char *path, int fmt, const void *pixels, int W, int H)`

## `src/apps/wubuoffice/src/wubuocr/binarize.h`

- `ocr_otsu_threshold(const OcrImage *im)`
- `ocr_binary_despeckle(OcrBinary *b)`

## `src/apps/wubuoffice/src/wubuocr/conv_gemm.h`

- `conv_im2col(const float *src, int H, int W, int K_in, int S, int Oh, int Ow, float *col)`
- `conv_gemm_fwd(float *C, const float *A, const float *B, int M, int N, int K, float beta)`
- `conv_gemm_dW(float *C, const float *A, const float *B, int M, int N, int K, float beta)`
- `conv_gemm_dX(float *C, const float *A, const float *B, int M, int N, int K, float beta)`

## `src/apps/wubuoffice/src/wubuocr/convnet.h`

- `convnet_destroy(ConvNet *cn)`
- `convnet_dim(const ConvNet *cn)`
- `convnet_forward(const ConvNet *cn, const float *img, float *out_features)`
- `convnet_layer_count(const ConvNet *cn)`
- `convnet_layer(ConvNet *cn, int idx)`

## `src/apps/wubuoffice/src/wubuocr/convnet3.h`

- `convnet3_destroy(ConvNet3 *cn)`
- `convnet3_dim(const ConvNet3 *cn)`
- `convnet3_zero_grad(ConvNet3 *cn)`
- `convnet3_scale_grad(ConvNet3 *cn, float s)`
- `convnet3_add_grad(ConvNet3 *dst, const ConvNet3 *src)`
- `convnet3_forward(const ConvNet3 *cn, const float *img, float *out_features)`
- `convnet3_backward(ConvNet3 *cn, const float *img, const float *feat, const float *dfeat)`
- `convnet3_gap(const ConvNet3 *cn, const float *features, float *gap_out, int *H, int *W, int *C)`
- `convnet3_layer(ConvNet3 *cn, int idx)`
- `convnet3_dbg_c(ConvNet3 *cn, int stage, float *out)`
- `convnet3_save(const ConvNet3 *cn, const char *path)`
- `convnet3_load(const char *path, ConvNet3 **out, ConvConfig3 *cfg)`
- `convnet3_cbam_size(const ConvNet3 *cn)`
- `convnet3_cbam_pack(const ConvNet3 *cn, float *out)`
- `convnet3_cbam_unpack(ConvNet3 *cn, const float *in)`
- `convnet3_enable_cbam(ConvNet3 *cn)`
- `convnet3_sgd_cbam(ConvNet3 *cn, float lr)`
- `convnet3_cbam_enabled(const ConvNet3 *cn)`
- `convnet3_set_leak(ConvNet3 *cn, float leak)`
- `convnet3_inorm_enabled(const ConvNet3 *cn)`

## `src/apps/wubuoffice/src/wubuocr/crnn.h`

- `crnn_free(CRNN *m)`
- `crnn_time_steps(const CRNN *m, int img_w)`
- `crnn_feat_dim(const CRNN *m)`
- `crnn_set_freeze_conv(CRNN *m, int freeze)`
- `crnn_set_stride(CRNN *m, int stride)`
- `crnn_get_stride(const CRNN *m)`
- `crnn_predict(CRNN *m, const OcrImage *img, int *out)`
- `crnn_save(const CRNN *m, const char *path)`
- `crnn_load(const char *path, CRNN **out)`
- `crnn_forward_img(CRNN *m, const OcrImage *img, float *logits)`
- `crnn_forward_seq(CRNN *m, int T, const float *seq_feats, float *logits)`
- `crnn_adam(CRNN *m, float lr, int t)`

## `src/apps/wubuoffice/src/wubuocr/crnn_transcribe.h`

- `wubuocr_nfc_latin(const char *in, char *out, size_t outsz)`
- `wubuocr_detect_math_line(const char *s)`

## `src/apps/wubuoffice/src/wubuocr/crnn_transcribe_internal.h`

- `is_ink(uint8_t g, int bg)`
- `precompose(unsigned int base, unsigned int mark)`
- `detect_math_line(const char *s)`
- `ba_append(char **buf, size_t *len, size_t *cap, const char *s, size_t n)`
- `ba_append_str(char **buf, size_t *len, size_t *cap, const char *s)`
- `ba_append_json_escaped(char **buf, size_t *len, size_t *cap, const char *s)`
- `row_paired_ink(const OcrImage *page, int y, int bg, int W)`

## `src/apps/wubuoffice/src/wubuocr/ctc.h`

- `ctc_greedy_decode(int T, int C, const float *logits, int *out)`
- `ctc_beam_decode(int T, int C, const float *logits, int beam, int *out)`

## `src/apps/wubuoffice/src/wubuocr/dft.h`

- `dft2d(const uint8_t *px, int W, int H, double *re, double *im)`
- `idft2d(const double *re, const double *im, int W, int H, uint8_t *out)`
- `dft_features(const double *re, const double *im, int W, int H, double *out)`
- `dft_ratio(int N, int keep)`

## `src/apps/wubuoffice/src/wubuocr/docfmt.h`

- `docfmt_to_xlsx(const char *json, char **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubuocr/goldplace.h`

- `goldplace_spiral(GoldPoint *pts, size_t npts, int W, int H)`
- `goldplace_subdivide(GoldRect *rects, size_t nrects, int W, int H)`

## `src/apps/wubuoffice/src/wubuocr/gru.h`

- `gru_free(GRU *r)`
- `gru_forward(GRU *r, int T, const float *x)`
- `gru_backward(GRU *r, int T, const float *dy, float *dx)`
- `gru_outdim(const GRU *r)`
- `gru_num_params(const GRU *r)`

## `src/apps/wubuoffice/src/wubuocr/image.h`

- `ocr_image_free(OcrImage *im)`
- `ocr_image_width(const OcrImage *im)`
- `ocr_image_height(const OcrImage *im)`
- `ocr_image_get(const OcrImage *im, size_t x, size_t y)`
- `ocr_image_to_pgm(const OcrImage *im, uint8_t **out, size_t *out_len)`

## `src/apps/wubuoffice/src/wubuocr/latin1.h`

- `ocr_classes_english_latin(const char **out)`

## `src/apps/wubuoffice/src/wubuocr/layout.h`

- `ocr_layout_free(OcrLayout *L)`

## `src/apps/wubuoffice/src/wubuocr/lexicon.h`

- `lex_free(Lexicon *lx)`
- `lex_charset_size(const Lexicon *lx)`
- `lex_sample(const Lexicon *lx, uint32_t *rng)`
- `lex_charset(const Lexicon *lx, uint32_t *out_cps, int cap)`
- `lex_class_of(const Lexicon *lx, uint32_t cp)`
- `lex_cp_of_class(const Lexicon *lx, int cls)`
- `lex_correct(const Lexicon *lx, const char *utf8, int len_slack, int *out_dist)`
- `utf8_decode(const char *s, uint32_t *cp)`
- `utf8_nfkc(const char *in, char *out, int cap)`

## `src/apps/wubuoffice/src/wubuocr/mlp.h`

- `mlp_forward(const MLP *m, const float *z, float *out_scores)`
- `mlp_backward(MLP *m, const float *z, int target)`
- `mlp_backward_smooth(MLP *m, const float *z, int target, float smooth)`
- `mlp_input_grad(MLP *m, const float *z, float *out_dz)`
- `mlp_scale_grad(MLP *m, float s)`
- `mlp_layer_count(const MLP *m)`
- `mlp_layer(MLP *m, int idx)`

## `src/apps/wubuoffice/src/wubuocr/qr.h`

- `qr_encode(const char *text, unsigned char **out, int *out_size)`
- `qr_decode_matrix(const unsigned char *matrix, int size, char *out, int outcap)`

## `src/apps/wubuoffice/src/wubuocr/recognize.h`

- `ocr_templates_finalize(OcrTemplates *t)`
- `ocr_templates_set_struct(OcrTemplates *t, double ar_w, double hole_w)`
- `ocr_templates_free(OcrTemplates *t)`

## `src/apps/wubuoffice/src/wubuocr/rnn.h`

- `lstm_free(LSTM *r)`
- `lstm_forward(LSTM *r, int T, const float *x)`
- `lstm_backward(LSTM *r, int T, const float *dy, float *dx)`
- `lstm_outdim(const LSTM *r)`
- `lstm_num_params(const LSTM *r)`

## `src/apps/wubuoffice/src/wubuocr/stylebank.h`

- `stylebank_forward(const StyleBank *b, const float *img, float *out_scores)`

## `src/apps/wubuoffice/src/wubuocr/unicode.h`

- `ocr_unicode_classes(const char **out, size_t out_cap, int include_cjk)`

## `src/apps/wubuoffice/src/wubuocr/wubuocr.h`

- `ocr_page_glyph_count(const OcrPage *pg, size_t bi)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lauxlib.h`

- `void(luaL_buffinit) (lua_State *L, luaL_Buffer *B)`
- `void(luaL_addlstring) (luaL_Buffer *B, const char *s, size_t l)`
- `void(luaL_addstring) (luaL_Buffer *B, const char *s)`
- `void(luaL_addvalue) (luaL_Buffer *B)`
- `void(luaL_pushresult) (luaL_Buffer *B)`
- `void(luaL_pushresultsize) (luaL_Buffer *B, size_t sz)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/ldebug.h`

- `luaG_getfuncline(const Proto *f, int pc)`
- `luaG_callerror(lua_State *L, const TValue *o)`
- `luaG_runerror(lua_State *L, const char *fmt, ...)`
- `luaG_errormsg(lua_State *L)`
- `luaG_traceexec(lua_State *L, const Instruction *pc)`
- `luaG_tracecall(lua_State *L)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/llex.h`

- `luaX_init(lua_State *L)`
- `luaX_next(LexState *ls)`
- `luaX_lookahead(LexState *ls)`
- `luaX_syntaxerror(LexState *ls, const char *s)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lmem.h`

- `luaM_toobig(lua_State *L)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lparser.h`

- `luaY_nvarstack(FuncState *fs)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lstring.h`

- `luaS_hash(const char *str, size_t l, unsigned int seed)`
- `luaS_hashlongstr(TString *ts)`
- `luaS_eqlngstr(TString *a, TString *b)`
- `luaS_resize(lua_State *L, int newsize)`
- `luaS_clearcache(global_State *g)`
- `luaS_init(lua_State *L)`
- `luaS_remove(lua_State *L, TString *ts)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/ltests.h`

- `lua_checkmemory(lua_State *L)`
- `lua_printobj(lua_State *L, struct GCObject *o)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/ltm.h`

- `luaT_init(lua_State *L)`
- `luaT_tryconcatTM(lua_State *L)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lua.h`

- `void(lua_close) (lua_State *L)`
- `int(lua_closethread) (lua_State *L, lua_State *from)`
- `int(lua_resetthread) (lua_State *L)`
- `lua_CFunction(lua_atpanic) (lua_State *L, lua_CFunction panicf)`
- `lua_Number(lua_version) (lua_State *L)`
- `int(lua_absindex) (lua_State *L, int idx)`
- `int(lua_gettop) (lua_State *L)`
- `void(lua_settop) (lua_State *L, int idx)`
- `void(lua_pushvalue) (lua_State *L, int idx)`
- `void(lua_rotate) (lua_State *L, int idx, int n)`
- `void(lua_copy) (lua_State *L, int fromidx, int toidx)`
- `int(lua_checkstack) (lua_State *L, int n)`
- `void(lua_xmove) (lua_State *from, lua_State *to, int n)`
- `int(lua_isnumber) (lua_State *L, int idx)`
- `int(lua_isstring) (lua_State *L, int idx)`
- `int(lua_iscfunction) (lua_State *L, int idx)`
- `int(lua_isinteger) (lua_State *L, int idx)`
- `int(lua_isuserdata) (lua_State *L, int idx)`
- `int(lua_type) (lua_State *L, int idx)`
- `lua_Number(lua_tonumberx) (lua_State *L, int idx, int *isnum)`
- `lua_Integer(lua_tointegerx) (lua_State *L, int idx, int *isnum)`
- `int(lua_toboolean) (lua_State *L, int idx)`
- `lua_Unsigned(lua_rawlen) (lua_State *L, int idx)`
- `lua_CFunction(lua_tocfunction) (lua_State *L, int idx)`
- `void(lua_arith) (lua_State *L, int op)`
- `int(lua_rawequal) (lua_State *L, int idx1, int idx2)`
- `int(lua_compare) (lua_State *L, int idx1, int idx2, int op)`
- `void(lua_pushnil) (lua_State *L)`
- `void(lua_pushnumber) (lua_State *L, lua_Number n)`
- `void(lua_pushinteger) (lua_State *L, lua_Integer n)`
- `void(lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n)`
- `void(lua_pushboolean) (lua_State *L, int b)`
- `void(lua_pushlightuserdata) (lua_State *L, void *p)`
- `int(lua_pushthread) (lua_State *L)`
- `int(lua_getglobal) (lua_State *L, const char *name)`
- `int(lua_gettable) (lua_State *L, int idx)`
- `int(lua_getfield) (lua_State *L, int idx, const char *k)`
- `int(lua_geti) (lua_State *L, int idx, lua_Integer n)`
- `int(lua_rawget) (lua_State *L, int idx)`
- `int(lua_rawgeti) (lua_State *L, int idx, lua_Integer n)`
- `int(lua_rawgetp) (lua_State *L, int idx, const void *p)`
- `void(lua_createtable) (lua_State *L, int narr, int nrec)`
- `int(lua_getmetatable) (lua_State *L, int objindex)`
- `int(lua_getiuservalue) (lua_State *L, int idx, int n)`
- `void(lua_setglobal) (lua_State *L, const char *name)`
- `void(lua_settable) (lua_State *L, int idx)`
- `void(lua_setfield) (lua_State *L, int idx, const char *k)`
- `void(lua_seti) (lua_State *L, int idx, lua_Integer n)`
- `void(lua_rawset) (lua_State *L, int idx)`
- `void(lua_rawseti) (lua_State *L, int idx, lua_Integer n)`
- `void(lua_rawsetp) (lua_State *L, int idx, const void *p)`
- `int(lua_setmetatable) (lua_State *L, int objindex)`
- `int(lua_setiuservalue) (lua_State *L, int idx, int n)`
- `int(lua_dump) (lua_State *L, lua_Writer writer, void *data, int strip)`
- `int(lua_status)     (lua_State *L)`
- `int(lua_isyieldable) (lua_State *L)`
- `void(lua_setwarnf) (lua_State *L, lua_WarnFunction f, void *ud)`
- `void(lua_warning)  (lua_State *L, const char *msg, int tocont)`
- `int(lua_gc) (lua_State *L, int what, ...)`
- `int(lua_error) (lua_State *L)`
- `int(lua_next) (lua_State *L, int idx)`
- `void(lua_concat) (lua_State *L, int n)`
- `void(lua_len)    (lua_State *L, int idx)`
- `size_t(lua_stringtonumber) (lua_State *L, const char *s)`
- `lua_Alloc(lua_getallocf) (lua_State *L, void **ud)`
- `void(lua_setallocf) (lua_State *L, lua_Alloc f, void *ud)`
- `void(lua_toclose) (lua_State *L, int idx)`
- `void(lua_closeslot) (lua_State *L, int idx)`
- `int(lua_getstack) (lua_State *L, int level, lua_Debug *ar)`
- `int(lua_getinfo) (lua_State *L, const char *what, lua_Debug *ar)`
- `void(lua_sethook) (lua_State *L, lua_Hook func, int mask, int count)`
- `lua_Hook(lua_gethook) (lua_State *L)`
- `int(lua_gethookmask) (lua_State *L)`
- `int(lua_gethookcount) (lua_State *L)`
- `int(lua_setcstacklimit) (lua_State *L, unsigned int limit)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lvm.h`

- `luaV_equalobj(lua_State *L, const TValue *t1, const TValue *t2)`
- `luaV_lessthan(lua_State *L, const TValue *l, const TValue *r)`
- `luaV_lessequal(lua_State *L, const TValue *l, const TValue *r)`
- `luaV_tonumber_(const TValue *obj, lua_Number *n)`
- `luaV_tointeger(const TValue *obj, lua_Integer *p, F2Imod mode)`
- `luaV_flttointeger(lua_Number n, lua_Integer *p, F2Imod mode)`
- `luaV_finishOp(lua_State *L)`
- `luaV_execute(lua_State *L, CallInfo *ci)`
- `luaV_concat(lua_State *L, int total)`
- `luaV_idiv(lua_State *L, lua_Integer x, lua_Integer y)`
- `luaV_mod(lua_State *L, lua_Integer x, lua_Integer y)`
- `luaV_modf(lua_State *L, lua_Number x, lua_Number y)`
- `luaV_shiftl(lua_Integer x, lua_Integer y)`
- `luaV_objlen(lua_State *L, StkId ra, const TValue *rb)`

## `src/lua/www/~WuBu~/thirdparty/lua5.4/lzio.h`

- `luaZ_read(ZIO* z, void *b, size_t n)`
- `luaZ_fill(ZIO *z)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/mjbatch.h`

- `mjb_defaultConfig(mjBatchConfig* config)`
- `mjb_deleteBatch(mjBatch* b)`
- `mjb_nstate(const mjBatch* b)`
- `mjb_state(mjBatch* b)`
- `mjb_stateComponent(mjBatch* b, int component, int* size)`
- `mjb_warning(mjBatch* b)`
- `mjb_step(mjBatch* b, const int* ids, int nid, int nstep, int stage)`
- `mjb_forward(mjBatch* b, const int* ids, int nid, int stage)`
- `mjb_reset(mjBatch* b, const int* ids, int nid, const int* key, int stage)`
- `mjb_status(const mjBatch* b)`
- `mjb_error(const mjBatch* b, int sim)`
- `mjb_output(mjBatch* b, const char* name, int* size, int* elemsize)`
- `mjb_expand(mjBatch* b, const char* name, int* size, int* elemsize)`
- `mjb_expanded(mjBatch* b, const char* name, int* size, int* elemsize)`
- `mjb_setConst(mjBatch* b, const int* ids, int nid)`
- `mjb_setModels(mjBatch* b, const mjModel* const* models)`
- `mjb_getModel(mjBatch* b, int sim, mjModel* m)`
- `mjb_getData(mjBatch* b, int sim, mjData* d)`
- `mjb_nsim(const mjBatch* b)`
- `mjb_nthread(const mjBatch* b)`
- `mjb_persistent(const mjBatch* b)`
- `mjb_model(const mjBatch* b)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/mjrfilament.h`

- `mjrf_defaultContextConfig(mjrfContextConfig* config)`
- `mjrf_createContext(const mjrfContextConfig* config)`
- `mjrf_destroyContext(mjrfContext* ctx)`
- `mjrf_getRendererInfo(mjrfContext* ctx, mjrRendererInfo* info)`
- `mjrf_defaultShadowOpts(mjrfShadowOpts* opts)`
- `mjrf_defaultMaterialOpts(mjrfMaterialOpts* opts)`
- `mjrf_defaultHeadlightOpts(mjrfHeadlightOpts* opts)`
- `mjrf_defaultSkyboxOpts(mjrfSkyboxOpts* opts)`
- `mjrf_defaultHazeOpts(mjrfHazeOpts* opts)`
- `mjrf_defaultFogOpts(mjrfFogOpts* opts)`
- `mjrf_defaultBloomOpts(mjrfBloomOpts* opts)`
- `mjrf_defaultVignetteOpts(mjrfVignetteOpts* opts)`
- `mjrf_defaultReflectionOpts(mjrfReflectionOpts* opts)`
- `mjrf_defaultDepthOfFieldOpts(mjrfDepthOfFieldOpts* opts)`
- `mjrf_defaultDitheringOpts(mjrfDitheringOpts* opts)`
- `mjrf_defaultColorGradingOpts(mjrfColorGradingOpts* opts)`
- `mjrf_defaultAntialiasingOpts(mjrfAntialiasingOpts* opts)`
- `mjrf_defaultAmbientOcclusionOpts(mjrfAmbientOcclusionOpts* opts)`
- `mjrf_defaultRenderOpts(mjrfRenderOpts* opts)`
- `mjrf_defaultRenderRequest(mjrfRenderRequest* request)`
- `mjrf_defaultReadRequest(mjrfReadRequest* request)`
- `mjrf_waitForFrame(mjrfContext* ctx, mjrfFrameHandle frame)`
- `mjrf_flush(mjrfContext* ctx)`
- `mjrf_setClearColor(mjrfContext* ctx, const float color[3])`
- `mjrf_defaultFrameStats(mjrfFrameStats* stats)`
- `mjrf_getFrameStats(mjrfContext* ctx, mjrfFrameHandle frame, mjrfFrameStats* stats_out)`
- `mjrf_defaultTextureConfig(mjrfTextureConfig* config)`
- `mjrf_createTexture(mjrfContext* ctx, const mjrfTextureConfig* config)`
- `mjrf_destroyTexture(mjrfTexture* texture)`
- `mjrf_defaultTextureData(mjrfTextureData* data)`
- `mjrf_uploadTexture(mjrfTexture* texture, const mjrfTextureData* data)`
- `mjrf_getTextureWidth(const mjrfTexture* texture)`
- `mjrf_getTextureHeight(const mjrfTexture* texture)`
- `mjrf_getTextureSamplerType(const mjrfTexture* texture)`
- `mjrf_defaultMeshConfig(mjrfMeshConfig* config)`
- `mjrf_createMesh(mjrfContext* ctx, const mjrfMeshConfig* config)`
- `mjrf_destroyMesh(mjrfMesh* mesh)`
- `mjrf_defaultMeshData(mjrfMeshData* data)`
- `mjrf_uploadMesh(mjrfMesh* mesh, const mjrfMeshData* data)`
- `mjrf_defaultSceneParams(mjrfSceneParams* params)`
- `mjrf_createScene(mjrfContext* ctx, const mjrfSceneParams* params)`
- `mjrf_destroyScene(mjrfScene* scene)`
- `mjrf_addSceneLight(mjrfScene* scene, mjrfLight* light)`
- `mjrf_removeSceneLight(mjrfScene* scene, mjrfLight* light)`
- `mjrf_addSceneRenderable(mjrfScene* scene, mjrfRenderable* renderable)`
- `mjrf_removeSceneRenderable(mjrfScene* scene, mjrfRenderable* renderable)`
- `mjrf_setSceneSkybox(mjrfScene* scene, const mjrfTexture* texture)`
- `mjrf_configureSceneFromModel(mjrfScene* scene, const mjModel* model)`
- `mjrf_setSceneMsaa(mjrfScene* scene, int samples)`
- `mjrf_defaultLightParams(mjrfLightParams* params)`
- `mjrf_createLight(mjrfContext* ctx, const mjrfLightParams* params)`
- `mjrf_destroyLight(mjrfLight* light)`
- `mjrf_setLightActive(mjrfLight* light, mjtBool active)`
- `mjrf_setLightCastShadow(mjrfLight* light, mjtBool enabled)`
- `mjrf_setLightColor(mjrfLight* light, const float color[3])`
- `mjrf_setLightIntensity(mjrfLight* light, float intensity)`
- `mjrf_setLightRange(mjrfLight* light, float range)`
- `mjrf_setLightCutoff(mjrfLight* light, float cutoff)`
- `mjrf_setLightSoftness(mjrfLight* light, float softness)`
- `mjrf_setLightBulbRadius(mjrfLight* light, float radius)`
- `mjrf_setLightShadowMapSize(mjrfLight* light, int map_size)`
- `mjrf_setLightPose(mjrfLight* light, const float position[3], const float direction[3])`
- `mjrf_getLightType(const mjrfLight* light)`
- `mjrf_defaultMaterial(mjrfMaterial* material)`
- `mjrf_defaultRenderableParams(mjrfRenderableParams* params)`
- `mjrf_createRenderable(mjrfContext* ctx, const mjrfRenderableParams* params)`
- `mjrf_destroyRenderable(mjrfRenderable* renderable)`
- `mjrf_setRenderableMaterial(mjrfRenderable* renderable, const mjrfMaterial* material)`
- `mjrf_getRenderableMaterial(mjrfRenderable* renderable, mjrfMaterial* material)`
- `mjrf_setRenderableSize(mjrfRenderable* renderable, const float size[3])`
- `mjrf_setRenderableCastShadow(mjrfRenderable* renderable, mjtBool enabled)`
- `mjrf_defaultRenderTargetConfig(mjrfRenderTargetConfig* config)`
- `mjrf_createRenderTarget(mjrfContext* ctx, const mjrfRenderTargetConfig* config)`
- `mjrf_destroyRenderTarget(mjrfRenderTarget* render_target)`
- `mjrf_resizeRenderTarget(mjrfRenderTarget* render_target, int width, int height)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/mjsan.h`

- `mj__markStack(mjData*) __attribute__((noinline))`
- `volatile("" ::: "memory")`
- `volatile("" ::: "memory")`
- `mj__freeStack(mjData*) __attribute__((noinline))`
- `volatile("" ::: "memory")`
- `volatile("" ::: "memory")`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/mujoco.h`

- `void(*mju_user_free)(void*)`
- `void(*mju_user_error)(const char*)`
- `void(*mju_user_warning)(const char*)`
- `mj_defaultVFS(mjVFS* vfs)`
- `mj_mountVFS(mjVFS* vfs, const char* filepath, const mjpResourceProvider* provider)`
- `mj_unmountVFS(mjVFS* vfs, const char* filename)`
- `mj_addFileVFS(mjVFS* vfs, const char* directory, const char* filename)`
- `mj_addBufferVFS(mjVFS* vfs, const char* name, const void* buffer, int nbuffer)`
- `mj_deleteFileVFS(mjVFS* vfs, const char* filename)`
- `mj_containsBufferVFS(mjVFS* vfs, const char* name)`
- `mj_containsFileVFS(mjVFS* vfs, const char* directory, const char* filename)`
- `mj_deleteVFS(mjVFS* vfs)`
- `mj_getCacheSize(const mjCache* cache)`
- `mj_getCacheCapacity(const mjCache* cache)`
- `mj_setCacheCapacity(mjCache* cache, size_t size)`
- `mj_getCache(void)`
- `mj_clearCache(mjCache* cache)`
- `mj_loadXML(const char* filename, const mjVFS* vfs, char* error, int error_sz)`
- `mj_parseXML(const char* filename, const mjVFS* vfs, char* error, int error_sz)`
- `mj_parseXMLString(const char* xml, const mjVFS* vfs, char* error, int error_sz)`
- `mj_compile(mjSpec* s, const mjVFS* vfs)`
- `mj_copyBack(mjSpec* s, const mjModel* m)`
- `mj_recompile(mjSpec* s, const mjVFS* vfs, mjModel* m, mjData* d)`
- `mj_embedAssets(const mjModel* m, char* error, int error_sz)`
- `mj_saveLastXML(const char* filename, const mjModel* m, char* error, int error_sz)`
- `mj_freeLastXML(void)`
- `mj_saveXMLString(const mjSpec* s, char* xml, int xml_sz, char* error, int error_sz)`
- `mj_saveXML(const mjSpec* s, const char* filename, char* error, int error_sz)`
- `mju_getXMLDependencies(const char* filename, mjStringVec* dependencies)`
- `mj_step(const mjModel* m, mjData* d)`
- `mj_step1(const mjModel* m, mjData* d)`
- `mj_step2(const mjModel* m, mjData* d)`
- `mj_forward(const mjModel* m, mjData* d)`
- `mj_inverse(const mjModel* m, mjData* d)`
- `mj_forwardSkip(const mjModel* m, mjData* d, int skipstage, int skipsensor)`
- `mj_inverseSkip(const mjModel* m, mjData* d, int skipstage, int skipsensor)`
- `mj_defaultLROpt(mjLROpt* opt)`
- `mj_defaultSolRefImp(mjtNum* solref, mjtNum* solimp)`
- `mj_defaultOption(mjOption* opt)`
- `mj_defaultVisual(mjVisual* vis)`
- `mj_copyModel(mjModel* dest, const mjModel* src)`
- `mj_saveModel(const mjModel* m, const char* filename, void* buffer, mjtSize buffer_sz)`
- `mj_loadModel(const char* filename, const mjVFS* vfs)`
- `mj_loadModelBuffer(const void* buffer, mjtSize buffer_sz)`
- `mj_deleteModel(mjModel* m)`
- `mj_sizeModel(const mjModel* m)`
- `mj_makeData(const mjModel* m)`
- `mj_copyData(mjData* dest, const mjModel* m, const mjData* src)`
- `mjv_copyData(mjData* dest, const mjModel* m, const mjData* src)`
- `mj_resetCtrl(const mjModel* m, mjData* d)`
- `mj_resetData(const mjModel* m, mjData* d)`
- `mj_resetDataDebug(const mjModel* m, mjData* d, unsigned char debug_value)`
- `mj_resetDataKeyframe(const mjModel* m, mjData* d, int key)`
- `mj_markStack(mjData* d)`
- `mj_freeStack(mjData* d)`
- `mj_stackAllocByte(mjData* d, size_t bytes, size_t alignment)`
- `mj_stackAllocNum(mjData* d, size_t size)`
- `mj_stackAllocInt(mjData* d, size_t size)`
- `mj_deleteData(mjData* d)`
- `mj_resetCallbacks(void)`
- `mj_setConst(mjModel* m, mjData* d)`
- `mj_makeSpec(void)`
- `mj_copySpec(const mjSpec* s)`
- `mj_deleteSpec(mjSpec* s)`
- `mjs_activatePlugin(mjSpec* s, const char* name)`
- `mjs_setDeepCopy(mjSpec* s, int deepcopy)`
- `mj_printFormattedModel(const mjModel* m, const char* filename, const char* float_format)`
- `mj_printModel(const mjModel* m, const char* filename)`
- `mj_printData(const mjModel* m, const mjData* d, const char* filename)`
- `mju_printMat(const mjtNum* mat, int nr, int nc)`
- `mj_printScene(const mjvScene* s, const char* filename)`
- `mj_fwdKinematics(const mjModel* m, mjData* d)`
- `mj_fwdPosition(const mjModel* m, mjData* d)`
- `mj_fwdVelocity(const mjModel* m, mjData* d)`
- `mj_fwdActuation(const mjModel* m, mjData* d)`
- `mj_fwdAcceleration(const mjModel* m, mjData* d)`
- `mj_fwdConstraint(const mjModel* m, mjData* d)`
- `mj_Euler(const mjModel* m, mjData* d)`
- `mj_RungeKutta(const mjModel* m, mjData* d, int N)`
- `mj_implicit(const mjModel* m, mjData* d)`
- `mj_invPosition(const mjModel* m, mjData* d)`
- `mj_invVelocity(const mjModel* m, mjData* d)`
- `mj_invConstraint(const mjModel* m, mjData* d)`
- `mj_compareFwdInv(const mjModel* m, mjData* d)`
- `mj_sensorPos(const mjModel* m, mjData* d)`
- `mj_sensorVel(const mjModel* m, mjData* d)`
- `mj_sensorAcc(const mjModel* m, mjData* d)`
- `mj_energyPos(const mjModel* m, mjData* d)`
- `mj_energyVel(const mjModel* m, mjData* d)`
- `mj_checkPos(const mjModel* m, mjData* d)`
- `mj_checkVel(const mjModel* m, mjData* d)`
- `mj_checkAcc(const mjModel* m, mjData* d)`
- `mj_kinematics(const mjModel* m, mjData* d)`
- `mj_comPos(const mjModel* m, mjData* d)`
- `mj_camlight(const mjModel* m, mjData* d)`
- `mj_flex(const mjModel* m, mjData* d)`
- `mj_tendon(const mjModel* m, mjData* d)`
- `mj_transmission(const mjModel* m, mjData* d)`
- `mj_crb(const mjModel* m, mjData* d)`
- `mj_makeM(const mjModel* m, mjData* d)`
- `mj_factorM(const mjModel* m, mjData* d)`
- `mj_solveM(const mjModel* m, mjData* d, mjtNum* x, const mjtNum* y, int n)`
- `mj_comVel(const mjModel* m, mjData* d)`
- `mj_passive(const mjModel* m, mjData* d)`
- `mj_subtreeVel(const mjModel* m, mjData* d)`
- `mj_rne(const mjModel* m, mjData* d, int flg_acc, mjtNum* result)`
- `mj_rnePostConstraint(const mjModel* m, mjData* d)`
- `mj_maxContact(const mjModel* m, int g1, int g2, int has_margin)`
- `mj_collision(const mjModel* m, mjData* d)`
- `mj_makeConstraint(const mjModel* m, mjData* d)`
- `mj_island(const mjModel* m, mjData* d)`
- `mj_projectConstraint(const mjModel* m, mjData* d)`
- `mj_referenceConstraint(const mjModel* m, mjData* d)`
- `mj_stateSize(const mjModel* m, int sig)`
- `mj_getState(const mjModel* m, const mjData* d, mjtNum* state, int sig)`
- `mj_setState(const mjModel* m, mjData* d, const mjtNum* state, int sig)`
- `mj_copyState(const mjModel* m, const mjData* src, mjData* dst, int sig)`
- `mj_setKeyframe(mjModel* m, const mjData* d, int k)`
- `mj_addContact(const mjModel* m, mjData* d, const mjContact* con)`
- `mj_isPyramidal(const mjModel* m)`
- `mj_isSparse(const mjModel* m)`
- `mj_isDual(const mjModel* m)`
- `mj_mulJacVec(const mjModel* m, const mjData* d, mjtNum* res, const mjtNum* vec)`
- `mj_mulJacTVec(const mjModel* m, const mjData* d, mjtNum* res, const mjtNum* vec)`
- `mj_jacBody(const mjModel* m, const mjData* d, mjtNum* jacp, mjtNum* jacr, int body)`
- `mj_jacBodyCom(const mjModel* m, const mjData* d, mjtNum* jacp, mjtNum* jacr, int body)`
- `mj_jacSubtreeCom(const mjModel* m, mjData* d, mjtNum* jacp, int body)`
- `mj_jacGeom(const mjModel* m, const mjData* d, mjtNum* jacp, mjtNum* jacr, int geom)`
- `mj_jacSite(const mjModel* m, const mjData* d, mjtNum* jacp, mjtNum* jacr, int site)`
- `mj_angmomMat(const mjModel* m, mjData* d, mjtNum* mat, int body)`
- `mj_name2id(const mjModel* m, int type, const char* name)`
- `mj_id2name(const mjModel* m, int type, int id)`
- `mj_actuatorInputName(const mjModel* m, int id, int input)`
- `mj_fullM(const mjModel* m, const mjData* d, mjtNum* dst)`
- `mj_mulM(const mjModel* m, const mjData* d, mjtNum* res, const mjtNum* vec)`
- `mj_mulM2(const mjModel* m, const mjData* d, mjtNum* res, const mjtNum* vec)`
- `mj_addM(const mjModel* m, mjData* d, mjtNum* dst, int* rownnz, int* rowadr, int* colind)`
- `mj_insideSite(const mjModel* m, const mjData* d, int siteid, const mjtNum point[3])`
- `mj_contactForce(const mjModel* m, const mjData* d, int id, mjtNum result[6])`
- `mj_integratePos(const mjModel* m, mjtNum* qpos, const mjtNum* qvel, mjtNum dt)`
- `mj_normalizeQuat(const mjModel* m, mjtNum* qpos)`
- `mj_getTotalmass(const mjModel* m)`
- `mj_setTotalmass(mjModel* m, mjtNum newmass)`
- `mj_getPluginConfig(const mjModel* m, int plugin_id, const char* attrib)`
- `mj_loadPluginLibrary(const char* path)`
- `mj_loadAllPluginLibraries(const char* directory, mjfPluginLibraryLoadCallback callback)`
- `mj_version(void)`
- `mj_versionString(void)`
- `mjv_defaultCamera(mjvCamera* cam)`
- `mjv_defaultFreeCamera(const mjModel* m, mjvCamera* cam)`
- `mjv_defaultPerturb(mjvPerturb* pert)`
- `mjv_frustumHeight(const mjvScene* scn)`
- `mjv_alignToCamera(mjtNum res[3], const mjtNum vec[3], const mjtNum forward[3])`
- `mjv_moveCamera(const mjModel* m, int action, mjtNum reldx, mjtNum reldy, mjvCamera* cam)`
- `mjv_initPerturb(const mjModel* m, mjData* d, const mjvScene* scn, mjvPerturb* pert)`
- `mjv_applyPerturbForce(const mjModel* m, mjData* d, const mjvPerturb* pert)`
- `mjv_averageCamera(const mjvGLCamera* cam1, const mjvGLCamera* cam2)`
- `mjv_defaultOption(mjvOption* opt)`
- `mjv_defaultFigure(mjvFigure* fig)`
- `mjv_defaultScene(mjvScene* scn)`
- `mjv_makeScene(const mjModel* m, mjvScene* scn, int maxgeom)`
- `mjv_freeScene(mjvScene* scn)`
- `mjv_copyModel(mjModel* dest, const mjModel* src)`
- `mjv_makeLights(const mjModel* m, const mjData* d, mjvScene* scn)`
- `mjv_updateCamera(const mjModel* m, const mjData* d, mjvCamera* cam, mjvScene* scn)`
- `mjv_updateSkin(const mjModel* m, const mjData* d, mjvScene* scn)`
- `mjr_defaultContext(mjrContext* con)`
- `mjr_defaultRendererInfo(mjrRendererInfo* info)`
- `mjr_getRendererInfo(mjrRendererInfo* info)`
- `mjr_makeContext(const mjModel* m, mjrContext* con, int fontscale)`
- `mjr_changeFont(int fontscale, mjrContext* con)`
- `mjr_addAux(int index, int width, int height, int samples, mjrContext* con)`
- `mjr_freeContext(mjrContext* con)`
- `mjr_resizeOffscreen(int width, int height, mjrContext* con)`
- `mjr_uploadTexture(const mjModel* m, const mjrContext* con, int texid)`
- `mjr_uploadMesh(const mjModel* m, const mjrContext* con, int meshid)`
- `mjr_uploadHField(const mjModel* m, const mjrContext* con, int hfieldid)`
- `mjr_restoreBuffer(const mjrContext* con)`
- `mjr_setBuffer(int framebuffer, mjrContext* con)`
- `mjr_setAux(int index, const mjrContext* con)`
- `mjr_blitAux(int index, mjrRect src, int left, int bottom, const mjrContext* con)`
- `mjr_maxViewport(const mjrContext* con)`
- `mjr_rectangle(mjrRect viewport, float r, float g, float b, float a)`
- `mjr_figure(mjrRect viewport, mjvFigure* fig, const mjrContext* con)`
- `mjr_render(mjrRect viewport, mjvScene* scn, const mjrContext* con)`
- `mjr_finish(void)`
- `mjr_getError(void)`
- `mjr_findRect(int x, int y, int nrect, const mjrRect* rect)`
- `mjui_themeSpacing(int ind)`
- `mjui_themeColor(int ind)`
- `mjui_add(mjUI* ui, const mjuiDef* def)`
- `mjui_addToSection(mjUI* ui, int sect, const mjuiDef* def)`
- `mjui_resize(mjUI* ui, const mjrContext* con)`
- `mjui_event(mjUI* ui, mjuiState* state, const mjrContext* con)`
- `mjui_render(mjUI* ui, const mjuiState* state, const mjrContext* con)`
- `mju_error(const char* msg, ...) mjPRINTFLIKE(1, 2)`
- `mju_warning(const char* msg, ...) mjPRINTFLIKE(1, 2)`
- `mju_clearHandlers(void)`
- `mju_setLogHandler(mjfLogHandler handler)`
- `mju_getLogConfig(void)`
- `mju_setLogConfig(mjLogConfig config)`
- `mju_info(int topic, const char* msg, ...) mjPRINTFLIKE(2, 3)`
- `mju_message(const mjLogMessage* msg)`
- `mju_malloc(size_t size)`
- `mju_free(void* ptr)`
- `mj_warning(mjData* d, int warning, int info)`
- `mju_writeLog(const char* type, const char* msg)`
- `mjs_getError(mjSpec* s)`
- `mjs_getTimer(mjSpec* s)`
- `mjs_isWarning(mjSpec* s)`
- `mjs_numWarnings(const mjSpec* spec)`
- `mjs_getWarning(const mjSpec* spec, int index)`
- `mju_zero3(mjtNum res[3])`
- `mju_copy3(mjtNum res[3], const mjtNum data[3])`
- `mju_scl3(mjtNum res[3], const mjtNum vec[3], mjtNum scl)`
- `mju_add3(mjtNum res[3], const mjtNum vec1[3], const mjtNum vec2[3])`
- `mju_sub3(mjtNum res[3], const mjtNum vec1[3], const mjtNum vec2[3])`
- `mju_addTo3(mjtNum res[3], const mjtNum vec[3])`
- `mju_subFrom3(mjtNum res[3], const mjtNum vec[3])`
- `mju_addToScl3(mjtNum res[3], const mjtNum vec[3], mjtNum scl)`
- `mju_addScl3(mjtNum res[3], const mjtNum vec1[3], const mjtNum vec2[3], mjtNum scl)`
- `mju_normalize3(mjtNum vec[3])`
- `mju_norm3(const mjtNum vec[3])`
- `mju_dot3(const mjtNum vec1[3], const mjtNum vec2[3])`
- `mju_dist3(const mjtNum pos1[3], const mjtNum pos2[3])`
- `mju_mulMatVec3(mjtNum res[3], const mjtNum mat[9], const mjtNum vec[3])`
- `mju_mulMatTVec3(mjtNum res[3], const mjtNum mat[9], const mjtNum vec[3])`
- `mju_cross(mjtNum res[3], const mjtNum a[3], const mjtNum b[3])`
- `mju_zero4(mjtNum res[4])`
- `mju_unit4(mjtNum res[4])`
- `mju_copy4(mjtNum res[4], const mjtNum data[4])`
- `mju_normalize4(mjtNum vec[4])`
- `mju_zero(mjtNum* res, int n)`
- `mju_fill(mjtNum* res, mjtNum val, int n)`
- `mju_copy(mjtNum* res, const mjtNum* vec, int n)`
- `mju_sum(const mjtNum* vec, int n)`
- `mju_L1(const mjtNum* vec, int n)`
- `mju_scl(mjtNum* res, const mjtNum* vec, mjtNum scl, int n)`
- `mju_add(mjtNum* res, const mjtNum* vec1, const mjtNum* vec2, int n)`
- `mju_sub(mjtNum* res, const mjtNum* vec1, const mjtNum* vec2, int n)`
- `mju_addTo(mjtNum* res, const mjtNum* vec, int n)`
- `mju_subFrom(mjtNum* res, const mjtNum* vec, int n)`
- `mju_addToScl(mjtNum* res, const mjtNum* vec, mjtNum scl, int n)`
- `mju_addScl(mjtNum* res, const mjtNum* vec1, const mjtNum* vec2, mjtNum scl, int n)`
- `mju_normalize(mjtNum* res, int n)`
- `mju_norm(const mjtNum* res, int n)`
- `mju_dot(const mjtNum* vec1, const mjtNum* vec2, int n)`
- `mju_mulMatVec(mjtNum* res, const mjtNum* mat, const mjtNum* vec, int nr, int nc)`
- `mju_mulMatTVec(mjtNum* res, const mjtNum* mat, const mjtNum* vec, int nr, int nc)`
- `mju_mulVecMatVec(const mjtNum* vec1, const mjtNum* mat, const mjtNum* vec2, int n)`
- `mju_transpose(mjtNum* res, const mjtNum* mat, int nr, int nc)`
- `mju_symmetrize(mjtNum* res, const mjtNum* mat, int n)`
- `mju_eye(mjtNum* mat, int n)`
- `mju_sqrMatTD(mjtNum* res, const mjtNum* mat, const mjtNum* diag, int nr, int nc)`
- `mju_rotVecQuat(mjtNum res[3], const mjtNum vec[3], const mjtNum quat[4])`
- `mju_negQuat(mjtNum res[4], const mjtNum quat[4])`
- `mju_mulQuat(mjtNum res[4], const mjtNum quat1[4], const mjtNum quat2[4])`
- `mju_mulQuatAxis(mjtNum res[4], const mjtNum quat[4], const mjtNum axis[3])`
- `mju_axisAngle2Quat(mjtNum res[4], const mjtNum axis[3], mjtNum angle)`
- `mju_quat2Vel(mjtNum res[3], const mjtNum quat[4], mjtNum dt)`
- `mju_subQuat(mjtNum res[3], const mjtNum qa[4], const mjtNum qb[4])`
- `mju_quat2Mat(mjtNum res[9], const mjtNum quat[4])`
- `mju_mat2Quat(mjtNum quat[4], const mjtNum mat[9])`
- `mju_derivQuat(mjtNum res[4], const mjtNum quat[4], const mjtNum vel[3])`
- `mju_quatIntegrate(mjtNum quat[4], const mjtNum vel[3], mjtNum scale)`
- `mju_quatZ2Vec(mjtNum quat[4], const mjtNum vec[3])`
- `mju_mat2Rot(mjtNum quat[4], const mjtNum mat[9])`
- `mju_euler2Quat(mjtNum quat[4], const mjtNum euler[3], const char* seq)`
- `mju_cholFactor(mjtNum* mat, int n, mjtNum mindiag)`
- `mju_cholSolve(mjtNum* res, const mjtNum* mat, const mjtNum* vec, int n)`
- `mju_cholUpdate(mjtNum* mat, mjtNum* x, int n, int flg_plus)`
- `mju_dense2Band(mjtNum* res, const mjtNum* mat, int ntotal, int nband, int ndense)`
- `mju_bandDiag(int i, int ntotal, int nband, int ndense)`
- `mju_eig3(mjtNum eigval[3], mjtNum eigvec[9], mjtNum quat[4], const mjtNum mat[9])`
- `mju_muscleDynamics(mjtNum ctrl, mjtNum act, const mjtNum prm[3])`
- `mju_encodePyramid(mjtNum* pyramid, const mjtNum* force, const mjtNum* mu, int dim)`
- `mju_decodePyramid(mjtNum* force, const mjtNum* pyramid, const mjtNum* mu, int dim)`
- `mju_springDamper(mjtNum pos0, mjtNum vel0, mjtNum Kp, mjtNum Kv, mjtNum dt)`
- `mju_min(mjtNum a, mjtNum b)`
- `mju_max(mjtNum a, mjtNum b)`
- `mju_clip(mjtNum x, mjtNum min, mjtNum max)`
- `mju_sign(mjtNum x)`
- `mju_round(mjtNum x)`
- `mju_type2Str(int type)`
- `mju_str2Type(const char* str)`
- `mju_writeNumBytes(size_t nbytes)`
- `mju_warningText(int warning, size_t info)`
- `mju_isBad(mjtNum x)`
- `mju_isZero(const mjtNum* vec, int n)`
- `mju_standardNormal(mjtNum* num2)`
- `mju_f2n(mjtNum* res, const float* vec, int n)`
- `mju_n2f(float* res, const mjtNum* vec, int n)`
- `mju_d2n(mjtNum* res, const double* vec, int n)`
- `mju_n2d(double* res, const mjtNum* vec, int n)`
- `mju_insertionSort(mjtNum* list, int n)`
- `mju_insertionSortInt(int* list, int n)`
- `mju_Halton(int index, int base)`
- `mju_strncpy(char *dst, const char *src, int n)`
- `mju_sigmoid(mjtNum x)`
- `mjc_getSDF(const mjModel* m, int id)`
- `mjc_distance(const mjModel* m, const mjData* d, const mjSDF* s, const mjtNum x[3])`
- `mjd_subQuat(const mjtNum qa[4], const mjtNum qb[4], mjtNum Da[9], mjtNum Db[9])`
- `mjp_defaultPlugin(mjpPlugin* plugin)`
- `mjp_registerPlugin(const mjpPlugin* plugin)`
- `mjp_pluginCount(void)`
- `mjp_getPlugin(const char* name, int* slot)`
- `mjp_getPluginAtSlot(int slot)`
- `mjp_defaultProvider(mjpResourceProvider* provider)`
- `mjp_registerProvider(const mjpResourceProvider* provider)`
- `mjp_providerCount(void)`
- `mjp_findProvider(const char* resource_name)`
- `mjp_getProviderAtSlot(int slot)`
- `mjp_registerDecoder(const mjpDecoder* decoder)`
- `mjp_defaultDecoder(mjpDecoder* decoder)`
- `mjp_findDecoder(const mjResource* resource, const char* content_type)`
- `mjp_registerEncoder(const mjpEncoder* encoder)`
- `mjp_defaultEncoder(mjpEncoder* encoder)`
- `mjp_findEncoder(const char* filename, const char* content_type)`
- `mjp_registerArchiveProvider(const mjpResourceProvider* provider)`
- `mjp_findArchiveProvider(const char* resource_name)`
- `mjp_archiveProviderCount(void)`
- `mju_closeResource(mjResource* resource)`
- `mju_readResource(mjResource* resource, const void** buffer)`
- `mju_getResourceDir(mjResource* resource, const char** dir, int* ndir)`
- `mju_isModifiedResource(const mjResource* resource, const char* timestamp)`
- `mju_threadpool(mjData* d, int nthread)`
- `mjs_addBody(mjsBody* body, const mjsDefault* def)`
- `mjs_addSite(mjsBody* body, const mjsDefault* def)`
- `mjs_addJoint(mjsBody* body, const mjsDefault* def)`
- `mjs_addFreeJoint(mjsBody* body)`
- `mjs_addGeom(mjsBody* body, const mjsDefault* def)`
- `mjs_addCamera(mjsBody* body, const mjsDefault* def)`
- `mjs_addLight(mjsBody* body, const mjsDefault* def)`
- `mjs_addFrame(mjsBody* body, mjsFrame* parentframe)`
- `mjs_delete(mjSpec* spec, mjsElement* element)`
- `mjs_addActuator(mjSpec* s, const mjsDefault* def)`
- `mjs_addSensor(mjSpec* s)`
- `mjs_addFlex(mjSpec* s)`
- `mjs_addPair(mjSpec* s, const mjsDefault* def)`
- `mjs_addExclude(mjSpec* s)`
- `mjs_addEquality(mjSpec* s, const mjsDefault* def)`
- `mjs_addTendon(mjSpec* s, const mjsDefault* def)`
- `mjs_wrapSite(mjsTendon* tendon, const char* name)`
- `mjs_wrapGeom(mjsTendon* tendon, const char* name, const char* sidesite)`
- `mjs_wrapJoint(mjsTendon* tendon, const char* name, double coef)`
- `mjs_wrapPulley(mjsTendon* tendon, double divisor)`
- `mjs_addNumeric(mjSpec* s)`
- `mjs_addText(mjSpec* s)`
- `mjs_addTuple(mjSpec* s)`
- `mjs_addKey(mjSpec* s)`
- `mjs_addPlugin(mjSpec* s)`
- `mjs_addDefault(mjSpec* s, const char* classname, const mjsDefault* parent)`
- `mjs_setToMotor(mjsActuator* actuator)`
- `mjs_setToVelocity(mjsActuator* actuator, double kv)`
- `mjs_setToDamper(mjsActuator* actuator, double kv)`
- `mjs_setToAdhesion(mjsActuator* actuator, double gain)`
- `mjs_addMesh(mjSpec* s, const mjsDefault* def)`
- `mjs_addHField(mjSpec* s)`
- `mjs_addSkin(mjSpec* s)`
- `mjs_addTexture(mjSpec* s, const mjsDefault* def)`
- `mjs_addMaterial(mjSpec* s, const mjsDefault* def)`
- `mjs_makeMesh(mjsMesh* mesh, mjtMeshBuiltin builtin, double* params, int nparams)`
- `mjs_getSpec(const mjsElement* element)`
- `mjs_getOriginSpec(const mjsElement* element)`
- `mjs_getCompiler(const mjsElement* element)`
- `mjs_findSpec(const mjSpec* spec, const char* name)`
- `mjs_findBody(const mjSpec* s, const char* name)`
- `mjs_findElement(const mjSpec* s, mjtObj type, const char* name)`
- `mjs_findChild(const mjsBody* body, const char* name)`
- `mjs_getParent(const mjsElement* element)`
- `mjs_getFrame(const mjsElement* element)`
- `mjs_findFrame(const mjSpec* s, const char* name)`
- `mjs_getDefault(const mjsElement* element)`
- `mjs_findDefault(const mjSpec* s, const char* classname)`
- `mjs_getSpecDefault(const mjSpec* s)`
- `mjs_getId(const mjsElement* element)`
- `mjs_firstChild(const mjsBody* body, mjtObj type, int recurse)`
- `mjs_nextChild(const mjsBody* body, const mjsElement* child, int recurse)`
- `mjs_firstElement(const mjSpec* s, mjtObj type)`
- `mjs_nextElement(const mjSpec* s, const mjsElement* element)`
- `mjs_getWrapTarget(const mjsWrap* wrap)`
- `mjs_getWrapSideSite(const mjsWrap* wrap)`
- `mjs_getWrapDivisor(const mjsWrap* wrap)`
- `mjs_getWrapCoef(const mjsWrap* wrap)`
- `mjs_setName(mjsElement* element, const char* name)`
- `mjs_setBuffer(mjByteVec* dest, const void* array, int size)`
- `mjs_setString(mjString* dest, const char* text)`
- `mjs_setStringVec(mjStringVec* dest, const char* text)`
- `mjs_setInStringVec(mjStringVec* dest, int i, const char* text)`
- `mjs_appendString(mjStringVec* dest, const char* text)`
- `mjs_setInt(mjIntVec* dest, const int* array, int size)`
- `mjs_appendIntVec(mjIntVecVec* dest, const int* array, int size)`
- `mjs_setFloat(mjFloatVec* dest, const float* array, int size)`
- `mjs_appendFloatVec(mjFloatVecVec* dest, const float* array, int size)`
- `mjs_setDouble(mjDoubleVec* dest, const double* array, int size)`
- `mjs_setPluginAttributes(mjsPlugin* plugin, void* attributes)`
- `mjs_getName(mjsElement* element)`
- `mjs_getString(const mjString* source)`
- `mjs_getDouble(const mjDoubleVec* source, int* size)`
- `mjs_getWrapNum(const mjsTendon* tendonspec)`
- `mjs_getWrap(const mjsTendon* tendonspec, int i)`
- `mjs_getPluginAttributes(const mjsPlugin* plugin)`
- `mjs_setDefault(mjsElement* element, const mjsDefault* def)`
- `mjs_setFrame(mjsElement* dest, mjsFrame* frame)`
- `mjs_fuseStatic(mjSpec* s, const mjVFS* vfs)`
- `mjs_discardVisual(mjSpec* s, const mjVFS* vfs)`
- `mjs_dedupAssets(mjSpec* s)`
- `mjs_adoptInertial(mjsBody* body, const mjVFS* vfs)`
- `mjs_bodyToFrame(mjsBody** body)`
- `mjs_setUserValue(mjsElement* element, const char* key, const void* data)`
- `mjs_getUserValue(mjsElement* element, const char* key)`
- `mjs_deleteUserValue(mjsElement* element, const char* key)`
- `mjs_sensorDim(const mjsSensor* sensor)`
- `mjs_defaultSpec(mjSpec* spec)`
- `mjs_defaultOrientation(mjsOrientation* orient)`
- `mjs_defaultBody(mjsBody* body)`
- `mjs_defaultFrame(mjsFrame* frame)`
- `mjs_defaultJoint(mjsJoint* joint)`
- `mjs_defaultGeom(mjsGeom* geom)`
- `mjs_defaultSite(mjsSite* site)`
- `mjs_defaultCamera(mjsCamera* camera)`
- `mjs_defaultLight(mjsLight* light)`
- `mjs_defaultFlex(mjsFlex* flex)`
- `mjs_defaultMesh(mjsMesh* mesh)`
- `mjs_defaultHField(mjsHField* hfield)`
- `mjs_defaultSkin(mjsSkin* skin)`
- `mjs_defaultTexture(mjsTexture* texture)`
- `mjs_defaultMaterial(mjsMaterial* material)`
- `mjs_defaultPair(mjsPair* pair)`
- `mjs_defaultEquality(mjsEquality* equality)`
- `mjs_defaultTendon(mjsTendon* tendon)`
- `mjs_defaultActuator(mjsActuator* actuator)`
- `mjs_defaultSensor(mjsSensor* sensor)`
- `mjs_defaultNumeric(mjsNumeric* numeric)`
- `mjs_defaultText(mjsText* text)`
- `mjs_defaultTuple(mjsTuple* tuple)`
- `mjs_defaultKey(mjsKey* key)`
- `mjs_defaultPlugin(mjsPlugin* plugin)`
- `mjs_asBody(mjsElement* element)`
- `mjs_asGeom(mjsElement* element)`
- `mjs_asJoint(mjsElement* element)`
- `mjs_asSite(mjsElement* element)`
- `mjs_asCamera(mjsElement* element)`
- `mjs_asLight(mjsElement* element)`
- `mjs_asFrame(mjsElement* element)`
- `mjs_asActuator(mjsElement* element)`
- `mjs_asSensor(mjsElement* element)`
- `mjs_asFlex(mjsElement* element)`
- `mjs_asPair(mjsElement* element)`
- `mjs_asEquality(mjsElement* element)`
- `mjs_asExclude(mjsElement* element)`
- `mjs_asTendon(mjsElement* element)`
- `mjs_asNumeric(mjsElement* element)`
- `mjs_asText(mjsElement* element)`
- `mjs_asTuple(mjsElement* element)`
- `mjs_asKey(mjsElement* element)`
- `mjs_asMesh(mjsElement* element)`
- `mjs_asHField(mjsElement* element)`
- `mjs_asSkin(mjsElement* element)`
- `mjs_asTexture(mjsElement* element)`
- `mjs_asMaterial(mjsElement* element)`
- `mjs_asPlugin(mjsElement* element)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/render/filament/support/imgui_bridge.h`

- `ImguiBridge(mjrfContext* ctx, mjrfScene* scene)`
- `Update()`
- `PrepareRenderables(int count)`
- `CreateTexture(ImTextureData* data)`
- `UpdateTexture(ImTextureData* data)`
- `DestroyTexture(ImTextureData* data)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/render/filament/support/model_decorations.h`

- `Clear()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/render/filament/support/model_lights.h`

- `Update(const mjData* data = nullptr)`
- `GetLight(int index)`
- `Prepare()`
- `UpdateShadowMapSize()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/render/filament/support/model_renderables.h`

- `Update(const mjData* data)`
- `MarkAsSelected(mjtObj obj_type, int obj_index)`
- `SetOptions(const mjvOption& opt)`
- `AddGeomGeoms()`
- `AddSiteGeoms()`
- `AddFlexGeoms()`
- `AddSkinGeoms()`
- `AddSliderCrankGeoms()`
- `UpdateQuality()`
- `UpdateSpatialTendons(const mjData* data, int tendon_id)`
- `AppendSegmentToTendon(int tendon_id)`
- `RemoveSegmentFromTendon(int tendon_id)`
- `UpdateSliderCranks(const mjData* data, int actuator_id)`
- `GetSegmentationId(mjtObj obj_type, int obj_index)`
- `SetVisibility(mjtObj obj_type, int idx, bool visible)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/render/filament/support/model_resources.h`

- `UploadMesh(const mjModel* model, int id)`
- `UploadTexture(const mjModel* model, int id)`
- `UploadHeightField(const mjModel* model, int id)`
- `RemoveFromKey(int id, const TextureKey& key)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/render/filament/support/renderer.h`

- `GetScene(std::string_view name)`
- `GetScene(SceneHandle handle)`
- `ResizeRenderTarget(RenderTargetHandle handle, int width, int height)`
- `Render(std::span<const View> views)`
- `GetContext() { return ctx_.get()`
- `UpdateReadRequest(mjrfReadRequest& request, RenderTargetHandle target)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/app.h`

- `App(Config config)`
- `InitEmptyModel()`
- `LoadModelFromFile(const std::string& filepath)`
- `LoadKeyframe(std::string_view keyframe)`
- `Update()`
- `BuildGui()`
- `Render()`
- `FromDict(const Dict& dict)`
- `RequestModelLoad(std::string model_file)`
- `RequestModelReload()`
- `BuildModel(const BuildModelInfo& info)`
- `OnModelLoaded(std::string_view filename, ModelKind model_kind)`
- `InitModelScene()`
- `RestoreKeyframeSelection(const KeyframeSelection& saved)`
- `SwitchGraphicsMode(int width, int height, GraphicsMode mode)`
- `SetLoadError(std::string error)`
- `RecordLoadError(std::string_view error)`
- `UpdateFilePaths(const std::string& resolved_path)`
- `ResetPhysics()`
- `UpdatePhysics()`
- `PreStep(const mjModel* m, mjData* d)`
- `PostStep(const mjModel* m, mjData* d)`
- `LoadSettings()`
- `SaveSettings()`
- `ApplyWindowStateStorage()`
- `LoadHistory(int offset)`
- `ApplyTimelineScrub()`
- `SetSpeedIndex(int idx)`
- `HandleWindowEvents()`
- `HandleMouseEvents()`
- `HandleKeyboardEvents()`
- `ProcessPendingLoads()`
- `MainMenuGui()`
- `ToolBarGui()`
- `StatusBarGui()`
- `HelpGui()`
- `FileDialogGui()`
- `ModelOptionsGui()`
- `DataInspectorGui()`
- `SpecExplorerGui()`
- `SpecEditorGui()`
- `spec() { return model_holder_->spec()`
- `model() { return model_holder_->model()`
- `data() { return model_holder_->data()`
- `has_spec() const { return model_holder_ && model_holder_->spec()`
- `has_model() const { return model_holder_ && model_holder_->model()`
- `has_data() const { return model_holder_ && model_holder_->data()`
- `PhysicsThreadLoop()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/launcher.h`

- `LaunchStudio(int argc, char** argv, LauncherConfig config = {})`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/render/model_scene.h`

- `GetHeadlightIntensity() { return model_lights_->GetHeadlightIntensity()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/camera_control.h`

- `ReadImGuiCameraInput()`
- `MoveCameraWithKeys(const mjModel* m, mjvCamera* cam, const CameraInput& input, float* speed)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/console.h`

- `Clear()`
- `Log(mjtLogLevel level, std::string_view message)`
- `Add(mjtLogLevel level, std::string_view message, std::string_view body = "")`
- `WindowGui()`
- `StatusGui(bool* show_console)`
- `AppendLinesLocked(mjtLogLevel level, std::string_view prefix, std::string_view text)`
- `ClearLocked()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/enum_utils.h`

- `pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2})`
- `pretty_name({__FUNCSIG__, sizeof(__FUNCSIG__) - 17})`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/file_dialog.h`

- `OpenFileDialog(std::string_view path, std::span<std::string_view> filters = {})`
- `SaveFileDialog(std::string_view path, std::span<std::string_view> filters = {})`
- `SelectPathDialog(std::string_view path)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/fonts.h`

- `AddStudioFonts(const FontLoaderFn& load)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/gui.h`

- `SectionHeader(const char* label, ImGuiTreeNodeFlags flags = 0, float arrow_scale = 0.55f)`
- `LoadTheme(const std::string& ini_path = "", GuiTheme def_theme = GuiTheme::kLight)`
- `LoadSettings(const std::string& ini_path = "", GuiTheme def_theme = GuiTheme::kLight)`
- `SaveSettings(GuiTheme theme, const std::string& ini_path = "")`
- `ResetConfig(const std::string& ini_path = "")`
- `RescaleDock(float ratio)`
- `ConfigureDockingLayout(bool show_toolbar = true, bool show_status_bar = false)`
- `StepControlGui(StepControl* step_control, int& speed_index)`
- `SetSpeedIndex(StepControl* step_control, int& speed_index, int request_idx)`
- `LoadHistoryFrame(SimHistory& history, const mjModel* model, mjData* data, int index)`
- `SimulationGui(const SimulationGuiContext& ctx)`
- `ThemeSelectGui(GuiTheme* theme, const ImVec2& size = ImVec2(0, 0))`
- `ThemeMenuGui(GuiTheme* theme)`
- `LabelSelectionGui(mjvOption* opts)`
- `FrameSelectionGui(mjvOption* opts)`
- `CameraSelectionGui(const mjModel* model, mjData* data, mjvCamera& camera, int& index)`
- `PhysicsGui(mjModel* model, mjSpec* spec = nullptr, float min_width = 150.0f)`
- `GroupsGui(const mjModel* model, mjvOption* vis_options, float min_width)`
- `VisualizationGui(mjModel* model, mjvOption* vis_options, mjvCamera* camera, float min_width)`
- `ControlsGui(const mjModel* model, mjData* data, const mjvOption* vis_options)`
- `JointsGui(const mjModel* model, const mjData* data, const mjvOption* vis_options)`
- `SensorGui(const mjModel* model, const mjData* data)`
- `NoiseGui(StepControl* step_control)`
- `ConvergenceGui(const mjModel* model, mjData* data, ImVec2 plot_size = ImVec2(-1, 0))`
- `CountsGui(const mjModel* model, mjData* data, ImVec2 plot_size = ImVec2(-1, 0))`
- `ProfilerGui(const mjModel* model, mjData* data, SimProfiler* profiler, bool show_iter)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/gui_helpers.h`

- `SaveText(const std::string& contents, const std::string& filename)`
- `GetValue(const mjModel* model, const mjData* data, const char* field, int index)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/gui_spec.h`

- `SpecTreeGui(mjsElement** element, mjSpec* spec, SpecEditor* editor = nullptr)`
- `ElementSpecGui(mjsElement* element, SpecEditor* editor = nullptr)`
- `ElementDataGui(const mjData* data, mjsElement* element)`
- `ElementModelGui(const mjModel* model, mjsElement* element)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/imgui_widgets.h`

- `ImGui_Checkbox(const char* name, T& value)`
- `AppendIniSection(std::string& ini, const std::string& section, const KeyValues& key_values)`
- `ReadIniSection(const std::string& contents, const std::string& section)`
- `DataPtr(const char* label, const char* ptr, int index, int n)`
- `DataPtr(const char* label, const mjtByte* ptr, int index, int n)`
- `DataPtr(const char* label, const mjtBool* ptr, int index, int n)`
- `DataPtr(const char* label, const mjtSize* ptr, int index, int n)`
- `DataPtr(const char* label, const int* ptr, int index, int n)`
- `DataPtr(const char* label, const float* ptr, int index, int n)`
- `DataPtr(const char* label, const double* ptr, int index, int n)`
- `DataPtr(const char* label, const uintptr_t* ptr, int index, int n)`
- `SetPrefix(const char* prefix)`
- `Numeric(const char* label, const T* ptr, int index, int n)`
- `MakeLabel(const char* label, int index = 0, int total = 1)`
- `operator()(const char* label, mjtByte& val, const mjtByte& ref, const char* tooltip)`
- `operator()(const char* label, bool& val, const bool& ref, const char* tooltip)`
- `operator()(const char* label, mjtSize& val, const mjtSize& ref, const char* tooltip)`
- `operator()(const char* label, int& val, const int& ref, const char* tooltip)`
- `operator()(const char* label, float& val, const float& ref, const char* tooltip)`
- `operator()(const char* label, double& val, const double& ref, const char* tooltip)`
- `operator()(const char* label, std::string* ptr, const std::string* ref, const char* tooltip)`
- `Label(const char* label, const char* tooltip, int i = 0, int n = 1)`
- `ImGui_BeginHSplit(const char* id, float* height, bool* open)`
- `ImGui_HSplit(const char* id, float* height, bool* open)`
- `ImGui_EndHSplit(bool open)`
- `ImGui_Slider(const char* name, mjtNum* value, mjtNum min, mjtNum max)`
- `ImGui_SliderLog(const char* name, mjtNum* value, mjtNum min, mjtNum max)`
- `ImGui_InputN(name, value, 1, opts)`
- `hover(color.Value.x, color.Value.y, color.Value.z, color.Value.w * hover_alpha)`
- `max(pos.x + btn_size.x, pos.y + btn_size.y)`
- `EndBoxSection() { ImGui::EndTable()`
- `MaybeSaveToClipboard(const std::string& contents)`
- `GetExpectedLabelWidth()`
- `ImPlot_SetupPlotFlags(ImVec2 plot_size)`
- `ImPlot_ComputePairLayout()`
- `DrawTextAt(const char* text, float x, float y, float z)`
- `DrawTextAtInRect(const char* text, float x, float y, float z, const ImVec4& rect)`
- `EndOverlay()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/interaction.h`

- `SetCamera(const mjModel* m, mjvCamera* camera, int request_idx)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/picture_gui.h`

- `PipGui(const std::vector<PipSource>& sources, std::vector<PipState>* pips)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/plugin.h`

- `RegisterPlugin(T plugin)`
- `ForEachPlugin(const std::function<void(T*)>& fn)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/spec_editor.h`

- `SpecEditor(int history_size = 256)`
- `Reset()`
- `Reset(const mjSpec& spec)`
- `AddElement(mjtObj type)`
- `AddBodyElement(mjsBody* body, mjtObj type)`
- `DeleteActiveElement()`
- `SetActiveElement(mjsElement* element)`
- `CommitChanges(mjsElement* element)`
- `Undo()`
- `Redo()`
- `Clear()`
- `Append(mjtObj type, ElementKey key)`
- `Remove(ElementKey key)`
- `Insert(ElementKey key, TypeIndex type_index)`
- `AppendHistory(HistoryEntry entry)`
- `UpdateReferenceElement()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/ux/theme.h`

- `SetupTheme(GuiTheme theme)`
- `ThemeName(GuiTheme theme)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/hal/filament_renderer.h`

- `Init(const mjModel* model)`
- `Submit(int width, int height, std::span<std::byte> pixels = {})`
- `UploadImage(int tex_id, std::span<const std::byte> pixels, int width, int height)`
- `GetGpuFrameMs()`
- `Deinit()`
- `BuildUxRenderRequest(mjrfRenderRequest* request, const mjrRect& viewport)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/hal/graphics_mode.h`

- `IsFilament(GraphicsMode gfx_mode)`
- `IsOpenGl(GraphicsMode gfx_mode)`
- `IsVulkan(GraphicsMode gfx_mode)`
- `IsWebGl(GraphicsMode gfx_mode)`
- `IsHeadless(GraphicsMode gfx_mode)`
- `IsSoftware(GraphicsMode gfx_mode)`
- `GraphicsModeFromString(std::string_view str, GraphicsMode default_mode)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/hal/window.h`

- `NewFrame()`
- `EndFrame()`
- `Present(std::span<const std::byte> pixels = {})`
- `SetTitle(std::string_view title)`
- `Resize(int width, int height)`
- `EnableWindowResizing()`
- `DisableWindowResizing()`
- `InitOffscreenEglContext()`
- `UpdateDrawableSize()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/io/resources.h`

- `RegisterProviders()`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/sim/model_holder.h`

- `Recompile()`
- `ReleaseSpec()`
- `ReleaseData()`
- `ReleaseModel()`
- `ModelHolder(mjVFS* vfs)`
- `InitFromSpec(mjSpec* spec)`
- `InitFromFile(std::string_view filepath)`
- `PostInit()`
- `SetLoadError(std::string_view error)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/sim/sim_history.h`

- `Init(int state_size, int max_history = 2000, int max_bytes = 1e8)`
- `Capacity() const { return static_cast<int>(history_.size())`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/sim/sim_profiler.h`

- `Clear()`
- `ResetTimers()`
- `Update(const mjModel* model, const mjData* data)`
- `CpuTimeGraph(ImVec2 plot_size = ImVec2(-1, 0))`
- `DimensionsGraph(ImVec2 plot_size = ImVec2(-1, 0))`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/studio/sim/step_control.h`

- `StepControl(ClockFn clock_fn = Clock::now)`
- `Advance(mjModel* m, mjData* d)`
- `ForceSync()`
- `SetSpeed(float speed)`
- `SetNoiseParameters(float noise_scale, float noise_rate)`
- `SetPreStepCallback(StepFn step_fn)`
- `SetPostStepCallback(StepFn step_fn)`
- `SetPauseState(PauseState state)`
- `ConsumeSingleStepRequest()`
- `InjectNoise(const mjModel* m, mjData* d)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/experimental/filament/compat/scene_bridge.h`

- `Update(const mjrRect& viewport, const mjvScene* scene)`
- `UploadMesh(const mjModel* model, int id)`
- `UploadTexture(const mjModel* model, int id)`
- `UploadHeightField(const mjModel* model, int id)`
- `SetDrawTextFunction(DrawTextAtFn fn)`

## `src/lua/www/~WuBu~/thirdparty/mujoco/include/mujoco/experimental/filament/compat/scene_objects.h`

- `SceneObjects(mjrfContext* ctx)`

## `src/lua/www/~WuBu~/src/lua/wb_mod.h`

- `wb_mod_free(wb_mod *mod)`
- `wb_mod_step(wb_mod *mod, double dt)`
- `wb_mod_reload(wb_mod *mod)`

## `src/lua/www/~WuBu~/src/lua/wb_script.h`

- `wb_world_setup_comps(wb_world *w)`
- `wb_script_push_entity(struct lua_State *L, wb_entity e)`
- `wb_script_step(struct lua_State *L, float dt)`
- `wb_physics_tick(wb_world *w, float dt)`

## `src/lua/www/~WuBu~/src/lua/wb_space_bind.h`

- `wb_space_bind_install(struct lua_State *L)`

## `src/lua/www/~WuBu~/src/net/wb_mesh.h`

- `wb_mesh_free(wb_mesh_node *m)`

## `src/lua/www/~WuBu~/src/net/wb_net_client.h`

- `wb_net_client_free(wb_net_client *c)`
- `wb_net_client_apply_input(wb_net_client *c, const wb_net_input *in, float dt)`

## `src/lua/www/~WuBu~/src/net/wb_net_loopback.h`

- `wb_net_loopback_free(wb_net_loopback *l)`
- `wb_net_loopback_deliver(wb_net_loopback *l, int client_id)`

## `src/lua/www/~WuBu~/src/net/wb_net_server.h`

- `wb_net_server_free(wb_net_server *s)`
- `wb_net_server_snapshot(wb_net_server *s, int client_id, wb_net_snapshot *out)`
- `wb_net_server_tick_count(const wb_net_server *s)`
- `wb_net_server_entity_count(const wb_net_server *s)`
- `wb_net_server_apply_impulse(wb_net_server *s, uint32_t id, float dx, float dy)`
- `wb_net_server_remove_entity(wb_net_server *s, uint32_t id)`
- `wb_net_server_entity(wb_net_server *s, uint32_t id)`

## `src/lua/www/~WuBu~/src/lv/src/wb_lv.h`

- `wb_lv_truthy(wb_lv_value v)`
- `wb_lv_equal(wb_lv *L, wb_lv_value a, wb_lv_value b)`
- `wb_lv_rawequal(wb_lv_value a, wb_lv_value b)`
- `wb_lv_multi_value(wb_lv *L, wb_lv_value *v, int n)`
- `wb_lv_free(wb_lv *L)`
- `wb_lv_tonumber(wb_lv_value v, int *ok)`
- `wb_lv_tointeger(wb_lv_value v, int *ok)`
- `wb_lv_call(wb_lv *L, wb_lv_value fn, wb_lv_value *args, int nargs)`
- `wb_lv_openlibs(wb_lv *L)`

## `src/lua/www/~WuBu~/src/lv/src/wb_lv_internal.h`

- `wb_lv_set_err(wb_lv *L, const char *fmt, ...)`
- `wb_lv_error(wb_lv *L, const char *fmt, ...)`
- `wb_lv_error_value(wb_lv *L, wb_lv_value v)`
- `wb_lv_next_tok(parser *P)`
- `wb_lv_perr(parser *P, const char *msg)`
- `wb_lv_eval(wb_lv *L, ex *e, env *en)`
- `wb_lv_eval_multi(wb_lv *L, ex *e, env *en, wb_lv_value **out, int *nout)`
- `wb_lv_exec(wb_lv *L, ex *e, env *en, int *was_return)`
- `wb_lv_call_lua(wb_lv *L, wb_lv_func *fn, wb_lv_value *args, int nargs)`
- `wb_lv_call_lua_results(wb_lv *L, wb_lv_func *fn, wb_lv_value *args, int nargs)`
- `wb_lv_call_value(wb_lv *L, wb_lv_value fn, wb_lv_value *args, int nargs, wb_lv_value **out)`
- `wb_lv_multi_value(wb_lv *L, wb_lv_value *v, int n)`
- `wb_lv_exec_root(wb_lv *L, ex *root)`

## `src/lua/www/~WuBu~/src/core/wb_assets.h`

- `wb_model_free(wb_model *m)`
- `wb_asset_load_gltf(const char *path, wb_model *out)`

## `src/lua/www/~WuBu~/src/core/wb_camera.h`

- `wb_camera_init(wb_camera *cam)`
- `wb_camera_right(const wb_camera *cam, double out[3])`

## `src/lua/www/~WuBu~/src/core/wb_character.h`

- `wb_character_init(wb_character *c)`

## `src/lua/www/~WuBu~/src/core/wb_ddc.h`

- `wb_ddc_is_fresh(const char *src_path)`

## `src/lua/www/~WuBu~/src/core/wb_dist.h`

- `wb_dist_registry_leave_player(wb_dist_registry *r, const char *player_id)`
- `wb_portal_init(wb_portal *p)`
- `wb_portal_check(const wb_portal *p, const float player_pos[3])`

## `src/lua/www/~WuBu~/src/core/wb_edit.h`

- `wb_editor_free(wb_editor *ed)`
- `wb_editor_apply(wb_editor *ed, const wb_edit_op *op)`
- `wb_editor_live(wb_editor *ed, const wb_edit_op *op)`
- `wb_editor_undo(wb_editor *ed)`
- `wb_editor_redo(wb_editor *ed)`
- `wb_editor_can_undo(const wb_editor *ed)`
- `wb_editor_can_redo(const wb_editor *ed)`
- `wb_editor_count(const wb_editor *ed)`

## `src/lua/www/~WuBu~/src/core/wb_exp.h`

- `wb_exp_join(wb_exp *e, const char *player_id)`

## `src/lua/www/~WuBu~/src/core/wb_gpu.h`

- `wb_gpu_get_device_name(const wb_gpu *g, char *out, size_t out_sz)`
- `wb_gpu_execute(wb_gpu *g)`
- `wb_gpu_read_pixel(wb_gpu *g, wb_gpu_texture *tex, float out_rgba[4])`

## `src/lua/www/~WuBu~/src/core/wb_host.h`

- `wb_host_close(wb_host *h)`
- `wb_host_player(const wb_host *h)`
- `wb_host_step(wb_host *h, float dt)`
- `wb_host_running(wb_host *h)`

## `src/lua/www/~WuBu~/src/core/wb_input.h`

- `wb_input_init(wb_input *in)`

## `src/lua/www/~WuBu~/src/core/wb_json.h`

- `wb_json_free(wb_json *v)`

## `src/lua/www/~WuBu~/src/core/wb_manifest.h`

- `wb_manifest_free(wb_manifest *mf)`
- `wb_manifest_load(const char *path, wb_manifest *out)`

## `src/lua/www/~WuBu~/src/core/wb_mode.h`

- `wb_mode_manifest_load(const char *path, wb_mode_manifest *out)`
- `wb_mode_manifest_validate(const wb_mode_manifest *m, char *err, size_t err_sz)`

## `src/lua/www/~WuBu~/src/core/wb_physics.h`

- `wb_physics_build(wb_physics *p)`
- `wb_physics_character_move(wb_physics *p, int body, const double vel[3])`
- `wb_physics_grounded(wb_physics *p, int body)`
- `wb_physics_set_yaw(wb_physics *p, int body, double yaw)`
- `wb_physics_bind(wb_physics *p, int body, wb_entity e)`

## `src/lua/www/~WuBu~/src/core/wb_physics_internal.h`

- `wb_physics_body_dof(const mjModel *m, int body)`

## `src/lua/www/~WuBu~/src/core/wb_spatial.h`

- `wb_spatial_lat_to_y(double lat, uint32_t level)`
- `wb_spatial_x_to_lon(double tx, uint32_t level)`
- `wb_spatial_y_to_lat(double ty, uint32_t level)`
- `wb_qkey_bounds(wb_qkey k, double *lon0, double *lat0, double *lon1, double *lat1)`
- `wb_qkey_east(wb_qkey k)`
- `wb_qkey_west(wb_qkey k)`
- `wb_qkey_north(wb_qkey k)`
- `wb_qkey_south(wb_qkey k)`

## `src/lua/www/~WuBu~/src/core/wb_spawn.h`

- `wb_spawn_free(wb_spawn_set *s)`
- `wb_spawn_pick(wb_spawn_set *s, const char *gamemode, int team, float out[3])`

## `src/lua/www/~WuBu~/src/core/wb_tiles.h`

- `wb_tiles_destroy(wb_tile_store *s)`
- `wb_tiles_evict(wb_tile_store *s, wb_qkey key, uint8_t plane)`
- `wb_tiles_resident(const wb_tile_store *s)`

## `src/lua/www/~WuBu~/src/core/wb_util.h`

- `wb_file_exists(const char *path)`
- `wb_set_err(char *buf, size_t buf_sz, const char *fmt, ...)`
- `wb_content_hash(const char *path, char out[17])`

## `src/lua/www/~WuBu~/src/core/wb_worldspace.h`

- `wb_worldspace_close(wb_worldspace *ws)`
- `wb_worldspace_mode_count(const wb_worldspace *ws)`
- `wb_worldspace_portal_count(const wb_worldspace *ws)`

## `src/shell/wubu_shell.h`

- `wubu_shell_run(void *arg)`

## `src/shell/wubu_shell_internal.h`

- `shell_print(ShellState *st, const char *msg)`
- `shell_history_count(const ShellState *st)`
- `shell_exec_pipeline(ShellState *st, const char *line)`

## `src/tools/iso9660.h`

- `iso_builder_build(iso_builder_t *b)`

## `src/tools/screenshot.h`

- `wubu_gif_start(const char *path, int w, int h, int delay_ms, int max_frames)`
- `wubu_gif_add_frame(int x, int y, int w, int h)`
- `wubu_gif_stop(void)`
- `wubu_snip_tool_init(void)`
- `wubu_snip_tool_shutdown(void)`
- `wubu_snip_tool_activate(WubuSnipMode mode)`
- `wubu_snip_tool_deactivate(void)`
- `wubu_snip_tool_render(uint32_t *fb, int fb_w, int fb_h)`
- `wubu_snip_tool_handle_mouse(int x, int y, int btn, int kind)`
- `wubu_snip_tool_save(const char *path, WubuShotFormat fmt)`

## `src/tools/weight_check.h`

- `weight_check(weight_check_t *result)`
- `weight_shard_path(int index, char *buf, int bufsz)`
- `weight_validate_file(const char *path, uint64_t min_size)`

## `src/tools/wubugrep/wubre.h`

- `wubre_search(const WURegex *re, const unsigned char *buf, size_t n)`
- `wubre_free(WURegex *re)`

## `src/tools/wubugrep/wubre_internal.h`

- `wubre_dfa_free(void *d)`
- `wubre_dfa_nstates(const WURegex *re)`

## `src/bridge/bridge.h`

- `bridge_enter_temple(void)`
- `bridge_exit_temple(void)`

## `src/bridge/vbe_ws_bridge.h`

- `vbe_ws_bridge_wire(vbe_ws_bridge_t *br, ws_simulation_t *sim)`
- `vbe_ws_bridge_frame(vbe_ws_bridge_t *br)`
- `vbe_ws_bridge_draw_hud(vbe_ws_bridge_t *br)`

## `src/kernel/ahci.h`

- `ahci_identify(ahci_hba_t *hba, int port_num)`
- `ahci_sim_disk_create(ahci_hba_t *hba, int port_num, int size_mb)`

## `src/kernel/fat32.h`

- `fat32_mount(fat32_volume *vol, const fat32_blk_ops *blk)`
- `fat32_boot_attach(const fat32_blk_ops *ops)`
- `fat32_unmount(fat32_volume *vol)`
- `fat32_next_cluster(fat32_volume *vol, uint32_t cluster)`
- `fat32_alloc_clusters(fat32_volume *vol, uint32_t count, uint32_t hint)`
- `fat32_free_chain(fat32_volume *vol, uint32_t cluster)`
- `fat32_count_free(fat32_volume *vol)`
- `fat32_delete(fat32_volume *vol, uint32_t dir_cluster, const char *name)`
- `fat32_close(fat32_file *fp)`
- `fat32_read(fat32_file *fp, void *buf, size_t n)`
- `fat32_write(fat32_file *fp, const void *buf, size_t n)`
- `fat32_seek(fat32_file *fp, int64_t offset, int whence)`
- `fat32_validate(fat32_volume *vol)`
- `fat32_info(fat32_volume *vol, char *buf, size_t buf_size)`

## `src/kernel/input.h`

- `input_key_pressed(uint32_t scancode)`
- `input_scancode_to_key(uint32_t scancode, uint32_t mods)`

## `src/kernel/interrupt.h`

- `pit_shutdown(void)`
- `interrupt_exception_count(uint8_t vec)`

## `src/kernel/interrupt_pic.h`

- `pic_remap(uint8_t offset1, uint8_t offset2)`
- `pic_eoi(uint8_t vector)`
- `irq_route_remove(uint8_t src_irq)`

## `src/kernel/klog.h`

- `klog_tx_poll(void)`
- `klog_printf(const char *fmt, ...)`
- `klog_ring_snapshot(char *out, size_t bufsz)`

## `src/kernel/libc.h`

- `tolower(int c)`
- `toupper(int c)`
- `isspace(int c)`
- `vsnprintf(char *str, size_t size, const char *fmt, va_list ap)`
- `snprintf(char *str, size_t size, const char *fmt, ...)`
- `vprintf(const char *fmt, va_list ap)`
- `printf(const char *fmt, ...)`

## `src/kernel/memory.h`

- `mem_free(void *ptr)`
- `mem_bloom_scan(uint32_t target_sig, MemBloomFn callback, void *ctx)`

## `src/kernel/ps2.h`

- `ps2_init(int screen_w, int screen_h, ps2_probe_t *probe)`

## `src/kernel/tasking.h`

- `task_prio_get(const struct CTask *t)`
- `task_prio_set(struct CTask *t, int prio)`
- `task_timer_tick(void)`
- `task_switch_to(CTask *target)`

## `src/kernel/txfs.h`

- `txfs_commit(txfs_t *tx)`
- `txfs_abort(txfs_t *tx)`
- `txfs_recover(txfs_t *tx)`

## `src/kernel/vbe.h`

- `vbe_bochs_present(void)`
- `vbe_bochs_id(void)`
- `vbe_dispi_read_reg(uint16_t idx)`
- `vbe_bochs_enable(int w, int h, int bpp)`
- `vbe_probe_native_mode(int *out_w, int *out_h)`
- `vbe_draw_char(int x, int y, char ch, uint32_t color, int scale)`
- `vbe_set_text_scale(int scale)`
- `vbe_text_scale(void)`
- `vbe_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha)`
- `vbe_set_clip(int x, int y, int w, int h)`
- `vbe_reset_clip(void)`
- `vbe_get_clip(int *x, int *y, int *w, int *h)`

## `src/kernel/wubu_acpi.h`

- `wubu_acpi_find_rsdp(void)`
- `wubu_acpi_find_table(const char sig[4])`
- `wubu_acpi_find_table_from(uint64_t rsdp, const char sig[4])`

## `src/kernel/wubu_adreno600.h`

- `wubu_adreno600_probe(void)`
- `wubu_adreno600_present(void)`
- `wubu_adreno600_has_vulkan(int vulkan_available)`
- `wubu_adreno600_is_a6xx(int a6xx)`
- `wubu_adreno600_summary(char *out, size_t cap)`

## `src/kernel/wubu_adreno700.h`

- `wubu_adreno700_probe(void)`
- `wubu_adreno700_present(void)`
- `wubu_adreno700_uses_freedreno(int freedreno_available)`
- `wubu_adreno700_gen(int gen)`
- `wubu_adreno700_summary(char *out, size_t cap)`

## `src/kernel/wubu_agi_kernel.h`

- `wubu_agi_kernel_run(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_tick(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_freeze(wubu_agi_kernel_t *k, bool frozen)`
- `wubu_agi_kernel_is_frozen(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_has_memory(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_cycle(wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_trace_count(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_promoted_total(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_region_count(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_restore(wubu_agi_kernel_t *k, const wubu_agi_ckp_t *in)`
- `wubu_agi_kernel_uptime_ms(const wubu_agi_kernel_t *k)`
- `wubu_agi_kernel_last_promote_tick(const wubu_agi_kernel_t *k)`

## `src/kernel/wubu_ampere.h`

- `wubu_ampere_probe(void)`
- `wubu_ampere_present(void)`
- `wubu_ampere_has_raytracing(int rt_cores)`
- `wubu_ampere_vulkan(int vulkan_level)`
- `wubu_ampere_summary(char *out, size_t cap)`

## `src/kernel/wubu_apic.h`

- `wubu_apic_enable(void)`
- `wubu_map_phys_range(uint64_t phys, uint32_t pages)`

## `src/kernel/wubu_arctic_islands.h`

- `wubu_arctic_islands_probe(void)`
- `wubu_arctic_islands_present(void)`
- `wubu_arctic_islands_uses_radv(int vulkan_available)`
- `wubu_arctic_islands_vulkan_level(int gcn_gen)`
- `wubu_arctic_islands_summary(char *out, size_t cap)`

## `src/kernel/wubu_as.h`

- `wubu_as_bind(wubu_as_t *as)`

## `src/kernel/wubu_atom.h`

- `wubu_atom_begin_update(wubu_atom_t *a)`
- `wubu_atom_commit(wubu_atom_t *a)`
- `wubu_atom_boot(wubu_atom_t *a)`
- `wubu_atom_boot_ok(wubu_atom_t *a)`
- `wubu_atom_rollback_target(const wubu_atom_t *a)`
- `wubu_atom_marker_crc(const wubu_atom_t *a, uint32_t *out)`
- `wubu_atom_marker_valid(const wubu_atom_t *a)`

## `src/kernel/wubu_attest.h`

- `wubu_attest_ingest(const void *raw)`
- `wubu_attest_ingest_handoff(const void *handoff)`
- `wubu_attest_load_scratch(void)`
- `wubu_attest_extend_runtime(const void *data, size_t len)`
- `wubu_attest_runtime_pcr(uint8_t out[WUBU_AGI_PCR_SZ])`
- `wubu_attest_kernel_size(void)`

## `src/kernel/wubu_audio.h`

- `wubu_audio_is_hdmi(void)`
- `wubu_audio_is_usb(void)`
- `wubu_audio_has_bt(void)`

## `src/kernel/wubu_audiofw.h`

- `wubu_audiofw_probe(void)`
- `wubu_audiofw_present(void)`
- `wubu_audiofw_codec(void)`
- `wubu_audiofw_dsp(void)`
- `wubu_audiofw_loader(void)`
- `wubu_audiofw_bios(void)`
- `wubu_audiofw_summary(char *out, size_t cap)`

## `src/kernel/wubu_auracast.h`

- `wubu_auracast_probe(void)`
- `wubu_auracast_present(void)`
- `wubu_auracast_streams(int active)`
- `wubu_auracast_is_broadcasting(int broadcaster, int pa)`
- `wubu_auracast_summary(char *out, size_t cap)`

## `src/kernel/wubu_bap.h`

- `wubu_bap_probe(void)`
- `wubu_bap_present(void)`
- `wubu_bap_codec(int sample_rate, int bit_depth)`
- `wubu_bap_is_ready(int configured, int connected)`
- `wubu_bap_summary(char *out, size_t cap)`

## `src/kernel/wubu_bcache.h`

- `wubu_bcache_probe(void)`
- `wubu_bcache_present(void)`
- `wubu_bcache_hit_pct(int hits, int misses)`
- `wubu_bcache_summary(char *out, size_t cap)`

## `src/kernel/wubu_bio.h`

- `wubu_bio_probe(void)`
- `wubu_bio_present(void)`
- `wubu_bio_vec(void)`
- `wubu_bio_bdi(void)`
- `wubu_bio_read(void)`
- `wubu_bio_write(void)`
- `wubu_bio_summary(char *out, size_t cap)`

## `src/kernel/wubu_blk.h`

- `wubu_blk_zero_on_success(int rc)`
- `wubu_blk_sectors_left(const wubu_blk_dev_t *d, uint64_t lba)`
- `wubu_blk_elevator(const wubu_blk_req_t *q, int n, int *order)`
- `wubu_blk_plug(wubu_blk_req_t *q, int *nq, int cap)`
- `wubu_blk_unplug(wubu_blk_req_t *q, int *nq, wubu_blk_dev_t *d)`
- `wubu_blk_merge(wubu_blk_req_t *a, wubu_blk_req_t b, wubu_blk_req_t *out)`
- `wubu_blk_split(wubu_blk_req_t r, uint32_t max_n, wubu_blk_req_t *a, wubu_blk_req_t *b)`
- `wubu_blk_discard_queue(wubu_blk_req_t *q, int *nq)`
- `wubu_blk_poll_complete(wubu_blk_dev_t *d)`
- `wubu_blk_irq_complete(wubu_blk_dev_t *d)`
- `wubu_blk_timeout(wubu_blk_req_t *q, int n, uint64_t now, uint64_t deadline)`
- `wubu_blk_retry(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf, int tries)`
- `wubu_blk_verify(wubu_blk_dev_t *d, uint64_t lba, const void *expect, uint32_t n)`
- `wubu_blk_zero(wubu_blk_dev_t *d, uint64_t lba, uint32_t n)`
- `wubu_blk_secure_erase(wubu_blk_dev_t *d, uint64_t lba, uint32_t n)`
- `wubu_blk_health(wubu_blk_dev_t *d, uint32_t *wear, uint32_t *temp)`
- `wubu_blk_throttle(uint32_t queue_depth, uint32_t max_depth)`
- `wubu_blk_priority_inherit(wubu_blk_req_t *r, uint32_t holder_prio)`
- `wubu_blk_deadline_sort(wubu_blk_req_t *q, int n, int *order)`
- `wubu_blk_fairness(const uint32_t *weights, int n, int *slot)`
- `wubu_blk_cgroup_limit(uint64_t budget, uint64_t used, uint64_t *grant)`
- `wubu_blk_io_stat(wubu_blk_dev_t *d, uint64_t *rd, uint64_t *wr, uint64_t *lat)`
- `wubu_blk_latency_percentile(const uint32_t *lats, int n, float pct)`
- `wubu_blk_throughput(uint64_t bytes, uint64_t ns)`
- `wubu_blk_io_priority_map(uint32_t cgroup, uint32_t *prio)`
- `wubu_blk_scheduler_select(const uint32_t *counters, int n, int *policy)`

## `src/kernel/wubu_blkqos.h`

- `wubu_blkqos_probe(void)`
- `wubu_blkqos_present(void)`
- `wubu_blkqos_throttle(void)`
- `wubu_blkqos_weight(void)`
- `wubu_blkqos_cg(void)`
- `wubu_blkqos_limit(void)`
- `wubu_blkqos_summary(char *out, size_t cap)`

## `src/kernel/wubu_bonzi.h`

- `wubu_bonzi_tick(wubu_bonzi_t *b)`

## `src/kernel/wubu_bonzi_study.h`

- `wubu_bs_animate(uint32_t expr, uint32_t *frames, int n)`
- `wubu_bs_microexp(uint32_t mood, float *intensity)`
- `wubu_bs_eye_gaze(uint32_t user_x, uint32_t user_y, float *gaze)`

## `src/kernel/wubu_bta2dp.h`

- `wubu_bta2dp_probe(void)`
- `wubu_bta2dp_present(void)`
- `wubu_bta2dp_bitrate(int codec)`
- `wubu_bta2dp_summary(char *out, size_t cap)`

## `src/kernel/wubu_btamesh.h`

- `wubu_btamesh_probe(void)`
- `wubu_btamesh_present(void)`
- `wubu_btamesh_hops(int ttl)`
- `wubu_btamesh_is_relay(int role)`
- `wubu_btamesh_summary(char *out, size_t cap)`

## `src/kernel/wubu_btaudio.h`

- `wubu_btaudio_probe(void)`
- `wubu_btaudio_present(void)`
- `wubu_btaudio_auto(int latency_ms)`
- `wubu_btaudio_summary(char *out, size_t cap)`

## `src/kernel/wubu_btbeacon.h`

- `wubu_btbeacon_probe(void)`
- `wubu_btbeacon_present(void)`
- `wubu_btbeacon_type(int rssi)`
- `wubu_btbeacon_valid_uuid(const char *uuid_prefix)`
- `wubu_btbeacon_summary(char *out, size_t cap)`

## `src/kernel/wubu_btclassic.h`

- `wubu_btclassic_probe(void)`
- `wubu_btclassic_present(void)`
- `wubu_btclassic_rate(int sco, int esc)`
- `wubu_btclassic_summary(char *out, size_t cap)`

## `src/kernel/wubu_bthfp.h`

- `wubu_bthfp_probe(void)`
- `wubu_bthfp_present(void)`
- `wubu_bthfp_call_state(int sco_active, int at_cmd_ready)`
- `wubu_bthfp_summary(char *out, size_t cap)`

## `src/kernel/wubu_cab.h`

- `cab_find(const cab_archive *cab, const char *name)`

## `src/kernel/wubu_chanmap.h`

- `wubu_chanmap_probe(void)`
- `wubu_chanmap_present(void)`
- `wubu_chanmap_stereo(void)`
- `wubu_chanmap_51(void)`
- `wubu_chanmap_71(void)`
- `wubu_chanmap_chmap(void)`
- `wubu_chanmap_summary(char *out, size_t cap)`

## `src/kernel/wubu_compressor.h`

- `wubu_compressor_probe(void)`
- `wubu_compressor_present(void)`
- `wubu_compressor_thresh(void)`
- `wubu_compressor_ratio(void)`
- `wubu_compressor_attack(void)`
- `wubu_compressor_release(void)`
- `wubu_compressor_summary(char *out, size_t cap)`

## `src/kernel/wubu_computectx.h`

- `wubu_computectx_probe(void)`
- `wubu_computectx_present(void)`
- `wubu_computectx_kfd(void)`
- `wubu_computectx_queue(void)`
- `wubu_computectx_opencl(void)`
- `wubu_computectx_cuda(void)`
- `wubu_computectx_summary(char *out, size_t cap)`

## `src/kernel/wubu_console_cmds.h`

- `cmd_help(void)`
- `cmd_uptime(void)`
- `cmd_mem(void)`
- `cmd_tasks(void)`
- `cmd_pci(void)`
- `cmd_theme(int argc, char **argv)`
- `cmd_input(int argc, char **argv)`
- `cmd_vmm(int argc, char **argv)`
- `cmd_stats(int argc, char **argv)`
- `cmd_dump(int argc, char **argv)`
- `cmd_attest(int argc, char **argv)`
- `cmd_date(int argc, char **argv)`
- `cmd_agi(int argc, char **argv)`
- `cmd_holyc(int argc, char **argv)`

## `src/kernel/wubu_crash.h`

- `wubu_crash_pickup(void)`

## `src/kernel/wubu_cuda.h`

- `wubu_cuda_probe(void)`
- `wubu_cuda_present(void)`
- `wubu_cuda_has_cuda_cc(int cuda_available)`
- `wubu_cuda_min_version(int cc)`
- `wubu_cuda_summary(char *out, size_t cap)`

## `src/kernel/wubu_dapm.h`

- `wubu_dapm_probe(void)`
- `wubu_dapm_present(void)`
- `wubu_dapm_path_active(const char *name)`
- `wubu_dapm_summary(char *out, size_t cap)`

## `src/kernel/wubu_dapmwidget.h`

- `wubu_dapmwidget_probe(void)`
- `wubu_dapmwidget_present(void)`
- `wubu_dapmwidget_widget(void)`
- `wubu_dapmwidget_power(void)`
- `wubu_dapmwidget_path(void)`
- `wubu_dapmwidget_stream(void)`
- `wubu_dapmwidget_summary(char *out, size_t cap)`

## `src/kernel/wubu_dappath.h`

- `wubu_dappath_probe(void)`
- `wubu_dappath_present(void)`
- `wubu_dappath_pb(void)`
- `wubu_dappath_cap(void)`
- `wubu_dappath_mux(void)`
- `wubu_dappath_mix(void)`
- `wubu_dappath_summary(char *out, size_t cap)`

## `src/kernel/wubu_dax.h`

- `wubu_dax_probe(void)`
- `wubu_dax_present(void)`
- `wubu_dax_pmem(void)`
- `wubu_dax_fs(void)`
- `wubu_dax_inode(void)`
- `wubu_dax_dev(void)`
- `wubu_dax_summary(char *out, size_t cap)`

## `src/kernel/wubu_ddcci.h`

- `wubu_ddcci_probe(void)`
- `wubu_ddcci_present(void)`
- `wubu_ddcci_i2c(void)`
- `wubu_ddcci_cec(void)`
- `wubu_ddcci_edid(void)`
- `wubu_ddcci_ctrl(void)`
- `wubu_ddcci_summary(char *out, size_t cap)`

## `src/kernel/wubu_decode.h`

- `wubu_decode_probe(void)`
- `wubu_decode_present(void)`
- `wubu_decode_h264(void)`
- `wubu_decode_h265(void)`
- `wubu_decode_av1(void)`
- `wubu_decode_vp9(void)`
- `wubu_decode_summary(char *out, size_t cap)`

## `src/kernel/wubu_density_plan.h`

- `wubu_density_plan_init(wubu_density_plan_t *p)`
- `wubu_density_plan_cycle(wubu_density_plan_t *p)`
- `wubu_density_plan_gpu_frame(wubu_density_plan_t *p, uint64_t n_tokens)`

## `src/kernel/wubu_devmapper.h`

- `wubu_devmapper_probe(void)`
- `wubu_devmapper_present(void)`
- `wubu_devmapper_linear(void)`
- `wubu_devmapper_stripe(void)`
- `wubu_devmapper_mirror(void)`
- `wubu_devmapper_snapshot(void)`
- `wubu_devmapper_summary(char *out, size_t cap)`

## `src/kernel/wubu_display.h`

- `wubu_display_xe_preferred(void)`
- `wubu_display_has_render_node(void)`
- `wubu_display_card_index(void)`

## `src/kernel/wubu_dmcrypt.h`

- `wubu_dmcrypt_probe(void)`
- `wubu_dmcrypt_present(void)`
- `wubu_dmcrypt_luks(void)`
- `wubu_dmcrypt_aes(void)`
- `wubu_dmcrypt_xts(void)`
- `wubu_dmcrypt_dm(void)`
- `wubu_dmcrypt_summary(char *out, size_t cap)`

## `src/kernel/wubu_dram_hedge.h`

- `wdh_init(wdh_hedge_t *h, size_t elem_size, unsigned num_replicas)`
- `wdh_fini(wdh_hedge_t *h)`
- `wdh_destroy(wdh_hedge_t *h)`
- `wdh_put(wdh_hedge_t *h, size_t idx, const void *data)`
- `wdh_get(wdh_hedge_t *h, size_t idx, void *out)`
- `wdh_detect_channel_bit(void)`
- `wdh_channel_bit(const wdh_hedge_t *h)`
- `wdh_channels_guaranteed(const wdh_hedge_t *h)`
- `wdh_probe_trefi(double *out_median_cyc, double *out_spike_pct)`
- `wdh_reader_read(wdh_reader_t *r, size_t idx, void *out)`
- `wdh_reader_destroy(wdh_reader_t *r)`

## `src/kernel/wubu_drm.h`

- `wubu_drm_probe(void)`
- `wubu_drm_present(void)`
- `wubu_drm_kms(void)`
- `wubu_drm_gem(void)`
- `wubu_drm_prime(void)`
- `wubu_drm_msi(void)`
- `wubu_drm_summary(char *out, size_t cap)`

## `src/kernel/wubu_drv_ahci.h`

- `wubu_ahci_present(void)`

## `src/kernel/wubu_drv_gpu.h`

- `wubu_gpu_present_wsl(void)`

## `src/kernel/wubu_drv_install.h`

- `wubu_drv_modalias(const wubu_drv_dev_t *dev, char *out, size_t cap)`
- `wubu_drv_acquire(const wubu_drv_manifest_t *m)`
- `wubu_drv_elf_load(const void *obj, size_t obj_len)`
- `wubu_drv_install(const wubu_drv_dev_t *dev)`
- `wubu_drv_install_report(char *out, size_t cap)`

## `src/kernel/wubu_dspgraph.h`

- `wubu_dspgraph_probe(void)`
- `wubu_dspgraph_present(void)`
- `wubu_dspgraph_dapm(void)`
- `wubu_dspgraph_widget(void)`
- `wubu_dspgraph_path(void)`
- `wubu_dspgraph_route(void)`
- `wubu_dspgraph_summary(char *out, size_t cap)`

## `src/kernel/wubu_dsptrace.h`

- `wubu_dsptrace_probe(void)`
- `wubu_dsptrace_present(void)`
- `wubu_dsptrace_level(int level)`
- `wubu_dsptrace_summary(char *out, size_t cap)`

## `src/kernel/wubu_ducking.h`

- `wubu_ducking_probe(void)`
- `wubu_ducking_present(void)`
- `wubu_ducking_comp(void)`
- `wubu_ducking_limiter(void)`
- `wubu_ducking_gate(void)`
- `wubu_ducking_sidechain(void)`
- `wubu_ducking_summary(char *out, size_t cap)`

## `src/kernel/wubu_edid_kms.h`

- `wubu_edid_kms_scan(wubu_connector_edid_t *out, int cap)`

## `src/kernel/wubu_edid_xrandr.h`

- `wubu_edid_xrandr_scan(wubu_connector_edid_t *out, int cap)`

## `src/kernel/wubu_encode.h`

- `wubu_encode_probe(void)`
- `wubu_encode_present(void)`
- `wubu_encode_h264(void)`
- `wubu_encode_h265(void)`
- `wubu_encode_vp9(void)`
- `wubu_encode_av1(void)`
- `wubu_encode_summary(char *out, size_t cap)`

## `src/kernel/wubu_evdev.h`

- `wubu_evdev_init(void)`
- `wubu_evdev_poll(void)`
- `wubu_evdev_inject(uint16_t type, uint16_t code, int32_t value)`

## `src/kernel/wubu_fantml.h`

- `wubu_fantml_probe(void)`
- `wubu_fantml_present(void)`
- `wubu_fantml_fan_pct(int rpm, int max_rpm)`
- `wubu_fantml_summary(char *out, size_t cap)`

## `src/kernel/wubu_fbcon.h`

- `wubu_fbcon_probe(void)`
- `wubu_fbcon_present(void)`
- `wubu_fbcon_drm(void)`
- `wubu_fbcon_rotate(void)`
- `wubu_fbcon_virtual(void)`
- `wubu_fbcon_mode(void)`
- `wubu_fbcon_summary(char *out, size_t cap)`

## `src/kernel/wubu_fence.h`

- `wubu_fence_probe(void)`
- `wubu_fence_present(void)`
- `wubu_fence_timeout(void)`
- `wubu_fence_signal(void)`
- `wubu_fence_amd(void)`
- `wubu_fence_i915(void)`
- `wubu_fence_summary(char *out, size_t cap)`

## `src/kernel/wubu_fencesync.h`

- `wubu_fencesync_probe(void)`
- `wubu_fencesync_present(void)`
- `wubu_fencesync_timeline(void)`
- `wubu_fencesync_wait(void)`
- `wubu_fencesync_timeout(void)`
- `wubu_fencesync_signal(void)`
- `wubu_fencesync_summary(char *out, size_t cap)`

## `src/kernel/wubu_flush2.h`

- `wubu_flush2_probe(void)`
- `wubu_flush2_present(void)`
- `wubu_flush2_barrier(void)`
- `wubu_flush2_fsync(void)`
- `wubu_flush2_cache(void)`
- `wubu_flush2_flushcmd(void)`
- `wubu_flush2_summary(char *out, size_t cap)`

## `src/kernel/wubu_fusefs.h`

- `wubu_fusefs_probe(void)`
- `wubu_fusefs_present(void)`
- `wubu_fusefs_dev(void)`
- `wubu_fusefs_mount(void)`
- `wubu_fusefs_ctl(void)`
- `wubu_fusefs_conn(void)`
- `wubu_fusefs_summary(char *out, size_t cap)`

## `src/kernel/wubu_gaad.h`

- `wubu_gaad_region_scale(const WubuGaadTranslate *t, int region_idx)`

## `src/kernel/wubu_gamepadbm.h`

- `wubu_gamepadbm_probe(void)`
- `wubu_gamepadbm_present(void)`
- `wubu_gamepadbm_map(int button)`
- `wubu_gamepadbm_is_pressed(int value)`
- `wubu_gamepadbm_summary(char *out, size_t cap)`

## `src/kernel/wubu_gamepaddz.h`

- `wubu_gamepaddz_probe(void)`
- `wubu_gamepaddz_present(void)`
- `wubu_gamepaddz_filter(int value, int deadzone)`
- `wubu_gamepaddz_is_drift(int value, int deadzone)`
- `wubu_gamepaddz_summary(char *out, size_t cap)`

## `src/kernel/wubu_gesture.h`

- `wubu_gesture_init(void)`
- `wubu_gesture_reset(void)`
- `wubu_gesture_poll(WubuGesture *out)`
- `wubu_gesture_rotation_deg(void)`
- `wubu_gesture_open_touchpad(void)`
- `wubu_gesture_poll_fd(int fd, WubuGesture *out)`

## `src/kernel/wubu_gpucsched.h`

- `wubu_gpucsched_probe(void)`
- `wubu_gpucsched_present(void)`
- `wubu_gpucsched_priority(int base)`
- `wubu_gpucsched_timeslice_ms(int queue)`
- `wubu_gpucsched_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpudc.h`

- `wubu_gpudc_probe(void)`
- `wubu_gpudc_present(void)`
- `wubu_gpudc_type(int outputs)`
- `wubu_gpudc_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpufw.h`

- `wubu_gpufw_probe(void)`
- `wubu_gpufw_present(void)`
- `wubu_gpufw_match(int vendor_id)`
- `wubu_gpufw_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpufwupd.h`

- `wubu_gpufwupd_probe(void)`
- `wubu_gpufwupd_present(void)`
- `wubu_gpufwupd_match(int current, int expected)`
- `wubu_gpufwupd_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpukms.h`

- `wubu_gpukms_probe(void)`
- `wubu_gpukms_present(void)`
- `wubu_gpukms_valid_mode(int width, int height, int refresh)`
- `wubu_gpukms_is_active(int crtc_active, int connector_connected)`
- `wubu_gpukms_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpumem.h`

- `wubu_gpumem_probe(void)`
- `wubu_gpumem_present(void)`
- `wubu_gpumem_tier(int bandwidth_gb_per_s)`
- `wubu_gpumem_is_gb(void)`
- `wubu_gpumem_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpurst.h`

- `wubu_gpurst_probe(void)`
- `wubu_gpurst_present(void)`
- `wubu_gpurst_ring(void)`
- `wubu_gpurst_hb(void)`
- `wubu_gpurst_timeout(void)`
- `wubu_gpurst_recover(void)`
- `wubu_gpurst_summary(char *out, size_t cap)`

## `src/kernel/wubu_gpushader.h`

- `wubu_gpushader_probe(void)`
- `wubu_gpushader_present(void)`
- `wubu_gpushader_model_int(int major, int minor)`
- `wubu_gpushader_summary(char *out, size_t cap)`

## `src/kernel/wubu_gt2xx.h`

- `wubu_gt2xx_probe(void)`
- `wubu_gt2xx_present(void)`
- `wubu_gt2xx_needs_nouveau(int legacy_eol)`
- `wubu_gt2xx_nouveau_available(int nouveau_present)`
- `wubu_gt2xx_summary(char *out, size_t cap)`

## `src/kernel/wubu_hid.h`

- `wubu_hid_filter(uint8_t device, uint32_t kind_mask)`
- `wubu_hid_disable(uint8_t device, bool disabled)`

## `src/kernel/wubu_hive.h`

- `wubu_hive_destroy(wubu_hive_t *h)`
- `wubu_hive_size(const wubu_hive_t *h)`
- `wubu_hive_capacity(const wubu_hive_t *h)`
- `wubu_hive_block_count(const wubu_hive_t *h)`
- `wubu_hive_empty(const wubu_hive_t *h)`
- `wubu_hive_erase_at(wubu_hive_t *h, wubu_hive_iter_t *it)`

## `src/kernel/wubu_hpet.h`

- `wubu_hpet_probe(uint64_t *period_fs)`

## `src/kernel/wubu_hw_detect.h`

- `wubu_hw_gpu_present(void)`
- `wubu_hw_gpu_vendor(void)`
- `wubu_hw_needs_amdvlk(void)`
- `wubu_hw_intel_uses_xe(void)`
- `wubu_hw_has_prime(void)`
- `wubu_hw_has_dzn(void)`

## `src/kernel/wubu_ieccontrol.h`

- `wubu_ieccontrol_probe(void)`
- `wubu_ieccontrol_present(void)`
- `wubu_ieccontrol_aes(void)`
- `wubu_ieccontrol_enc(void)`
- `wubu_ieccontrol_clock(void)`
- `wubu_ieccontrol_rate(void)`
- `wubu_ieccontrol_summary(char *out, size_t cap)`

## `src/kernel/wubu_input.h`

- `wubu_input_uses_ble(void)`
- `wubu_input_usb_bus_present(void)`

## `src/kernel/wubu_instinct.h`

- `wubu_instinct_probe(void)`
- `wubu_instinct_present(void)`
- `wubu_instinct_uses_rocm(int rocm_available)`
- `wubu_instinct_is_datacenter(int gpu_type)`
- `wubu_instinct_summary(char *out, size_t cap)`

## `src/kernel/wubu_intel_gma.h`

- `wubu_intel_gma_probe(void)`
- `wubu_intel_gma_present(void)`
- `wubu_intel_gma_uses_i915(int gen)`
- `wubu_intel_gma_needs_llvmpipe(int accel_available)`
- `wubu_intel_gma_summary(char *out, size_t cap)`

## `src/kernel/wubu_intel_icelake.h`

- `wubu_intel_icelake_probe(void)`
- `wubu_intel_icelake_present(void)`
- `wubu_intel_icelake_uses_iris(int gl_available)`
- `wubu_intel_icelake_has_anv(int anv_available)`
- `wubu_intel_icelake_summary(char *out, size_t cap)`

## `src/kernel/wubu_intel_skylake.h`

- `wubu_intel_skylake_probe(void)`
- `wubu_intel_skylake_present(void)`
- `wubu_intel_skylake_uses_iris(int gl_available)`
- `wubu_intel_skylake_has_anv(int anv_available)`
- `wubu_intel_skylake_summary(char *out, size_t cap)`

## `src/kernel/wubu_intelgpu.h`

- `wubu_intelgpu_probe(void)`
- `wubu_intelgpu_present(void)`
- `wubu_intelgpu_driver(int gen)`
- `wubu_intelgpu_needs_firmware(int gen)`
- `wubu_intelgpu_summary(char *out, size_t cap)`

## `src/kernel/wubu_iommu.h`

- `wubu_iommu_probe(wubu_iommu_t *out)`

## `src/kernel/wubu_ioprio.h`

- `wubu_ioprio_probe(void)`
- `wubu_ioprio_present(void)`
- `wubu_ioprio_rt(void)`
- `wubu_ioprio_be(void)`
- `wubu_ioprio_idle(void)`
- `wubu_ioprio_sched(void)`
- `wubu_ioprio_summary(char *out, size_t cap)`

## `src/kernel/wubu_iosched.h`

- `wubu_iosched_probe(void)`
- `wubu_iosched_present(void)`
- `wubu_iosched_mq(void)`
- `wubu_iosched_deadline(void)`
- `wubu_iosched_kyber(void)`
- `wubu_iosched_bfq(void)`
- `wubu_iosched_none(void)`
- `wubu_iosched_summary(char *out, size_t cap)`

## `src/kernel/wubu_jack.h`

- `wubu_jack_probe(void)`
- `wubu_jack_present(void)`
- `wubu_jack_headphone(void)`
- `wubu_jack_mic(void)`
- `wubu_jack_spdif(void)`
- `wubu_jack_impedance(void)`
- `wubu_jack_summary(char *out, size_t cap)`

## `src/kernel/wubu_jackdetect.h`

- `wubu_jackdetect_probe(void)`
- `wubu_jackdetect_present(void)`
- `wubu_jackdetect_headset(void)`
- `wubu_jackdetect_mic(void)`
- `wubu_jackdetect_omtp(void)`
- `wubu_jackdetect_ctia(void)`
- `wubu_jackdetect_summary(char *out, size_t cap)`

## `src/kernel/wubu_jackimpedance.h`

- `wubu_jackimpedance_probe(void)`
- `wubu_jackimpedance_present(void)`
- `wubu_jackimpedance_headphone(void)`
- `wubu_jackimpedance_mic(void)`
- `wubu_jackimpedance_line(void)`
- `wubu_jackimpedance_threshold(void)`
- `wubu_jackimpedance_summary(char *out, size_t cap)`

## `src/kernel/wubu_jackstate.h`

- `wubu_jackstate_probe(void)`
- `wubu_jackstate_present(void)`
- `wubu_jackstate_plug(void)`
- `wubu_jackstate_unplug(void)`
- `wubu_jackstate_debounce(void)`
- `wubu_jackstate_stable(void)`
- `wubu_jackstate_summary(char *out, size_t cap)`

## `src/kernel/wubu_leaudio.h`

- `wubu_leaudio_probe(void)`
- `wubu_leaudio_present(void)`
- `wubu_leaudio_latency(int frame_length)`
- `wubu_leaudio_summary(char *out, size_t cap)`

## `src/kernel/wubu_leaudioldr.h`

- `wubu_leaudioldr_probe(void)`
- `wubu_leaudioldr_present(void)`
- `wubu_leaudioldr_samples(int frame_us)`
- `wubu_leaudioldr_is_valid_frame(int samples)`
- `wubu_leaudioldr_summary(char *out, size_t cap)`

## `src/kernel/wubu_lvm.h`

- `wubu_lvm_probe(void)`
- `wubu_lvm_present(void)`
- `wubu_lvm_health(int size_mb, int used_mb)`
- `wubu_lvm_summary(char *out, size_t cap)`

## `src/kernel/wubu_lzx.h`

- `lzx_free(struct lzx_state *s)`

## `src/kernel/wubu_mali_g52.h`

- `wubu_mali_g52_probe(void)`
- `wubu_mali_g52_present(void)`
- `wubu_mali_g52_uses_panfrost(int panfrost_available)`
- `wubu_mali_g52_opengl_es(int gles_level)`
- `wubu_mali_g52_summary(char *out, size_t cap)`

## `src/kernel/wubu_mali_g720.h`

- `wubu_mali_g720_probe(void)`
- `wubu_mali_g720_present(void)`
- `wubu_mali_g720_uses_panthor(int panthor_available)`
- `wubu_mali_g720_vulkan(int vk_level)`
- `wubu_mali_g720_summary(char *out, size_t cap)`

## `src/kernel/wubu_mali_g77.h`

- `wubu_mali_g77_probe(void)`
- `wubu_mali_g77_present(void)`
- `wubu_mali_g77_uses_panfrost(int panfrost_available)`
- `wubu_mali_g77_has_panvk(int panvk_available)`
- `wubu_mali_g77_summary(char *out, size_t cap)`

## `src/kernel/wubu_mdraid.h`

- `wubu_mdraid_probe(void)`
- `wubu_mdraid_present(void)`
- `wubu_mdraid_degraded(int total, int active)`
- `wubu_mdraid_summary(char *out, size_t cap)`

## `src/kernel/wubu_mem.h`

- `wubu_mem_ue_count(void)`

## `src/kernel/wubu_memmap.h`

- `wubu_memmap_init(wubu_memmap_info_t *info)`

## `src/kernel/wubu_memmgr.h`

- `wubu_memmgr_probe(void)`
- `wubu_memmgr_present(void)`
- `wubu_memmgr_gem(void)`
- `wubu_memmgr_ttm(void)`
- `wubu_memmgr_vram(void)`
- `wubu_memmgr_gtt(void)`
- `wubu_memmgr_summary(char *out, size_t cap)`

## `src/kernel/wubu_mixergraph.h`

- `wubu_mixergraph_probe(void)`
- `wubu_mixergraph_present(void)`
- `wubu_mixergraph_pb(void)`
- `wubu_mixergraph_cap(void)`
- `wubu_mixergraph_mon(void)`
- `wubu_mixergraph_groups(void)`
- `wubu_mixergraph_summary(char *out, size_t cap)`

## `src/kernel/wubu_mmu.h`

- `wubu_mmu_probe(void)`
- `wubu_mmu_present(void)`
- `wubu_mmu_pagetable(void)`
- `wubu_mmu_fault(void)`
- `wubu_mmu_vma(void)`
- `wubu_mmu_ctx(void)`
- `wubu_mmu_summary(char *out, size_t cap)`

## `src/kernel/wubu_modsym.h`

- `wubu_modsym_register(const char *dll, const char *name, uintptr_t addr)`
- `wubu_modsym_lookup(const char *dll, const char *name, uintptr_t *out)`

## `src/kernel/wubu_n9.h`

- `wubu_n9_boot(void)`

## `src/kernel/wubu_navi10.h`

- `wubu_navi10_probe(void)`
- `wubu_navi10_present(void)`
- `wubu_navi10_uses_radv(int vulkan_available)`
- `wubu_navi10_kernel_min(int kernel_version)`
- `wubu_navi10_summary(char *out, size_t cap)`

## `src/kernel/wubu_net.h`

- `wubu_net_wifi_vendor(void)`

## `src/kernel/wubu_nfsclient.h`

- `wubu_nfsclient_probe(void)`
- `wubu_nfsclient_present(void)`
- `wubu_nfsclient_idmapd(void)`
- `wubu_nfsclient_statd(void)`
- `wubu_nfsclient_mount(void)`
- `wubu_nfsclient_sec(void)`
- `wubu_nfsclient_summary(char *out, size_t cap)`

## `src/kernel/wubu_nfsmount.h`

- `wubu_nfsmount_probe(void)`
- `wubu_nfsmount_present(void)`
- `wubu_nfsmount_mount(void)`
- `wubu_nfsmount_vers(void)`
- `wubu_nfsmount_rsize(void)`
- `wubu_nfsmount_wsize(void)`
- `wubu_nfsmount_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvidia_fermi.h`

- `wubu_nvidia_fermi_probe(void)`
- `wubu_nvidia_fermi_present(void)`
- `wubu_nvidia_fermi_needs_legacy(int fermi)`
- `wubu_nvidia_fermi_eol_status(void)`
- `wubu_nvidia_fermi_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvidia_kepler.h`

- `wubu_nvidia_kepler_probe(void)`
- `wubu_nvidia_kepler_present(void)`
- `wubu_nvidia_kepler_needs_legacy(int kepler)`
- `wubu_nvidia_kepler_nvk_support(int gen)`
- `wubu_nvidia_kepler_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvidia_maxwell.h`

- `wubu_nvidia_maxwell_probe(void)`
- `wubu_nvidia_maxwell_present(void)`
- `wubu_nvidia_maxwell_uses_proprietary(int nvidia_version)`
- `wubu_nvidia_maxwell_has_nvk(int nvk_status)`
- `wubu_nvidia_maxwell_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvidia_pascal.h`

- `wubu_nvidia_pascal_probe(void)`
- `wubu_nvidia_pascal_present(void)`
- `wubu_nvidia_pascal_needs_535(int cuda_version)`
- `wubu_nvidia_pascal_max_cuda(int driver_major)`
- `wubu_nvidia_pascal_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvidia_turing.h`

- `wubu_nvidia_turing_probe(void)`
- `wubu_nvidia_turing_present(void)`
- `wubu_nvidia_turing_has_rt_core(int rt_supported)`
- `wubu_nvidia_turing_vulkan(int vulkan_level)`
- `wubu_nvidia_turing_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvidia_volta.h`

- `wubu_nvidia_volta_probe(void)`
- `wubu_nvidia_volta_present(void)`
- `wubu_nvidia_volta_is_datacenter(int gpu_type)`
- `wubu_nvidia_volta_cuda_gencode(int cuda_version)`
- `wubu_nvidia_volta_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvme_gen4.h`

- `wubu_nvme_gen4_probe(void)`
- `wubu_nvme_gen4_present(void)`
- `wubu_nvme_gen4_speed_gbps(int lanes)`
- `wubu_nvme_gen4_is_fast(int speed_gbps)`
- `wubu_nvme_gen4_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvme_gen5.h`

- `wubu_nvme_gen5_probe(void)`
- `wubu_nvme_gen5_present(void)`
- `wubu_nvme_gen5_speed_gbps(int gen, int lanes)`
- `wubu_nvme_gen5_is_fast(int speed_gbps)`
- `wubu_nvme_gen5_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvmehotplug.h`

- `wubu_nvmehotplug_probe(void)`
- `wubu_nvmehotplug_present(void)`
- `wubu_nvmehotplug_stable(int events, int interval_ms)`
- `wubu_nvmehotplug_is_event(int prev, int curr)`
- `wubu_nvmehotplug_summary(char *out, size_t cap)`

## `src/kernel/wubu_nvmepower.h`

- `wubu_nvmepower_probe(void)`
- `wubu_nvmepower_present(void)`
- `wubu_nvmepower_state(int state)`
- `wubu_nvmepower_apst_latency(int us)`
- `wubu_nvmepower_summary(char *out, size_t cap)`

## `src/kernel/wubu_opencl.h`

- `wubu_opencl_probe(void)`
- `wubu_opencl_present(void)`
- `wubu_opencl_has_rocm(int rocm_available)`
- `wubu_opencl_has_cuda(int cuda_available)`
- `wubu_opencl_summary(char *out, size_t cap)`

## `src/kernel/wubu_overclock.h`

- `wubu_overclock_probe(void)`
- `wubu_overclock_present(void)`
- `wubu_overclock_od(void)`
- `wubu_overclock_sysfs(void)`
- `wubu_overclock_core(void)`
- `wubu_overclock_mem(void)`
- `wubu_overclock_summary(char *out, size_t cap)`

## `src/kernel/wubu_panel.h`

- `wubu_panel_probe(void)`
- `wubu_panel_present(void)`
- `wubu_panel_lvds(void)`
- `wubu_panel_edp(void)`
- `wubu_panel_hdmi(void)`
- `wubu_panel_dp(void)`
- `wubu_panel_vga(void)`
- `wubu_panel_summary(char *out, size_t cap)`

## `src/kernel/wubu_pci.h`

- `wubu_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)`

## `src/kernel/wubu_pcipme.h`

- `wubu_pcipme_probe(void)`
- `wubu_pcipme_present(void)`
- `wubu_pcipme_pmcsr(void)`
- `wubu_pcipme_acpi(void)`
- `wubu_pcipme_wake(void)`
- `wubu_pcipme_pmeint(void)`
- `wubu_pcipme_summary(char *out, size_t cap)`

## `src/kernel/wubu_pcmlink.h`

- `wubu_pcmlink_probe(void)`
- `wubu_pcmlink_present(void)`
- `wubu_pcmlink_channels(const char *fmt)`
- `wubu_pcmlink_summary(char *out, size_t cap)`

## `src/kernel/wubu_pcmplugin.h`

- `wubu_pcmplugin_probe(void)`
- `wubu_pcmplugin_present(void)`
- `wubu_pcmplugin_rate(void)`
- `wubu_pcmplugin_vol(void)`
- `wubu_pcmplugin_copy(void)`
- `wubu_pcmplugin_plugtype(void)`
- `wubu_pcmplugin_summary(char *out, size_t cap)`

## `src/kernel/wubu_pcmring.h`

- `wubu_pcmring_probe(void)`
- `wubu_pcmring_present(void)`
- `wubu_pcmring_latency_us(int rate, int period, int buf)`
- `wubu_pcmring_summary(char *out, size_t cap)`

## `src/kernel/wubu_pdpolicy.h`

- `wubu_pdpolicy_probe(void)`
- `wubu_pdpolicy_present(void)`
- `wubu_pdpolicy_contract(void)`
- `wubu_pdpolicy_pdo(void)`
- `wubu_pdpolicy_pps(void)`
- `wubu_pdpolicy_dual(void)`
- `wubu_pdpolicy_summary(char *out, size_t cap)`

## `src/kernel/wubu_perf.h`

- `wubu_perf_probe(void)`
- `wubu_perf_present(void)`
- `wubu_perf_summary(char *out, size_t cap)`

## `src/kernel/wubu_perfmon.h`

- `wubu_perfmon_probe(void)`
- `wubu_perfmon_present(void)`
- `wubu_perfmon_event(void)`
- `wubu_perfmon_cycles(void)`
- `wubu_perfmon_cache(void)`
- `wubu_perfmon_occ(void)`
- `wubu_perfmon_summary(char *out, size_t cap)`

## `src/kernel/wubu_power.h`

- `wubu_power_has_battery(void)`
- `wubu_power_has_thermal(void)`
- `wubu_power_has_fan(void)`
- `wubu_power_ncores(void)`

## `src/kernel/wubu_powergate.h`

- `wubu_powergate_probe(void)`
- `wubu_powergate_present(void)`
- `wubu_powergate_runtime(void)`
- `wubu_powergate_shader(void)`
- `wubu_powergate_texture(void)`
- `wubu_powergate_l2(void)`
- `wubu_powergate_summary(char *out, size_t cap)`

## `src/kernel/wubu_powervr.h`

- `wubu_powervr_probe(void)`
- `wubu_powervr_present(void)`
- `wubu_powervr_uses_pvrsrvkm(int kernel_616)`
- `wubu_powervr_has_vulkan(int mesa_253)`
- `wubu_powervr_summary(char *out, size_t cap)`

## `src/kernel/wubu_psych.h`

- `wubu_psych_disclose(uint32_t session, uint32_t skill)`
- `wubu_psych_input_seen(wubu_psych_attention_t *a, uint64_t now_ms)`
- `wubu_psych_ai_act(wubu_psych_attention_t *a, uint64_t now_ms)`
- `wubu_psych_ai_yield(wubu_psych_attention_t *a)`
- `wubu_psych_should_speak(const wubu_psych_user_t *u, uint64_t idle_ms)`
- `wubu_psych_should_assist(const wubu_psych_user_t *u, uint32_t stuck_ms)`
- `wubu_psych_should_yield(const wubu_psych_user_t *u, uint64_t idle_ms)`
- `wubu_psych_wait_before_hint(uint32_t stuck_ms, uint32_t skill)`
- `wubu_psych_typing_pause(uint32_t wpm)`
- `wubu_psych_praise_strength(uint32_t progress)`

## `src/kernel/wubu_quadro.h`

- `wubu_quadro_probe(void)`
- `wubu_quadro_present(void)`
- `wubu_quadro_is_professional(int gpu_type)`
- `wubu_quadro_has_isv(int isv_certified)`
- `wubu_quadro_summary(char *out, size_t cap)`

## `src/kernel/wubu_radeon_5000.h`

- `wubu_radeon_5000_probe(void)`
- `wubu_radeon_5000_present(void)`
- `wubu_radeon_5000_supports_legacy(int legacy_available)`
- `wubu_radeon_5000_is_evergreen(int family)`
- `wubu_radeon_5000_summary(char *out, size_t cap)`

## `src/kernel/wubu_radeon_6000.h`

- `wubu_radeon_6000_probe(void)`
- `wubu_radeon_6000_present(void)`
- `wubu_radeon_6000_needs_legacy(int amdgpu_support)`
- `wubu_radeon_6000_is_pre_gcn(int family)`
- `wubu_radeon_6000_summary(char *out, size_t cap)`

## `src/kernel/wubu_radeon_legacy.h`

- `wubu_radeon_legacy_probe(void)`
- `wubu_radeon_legacy_present(void)`
- `wubu_radeon_legacy_gen(int family)`
- `wubu_radeon_legacy_supported_by_amdgpu(int family)`
- `wubu_radeon_legacy_summary(char *out, size_t cap)`

## `src/kernel/wubu_raid5.h`

- `wubu_raid5_probe(void)`
- `wubu_raid5_present(void)`
- `wubu_raid5_stripe(void)`
- `wubu_raid5_layout(void)`
- `wubu_raid5_parity(void)`
- `wubu_raid5_disks(void)`
- `wubu_raid5_summary(char *out, size_t cap)`

## `src/kernel/wubu_rendernode.h`

- `wubu_rendernode_probe(void)`
- `wubu_rendernode_present(void)`
- `wubu_rendernode_valid_fd(int fd)`
- `wubu_rendernode_priority(int ctx_priority)`
- `wubu_rendernode_summary(char *out, size_t cap)`

## `src/kernel/wubu_renoir.h`

- `wubu_renoir_probe(void)`
- `wubu_renoir_present(void)`
- `wubu_renoir_uses_radv(int vulkan_available)`
- `wubu_renoir_is_apu(int integration)`
- `wubu_renoir_summary(char *out, size_t cap)`

## `src/kernel/wubu_rtc.h`

- `wubu_rtc_read(wubu_rtc_tm *tm)`
- `wubu_rtc_12h_to_24h(uint8_t reg)`

## `src/kernel/wubu_samplerate.h`

- `wubu_samplerate_probe(void)`
- `wubu_samplerate_present(void)`
- `wubu_samplerate_pcm(void)`
- `wubu_samplerate_float(void)`
- `wubu_samplerate_24bit(void)`
- `wubu_samplerate_hi(void)`
- `wubu_samplerate_summary(char *out, size_t cap)`

## `src/kernel/wubu_self_test.h`

- `wubu_self_test_run(uint32_t *total)`

## `src/kernel/wubu_serial.h`

- `inb(COM1_PORT)`

## `src/kernel/wubu_sha256.h`

- `wubu_sha256(const void *data, size_t len, uint8_t out[WUBU_SHA256_SZ])`

## `src/kernel/wubu_smart.h`

- `wubu_smart_probe(void)`
- `wubu_smart_present(void)`
- `wubu_smart_ata(void)`
- `wubu_smart_nvme(void)`
- `wubu_smart_health(void)`
- `wubu_smart_temp(void)`
- `wubu_smart_summary(char *out, size_t cap)`

## `src/kernel/wubu_smbios.h`

- `wubu_smbios_find_eps(void)`
- `wubu_smbios_probe(wubu_smbios_t *out)`
- `wubu_smbios_walk(uint64_t table, wubu_smbios_t *out)`

## `src/kernel/wubu_smc.h`

- `wubu_smc_probe(void)`
- `wubu_smc_present(void)`
- `wubu_smc_smu(void)`
- `wubu_smc_vcn(void)`
- `wubu_smc_uvd(void)`
- `wubu_smc_fw(void)`
- `wubu_smc_summary(char *out, size_t cap)`

## `src/kernel/wubu_smp.h`

- `wubu_smp_start_aps(void)`

## `src/kernel/wubu_smr.h`

- `wubu_smr_probe(void)`
- `wubu_smr_present(void)`
- `wubu_smr_zone(void)`
- `wubu_smr_zns(void)`
- `wubu_smr_wp(void)`
- `wubu_smr_zonefs(void)`
- `wubu_smr_summary(char *out, size_t cap)`

## `src/kernel/wubu_spdifrx.h`

- `wubu_spdifrx_probe(void)`
- `wubu_spdifrx_present(void)`
- `wubu_spdifrx_rate(void)`
- `wubu_spdifrx_lock(void)`
- `wubu_spdifrx_format(void)`
- `wubu_spdifrx_pcm(void)`
- `wubu_spdifrx_summary(char *out, size_t cap)`

## `src/kernel/wubu_spdifstatus.h`

- `wubu_spdifstatus_probe(void)`
- `wubu_spdifstatus_present(void)`
- `wubu_spdifstatus_lock(void)`
- `wubu_spdifstatus_valid(void)`
- `wubu_spdifstatus_aes(void)`
- `wubu_spdifstatus_rate(void)`
- `wubu_spdifstatus_summary(char *out, size_t cap)`

## `src/kernel/wubu_spdiftx.h`

- `wubu_spdiftx_probe(void)`
- `wubu_spdiftx_present(void)`
- `wubu_spdiftx_iec(void)`
- `wubu_spdiftx_ac3(void)`
- `wubu_spdiftx_dts(void)`
- `wubu_spdiftx_optical(void)`
- `wubu_spdiftx_summary(char *out, size_t cap)`

## `src/kernel/wubu_storage.h`

- `wubu_storage_queue_depth(void)`

## `src/kernel/wubu_storagesched.h`

- `wubu_storagesched_probe(void)`
- `wubu_storagesched_present(void)`
- `wubu_storagesched_mq(void)`
- `wubu_storagesched_bfq(void)`
- `wubu_storagesched_deadline(void)`
- `wubu_storagesched_none(void)`
- `wubu_storagesched_summary(char *out, size_t cap)`

## `src/kernel/wubu_swap.h`

- `wubu_swap_out(uint64_t va, uint64_t phys)`
- `wubu_swap_in(uint64_t va, uint32_t slot)`

## `src/kernel/wubu_sync.h`

- `wubu_spin_lock(wubu_spinlock_t *l)`
- `wubu_fifo_init(wubu_fifo_t *f, uint32_t cap)`

## `src/kernel/wubu_theme.h`

- `wubu_theme_node_set(const char *path, uint32_t value)`
- `wubu_theme_set_write_observer(wubu_theme_write_observer_fn fn)`
- `wubu_theme_node_list(char *buf, int bufsz)`

## `src/kernel/wubu_thermal_read.h`

- `wubu_thermal_read_temp(int zone)`
- `wubu_thermal_nvme(void)`
- `wubu_thermal_ram(void)`

## `src/kernel/wubu_thermalthrottle.h`

- `wubu_thermalthrottle_probe(void)`
- `wubu_thermalthrottle_present(void)`
- `wubu_thermalthrottle_zone(void)`
- `wubu_thermalthrottle_cooling(void)`
- `wubu_thermalthrottle_trip(void)`
- `wubu_thermalthrottle_governor(void)`
- `wubu_thermalthrottle_summary(char *out, size_t cap)`

## `src/kernel/wubu_uac.h`

- `wubu_uac_probe(void)`
- `wubu_uac_present(void)`
- `wubu_uac_uac1(void)`
- `wubu_uac_uac2(void)`
- `wubu_uac_iso(void)`
- `wubu_uac_alt(void)`
- `wubu_uac_summary(char *out, size_t cap)`

## `src/kernel/wubu_uas.h`

- `wubu_uas_probe(void)`
- `wubu_uas_present(void)`
- `wubu_uas_bot(void)`
- `wubu_uas_uasp(void)`
- `wubu_uas_queue(void)`
- `wubu_uas_part(void)`
- `wubu_uas_summary(char *out, size_t cap)`

## `src/kernel/wubu_usbf.h`

- `wubu_usbf_has_ehci(void)`
- `wubu_usbf_has_ohci(void)`
- `wubu_usbf_has_usb4(void)`
- `wubu_usbf_has_usbc(void)`
- `wubu_usbf_has_otg(void)`
- `wubu_usbf_has_hid(void)`
- `wubu_usbf_has_storage(void)`
- `wubu_usbf_has_audio(void)`
- `wubu_usbf_has_video(void)`
- `wubu_usbf_has_network(void)`
- `wubu_usbf_has_serial(void)`
- `wubu_usbf_host_vendor(void)`

## `src/kernel/wubu_user.h`

- `wubu_user_enter(uint64_t entry, uint64_t user_stack_top)`
- `wubu_user_selftest(void)`

## `src/kernel/wubu_vblank.h`

- `wubu_vblank_probe(void)`
- `wubu_vblank_present(void)`
- `wubu_vblank_counter(void)`
- `wubu_vblank_event(void)`
- `wubu_vblank_time(void)`
- `wubu_vblank_flip(void)`
- `wubu_vblank_summary(char *out, size_t cap)`

## `src/kernel/wubu_vc4.h`

- `wubu_vc4_probe(void)`
- `wubu_vc4_present(void)`
- `wubu_vc4_uses_vc4_v3d(int dual_driver)`
- `wubu_vc4_has_3d(int v3d_available)`
- `wubu_vc4_summary(char *out, size_t cap)`

## `src/kernel/wubu_vc6.h`

- `wubu_vc6_probe(void)`
- `wubu_vc6_present(void)`
- `wubu_vc6_uses_v3d(int v3d_available)`
- `wubu_vc6_has_vulkan(int vulkan_available)`
- `wubu_vc6_summary(char *out, size_t cap)`

## `src/kernel/wubu_vdso.h`

- `wubu_vdso_init(void)`

## `src/kernel/wubu_vega.h`

- `wubu_vega_probe(void)`
- `wubu_vega_present(void)`
- `wubu_vega_radv(int vulkan_available)`
- `wubu_vega_hbm_memory(int hbm_available)`
- `wubu_vega_summary(char *out, size_t cap)`

## `src/kernel/wubu_virt.h`

- `wubu_virt_has_virtio(void)`

## `src/kernel/wubu_vmm.h`

- `wubu_vmm_ref(uint64_t phys, uint32_t n)`
- `wubu_vmm_refcount(uint64_t phys)`
- `wubu_vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags)`
- `wubu_vmm_register_demand(uint64_t base, uint32_t pages)`
- `wubu_vmm_demand_fill(uint64_t va)`

## `src/kernel/wubu_volcanic_islands.h`

- `wubu_volcanic_islands_probe(void)`
- `wubu_volcanic_islands_present(void)`
- `wubu_volcanic_islands_uses_amdgpu(int amdgpu_folded)`
- `wubu_volcanic_islands_radv(int vulkan_available)`
- `wubu_volcanic_islands_summary(char *out, size_t cap)`

## `src/kernel/wubu_voltagectl.h`

- `wubu_voltagectl_probe(void)`
- `wubu_voltagectl_present(void)`
- `wubu_voltagectl_mv_to_uv(int mV)`
- `wubu_voltagectl_summary(char *out, size_t cap)`

## `src/kernel/wubu_vpudecode.h`

- `wubu_vpudecode_probe(void)`
- `wubu_vpudecode_present(void)`
- `wubu_vpudecode_supported(int codec)`
- `wubu_vpudecode_summary(char *out, size_t cap)`

## `src/kernel/wubu_vpuencode.h`

- `wubu_vpuencode_probe(void)`
- `wubu_vpuencode_present(void)`
- `wubu_vpuencode_rate(int width, int height, int fps)`
- `wubu_vpuencode_summary(char *out, size_t cap)`

## `src/kernel/wubu_vrr.h`

- `wubu_vrr_probe(void)`
- `wubu_vrr_present(void)`
- `wubu_vrr_is_freesync(int freesync_available)`
- `wubu_vrr_is_gsync(int gsync_available)`
- `wubu_vrr_summary(char *out, size_t cap)`

## `src/kernel/wubu_vulkan14.h`

- `wubu_vulkan14_probe(void)`
- `wubu_vulkan14_present(void)`
- `wubu_vulkan14_full_profile(int full_available)`
- `wubu_vulkan14_is_conformant(int radv_conformant)`
- `wubu_vulkan14_summary(char *out, size_t cap)`

## `src/kernel/wubu_wifi7.h`

- `wubu_wifi7_6ghz(void)`
- `wubu_wifi7_320mhz(void)`
- `wubu_wifi7_vendor(void)`

## `src/kernel/wubu_wifiutil.h`

- `wubu_wifiutil_probe(void)`
- `wubu_wifiutil_present(void)`
- `wubu_wifiutil_cca(void)`
- `wubu_wifiutil_airtime(void)`
- `wubu_wifiutil_survey(void)`
- `wubu_wifiutil_chan(void)`
- `wubu_wifiutil_summary(char *out, size_t cap)`

## `src/kernel/wubu_writeback.h`

- `wubu_writeback_probe(void)`
- `wubu_writeback_present(void)`
- `wubu_writeback_dirty(void)`
- `wubu_writeback_sync(void)`
- `wubu_writeback_interval(void)`
- `wubu_writeback_thread(void)`
- `wubu_writeback_summary(char *out, size_t cap)`

## `src/kernel/wubu_xe3.h`

- `wubu_xe3_probe(void)`
- `wubu_xe3_present(void)`
- `wubu_xe3_uses_xe_driver(int xe_available)`
- `wubu_xe3_uses_iris_anv(int mesa_ready)`
- `wubu_xe3_summary(char *out, size_t cap)`

## `src/kernel/wubu_xhci.h`

- `wubu_xhci_probe(wubu_xhci_t *out)`
- `wubu_xhci_start(wubu_xhci_t *xh)`
- `wubu_xhci_slot_alloc(wubu_xhci_t *xh)`
- `wubu_xhci_probe_regs(uint64_t mmio, wubu_xhci_t *out)`

## `src/kernel/wubu_zip.h`

- `wubu_zip_open(const uint8_t *data, uint32_t len, wubu_zip_archive *z)`

## `src/kernel/wubu_znszone.h`

- `wubu_znszone_probe(void)`
- `wubu_znszone_present(void)`
- `wubu_znszone_active(int total, int used)`
- `wubu_znszone_summary(char *out, size_t cap)`

## `src/kernel/wubu_zoneappend.h`

- `wubu_zoneappend_probe(void)`
- `wubu_zoneappend_present(void)`
- `wubu_zoneappend_ok(int zone_state, int seq_pos)`
- `wubu_zoneappend_summary(char *out, size_t cap)`

## `src/kernel/wubu_zonecap.h`

- `wubu_zonecap_probe(void)`
- `wubu_zonecap_present(void)`
- `wubu_zonecap_safe(int wp, int zone_cap, int len)`
- `wubu_zonecap_full(int wp, int zone_cap)`
- `wubu_zonecap_summary(char *out, size_t lim)`

## `src/kernel/wubu_zonefmt.h`

- `wubu_zonefmt_probe(void)`
- `wubu_zonefmt_present(void)`
- `wubu_zonefmt_reset_ok(int zone_state)`
- `wubu_zonefmt_summary(char *out, size_t cap)`

## `src/kernel/wubu_zonseqwrite.h`

- `wubu_zonseqwrite_probe(void)`
- `wubu_zonseqwrite_present(void)`
- `wubu_zonseqwrite_ok(int zone_state, int wp, int len, int max_len)`
- `wubu_zonseqwrite_summary(char *out, size_t cap)`

## `src/kernel/WuBuKernel/include/wubudriver.h`

- `wubu_driver_unregister(wubu_driver *drv)`
- `wubu_device_register(wubu_device *dev)`
- `wubu_device_unregister(wubu_device *dev)`
- `wubu_bus_bind(void)`
- `wubu_bus_detach_all(void)`
- `wubu_device_release(wubu_device *dev)`
- `wubu_driver_count(void)`
- `wubu_device_count(void)`

## `src/kernel/WuBuKernel/include/wubukcore.h`

- `wubu_console_set_sink(wubu_log_sink sink, void *arg)`
- `wubu_klog(const char *fmt, ...)`
- `wubu_klog_raw(const char *s)`
- `wubu_kfree(void *p)`
- `wubu_heap_free(void)`
- `wubu_heap_used(void)`

## `src/gui/daemon_panel.h`

- `daemon_panel_holyd_state(void)`
- `daemon_panel_container_count(void)`
- `daemon_panel_holyd_session_count(void)`

## `src/gui/dos_guest_view.h`

- `dos_window_close(DosGuiWindow *win)`

## `src/gui/era_apps.h`

- `era_apps_register(void)`
- `era_apps_launch(int idx)`

## `src/gui/explorer_internal.h`

- `ex_show_context_menu(ExExplorerState *ex, int mx, int my, int entry_idx)`
- `ex_hide_context_menu(ExExplorerState *ex)`
- `explorer_update_drive_list(void)`
- `str_contains_nocase(const char *haystack, const char *needle)`
- `ex_9p_stat(const char *path, struct stat *st)`
- `ex_strcasecmp(const char *a, const char *b)`
- `ex_sort_entries(ExExplorerState *ex)`
- `ex_file_compare(const void *a, const void *b)`
- `ex_populate_tree(ExTreeNode *node, const char *path)`
- `ex_tree_free(ExTreeNode *node)`

## `src/gui/service_mgr.h`

- `service_mgr_init(void)`
- `service_mgr_register_autostart(const char *root, const char *svc)`
- `service_mgr_boot(void)`

## `src/gui/term.h`

- `term_spawn_exec(char *const *argv, const char *label)`

## `src/gui/term_internal.h`

- `term_screen_bind_pty(TermScreen *out, TermPtySession *pty)`
- `term_ansi_parse(TermScreen *scr, const char *buf, int n)`
- `term_ansi_drain_fd(int fd, TermScreen *scr)`

## `src/gui/term_pty.h`

- `term_new_tab_unspawned(const char *label)`
- `term_handle_key_container(TermState *term, uint32_t key, uint32_t mods)`

## `src/gui/wm.h`

- `wm_typer_start(DosGuiWindow *win, const char *text)`
- `wm_typer_tick(int dt_ms)`
- `wm_typer_busy(void)`
- `wm_typer_pos(void)`
- `wm_window_id_at(int i)`
- `wm_window_minimized_at(int i)`
- `wm_restore(DosGuiWindow *win)`
- `wm_is_maximized(DosGuiWindow *win)`
- `wm_is_minimized(DosGuiWindow *win)`
- `wm_get_mouse(int *x, int *y)`
- `systray_remove(const char *name)`
- `systray_update_color(const char *name, uint32_t color)`
- `systray_color(const char *name)`
- `systray_set_notification_count(const char *name, int count)`
- `notif_center_mark_read(uint32_t id)`
- `notif_center_clear(void)`
- `notif_center_render(uint32_t *fb, int fb_w, int fb_h)`
- `notif_center_is_open(void)`
- `notif_center_toggle(void)`
- `wm_restore_icon_layout(void)`
- `wm_reload_wallpaper(void)`
- `wm_is_initialized(void)`
- `wm_wallpaper_mode(void)`
- `wm_wallpaper_w(void)`
- `wm_wallpaper_h(void)`
- `wm_set_auto_arrange(bool on)`
- `wm_get_auto_arrange(void)`
- `wm_set_icons_visible(bool show)`
- `wm_dirty_count(void)`
- `chrome_hit_test_button(DosGuiWindow *win, int mx, int my)`

## `src/gui/wm_internal.h`

- `taskbar_top(void)`
- `taskbar_modern_layout(int fb_w, int fb_h, WmTaskLayout *L)`
- `taskbar_modern_handle(int x, int y, int btn, int kind)`
- `border_width(void)`
- `theme_radius(void)`
- `load_default_wallpaper(void)`
- `draw_wallpaper(int fb_w, int fb_h)`
- `draw_desktop_bg(int fb_w, int fb_h)`
- `snap_icon_to_grid(DosGuiIcon *icon)`
- `icon_grid_x(int x)`
- `icon_grid_y(int y)`
- `wm_draw_icon_selection(int ox, int oy)`
- `snap_window_to_gaad(DosGuiWindow *w)`
- `snap_window_to_zone(DosGuiWindow *w, int zone)`
- `wubu_snap_layout_zones(int layout)`
- `wubu_snap_apply_layout_zone(DosGuiWindow *w, int layout, int zone)`
- `icon_hit_test(int mx, int my)`
- `icon_select_in_rect(int x0, int y0, int x1, int y1)`
- `spawn_window(int x, int y, int w, int h, const char *title)`

## `src/gui/wubu_a11y.h`

- `wubu_a11y_mouse(DosGuiWindow *win, int x, int y, int btn, int kind)`
- `wubu_a11y_key(DosGuiWindow *win, uint32_t key, uint32_t mods)`
- `wubu_a11y_minimize_window(DosGuiWindow *win)`
- `wubu_a11y_close_window(DosGuiWindow *win)`
- `wubu_a11y_purge_and_close(DosGuiWindow *win)`
- `wubu_a11y_min_contrast(void)`

## `src/gui/wubu_bonzi.h`

- `wubu_bonzi_activity(void)`
- `wubu_bonzi_mood(int state)`
- `wubu_bonzi_init(int x, int y)`
- `wubu_bonzi_place(int screen_w, int screen_h, int taskbar_h)`
- `wubu_bonzi_tick(int dt_ms)`
- `wubu_bonzi_draw(uint32_t *fb, int fb_w, int fb_h)`
- `wubu_bonzi_mouse(int x, int y, int btn, int kind)`
- `wubu_bonzi_set_bubble(const char *line1, const char *line2)`

## `src/gui/wubu_clipboard.h`

- `wubu_clipboard_history_count(void)`
- `wubu_clipboard_history_is_pinned(int index)`
- `wubu_clipboard_history_set_pinned(int index, bool pinned)`
- `wubu_clipboard_history_paste(int index)`
- `wubu_clipboard_history_clear(void)`

## `src/gui/wubu_gamelib.h`

- `wubu_gamelib_clear_start_menu(void)`

## `src/gui/wubu_gamelib_internal.h`

- `gamelib_ensure_dir(const char *path)`

## `src/gui/wubu_ladder.h`

- `wubu_ladder_gain_at(const wubu_ladder_t *f, float freq_hz)`

## `src/gui/wubu_mascot.h`

- `wubu_mascot_blit(int x, int y)`
- `wubu_mascot_blit_rgba(int x, int y, const unsigned char *rgba, int size)`

## `src/gui/wubu_pkgmgr.h`

- `wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t* out)`

## `src/gui/wubu_runline.h`

- `wubu_runline_execute(const char *command_line, const char *result_file)`

## `src/gui/wubu_screenshot.h`

- `png_encode_rgba(const uint32_t *px, int w, int h, uint8_t **out)`
- `wubu_screenshot_handle_alt_printscr(void)`
- `wubu_screenshot_handle_shift_printscr(void)`

## `src/gui/wubu_session.h`

- `wubu_session_restore(void)`

## `src/gui/wubu_settings.h`

- `wubu_settings_get_cursor_size(void)`
- `wubu_settings_high_contrast(void)`
- `wubu_settings_reduce_motion(void)`
- `wubu_settings_font_size(void)`

## `src/gui/wubu_sound.h`

- `wubu_sound_set_volume(wubu_sound_t *s, float volume)`
- `wubu_sound_set_mute(wubu_sound_t *s, int muted)`

## `src/gui/wubu_theme.h`

- `wubu_theme_apply_seed(uint32_t accent)`
- `wubu_theme_sync_from_kernel(void)`
- `wubu_theme_set_high_contrast(bool on)`
- `wubu_theme_high_contrast(void)`
- `wubu_theme_contrast_audit(void)`

## `src/gui/wubu_waveosc.h`

- `wubu_waveosc_init(wubu_waveosc_t *o)`

## `src/gui/lua/wubu_lua_gui.h`

- `wubu_lua_gui_free(WubuLuaGui *g)`
- `wubu_lua_gui_load(WubuLuaGui *g, const char *path, char *err, size_t err_cap)`
- `wubu_lua_gui_draw(WubuLuaGui *g, int fb_w, int fb_h)`
- `wubu_lua_gui_key(WubuLuaGui *g, const char *key, uint32_t mods)`

## `src/gui/shell/shell_session.h`

- `desktop_launch_icon(int icon_id)`

## `src/gui/shell/power_menu.h`

- `power_menu_open(void)`
- `power_menu_close(void)`
- `power_menu_is_open(void)`

## `src/gui/shell/run_dialog.h`

- `run_dialog_open(void)`
- `run_dialog_close(void)`
- `run_dialog_is_open(void)`
- `run_dialog_handle_key(uint32_t key, uint32_t mods)`

## `src/gui/shell/startmenu.h`

- `startmenu_search_init(void)`
- `startmenu_search_update(const char *query)`
- `startmenu_search_clear(void)`
- `startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count)`
- `startmenu_recent_add(const char *app_name)`
- `startmenu_recent_get(SmProgramEntry **out_entries, int max)`
- `startmenu_tree_build(void)`
- `startmenu_tree_toggle(SmTreeNode *node)`
- `startmenu_tree_render(uint32_t *fb, int fb_w, int fb_h, int x, int y)`
- `startmenu_power(PowerAction action)`
- `startmenu_render_search_bar(uint32_t *fb, int fb_w, int fb_h, int menu_x, int menu_y, int w)`
- `startmenu_render_recent(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int max_items)`
- `startmenu_render_all_programs(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h)`
- `startmenu_render_power_options(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w)`
- `startmenu_handle_key(int key, uint32_t mods)`
- `startmenu_handle_search_input(int key, uint32_t mods)`
- `startmenu_build_programs_db(void)`
- `startmenu_init_enhanced(void)`

## `src/gui/shell/startmenu_internal.h`

- `startmenu_build_main_menu(void)`
- `startmenu_build_submenus(void)`

## `src/gui/shell/win11_snap.h`

- `win11_snap_toggle(void)`
- `win11_snap_is_open(void)`
- `win11_snap_close(void)`
- `win11_snap_render(uint32_t *fb, int fb_w, int fb_h)`
- `win11_snap_button_count(void)`
- `win11_snap_button_size(int *w, int *h)`

## `src/gui/shell/win11_widgets.h`

- `win11_widgets_toggle(void)`
- `win11_widgets_is_open(void)`
- `win11_widgets_close(void)`
- `win11_widgets_render(uint32_t *fb, int fb_w, int fb_h)`

## `src/gui/shell/window_chrome.h`

- `chrome_caption_metrics(void)`
- `chrome_is_win11(void)`

## `src/gui/shell/holyc_repl.h`

- `holyc_term_set_eval(holyc_term_eval_fn fn)`

## `src/gui/shell/wm_rim.h`

- `wm_rim_add(float t, float radius, uint32_t color, const char *exec)`
- `wm_rim_clear(void)`
- `wm_rim_count(void)`
- `wm_rim_handle_pointer(int x, int y, int btn, int kind)`

## `src/gui/shell/wm_slime.h`

- `wm_slime_clear(void)`
- `wm_slime_count(void)`
- `wm_slime_handle_pointer(int x, int y, int btn, int kind)`

## `src/gui/shell/wubu_lock_screen.h`

- `wubu_lock_screen_lock(void)`
- `wubu_lock_screen_unlock(void)`
- `wubu_lock_screen_is_locked(void)`
- `wubu_lock_screen_set_enabled(bool enabled)`
- `wubu_lock_screen_is_enabled(void)`
- `wubu_lock_screen_handle_key(uint32_t key, uint32_t mods)`
- `wubu_lock_screen_render(uint32_t *fb, int fb_w, int fb_h)`

## `src/gui/shell/wubu_osd.h`

- `wubu_osd_volume(void)`
- `wubu_osd_set_muted(bool muted)`
- `wubu_osd_muted(void)`
- `wubu_osd_brightness(void)`

## `src/gui/shell/wubu_screensaver.h`

- `wubu_screensaver_set_enabled(bool enabled)`
- `wubu_screensaver_is_enabled(void)`

## `src/gui/shell/wubu_shell_greeter.h`

- `wubu_shell_greeter_run(void)`
- `wubu_shell_greeter_key(uint32_t key)`
- `wubu_shell_greeter_button_rect(int *x, int *y, int *w, int *h)`
- `wubu_shell_greeter_steps(void)`
- `wubu_shell_greeter_step(void)`
- `wubu_shell_greeter_advance(void)`
- `wubu_shell_greeter_back(void)`
- `wubu_shell_greeter_offer_rect(int i, int *x, int *y, int *w, int *h)`

## `src/gui/shell/wubu_wallpaper.h`

- `wubu_wallpaper_load(const char *path, WubuWallpaper *out)`

## `src/gui/vws/vws.h`

- `vws_form_move(VwsForm *f, const char *id, int left, int top)`
- `vws_form_resize(VwsForm *f, const char *id, int w, int h)`
- `vws_form_set_text(VwsForm *f, const char *id, const char *text)`
- `vws_form_set_handler(VwsForm *f, const char *id, const char *fn)`
- `vws_form_remove(VwsForm *f, const char *id)`
- `vws_form_bring_front(VwsForm *f, const char *id)`
- `vws_form_load_template(const char *name, VwsForm *f)`

## `src/gui/vws/vws_internal.h`

- `vws_scan_handlers(VwsSource *src, VwsForm *f)`
- `vws_source_free(VwsSource *src)`

## `src/gui/vws/vws_lua.h`

- `vws_lua_free(VwsLua *vl)`

## `src/gui/vws/vws_templates.h`

- `vws_template_foreach_ctrl(const char *name, VwsTemplateCtrlFn fn, void *ud)`

## `src/gui/vws/externals/wet/wet.h`

- `wet_app_set_origin(WetApp *app, int x, int y)`
- `wet_app_root(WetApp *app, WetWidget *w)`
- `wet_app_width(const WetApp *app)`
- `wet_app_height(const WetApp *app)`
- `wet_widget_free(WetWidget *w)`
- `wet_widget_nchildren(const WetWidget *w)`
- `wet_widget_add(WetWidget *parent, WetWidget *child)`
- `wet_widget_remove(WetWidget *parent, WetWidget *child)`
- `wet_widget_class(const WetWidget *w)`
- `wet_widget_rect(const WetWidget *w)`
- `wet_widget_desired(const WetWidget *w)`
- `wet_widget_set_visible(WetWidget *w, bool visible)`
- `wet_widget_visible(const WetWidget *w)`
- `wet_widget_set_enabled(WetWidget *w, bool enabled)`
- `wet_widget_enabled(const WetWidget *w)`
- `wet_widget_set_focus(WetApp *app, WetWidget *w)`
- `wet_app_pointer(WetApp *app, WetEventKind kind, int x, int y, int button)`
- `wet_app_key(WetApp *app, WetEventKind kind, int key, uint32_t mods)`

## `src/gui/vws/externals/wet/wet_internal.h`

- `wet_theme_set(const WetTheme *t)`
- `wet_dispatch(WetApp *app, WetWidget *w, const WetEvent *ev)`
- `wet_app_index(WetApp *app, WetWidget *w)`
- `wet_widget_arrange(WetWidget *w, WetRect r)`
- `wet_widget_render(WetWidget *w, WetRender *r)`

## `src/gui/vws/externals/wet/wet_lua.h`

- `wet_lua_free(WetLua *wl)`
- `wet_lua_dispatch(WetLua *wl, WetWidget *w, const void *ev)`

## `src/gui/vws/externals/wet/externals/wua/src/wb_lv.h`

- `wb_lv_truthy(wb_lv_value v)`
- `wb_lv_equal(wb_lv *L, wb_lv_value a, wb_lv_value b)`
- `wb_lv_rawequal(wb_lv_value a, wb_lv_value b)`
- `wb_lv_free(wb_lv *L)`
- `wb_lv_tonumber(wb_lv_value v, int *ok)`
- `wb_lv_tointeger(wb_lv_value v, int *ok)`
- `wb_lv_openlibs(wb_lv *L)`

## `src/gui/vws/externals/wet/externals/wua/src/wb_lv_internal.h`

- `wb_lv_set_err(wb_lv *L, const char *fmt, ...)`
- `wb_lv_error(wb_lv *L, const char *fmt, ...)`
- `wb_lv_error_value(wb_lv *L, wb_lv_value v)`
- `wb_lv_next_tok(parser *P)`
- `wb_lv_perr(parser *P, const char *msg)`
- `wb_lv_eval(wb_lv *L, ex *e, env *en)`
- `wb_lv_eval_multi(wb_lv *L, ex *e, env *en, wb_lv_value **out, int *nout)`
- `wb_lv_exec(wb_lv *L, ex *e, env *en, int *was_return)`
- `wb_lv_call_lua(wb_lv *L, wb_lv_func *fn, wb_lv_value *args, int nargs)`
- `wb_lv_call_lua_results(wb_lv *L, wb_lv_func *fn, wb_lv_value *args, int nargs)`
- `wb_lv_call_value(wb_lv *L, wb_lv_value fn, wb_lv_value *args, int nargs, wb_lv_value **out)`
- `wb_lv_multi_value(wb_lv *L, wb_lv_value *v, int n)`
- `wb_lv_exec_root(wb_lv *L, ex *root)`

## `src/gui/compositor/gui_dbuf.h`

- `gui_dbuf_flip(gui_dbuf_t *db)`
- `gui_dbuf_effect_apply(gui_dbuf_t *db)`

## `src/gui/compositor/wubu_compositor.h`

- `wubu_window_close(WuBuWindow *win)`
- `wubu_compositor_gpu_init(WuBuCompositor *comp)`
- `wubu_compositor_gpu_fini(WuBuCompositor *comp)`
- `wubu_compositor_a11y_tree_get_root(WuBuCompositor *comp, WuBuA11yNode *root)`
- `wubu_compositor_a11y_node_get_children(WuBuA11yNode *parent, WuBuA11yNode *children, int max)`
- `wubu_compositor_a11y_announce(WuBuCompositor *comp, const char *message)`

## `src/gui/wet/wet.h`

- `wet_app_set_origin(WetApp *app, int x, int y)`
- `wet_app_root(WetApp *app, WetWidget *w)`
- `wet_app_width(const WetApp *app)`
- `wet_app_height(const WetApp *app)`
- `wet_widget_free(WetWidget *w)`
- `wet_widget_nchildren(const WetWidget *w)`
- `wet_widget_add(WetWidget *parent, WetWidget *child)`
- `wet_widget_remove(WetWidget *parent, WetWidget *child)`
- `wet_widget_class(const WetWidget *w)`
- `wet_widget_rect(const WetWidget *w)`
- `wet_widget_desired(const WetWidget *w)`
- `wet_widget_set_visible(WetWidget *w, bool visible)`
- `wet_widget_visible(const WetWidget *w)`
- `wet_widget_set_enabled(WetWidget *w, bool enabled)`
- `wet_widget_enabled(const WetWidget *w)`
- `wet_widget_set_focus(WetApp *app, WetWidget *w)`
- `wet_app_pointer(WetApp *app, WetEventKind kind, int x, int y, int button)`
- `wet_app_key(WetApp *app, WetEventKind kind, int key, uint32_t mods)`

## `src/gui/wet/wet_internal.h`

- `wet_theme_set(const WetTheme *t)`
- `wet_theme_count(void)`
- `wet_dispatch(WetApp *app, WetWidget *w, const WetEvent *ev)`
- `wet_app_index(WetApp *app, WetWidget *w)`
- `wet_widget_arrange(WetWidget *w, WetRect r)`
- `wet_widget_render(WetWidget *w, WetRender *r)`

## `src/gui/wet/wet_lua.h`

- `wet_lua_free(WetLua *wl)`
- `wet_lua_dispatch(WetLua *wl, WetWidget *w, const void *ev)`

## `src/gui/wet/externals/wua/src/wb_lv.h`

- `wb_lv_truthy(wb_lv_value v)`
- `wb_lv_equal(wb_lv *L, wb_lv_value a, wb_lv_value b)`
- `wb_lv_rawequal(wb_lv_value a, wb_lv_value b)`
- `wb_lv_multi_value(wb_lv *L, wb_lv_value *v, int n)`
- `wb_lv_free(wb_lv *L)`
- `wb_lv_tonumber(wb_lv_value v, int *ok)`
- `wb_lv_tointeger(wb_lv_value v, int *ok)`
- `wb_lv_call(wb_lv *L, wb_lv_value fn, wb_lv_value *args, int nargs)`
- `wb_lv_openlibs(wb_lv *L)`

## `src/gui/wet/externals/wua/src/wb_lv_internal.h`

- `wb_lv_set_err(wb_lv *L, const char *fmt, ...)`
- `wb_lv_error(wb_lv *L, const char *fmt, ...)`
- `wb_lv_error_value(wb_lv *L, wb_lv_value v)`
- `wb_lv_next_tok(parser *P)`
- `wb_lv_perr(parser *P, const char *msg)`
- `wb_lv_eval(wb_lv *L, ex *e, env *en)`
- `wb_lv_eval_multi(wb_lv *L, ex *e, env *en, wb_lv_value **out, int *nout)`
- `wb_lv_exec(wb_lv *L, ex *e, env *en, int *was_return)`
- `wb_lv_call_lua(wb_lv *L, wb_lv_func *fn, wb_lv_value *args, int nargs)`
- `wb_lv_call_lua_results(wb_lv *L, wb_lv_func *fn, wb_lv_value *args, int nargs)`
- `wb_lv_call_value(wb_lv *L, wb_lv_value fn, wb_lv_value *args, int nargs, wb_lv_value **out)`
- `wb_lv_multi_value(wb_lv *L, wb_lv_value *v, int n)`
- `wb_lv_exec_root(wb_lv *L, ex *root)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d.h`

- `wubu_vkd3d_state_map(uint32_t d3d12_state, wubu_vkd3d_state_t *out)`
- `wubu_vkd3d_tracker_state(const wubu_vkd3d_tracker_t *t, uint32_t resource_id)`
- `wubu_vkd3d_tracker_count(const wubu_vkd3d_tracker_t *t)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_cmd.h`

- `wubu_vkd3d_cmd_alloc_reset(wubu_vkd3d_cmd_alloc_t *a)`
- `wubu_vkd3d_cmd_list_destroy(wubu_vkd3d_cmd_list_t *l)`
- `wubu_vkd3d_cmd_list_state(const wubu_vkd3d_cmd_list_t *l)`
- `wubu_vkd3d_cmd_list_type(const wubu_vkd3d_cmd_list_t *l)`
- `wubu_vkd3d_cmd_list_fence(const wubu_vkd3d_cmd_list_t *l)`
- `wubu_vkd3d_cmd_list_close(wubu_vkd3d_cmd_list_t *l)`
- `wubu_vkd3d_cmd_vk(wubu_vkd3d_cmd_type_t type, wubu_vkd3d_cmd_vk_t *out)`
- `wubu_vkd3d_cmd_pool_flags(void)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_device.h`

- `wubu_vkd3d_device_destroy(wubu_vkd3d_device_t *dev)`
- `wubu_vkd3d_device_feature_level(const wubu_vkd3d_device_t *dev)`
- `wubu_vkd3d_device_supports_command(const wubu_vkd3d_device_t *dev, uint32_t cmd_type)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_fence.h`

- `wubu_vkd3d_fence_init(wubu_vkd3d_fence_t *f)`
- `wubu_vkd3d_fence_signal(wubu_vkd3d_fence_t *f, uint64_t value)`
- `wubu_vkd3d_fence_wait(wubu_vkd3d_fence_t *f, uint64_t value)`
- `wubu_vkd3d_fence_vk(const wubu_vkd3d_fence_t *f, wubu_vkd3d_fence_vk_t *out)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_framebuffer.h`

- `wubu_vkd3d_aspect_flags(VkFormat format, VkImageAspectFlags *out)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_heap.h`

- `wubu_vkd3d_heap_destroy(wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_capacity(const wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_descriptor_size(const wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_gpu_visible(const wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_type(const wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_id(const wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_bytes(const wubu_vkd3d_heap_t *h)`
- `wubu_vkd3d_heap_write(wubu_vkd3d_heap_t *h, uint32_t index, const void *bytes)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_msaa.h`

- `wubu_vkd3d_needs_resolve(uint32_t count)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_physdev.h`

- `wubu_vkd3d_physdev_can_present(const wubu_vkd3d_physdev_t *p, uint32_t family)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_precache.h`

- `wubu_vkd3d_precache_destroy(wubu_vkd3d_precache_t *c)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_pso.h`

- `wubu_vkd3d_pso_key(const wubu_vkd3d_pso_desc_t *d)`
- `wubu_vkd3d_pso_release(wubu_vkd3d_pso_cache_t *c, uint32_t pipeline)`
- `wubu_vkd3d_pso_cache_size(const wubu_vkd3d_pso_cache_t *c)`
- `wubu_vkd3d_pso_cache_hits(const wubu_vkd3d_pso_cache_t *c)`
- `wubu_vkd3d_pso_cache_misses(const wubu_vkd3d_pso_cache_t *c)`
- `wubu_vkd3d_pso_vk(const wubu_vkd3d_pso_desc_t *d, wubu_vkd3d_pso_vk_t *out)`

## `src/spells/graphics/render/vkd3d/wubu_vkd3d_swapchain.h`

- `wubu_vkd3d_frame_latency_can_present(const wubu_vkd3d_frame_latency_t *f)`
- `wubu_vkd3d_frame_latency_submit(wubu_vkd3d_frame_latency_t *f)`
- `wubu_vkd3d_frame_latency_complete(wubu_vkd3d_frame_latency_t *f)`
- `wubu_vkd3d_frame_latency_in_flight(const wubu_vkd3d_frame_latency_t *f)`

## `src/spells/networking/aggregation/swarm/wubu_swarm.h`

- `wubu_swarm_self(wubu_swarm_t *s, const char *name)`
- `wubu_swarm_count_kind(const wubu_swarm_t *s, const char *kind)`

## `src/spells/networking/aggregation/bootattest/wubu_boot_attest.h`

- `wubu_boot_attest_destroy(wubu_boot_attest_t *b)`
- `wubu_boot_attest_seal(wubu_boot_attest_t *b, uint64_t now_unix)`
- `wubu_boot_attest_get_boot_counter(const wubu_boot_attest_t *b)`
- `wubu_boot_attest_get_sealed_at(const wubu_boot_attest_t *b)`
- `wubu_boot_attest_get_stage_count(const wubu_boot_attest_t *b)`

## `src/brain/include/bench.h`

- `gpu_free_ssm_weights(gpu_ssm_weights *w)`
- `gpu_free_gqa_weights(gpu_gqa_weights *w)`
- `gpu_free_output_weight(float *d_weight)`
- `gpu_ssm_scratch_needed(int B, int T)`

## `src/brain/include/cpu_timing.h`

- `wubu_rdtsc()`
- `wubu_timing_end(t0)`

## `src/brain/include/cuda_kernels.h`

- `wubu_cuda_silu(int n, const float *x, float *y, cudaStream_t stream)`
- `wubu_cuda_sigmoid(int n, const float *x, float *y, cudaStream_t stream)`
- `wubu_cuda_softplus(int n, const float *x, float *y, cudaStream_t stream)`
- `wubu_cuda_exp(int n, const float *x, float *y, cudaStream_t stream)`
- `wubu_cuda_precompute_rotary(int T, float *d_sincos, cudaStream_t stream)`
- `wubu_cuda_chunked_attn_query_scratch(int C, int T_max)`
- `dequant_q4_0_cache_kernel(int n_blocks, const uint8_t *blocks, __half *out)`
- `quant_q4_0_cache_kernel(int n, const __half *in, uint8_t *blocks)`
- `wubu_cuda_norm(const float *d_in, float *d_out, int n_vecs, int dim, cudaStream_t stream)`
- `wubu_cuda_moe_dispatch_query_scratch(int B, int T)`
- `wubu_cuda_init(cublasHandle_t *handle, cudaStream_t *stream)`
- `wubu_cuda_destroy(cublasHandle_t handle, cudaStream_t stream)`
- `wubu_cuda_free(float *ptr)`
- `wubu_cuda_to_device(const float *host, float *dev, size_t n_bytes, cudaStream_t stream)`
- `wubu_cuda_to_host(const float *dev, float *host, size_t n_bytes, cudaStream_t stream)`
- `wubu_cuda_softmax(float *x, int N, int D, cudaStream_t stream)`
- `dequant_q4_0_cache_kernel(int n_blocks, const uint8_t *blocks, __half *out)`
- `quant_q4_0_cache_kernel(int n, const __half *in, uint8_t *blocks)`

## `src/brain/include/cuda_vision.h`

- `gelu_kernel(float *x, int n)`
- `add_kernel(const float *x, float *y, int n)`
- `gpu_vision_free_layer(gpu_vision_weights_t *gpu)`

## `src/brain/include/gaad_nesting_llm.h`

- `create_context(int64_t seq_len, int max_segments)`
- `destroy_context(Context* ctx)`
- `decompose_segments(Context* ctx, const float* per_token_score)`
- `build_tree(Context* ctx, int max_depth)`
- `build_mask(Context* ctx, float threshold)`

## `src/brain/include/gguf_reader.h`

- `gguf_open(const char *path)`
- `gguf_buffer_data(gguf_ctx *ctx)`
- `gguf_tokenizer_token_count(gguf_ctx *ctx)`
- `gguf_read_kv_i64(gguf_ctx *ctx, const char *want, int64_t def)`
- `gguf_kv_get_i32(gguf_ctx *ctx, const char *key, int *out)`
- `gguf_kv_get_f32(gguf_ctx *ctx, const char *key, float *out)`
- `gguf_kv_get_i32_arr(gguf_ctx *ctx, const char *key, int *out, int max)`
- `gguf_raw_size(int ggml_type, int64_t n_elems)`
- `gguf_dequantize(const uint8_t *data, int ggml_type, int64_t n_elems, float *output)`
- `gguf_find_tensor(gguf_ctx *ctx, const char *name)`
- `gguf_read_tensor_f32(gguf_ctx *ctx, gguf_tensor_info *tensor, float *output, int64_t max_elems)`
- `gguf_close(gguf_ctx *ctx)`
- `dequantize_q6_K_row(const uint8_t *data, float *output, int64_t n_elems)`
- `dequantize_iq1_s_row(const uint8_t *data, float *output, int64_t n_elems)`
- `dequantize_iq2_xxs_row(const uint8_t *data, float *output, int64_t n_elems)`
- `dequantize_iq2_s_row(const uint8_t *data, float *output, int64_t n_elems)`
- `dequantize_iq2_xxs_block(const uint8_t *block, float *output)`
- `iq2_xxs_dot_block(const uint8_t *block, const float *x)`
- `iq2_xxs_dot_row(const uint8_t *qdata, int n_elems, const float *x)`
- `wubu_exp_map(const float *input, int dim, float R, float *output)`
- `wubu_log_map(const float *input, int dim, float R, float *output)`
- `wubu_norm(const float *v, int dim)`
- `wubu_dot(const float *a, const float *b, int dim)`
- `quantize_row_q8_K(const float *x, block_q8_K *y, int64_t k)`
- `quantize_row_q8_0(const float *x, block_q8_0 *y, int64_t k)`
- `gguf_read_raw_tensor(gguf_ctx *ctx, gguf_tensor_info *tensor, void *output)`
- `gguf_f16_to_f32(uint16_t h)`

## `src/brain/include/gpu_moe_kernel.h`

- `wubu_gpu_moe_init(void)`

## `src/brain/include/gpu_output_proj.h`

- `gpu_output_init(const uint8_t *weight_q, int D, int V, int weight_type)`
- `gpu_output_project_batch(const float *input, float *output, int T)`
- `gpu_output_project(const float *input, float *output)`
- `gpu_output_cleanup(void)`

## `src/brain/include/gpu_quant_matmul.h`

- `wubu_cuda_quant_matmul_scratch(int n_rows, int n_cols, int quant_type)`
- `wubu_cuda_quant_matmul_set_iq1s_grid(const uint64_t *grid)`

## `src/brain/include/gpu_wubu.h`

- `gpu_wubu_init(void)`
- `gpu_wubu_free(void)`
- `gpu_wubu_ready(void)`
- `gpu_wubu_mark_weights_dirty(void)`
- `gpu_wubu_ns5(float *X, int rows, int cols)`
- `gpu_wubu_ns5_gram(float *X, int rows, int cols)`
- `gpu_wubu_elt_add(float *y, const float *x, int n)`

## `src/brain/include/kv_arena.h`

- `kv_arena_init(int max_ctx, cudaStream_t stream)`
- `kv_arena_free(kv_arena_t* arena)`
- `kv_arena_append(kv_arena_t* arena, int C, cudaStream_t stream)`

## `src/brain/include/lfm2_math.h`

- `lfm2_matmul_f32(const float *x, const float *W, int M, int K, int N, float *y)`

## `src/brain/include/qlearner.h`

- `qlearner_init(qlearner_t *ql)`
- `qlearner_step(qlearner_t *ql, float loss)`

## `src/brain/include/safetensors_reader.h`

- `st_n_tensors(const st_ctx *ctx)`
- `st_dtype_size(st_dtype_t dt)`
- `st_f16_to_f32(uint16_t v)`
- `st_bf16_to_f32(uint16_t v)`
- `st_close(st_ctx *ctx)`

## `src/brain/include/thread_pool.h`

- `thread_pool_destroy(thread_pool_t *pool)`
- `thread_pool_count(thread_pool_t *pool)`
- `thread_pool_parallel_for(thread_pool_t *pool, int total, void *userdata, range_fn_t fn)`

## `src/brain/include/wubu.h`

- `wubu_model_random_init(wubu_model_t *m)`
- `wubu_set_wubu_mode(wubu_model_t *m, int mode, void *moe)`

## `src/brain/include/wubu1_block.h`

- `wubu1_block_fill(wubu1_block_t *out, const wubu_layer_t *layer)`

## `src/brain/include/wubu_actor_critic.h`

- `wubu_ac_update(wubu_ac_t *ac, const double *s, int a, double r, const double *s_next)`

## `src/brain/include/wubu_adapter.h`

- `wubu_adapter_swap(const char *name, wubu_adapter_t *new_adapter)`

## `src/brain/include/wubu_adapter_compat.h`

- `wubu_adapter_register_shim(const char *old_name, const char *new_name)`

## `src/brain/include/wubu_agi.h`

- `wubu_agi_think(wubu_agi_t *agi, uint16_t *out_tokens, int max_out)`
- `wubu_agi_verify(wubu_agi_t *agi, const wubu_pf_step_t *step)`
- `wubu_orch_judge(wubu_orch_t *orch, const float *cell_confidences)`

## `src/brain/include/wubu_align.h`

- `wubu_unlearn_ascent(float lr, float grad_forget)`
- `wubu_unlearn_anchor_weight(float alpha, float kl_theta_theta0)`
- `wubu_align_push(wubu_align_buffer_t *b, float preference)`
- `wubu_align_monitor_init(wubu_align_monitor_t *m, float drift_sigma)`
- `wubu_align_monitor_feed(wubu_align_monitor_t *m, float reward)`
- `wubu_align_monitor_drifted(const wubu_align_monitor_t *m)`

## `src/brain/include/wubu_amoeba.h`

- `wubu_amoeba_feed_grads(wubu_amoeba_t *am, const float *grad_norms)`
- `wubu_amoeba_diagnose(wubu_amoeba_t *am)`
- `wubu_amoeba_mutate(wubu_amoeba_t *am)`
- `wubu_amoeba_validate(wubu_amoeba_t *am, double held_out_loss)`
- `wubu_amoeba_commit(wubu_amoeba_t *am, int accepted)`

## `src/brain/include/wubu_arena.h`

- `wubu_arena_reset(wubu_arena_t *a)`

## `src/brain/include/wubu_audio.h`

- `wubu_audio_init(wubu_audio_t *a)`

## `src/brain/include/wubu_bi.h`

- `wubu_bi_shrink_candidate(const float *bis, int n_layers, float threshold)`
- `wubu_bi_grow_candidate(const float *bis, int n_layers, float threshold)`

## `src/brain/include/wubu_blueprint.h`

- `wubu_blueprint_allows(wubu_blueprint_t *bp, wubu_bp_axis_t axis, float v)`

## `src/brain/include/wubu_bonzi.h`

- `wubu_bonzi_empathy(float user_valence, float warmth)`
- `wubu_bonzi_turn_delay(float arousal, int msg_len)`
- `wubu_bonzi_chat_prune(int turns, int budget)`
- `wubu_bonzi_self_mood(const wubu_bonzi_mood_t *m, float clarity)`
- `wubu_bonzi_session_id(const char *user, uint32_t seed)`
- `wubu_bonzi_honesty(float confidence, float calibration)`
- `wubu_bonzi_mem_decay(float base, int age, float halflife)`

## `src/brain/include/wubu_bridge.h`

- `wubu_br_confidence_gate(float confidence, float thresh, int budget_left)`
- `wubu_br_persona_guard(float safety_score, float bar)`
- `wubu_br_credit(float calibration, float outcome)`
- `wubu_br_chat_prune(int turns, int budget)`
- `wubu_br_regulate(float confidence, int budget_left, float low, float mid)`
- `wubu_br_mood_predict(float current, int dt, float kernel_scale)`
- `wubu_br_forget_retain(int age, float halflife)`
- `wubu_br_memory_weight(float base, int age, float halflife)`
- `wubu_br_self_pattern(float capability, float *slot)`
- `wubu_br_verify_output(float verifier_score, float bar)`
- `wubu_br_monitor_log(int *log_len, int max_len)`
- `wubu_br_mood_anomaly(float mood_delta, float thresh)`
- `wubu_br_empathy_reward(float emp_w, float emp_l, float beta)`
- `wubu_br_tier(float competence, float low, float high)`
- `wubu_br_monitor_component(float *component, int n, const float *vals)`
- `wubu_br_close_rate(float prev_rate, int closed_this_batch, int batch_size)`

## `src/brain/include/wubu_cache_advice.h`

- `wubu_cache_advice_free(wubu_cache_advice_t *a)`

## `src/brain/include/wubu_causal.h`

- `wubu_scm_add_edge(wubu_scm_t *m, int cause, int effect)`
- `wubu_scm_do(const wubu_scm_t *m, int a, double val, double *out)`
- `wubu_scm_identifiable(const wubu_scm_t *m, int x, int y)`
- `wubu_counter_abduct(const wubu_abduct_t *a, const wubu_abduct_t *b)`

## `src/brain/include/wubu_cegis.h`

- `wubu_cegis_verify(wubu_cegis_spec_t *spec, wubu_cegis_cand_t cand)`

## `src/brain/include/wubu_codec.h`

- `wubu_codec_audio_to_image(const float *pcm, int n_samples, float *out)`
- `wubu_codec_image_to_audio(const float *img, float *out, int out_cap)`

## `src/brain/include/wubu_codeexec.h`

- `wubu_codeexec_init(wubu_codeexec_t *ce)`

## `src/brain/include/wubu_colonel.h`

- `wubu_colonel_pull(wubu_colonel_t *col)`
- `wubu_colonel_report(wubu_colonel_t *col, int idx, int ok)`
- `wubu_colonel_save(const wubu_colonel_t *col, void *buf, long cap)`

## `src/brain/include/wubu_contbatch.h`

- `wubu_contbatch_step(wubu_contbatch_t *cb)`

## `src/brain/include/wubu_contracts.h`

- `wubu_contracts_check(wubu_contracts_t *ct, const float *probes)`
- `wubu_contracts_finite(const float *buf, size_t n)`

## `src/brain/include/wubu_coord.h`

- `wubu_lock_release(wubu_locktab_t *t, const char *file, const char *agent)`
- `wubu_mem_allowed(const wubu_memacl_t *a, const char *agent)`
- `wubu_heartbeat_alive(long last_ms, long now_ms, long window_ms)`

## `src/brain/include/wubu_credit.h`

- `wubu_credit_sign(double credit, double eps)`
- `wubu_credit_sum(const double *credits, int n)`

## `src/brain/include/wubu_credit_sft.h`

- `wubu_credit_mask(const int *succ, int n, int *mask)`

## `src/brain/include/wubu_cross_attn.h`

- `wubu_cross_attn_free(wubu_cross_attn_ctx_t *ctx)`

## `src/brain/include/wubu_ctxvm.h`

- `wubu_ctx_tier(float importance, long ttl)`
- `wubu_ctx_evict_fifo(wubu_ctxring_t *r, long tok)`
- `wubu_ctx_resident(const wubu_ctxring_t *r, long tok, int ws)`
- `wubu_cosine(const float *a, const float *b, int n)`
- `wubu_sem_cache_hit(const float *q, const float *c, int n, float thr)`

## `src/brain/include/wubu_cuda_graph.h`

- `wubu_cuda_graph_free(wubu_cuda_graph_t *g)`
- `wubu_cuda_graph_plan(wubu_cuda_graph_t *g, int seq)`
- `wubu_cuda_graph_update_kv(wubu_cuda_graph_t *g, int step, void *kv_dev_ptr)`
- `wubu_cuda_graph_replay(wubu_cuda_graph_t *g)`

## `src/brain/include/wubu_dedup.h`

- `wubu_dedup_scan(const uint16_t *toks, long n, int win, uint8_t *dup)`

## `src/brain/include/wubu_density_planner.h`

- `wubu_density_planner_default_cfg(void)`

## `src/brain/include/wubu_dequant_fp4.h`

- `wubu_fp4_raw_size(int ggml_type, long n_elems)`

## `src/brain/include/wubu_dgm.h`

- `wubu_dgm_init(wubu_dgm_t *dgm)`
- `wubu_dgm_count_verified(const wubu_dgm_t *dgm)`

## `src/brain/include/wubu_diag.h`

- `wubu_diag_set_capacity(wubu_diag_t *d, size_t capacity)`
- `wubu_diag_classify(wubu_diag_t *d, float *grow, float *shrink)`
- `wubu_diag_free(wubu_diag_t *d)`

## `src/brain/include/wubu_diagnosis.h`

- `wubu_diag_record(wubu_diag_loop_t *loop, const wubu_diag_record_t *rec)`
- `wubu_diag_save(const wubu_diag_loop_t *loop, const char *path)`
- `wubu_diag_load(wubu_diag_loop_t *loop, const char *path)`

## `src/brain/include/wubu_dims.h`

- `wubu_dims_sync_gpu(void)`
- `wubu_dims_finalize(wubu_dims_t *d)`
- `wubu_dims_default(void)`

## `src/brain/include/wubu_distill.h`

- `wubu_distill_set_teacher(wubu_distill_t *d, const double *teacher_params, int ndim, double temperature)`

## `src/brain/include/wubu_dsv4.h`

- `wubu_dsv4_sinkhorn_norm(float *w, int n_tokens, int n_experts, int iters)`

## `src/brain/include/wubu_early_exit.h`

- `wubu_early_exit_free(wubu_early_exit_t *e)`

## `src/brain/include/wubu_ecosystem.h`

- `wubu_ecosystem_forward(wubu_ecosystem_t *eco, const float *x, float *out)`
- `wubu_ecosystem_grow(wubu_ecosystem_t *eco, int parent)`
- `wubu_ecosystem_shrink(wubu_ecosystem_t *eco, int idx)`
- `wubu_ecosystem_prefetch(wubu_ecosystem_t *eco, int *top_k, int k)`

## `src/brain/include/wubu_ecs.h`

- `wubu_ecs_add(wubu_ecs_t *e, const char *name, size_t size, void (*dtor)(void *))`
- `wubu_ecs_restore(wubu_ecs_t *e, const uint8_t *buf, size_t bytes)`

## `src/brain/include/wubu_enc_h3.h`

- `wubu_enc_h3_unrotate(float *W, int rows, int cols)`

## `src/brain/include/wubu_encoder.h`

- `wubu_encoder_register(wubu_encoder_t *enc)`

## `src/brain/include/wubu_encoder_impl.h`

- `wubu_encoder_register_ours(void)`

## `src/brain/include/wubu_energy.h`

- `wubu_energy_ledger_init(wubu_energy_ledger_t *e, float budget_j)`
- `wubu_energy_freq_for_cap(float cap_w, float p_base, float f_base)`

## `src/brain/include/wubu_epcap.h`

- `wubu_epcap(const int *cost, int n, int budget, int *out)`

## `src/brain/include/wubu_equiv_check.h`

- `wubu_equiv_gemv(wubu_gemv_fn fn, int n, int trials, float tol_rel)`

## `src/brain/include/wubu_events.h`

- `wubu_events_open_batch(wubu_events_t *ev, const char *path, int batch_size)`
- `wubu_events_append(wubu_events_t *ev, const wubu_event_t *e)`
- `wubu_events_read(const char *path, wubu_event_t *out, int cap)`

## `src/brain/include/wubu_evict2026.h`

- `wubu_ev_pool_obs(const float *attn, int n, int w, float *out)`
- `wubu_ev_proxy_evict(const float *scores, int n, int keep, int *out_keep)`
- `wubu_ev_sink_pos(const float *attn, int n)`
- `wubu_ev_semantic_sponsor(float semantic, float thresh)`
- `wubu_ev_loss_bound(float dropped_mass, float total_mass)`
- `wubu_ev_block_drift(float drift_so_far, float step_drift, float cap)`
- `wubu_ev_reserve_sink(int budget, int sink_count, int outlier_count)`
- `wubu_ev_hybrid_choose(float value, float evict_cost, float compress_cost)`
- `wubu_ev_hysteresis(float score, float thresh, float band, int state)`
- `wubu_ev_head_disparity(const float *dispersion, int n_heads)`

## `src/brain/include/wubu_evict2026b.h`

- `wubu_ev_pq_free(wubu_ev_pq_t *q)`

## `src/brain/include/wubu_evolve.h`

- `wubu_evolve_init(wubu_evolve_t *e)`
- `wubu_evolve_rollback(wubu_evolve_t *e, const char *proposal_id)`

## `src/brain/include/wubu_experibuf.h`

- `wubu_experibuf_init(wubu_experibuf_t *r)`
- `wubu_experibuf_size(const wubu_experibuf_t *r)`

## `src/brain/include/wubu_expert_allreduce.h`

- `wubu_allreduce_sum(const float *const *partials, int nranks, int len, float *out)`

## `src/brain/include/wubu_fmt.h`

- `wubu_fmt_check(int type, const char *out, int limit, const char *extra)`

## `src/brain/include/wubu_fp8.h`

- `wubu_fp8_e4m3_from_f32(float x)`
- `wubu_fp8_e5m2_from_f32(float x)`
- `wubu_fp8_e4m3_to_f32(uint8_t b)`
- `wubu_fp8_e5m2_to_f32(uint8_t b)`
- `wubu_fp8_quantize(const float *x, uint8_t *out, int n, int e5m2)`
- `wubu_fp8_dot(const uint8_t *w_fp8, const float *act, int n, int e5m2)`

## `src/brain/include/wubu_fraud.h`

- `wubu_fraud_init(wubu_fraud_t *fr, int n_agents)`

## `src/brain/include/wubu_freeenergy.h`

- `wubu_fe_free_energy(float log_likelihood, float complexity)`
- `wubu_fe_expected_free_energy(float pragmatic, float epistemic)`
- `wubu_fe_policy_prior(const float *G, int n, float gamma, float *out)`
- `wubu_fe_percept_step(float mu_hat, float error, float precision, float lr)`
- `wubu_fe_epistemic_value(float uncertainty_before, float uncertainty_after)`

## `src/brain/include/wubu_fs_dataset.h`

- `wubu_fs_dataset_rescan(wubu_fs_dataset_t *ds)`

## `src/brain/include/wubu_fs_trainer.h`

- `wubu_fs_trainer_should_grow(const wubu_fs_trainer_t *ft)`
- `wubu_fs_trainer_free(wubu_fs_trainer_t *ft)`

## `src/brain/include/wubu_gaad.h`

- `wubu_isqrt(int n)`
- `wubu_dist(int x1, int y1, int x2, int y2)`
- `wubu_phi_pow(int n)`
- `wubu_clamp(int val, int lo, int hi)`

## `src/brain/include/wubu_gamebud.h`

- `wubu_gamebud_free(wubu_gamebud_t *g)`
- `wubu_gamebud_can_spend(wubu_gamebud_t *g, uint64_t us_optional)`
- `wubu_gamebud_effective_budget(const wubu_gamebud_t *g)`
- `wubu_gamebud_elapsed_us(const wubu_gamebud_t *g)`

## `src/brain/include/wubu_gemm.h`

- `wubu_gemv_f32(const float *A, const float *x, float *y, int M, int K)`
- `wubu_gemm_register_device(wubu_gemm_fn fn, const char *name)`

## `src/brain/include/wubu_gemv_tune.h`

- `wubu_gemv_autotune(int M, int K, double beta_eff_tb_s)`

## `src/brain/include/wubu_graph.h`

- `wubu_graph_validate(const wubu_graph_t *g, int *bad_op)`
- `wubu_graph_register_op(int kind, wubu_graph_op_fn fn)`

## `src/brain/include/wubu_gravity.h`

- `wubu_gravity_step(wubu_gravity_t *g)`
- `wubu_gravity_route(const wubu_gravity_t *g, double r_in, double theta_in)`
- `wubu_gravity_grow(wubu_gravity_t *g, int cell_id, double dr, double dtheta)`
- `wubu_gravity_shrink(wubu_gravity_t *g, int cell_id)`
- `wubu_gravity_core_count(const wubu_gravity_t *g)`

## `src/brain/include/wubu_grow.h`

- `wubu_grow_insert_block(wubu_model_t *m, int pos)`
- `wubu_grow_stack_block(wubu_model_t *m, int src)`
- `wubu_train_grow(wubu_train_t *tr, int pos, int n_layers)`

## `src/brain/include/wubu_grow_kv.h`

- `wubu_grow_kv_grow(wubu_grow_kv_t *g, int max_grow)`
- `wubu_grow_kv_shrink(wubu_grow_kv_t *g)`

## `src/brain/include/wubu_hadamard.h`

- `wubu_hadamard_fwd(float *x, int n)`
- `wubu_hadamard_inv(float *x, int n)`
- `wubu_hadamard_rotate_rows(float *M, int rows, int d)`

## `src/brain/include/wubu_harness.h`

- `wubu_harness_may_continue(wubu_harness_t *h)`
- `wubu_harness_suite_score(wubu_harness_t *h)`

## `src/brain/include/wubu_harness_file.h`

- `wubu_file_harness_score(const wubu_file_harness_t *fh)`

## `src/brain/include/wubu_hashrouter.h`

- `wubu_hashrouter_free(wubu_hashrouter_t *hr)`

## `src/brain/include/wubu_hive.h`

- `wubu_hive_insert(wubu_hive_t *h, void *ptr)`
- `wubu_hive_clear_with(wubu_hive_t *h, void (*free_fn)(void *ptr))`

## `src/brain/include/wubu_hopfield2.h`

- `wubu_hf_spectral_capacity(int dim, float alpha, float spectral_sat)`
- `wubu_hf_separation(const float *X, int n_pat, int dim)`
- `wubu_hf_should_store(float novelty, float novelty_thresh, int capacity_left)`
- `wubu_hf_rehearse(float weight, float reward, float alpha)`
- `wubu_hf_beta_anneal(float beta_max, float beta_min, int t, int t_max)`
- `wubu_hf_denoise_quality(float snr, float beta)`
- `wubu_hf_decay_schedule(float base_halflife, float utility)`
- `wubu_hf_context_gate(const float *ctx, const float *pat_ctx, int dim)`
- `wubu_hf_orthogonalize(const float *a, int dim, float *b)`
- `wubu_hf_episodic_weight(float base, int age, float halflife)`

## `src/brain/include/wubu_hopfield3.h`

- `wubu_mem_compress_init(wubu_mem_compress_t *m, int k, int d)`
- `wubu_mem_compress_add(wubu_mem_compress_t *m, const float *pat)`
- `wubu_mem_compress_free(wubu_mem_compress_t *m)`

## `src/brain/include/wubu_hyper.h`

- `wubu_hyper_mobius_add(double c, double x, double y)`
- `wubu_hyper_log(double c, double y)`
- `wubu_hyper_dist(double c, const double *x, const double *y, int n)`

## `src/brain/include/wubu_imgenc.h`

- `wubu_imgenc_init(wubu_imgenc_t *v, unsigned seed)`
- `wubu_imgenc_encode(const wubu_imgenc_t *v, const float *img_64x64x3, float *out_tokens)`

## `src/brain/include/wubu_integrate.h`

- `wubu_decode_policy_destroy(wubu_decode_policy_t *p)`
- `wubu_decode_policy_set_hybrid(wubu_decode_policy_t *p, int period)`
- `wubu_decode_policy_set_pd(wubu_decode_policy_t *p, int on)`

## `src/brain/include/wubu_invariant.h`

- `wubu_invariant_discover(const wubu_inv_trace_t *trace, wubu_inv_set_t *out)`

## `src/brain/include/wubu_kernel.h`

- `wubu_kernel_softmax_scalar(float *logits, int M, int N)`

## `src/brain/include/wubu_kernel_budget.h`

- `wubu_kernel_total_bytes(const wubu_budget_t *budget, int n)`

## `src/brain/include/wubu_kv_adaptive.h`

- `wubu_kvq_adaptive_roundtrip(const float *z, int n)`

## `src/brain/include/wubu_kv_evict.h`

- `wubu_kv_evict_track_attn(wubu_kv_evict_t *e, int block_id, float attn)`
- `wubu_kv_evict_score(const wubu_kv_evict_entry_t *ent)`
- `wubu_kv_evict_select(wubu_kv_evict_t *e, int *out_ids, int out_n)`

## `src/brain/include/wubu_kv_runtime.h`

- `wubu_kv_set_scheme(int scheme)`
- `wubu_kv_get_scheme(void)`

## `src/brain/include/wubu_kv_shrink.h`

- `wubu_kv_shrink_default_cfg(void)`
- `wubu_kv_shrink_sweep(wubu_kv_shrink_t *s, char ***out_paths)`

## `src/brain/include/wubu_kv_styx.h`

- `wubu_kv_styx_init(void)`
- `wubu_kv_styx_shutdown(void)`
- `wubu_kv_styx_unregister(const char *layer_path)`
- `wubu_kv_styx_registered_count(void)`

## `src/brain/include/wubu_kv_tiering.h`

- `wubu_kv_tiering_get(const wubu_kv_tiering_t *t, const char *path)`

## `src/brain/include/wubu_kv_transfer.h`

- `wubu_kv_transfer_free(wubu_kv_transfer_t *t)`

## `src/brain/include/wubu_kvcache_quant.h`

- `wubu_kvq_q8_quant(const float *x, int8_t *q, float *scale, int n)`

## `src/brain/include/wubu_kvfs.h`

- `wubu_kvfs_unmount(wubu_kvfs_t *fs, const char *path)`
- `wubu_kvfs_free(wubu_kvfs_t *fs)`

## `src/brain/include/wubu_latency.h`

- `wubu_edf_order(wubu_task_t *t, int n)`
- `wubu_wcet_account(const long *samples, int n, wubu_wcet_t *out)`
- `wubu_deadline_miss(const wubu_wcet_t *w, long budget_ms)`
- `wubu_slo_check(wubu_latclass_t cls, const wubu_slo_meas_t *m)`

## `src/brain/include/wubu_lfm2.h`

- `lfm2_load(const char *model_dir, lfm2_model_t *m)`

## `src/brain/include/wubu_lineage.h`

- `wubu_lineage_extinction_pass(wubu_lineage_tracker_t *lt)`

## `src/brain/include/wubu_lookahead.h`

- `wubu_lookahead_probe(const int *history, int hist_len, int pos, int n)`

## `src/brain/include/wubu_loopguard.h`

- `wubu_loop_may_continue(const wubu_loopguard_t *g, long step, long now_ns)`
- `wubu_traj_append(wubu_traj_t *t, const char *agent, const char *action)`
- `wubu_tool_allowed(wubu_toolcap_t *c, const char *agent, long window_now)`
- `wubu_hitl_approve(const wubu_hitl_t *h, float severity, int approval_token)`

## `src/brain/include/wubu_lora.h`

- `wubu_lora_free(wubu_lora_t *l)`
- `wubu_lora_apply(const wubu_lora_t *l, float *W)`
- `wubu_lora_scale(const wubu_lora_t *l)`

## `src/brain/include/wubu_lruk.h`

- `wubu_lruk_touch(wubu_lruk_t *e, int block_id)`

## `src/brain/include/wubu_mem_budget.h`

- `wubu_mem_detect_available_ram(void)`

## `src/brain/include/wubu_metacog.h`

- `wubu_mc_init(wubu_metacog_t *m, int n_agents)`
- `wubu_mc_assess(const wubu_metacog_t *m, int agent, float claimed)`
- `wubu_mc_jol(float logprob_top1, float logprob_top2)`
- `wubu_mc_harness(float jol, float low, float high, int retries_left)`
- `wubu_mc_separated(float monitor_pred, float actor_pred, float drift)`
- `wubu_mc_progress(float competence, float per_trial_gain, int k)`
- `wubu_mc_recalibrate(float confidence, float entropy, float entropy_base)`
- `wubu_mc_faithfulness(float verbalized, float actual)`
- `wubu_mc_delegate(const wubu_metacog_t *m, float difficulty)`
- `wubu_mc_gap(float competence, float difficulty)`
- `wubu_mc_sample_temp(float calibration, float base_temp)`

## `src/brain/include/wubu_metadiag.h`

- `wubu_metadiag_slow(wubu_metadiag_t *md)`
- `wubu_metadiag_state(const wubu_metadiag_t *md, float *rate, float *floor)`

## `src/brain/include/wubu_metagame.h`

- `wubu_improvement_delta(double child, double parent, double min_gain)`
- `wubu_archive_best(const wubu_archive_t *a)`

## `src/brain/include/wubu_mhc.h`

- `wubu_mhc_free(wubu_mhc_t *m)`
- `wubu_mhc_apply_nonneg(float *w, int n)`
- `wubu_mhc_identity_ok(const wubu_mhc_t *m)`
- `wubu_mhc_set_identity(wubu_mhc_t *m)`
- `wubu_mhc_forward(const wubu_mhc_t *m, const float *x, float *r_out, float *y_out)`

## `src/brain/include/wubu_mhc_mh.h`

- `wubu_mhc_mh_free(wubu_mhc_mh_t *m)`
- `wubu_mhc_mh_set_identity(wubu_mhc_mh_t *m)`
- `wubu_mhc_mh_constrain(wubu_mhc_mh_t *m)`

## `src/brain/include/wubu_mm_align.h`

- `wubu_mm_align_init(wubu_mm_align_t *m, unsigned seed)`

## `src/brain/include/wubu_mmrope.h`

- `wubu_mmrope_close(wubu_mmrope_t *ctx)`

## `src/brain/include/wubu_mobius.h`

- `wubu_mobius_add(const float *x, const float *y, int d, float R, float *z)`
- `wubu_exp_map(const float *v, int d, float R, float *z)`
- `wubu_log_map(const float *x, int d, float R, float *v)`
- `wubu_mobius_scalar_mul(float r, const float *x, int d, float R, float *z)`
- `wubu_poincare_dist(const float *x, const float *y, int d, float R)`
- `wubu_poincare_linear_comb(const float **xi, const float *wi, int n, int d, float R, float *z)`
- `wubu_mobius_gyrate(const float *x, const float *y, const float *z, int d, float R, float *out)`
- `sqrtf(sum)`

## `src/brain/include/wubu_model.h`

- `ldexpf(1.0f + (float)mant / 1024.0f, exp - 15) * (sign ? -1.0f : 1.0f)`
- `if(exp == 0xFF) { fp16 = (sign << 15) | (31 << 10) | (mant >> 13)`
- `if(newexp <= 0) fp16 = (sign << 15)`
- `fp16_to_fp32(((const uint16_t *)cache)[idx])`
- `wubu_model_init(wubu_model_t *model, const char *gguf_path)`
- `wubu_model_free(wubu_model_t *model)`
- `wubu_model_gpu_init(wubu_model_t *model, int max_ctx, int chunk_sz)`
- `wubu_model_gpu_chunk_sz(wubu_model_t *model)`
- `wubu_gpu_set_ssm_hybrid(void *gpu_ctx, int layer_idx, ssm_layer_weights *ssm)`
- `wubu_model_gpu_free(wubu_model_t *model)`
- `wubu_mtp_free(mtp_head_t *mtp)`
- `wubu_model_checkpoint(wubu_model_t *model)`
- `wubu_model_rollback(wubu_model_t *model)`

## `src/brain/include/wubu_model_format.h`

- `wubu_model_format_register(const wubu_model_format_t *fmt)`
- `wubu_model_format_register_safetensors(void)`
- `wubu_model_format_register_onnx(void)`

## `src/brain/include/wubu_model_gpu.h`

- `wubu_model_gpu_init(wubu_model_t *model, int max_ctx, int chunk_sz)`
- `wubu_gpu_set_ssm_hybrid(void *gpu_ctx, int layer_idx, ssm_layer_weights *ssm)`

## `src/brain/include/wubu_model_safetensors_bridge.h`

- `wubu_model_safetensors_free(wubu_model_t *m)`

## `src/brain/include/wubu_model_scalable.h`

- `wubu_scalable_budget_depth(wubu_scalable_model_t *m)`
- `wubu_scalable_mark_hot(wubu_scalable_model_t *m, const char *name)`
- `wubu_scalable_prune_cold(wubu_scalable_model_t *m)`

## `src/brain/include/wubu_moe.h`

- `wubu_moe_load_layer(gguf_ctx *ctx, int layer, moe_weights_t *moe)`
- `wubu_moe_free_layer(moe_weights_t *moe)`

## `src/brain/include/wubu_moe2.h`

- `wubu_moe2_forward(const wubu_moe2_t *moe, const float *x, float *out)`

## `src/brain/include/wubu_moe_grouped.h`

- `wubu_moe_router_free(wubu_moe_router_t *r)`
- `wubu_moe_router_assign(wubu_moe_router_t *r, const int *routes, int n_tokens)`
- `wubu_moe_router_top_hot(wubu_moe_router_t *r, int k, int *out_experts)`

## `src/brain/include/wubu_moe_hyperbolic.h`

- `wubu_poincare_router_init_random(float *centroids_out, unsigned int seed)`
- `wubu_poincare_router_free(poincare_router_t *router)`
- `wubu_nested_moe_router_free(nested_moe_router_t *router)`
- `euclidean_to_poincare_ball(const float *v, int d, float R, float *out)`

## `src/brain/include/wubu_moeroute.h`

- `wubu_moeroute_step(wubu_moeroute_t *mr, int n_tokens)`

## `src/brain/include/wubu_more_spec.h`

- `wubu_rest_accept(float residual, float thr)`
- `wubu_tree_restructure(int accepted_depth, int budget)`
- `wubu_contrastive_accept(float draft_p, float ref_p)`
- `wubu_distil_gate(float quality_est, float qmin)`
- `wubu_spec_moe_skip(float route_score, float floor)`
- `wubu_cascade_accept(int draft_match)`
- `wubu_swap_check(int *streak, float acceptance, float low, int patience)`
- `wubu_layer_resume(int streamed, int total)`
- `wubu_cascade_earlyexit(int draft_match, int layer, int floor_layer)`

## `src/brain/include/wubu_mxfp4.h`

- `wubu_mxfp4_pack(const float *x, int n, uint8_t *out)`
- `wubu_mxfp4_unpack(const uint8_t *in, int n, float *out)`
- `wubu_mxfp8_pack(const float *x, int n, uint8_t *out)`
- `wubu_mxfp8_unpack(const uint8_t *in, int n, float *out)`

## `src/brain/include/wubu_nest.h`

- `wubu_nest_learned_rotation(const float ld[4], float angle)`

## `src/brain/include/wubu_nested_ssm.h`

- `wubu_nested_ssm_fwd_save_free(nested_ssm_fwd_save_t *save)`
- `wubu_nested_ssm_init(wubu_nested_ssm_state_t *state, int K, const float *R)`
- `wubu_nested_ssm_free(wubu_nested_ssm_state_t *state)`
- `wubu_nested_ssm_validate(const wubu_nested_ssm_state_t *state)`

## `src/brain/include/wubu_nf4.h`

- `wubu_nf4_quantize_block(const float *input, wubu_nf4_block *out)`
- `wubu_nf4_dequantize_block(const wubu_nf4_block *block, float *output)`
- `wubu_nf4_quantize(const float *input, wubu_nf4_block *output, int n)`
- `wubu_nf4_dequantize(const wubu_nf4_block *blocks, float *output, int n)`
- `wubu_nf4_dequant_dot(const float *q, const wubu_nf4_block *blocks, int n)`

## `src/brain/include/wubu_ngram.h`

- `wubu_ngram_propose(wubu_ngram_draft_t *d, int k, int *out)`

## `src/brain/include/wubu_nvfp4.h`

- `wubu_nvfp4_from_f32(float x)`
- `wubu_nvfp4_to_f32(uint8_t b)`

## `src/brain/include/wubu_optim.h`

- `wubu_tgt_update(wubu_tgt_t *t, float loss, float theta)`

## `src/brain/include/wubu_orbits.h`

- `wubu_orbits_write(wubu_orbits_t *o, const float *x, int d, void *item)`
- `wubu_orbits_nest(wubu_orbits_t *o)`

## `src/brain/include/wubu_paged_kv.h`

- `wubu_paged_kv_free(wubu_paged_kv_t *m)`
- `wubu_paged_kv_new_seq(wubu_paged_kv_t *m)`

## `src/brain/include/wubu_passk.h`

- `wubu_passk(const int *succ, int n, int k)`

## `src/brain/include/wubu_patch_embed.h`

- `wubu_patch_embed_close(wubu_patch_embed_t *ctx)`

## `src/brain/include/wubu_pd_split.h`

- `wubu_pd_split_free(wubu_pd_split_t *s)`
- `wubu_pd_route_decode(const wubu_pd_split_t *s, const int *queues)`

## `src/brain/include/wubu_planediv.h`

- `wubu_plane_enforce(const wubu_plane_t *p, int item_plane, const char *content)`
- `wubu_mem_fingerprint(const char *blob, int n)`
- `wubu_mem_diverged(unsigned long long trusted_fp, unsigned long long cur_fp)`
- `wubu_replay_flagged(unsigned long long fp, const unsigned long long *seen, int n_seen)`

## `src/brain/include/wubu_plateau.h`

- `wubu_plateau_slope(const float *losses, int n, int window)`

## `src/brain/include/wubu_poincare_gqa.h`

- `poincare_kv_cache_init(poincare_kv_cache_t *cache, int init_capacity)`
- `poincare_kv_cache_resize(poincare_kv_cache_t *cache, int new_capacity)`
- `poincare_kv_cache_free(poincare_kv_cache_t *cache)`

## `src/brain/include/wubu_precision_plan.h`

- `wubu_hw_profile_probe(wubu_hw_profile_t *prof)`

## `src/brain/include/wubu_pref.h`

- `wubu_pref_update(wubu_pref_t *pf)`
- `wubu_pref_survival(const wubu_pref_t *pf, int cell_idx)`

## `src/brain/include/wubu_priority_store.h`

- `wubu_prio_gate(wubu_priority_store_t *ps, uint8_t cell_idx)`
- `wubu_prio_save(const wubu_priority_store_t *ps, void *buf, long cap)`

## `src/brain/include/wubu_prover.h`

- `wubu_prover_prove(const wubu_proof_t *proof)`
- `wubu_prover_check_chain(const wubu_pf_step_t *steps, int n)`
- `wubu_prover_mobius_closure(double c, double x, double y)`

## `src/brain/include/wubu_q4k_m.h`

- `wubu_q4k_quant(const float *x, wubu_q4k_block_t *blocks, int n, int is_sensitive)`

## `src/brain/include/wubu_qwen35.h`

- `wubu_q35_compute_split(const wubu_q35_cfg_t *cfg, wubu_q35_split_t *out)`

## `src/brain/include/wubu_rambus.h`

- `wubu_rambus_free(wubu_rambus_t *r)`

## `src/brain/include/wubu_recency.h`

- `wubu_recency_weight(long i, long n, float base, float power)`

## `src/brain/include/wubu_repetition.h`

- `wubu_rep_free(wubu_rep_state_t *s)`
- `wubu_rep_observe(wubu_rep_state_t *s, int token_id)`
- `wubu_rep_apply(wubu_rep_state_t *s, float *logits)`
- `wubu_rep_reset(wubu_rep_state_t *s)`

## `src/brain/include/wubu_resource.h`

- `wubu_pick_tier(double vram_gb, int billions)`
- `wubu_est_toks(double bandwidth_gbs, int billions, int quant_bits)`
- `wubu_degrade_tier(double vram_gb, int desired_b)`

## `src/brain/include/wubu_resources.h`

- `wubu_res_update(wubu_res_t *r, uint64_t events_since_last)`

## `src/brain/include/wubu_ring_attn.h`

- `wubu_ring_attn_free(wubu_ring_attn_ctx_t *ctx)`

## `src/brain/include/wubu_roofline.h`

- `wubu_roofline_default(void)`
- `wubu_roofline_bstar(const wubu_roofline_cfg_t *c, double P_params, int s)`

## `src/brain/include/wubu_rotate.h`

- `wubu_hadamard(float *v, int n)`
- `wubu_rotate_fuse_right(float *W, int rows, int cols)`
- `wubu_rotate_input(float *x, int n)`

## `src/brain/include/wubu_router.h`

- `wubu_router_register(wubu_router_t *router)`

## `src/brain/include/wubu_runtime_dims.h`

- `wubu_runtime_dims_probe(const char *safetensors_path, wubu_runtime_dims_t *d)`

## `src/brain/include/wubu_safekern.h`

- `wubu_stop_honored(const wubu_safekern_t *k)`
- `wubu_containment_level(float severity)`
- `wubu_containment_reversible(int level)`

## `src/brain/include/wubu_safetensors_model.h`

- `wubu_st_n_layers(const wubu_st_model_t *m)`
- `wubu_st_embed(const wubu_st_model_t *m, float *embd)`
- `wubu_st_lm_head(const wubu_st_model_t *m, float *lm_head)`
- `wubu_st_close(wubu_st_model_t *m)`

## `src/brain/include/wubu_sandbox_safekern.h`

- `wubu_sbox_init(wubu_sandbox_t *s)`
- `wubu_sbox_add_cap(wubu_sandbox_t *s, const char *cap)`
- `wubu_sbox_set_seccomp(wubu_sandbox_t *s, int enabled)`
- `wubu_sbox_set_network(wubu_sandbox_t *s, int allowed)`
- `wubu_sbox_set_fs_ro(wubu_sandbox_t *s, int ro)`

## `src/brain/include/wubu_scale.h`

- `wubu_scale_probe_hw(wubu_scale_hw_t *hw)`
- `wubu_scale_measure(const wubu_scale_hw_t *hw, wubu_scale_prec_t *best)`

## `src/brain/include/wubu_scheduler.h`

- `wubu_sched_submit(wubu_sched_t *s, wubu_req_t *req)`
- `wubu_sched_step(wubu_sched_t *s)`

## `src/brain/include/wubu_sd_taesd.h`

- `wubu_sd_taesd_free(wubu_sd_taesd_t *t)`

## `src/brain/include/wubu_sd_unet.h`

- `wubu_sd_unet_free(wubu_sd_unet_t *u)`
- `wubu_sd_unet_set_step(wubu_sd_unet_t *u, int step)`
- `wubu_sd_unet_set_dc_schedule(wubu_sd_unet_t *u, const int *full_map, int n)`
- `wubu_sd_unet_set_resolution(wubu_sd_unet_t *u, int h, int w)`

## `src/brain/include/wubu_selfimprove.h`

- `wubu_selfimprove_experience(wubu_selfimprove_t *si, int win, float value)`
- `wubu_selfimprove_report(wubu_selfimprove_t *si, int accepted)`

## `src/brain/include/wubu_semcons.h`

- `wubu_semcons_init(wubu_semcons_t *sc)`

## `src/brain/include/wubu_shrink.h`

- `wubu_shrink_block(wubu_model_t *m, int pos)`
- `wubu_shrink_ffn(wubu_model_t *m, int pos, float prune_frac)`
- `wubu_train_shrink(wubu_train_t *tr, int pos, int n_layers)`

## `src/brain/include/wubu_siglip.h`

- `wubu_siglip_init(wubu_siglip_t *enc, const char *path)`
- `wubu_siglip_free(wubu_siglip_t *enc)`

## `src/brain/include/wubu_skillcell.h`

- `wubu_skill_accept(wubu_skill_tracker_t *sk, int64_t cell_idx)`
- `wubu_skill_use(wubu_skill_tracker_t *sk, int64_t cell_idx)`
- `wubu_skill_save(const wubu_skill_tracker_t *sk, const char *path)`
- `wubu_skill_load(wubu_skill_tracker_t *sk, const char *path)`

## `src/brain/include/wubu_smt_check.h`

- `wubu_smt_check_gemv(int K, float tolerance)`

## `src/brain/include/wubu_spawn.h`

- `wubu_spawn_wait(const char *file, char *const argv[], bool silent)`
- `wubu_spawn_win_wait(const char *file, char *const argv[], int silent)`

## `src/brain/include/wubu_spec_cascade.h`

- `wubu_ngram_cascade_free(wubu_ngram_cascade_t *c)`

## `src/brain/include/wubu_spec_decode.h`

- `wubu_spec_decode_free(wubu_spec_decode_ctx_t *ctx)`
- `wubu_spec_decode_throughput(int n_draft_tokens, float accept_rate)`

## `src/brain/include/wubu_specdec.h`

- `wubu_specdec_verify(wubu_specdec_t *sd, unsigned *seed)`

## `src/brain/include/wubu_ssd_moe.h`

- `wubu_ssd_moe_get(wubu_ssd_moe_t *m, int layer, int expert, float *out[3])`

## `src/brain/include/wubu_ssm.h`

- `wubu_ssm_workspace_free(ssm_workspace_t *ws)`
- `wubu_is_ssm_layer(int layer_idx)`
- `wubu_softplus(int n, const float *x, float *out)`
- `wubu_silu(int n, const float *x, float *out)`
- `wubu_sigmoid(int n, const float *x, float *out)`

## `src/brain/include/wubu_ssm_workspace.h`

- `wubu_ssm_workspace_init(int max_layers, int B, int T)`
- `wubu_ssm_workspace_shutdown(void)`
- `wubu_ssm_workspace_set_t(int T)`

## `src/brain/include/wubu_stream_kv.h`

- `wubu_stream_kv_destroy(wubu_stream_kv_t *s)`
- `wubu_stream_kv_set(wubu_stream_kv_t *s, int sink, int window)`
- `wubu_stream_kv_enabled(const wubu_stream_kv_t *s)`
- `wubu_stream_kv_sink(const wubu_stream_kv_t *s)`
- `wubu_stream_kv_window(const wubu_stream_kv_t *s)`
- `wubu_stream_kv_remap(const wubu_stream_kv_t *s, int len, int pos)`

## `src/brain/include/wubu_symbolic.h`

- `wubu_fact_add(wubu_kb_t *kb, int pred, int a0, int a1, int a2)`
- `wubu_fact_query(const wubu_kb_t *kb, int pred, int a0, int a1, int a2)`
- `wubu_rule_apply(wubu_kb_t *kb, const wubu_rule_t *r)`
- `wubu_rules_run(wubu_kb_t *kb, const wubu_rule_t *rules, int n_rules)`

## `src/brain/include/wubu_symreg.h`

- `wubu_symreg_eval(const char *expr, const double *x, int n_vars)`

## `src/brain/include/wubu_synth.h`

- `wubu_synth_init(wubu_synth_t *s)`

## `src/brain/include/wubu_tandem.h`

- `wubu_tandem_free(wubu_tandem_t *t)`
- `wubu_tandem_submit(wubu_tandem_t *t, void *arg)`
- `wubu_tandem_drain(wubu_tandem_t *t)`

## `src/brain/include/wubu_taskbd.h`

- `wubu_taskbd_init(wubu_taskbd_t *tb, double threshold)`

## `src/brain/include/wubu_tensor_store.h`

- `wubu_ts_export_q8(const wubu_tensor_store_t *ts, const char *out_path)`
- `wubu_ts_export_mixed(const wubu_tensor_store_t *ts, const char *out_path)`
- `wubu_ts_close(wubu_tensor_store_t *ts)`

## `src/brain/include/wubu_thread_spec.h`

- `wubu_thread_spec_free(wubu_thread_spec_t *ts)`

## `src/brain/include/wubu_tokenizer.h`

- `wubu_tokenizer_init(wubu_tokenizer_t *tok, const char *gguf_path)`
- `wubu_tokenizer_init_from_gguf(wubu_tokenizer_t *tok, const char *gguf_path)`
- `wubu_tokenizer_free(wubu_tokenizer_t *tok)`

## `src/brain/include/wubu_toolreg.h`

- `wubu_toolreg_report(wubu_toolreg_t *tr, const char *name, int ok)`

## `src/brain/include/wubu_tooluse.h`

- `wubu_tool_registry_init(wubu_tool_registry_t *reg)`

## `src/brain/include/wubu_traj_sft.h`

- `wubu_traj_sft_train_frac(const wubu_sft_seg_t *segs, int n)`

## `src/brain/include/wubu_tst.h`

- `tst_softmax_inplace(float *logits, int N, int V)`
- `tst_cross_entropy(const float *logits, int V, int label)`

## `src/brain/include/wubu_turboquant.h`

- `make_float2((q0 - 2.0f) * scale, (q1 - 2.0f) * scale)`
- `tile_manager_init(int max_ctx, int block_size, int n_layers, int n_kv_heads)`
- `tile_manager_free(tile_manager_t* mgr)`
- `tile_manager_alloc_tile(tile_manager_t* mgr, int layer, int kv_head)`
- `tile_manager_free_tile(tile_manager_t* mgr, int tile_id)`
- `tile_manager_plan_frame(tile_manager_t* mgr, int req_id, int current_pos, int window_size)`

## `src/brain/include/wubu_ubus.h`

- `ubus_free(ubus_t *u)`
- `ubus_report(const ubus_t *u)`
- `ubus_backend_count(const ubus_t *u)`

## `src/brain/include/wubu_uq.h`

- `wubu_uq_calibrate(wubu_uq_t *uq, const double *residuals, int n_res, double alpha)`

## `src/brain/include/wubu_userfs.h`

- `wubu_userfs_ingest(wubu_userfs_t *m, const char *real_path)`
- `wubu_userfs_ingest_dir(wubu_userfs_t *m, const char *dir_path)`
- `wubu_userfs_read_index(wubu_userfs_t *m, char *buf, size_t buflen)`

## `src/brain/include/wubu_vecsearch.h`

- `wubu_hnsw_insert(wubu_hnsw_t *h, const float *vec, uint32_t id)`

## `src/brain/include/wubu_verify.h`

- `wubu_verify_init(wubu_verify_t *v)`
- `wubu_verify_assert_int(wubu_verify_t *v, int cond, const char *expr_str)`
- `wubu_verify_assert_ptr(wubu_verify_t *v, const void *ptr, const char *expr_str)`
- `wubu_verify_assert_range(wubu_verify_t *v, long val, long lo, long hi, const char *expr_str)`
- `wubu_verify_all_passed(const wubu_verify_t *v)`
- `wubu_verify_count(const wubu_verify_t *v, int *out_passed, int *out_failed)`

## `src/brain/include/wubu_vision_encoder.h`

- `vision_encoder_init(vision_encoder_t *enc, const char *path)`
- `vision_encoder_free(vision_encoder_t *enc)`

## `src/brain/include/wubu_vision_moondream.h`

- `vm_init(vm_state_t *state, const char *bin_path, const char *index_path)`
- `vm_free(vm_state_t *state)`
- `vm_forward(vm_state_t *state, const float *pixels, int H, int W, float *output)`
- `vm_create_patches(const float *pixels, int H, int W, float *patches)`
- `vm_layer_norm(float *x, const float *weight, const float *bias, int n, float eps)`
- `vm_exp_map(const float *vec, float *out, int n, float R)`

## `src/brain/include/wubu_weight.h`

- `wubu_weight_nbytes(int type, int64_t n_elems)`
- `wubu_weight_is_float(int type)`
- `wubu_weight_direct_ok(int type, int64_t n_rows)`
- `wubu_weight_to_f32(const wubu_weight_t *w, float *out)`

## `src/brain/include/wubu_width.h`

- `wubu_width_expand(wubu_model_t *m)`

## `src/brain/include/wubu_win.h`

- `wubu_munmap(void *addr, size_t len)`
- `wubu_msync(void *addr, size_t len, int flags)`
- `wubu_mmap(addr, len, prot, flags, fd, off)`
- `wubu_munmap(addr, len)`
- `wubu_msync(addr, len, flags)`

## `src/brain/include/wubu_worldmodel.h`

- `wubu_wm_predict(const wubu_wm_t *m, const double *s, double *sp)`
- `wubu_wm_divergence(const double *pred, const double *obs, int n)`

## `src/brain/tools/agent_gauntlet/agent_gauntlet.h`

- `gauntlet_load_models(void)`
- `gauntlet_run_all(GauntletScore *scores)`
- `gauntlet_edr_init(void)`
- `gauntlet_edr_stop(void)`

## `src/brain/ENCODERS/hash-mind/c/include/tokenizer.h`

- `tokenizer_init(Tokenizer* tok)`
- `tokenizer_encode(const Tokenizer* tok, const char* text, int* output, int max_len)`
- `tokenizer_decode(const Tokenizer* tok, const int* tokens, int n, char* output, int max_len)`

## `src/worldsim/worldsim.h`

- `ws_terrain_generate(ws_terrain_t *t, uint32_t seed)`
- `ws_terrain_erode(ws_terrain_t *t, int iterations)`
- `ws_terrain_height(const ws_terrain_t *t, int x, int y)`
- `ws_terrain_biome(const ws_terrain_t *t, int x, int y)`
- `ws_physics_init(ws_physics_config_t *cfg)`
- `ws_physics_step(ws_world_t *w, const ws_terrain_t *t, const ws_physics_config_t *cfg)`
- `ws_render_terrain(const ws_terrain_t *t, ws_render_ctx_t *ctx)`
- `ws_render_entities(const ws_world_t *w, ws_render_ctx_t *ctx)`
- `ws_render_minimap(const ws_terrain_t *t, ws_render_ctx_t *ctx, int mx, int my, int size)`
- `ws_rng_int(uint64_t *state, int lo, int hi)`

## `src/bear/bear_cuda.h`

- `bear_cuda_profile_enable(BearCudaContext* ctx, int enable)`
- `bear_cuda_profile_get_events(BearCudaContext* ctx, BearCudaProfileEvent* out, int max_events)`
- `bear_cuda_profile_reset(BearCudaContext* ctx)`

## `src/bear/bear_cudnn.h`

- `hc_builtin_cublas_create(void)`
- `hc_builtin_cublas_destroy(BearCublasHandle handle)`
- `hc_builtin_cudnn_create(void)`
- `hc_builtin_cudnn_destroy(BearCudnnHandle handle)`
- `hc_builtin_cudnn_destroy_tensor_desc(BearCudnnTensorDesc desc)`
- `hc_builtin_cuda_malloc(size_t bytes)`
- `hc_builtin_cuda_free(void* ptr)`
- `hc_builtin_cublas_get_error_string(int status)`
- `hc_builtin_cudnn_get_error_string(int status)`

## `src/bear/bear_holo_opt.h`

- `bear_holo_step(BearHoloOptimizer* opt, double* params, const double* grads, int n)`

## `src/bear/bear_mujoco.h`

- `bear_mujoco_init(int num_poles, int num_envs)`

## `src/bear/bear_nn.h`

- `bear_orthogonal_init_params(BearPolicyNet* net, float gain)`
- `bear_value_orthogonal_init(BearValueNet* vnet, float gain)`
- `bear_checkpoint_save(const BearPolicyNet* net, const char* path)`
- `bear_checkpoint_load(BearPolicyNet* net, const char* path)`

## `src/bear/bear_ppo.h`

- `bear_ppo_clip_grad_norm(BearPolicyNet* policy, BearValueNet* critic, float max_norm)`
- `bear_trainer_set_logger(BearTrainer* trainer, bear_log_fn fn, void* user_data)`

## `src/bear/bear_simd.h`

- `tanhf(x)`

## `src/bear/bear_vulkan.h`

- `bear_vulkan_profile_enable(BearVulkanContext* ctx, int enable)`
- `bear_vulkan_profile_get_events(BearVulkanContext* ctx, BearVulkanProfileEvent* out, int max_events)`
- `bear_vulkan_profile_reset(BearVulkanContext* ctx)`

## `src/bear/wubu_warehouse.h`

- `warehouse_build_mjcf(const WarehouseConfig* cfg)`

## `src/firmware/fw.h`

- `fw_media_count(void)`
- `fw_pe_load(const void *file, uint64_t file_size, fw_pe_image *out)`
- `fw_efi_new_handle(void)`
- `fw_efi_install(EFI_HANDLE h, EFI_GUID *guid, void *iface)`
- `fw_efi_boot_services_active(void)`

## `src/firmware/fw_acpi.h`

- `fw_acpi_init(void)`
- `fw_acpi_set_rsdp(void *rsdp)`
- `fw_acpi_tpm2_control_area(void)`
- `fw_acpi_tpm2_start_method(void)`
- `fw_acpi_tpm_log_addr(void)`
- `fw_acpi_tpm_log_size(void)`

## `src/firmware/fw_agi.h`

- `fw_agi_publish_attest(void)`
- `fw_agi_attest_and_boot(const char *path)`

## `src/firmware/fw_block.h`

- `fw_block_count(void)`
- `fw_block_read(int i, uint64_t lba, uint32_t count, void *buf)`
- `fw_block_write(int i, uint64_t lba, uint32_t count, const void *buf)`

## `src/firmware/fw_fwcfg.h`

- `fw_cfg_init(void)`
- `fw_cfg_present(void)`
- `fw_cfg_read_file(const char *name, void *buf, uint32_t max, uint32_t *out_len)`
- `fw_cfg_file_size(const char *name)`

## `src/firmware/fw_secureboot.h`

- `fw_sb_enroll_db(const uint8_t *der_cert, uint32_t len)`
- `fw_sb_enroll_dbx(const uint8_t *der_cert, uint32_t len)`
- `fw_sb_set_pk(void)`
- `fw_sb_secureboot_enabled(void)`
- `fw_sb_selftest(void)`
- `fw_sb_selftest_esl(void)`
- `fw_sb_selftest_pe(void)`
- `fw_sb_setup_mode(void)`
- `fw_sb_enroll_esl(const char *which, const uint8_t *esl, uint32_t len)`
- `fw_sb_enroll_file(const char *which, const char *path)`
- `fw_sb_create_platform_key(void)`
- `fw_sb_var_present(const char *name)`
- `fw_sb_status(void)`
- `fw_sb_verify(const uint8_t *image, uint32_t size, uint8_t *hash)`

## `src/firmware/fw_sha256.h`

- `sha256_init(sha256_ctx *c)`
- `sha256_update(sha256_ctx *c, const void *data, uint64_t len)`
- `sha256_final(sha256_ctx *c, uint8_t out[32])`
- `sha256(const void *data, uint64_t len, uint8_t out[32])`

## `src/firmware/fw_tpm.h`

- `fw_tpm_init(void)`
- `fw_tpm_present(void)`
- `fw_tpm_interface(void)`

## `src/firmware/loader/sha256.h`

- `sha256_init(sha256_ctx *c)`
- `sha256_update(sha256_ctx *c, const void *data, uint64_t len)`
- `sha256_final(sha256_ctx *c, uint8_t out[32])`
- `sha256(const void *data, uint64_t len, uint8_t out[32])`

## `src/framework/wubufx.h`

- `wubufx_mount(const char *id, WubuFxCap caps, WubuFxApp **out_app)`
- `wubufx_close(WubuFxApp *app)`
- `wubufx_unmount(WubuFxApp *app)`
- `wubufx_node_close(WubuFxNode *node)`
- `wubufx_state_get(WubuFxNode *node, char *out, size_t out_size)`
- `wubufx_state_set(WubuFxNode *node, const char *value)`

## `src/runtime/app_bundle.h`

- `wubu_app_close(wubu_app_bundle_t *a)`
- `wubu_app_needs(const wubu_app_bundle_t *a, char out[][WUBU_APP_NAME_MAX], int cap)`
- `wubu_app_launch(const wubu_app_bundle_t *a)`

## `src/runtime/ct_iso_cgroup.h`

- `wubu_cgroup_write(const char *path, const char *value)`

## `src/runtime/ct_iso_seccomp.h`

- `wubu_ct_apply_seccomp(void *ct_ptr)`
- `wubu_seccomp_install(SeccompProfile profile)`
- `runtime_to_seccomp(CtRuntime runtime)`
- `wubu_ct_child_isolation(void)`
- `wubu_seccomp_profile_register(const char *name, const int *allowlist)`

## `src/runtime/styx.h`

- `styx_msg_name_register(uint8_t type, const char *name)`

## `src/runtime/styxfs.h`

- `styxfs_mount(styxfs_server_t *srv, const char *path, const char *source, int is_repo)`

## `src/runtime/styxfs_internal.h`

- `styxfs_add_child(styxfs_node_t *parent, styxfs_node_t *child)`
- `styxfs_next_qid_path(styxfs_server_t *srv)`
- `styxfs_file_free(styxfs_server_t *srv, uint64_t qid_path)`

## `src/runtime/styxfs_server.h`

- `styxfs_server_destroy(styx_server_t *srv)`

## `src/runtime/win32_core.h`

- `SetLastError(uint32_t err)`
- `GetLastError(void)`
- `Win32FromNTStatus(int64_t status)`

## `src/runtime/wubu_account.h`

- `wubu_account_record_str(const wubu_account_t *rec, char *out, size_t cap)`
- `wubu_account_verify(const wubu_account_t *rec, const char *password)`

## `src/runtime/wubu_agent.h`

- `wubu_agent_format_aid(const char *cls, int yy, int seq, char *out, size_t cap)`
- `wubu_agent_enrol(WubuAgentCard *card)`
- `wubu_agent_load(const char *path)`
- `wubu_agent_save(const char *path)`
- `wubu_agent_is_enroled(void)`
- `wubu_agent_seal(char *hex_out, size_t cap)`
- `wubu_ns_publish_agent(void)`

## `src/runtime/wubu_agi_play.h`

- `wubu_agi_play_set_input_injection(int on)`

## `src/runtime/wubu_anticheat.h`

- `wubu_anticheat_db_register(const AntiCheatInfo *info)`
- `wubu_anticheat_db_reset(const AntiCheatInfo *db, int count)`

## `src/runtime/wubu_arch.h`

- `wubu_arch_install(const char *root_path, const char *packages)`
- `wubu_arch_pacman(const char *root_path, const char *args)`
- `wubu_arch_update(const char *root_path)`
- `wubu_arch_enable_service(const char *root_path, const char *service)`
- `wubu_arch_mkdir_p(const char *path, mode_t mode)`
- `wubu_arch_configure(const char *root_path)`
- `wubu_arch_bootstrap_gui(const char *root_path, const char *mirror)`
- `wubu_arch_bootstrap_steam(const char *root_path, const char *mirror)`
- `wubu_arch_bootstrap_steam_runtime2(const char *root_path, const char *mirror)`
- `wubu_arch_bootstrap_gaming(const char *root_path, const char *mirror)`
- `wubu_arch_root_valid(const char *root_path)`
- `wubu_arch_root_free(WubuArchRoot *info)`
- `run_argv(const char *file, char *const argv[])`
- `run_chroot_argv(const char *root, const char *file, char *const argv[])`

## `src/runtime/wubu_archd.h`

- `wubu_archd_stop(WubuArchd *d)`
- `wubu_archd_shutdown(WubuArchd *d)`

## `src/runtime/wubu_archd_internal.h`

- `run_argv(const char *file, char *const argv[])`
- `run_chroot_argv(const char *root, const char *file, char *const argv[])`
- `archd_write_file(const char *path, const char *content)`
- `archd_mkdir_p(const char *path, mode_t mode)`
- `archd_rm_rf(const char *path)`

## `src/runtime/wubu_archd_svc.h`

- `wubu_svc_supervisor_poll(wubu_svc_supervisor_t *s)`
- `wubu_svc_supervisor_boot(wubu_svc_supervisor_t *s)`
- `wubu_archd_svc_set_supervisor(wubu_svc_supervisor_t *s)`

## `src/runtime/wubu_atspi.h`

- `wb_atspi_node_init(wb_atspi_node_t *n)`
- `wb_atspi_is_describable(const wb_atspi_node_t *n)`

## `src/runtime/wubu_atspi_tree.h`

- `wb_atspi_tree_free(wb_atspi_tree_node_t *n)`

## `src/runtime/wubu_auth.h`

- `wubu_auth_set_agent(wubu_auth_t *auth, wubu_auth_agent_fn fn, void *ud)`

## `src/runtime/wubu_boot_attest_edr.h`

- `wubu_boot_attest_edr_log(const wubu_boot_attest_t *report)`

## `src/runtime/wubu_bottles.h`

- `wubu_bottle_list(const char *install_dir, WubuBottle ***out_bottles, int *count)`
- `wubu_bottle_verify(WubuBottle *bottle)`

## `src/runtime/wubu_buddy_anim.h`

- `wubu_buddy_vibe_count(void)`
- `wubu_buddy_anim_count(void)`
- `wubu_buddy_ease_step(unsigned step, unsigned stepCount, unsigned ease)`

## `src/runtime/wubu_buddy_orb.h`

- `wubu_buddy_orb_set_time(wubu_buddy_orb_t *orb, float t)`
- `wubu_buddy_orb_blob_count(const wubu_buddy_orb_t *orb)`

## `src/runtime/wubu_bus.h`

- `wubu_bus_connect(wubu_bus_t *bus)`
- `wubu_bus_disconnect(wubu_bus_t *bus, int32_t conn)`
- `wubu_bus_request_name(wubu_bus_t *bus, int32_t conn, const char *name)`

## `src/runtime/wubu_colonel.h`

- `wubu_colonel_app_known(const char *name)`
- `wubu_colonel_app_register(const char *name)`

## `src/runtime/wubu_compat_db.h`

- `wubu_compat_cache_dir(const char *title, char *out_path, int path_len)`
- `wubu_compat_normalize_title(const char *raw, char *out, int out_len)`

## `src/runtime/wubu_container.h`

- `wubu_container_validate(const void *data, size_t data_size)`
- `wubu_detect_payload_type(const void *data, size_t size)`
- `wubu_launch_windows(const void *data, size_t size, const char *cmdline)`

## `src/runtime/wubu_ct_isolate.h`

- `wubu_ct_cgroup_set_memory(const char *cgroup_path, uint64_t mem_mb)`
- `wubu_ct_cgroup_set_cpu(const char *cgroup_path, int cpu_count)`
- `wubu_ct_cgroup_set_pids(const char *cgroup_path, int max_pids)`
- `wubu_ct_cgroup_set_io_max(const char *cgroup_path, uint64_t read_bps, uint64_t write_bps)`
- `wubu_ct_cgroup_set_io_weight(const char *cgroup_path, uint32_t weight)`
- `wubu_ct_cgroup_attach(const char *cgroup_path, pid_t pid)`
- `wubu_ct_cgroup_destroy(const char *cgroup_path)`
- `wubu_ct_apply_seccomp(void *ct)`
- `wubu_ct_child_isolation(void)`
- `wubu_ct_setup_isolation(void *ct_ptr)`
- `wubu_ns_unshare(int flags)`
- `wubu_cgroup_write(const char *path, const char *value)`
- `wubu_seccomp_install(SeccompProfile profile)`

## `src/runtime/wubu_daemon_client.h`

- `wubu_daemon_recv_line(DaemonConn *conn, char *line, int max_len)`

## `src/runtime/wubu_dbus.h`

- `wb_dbus_buf_init(wb_dbus_buf_t *b)`
- `wb_dbus_buf_free(wb_dbus_buf_t *b)`
- `wb_dbus_array_begin(wb_dbus_buf_t *b)`
- `wb_dbus_array_end(wb_dbus_buf_t *b)`

## `src/runtime/wubu_deskenv.h`

- `wubu_deskenv_destroy(wubu_deskenv_t *de)`
- `wubu_deskenv_tray_claim(wubu_deskenv_t *de, const char *slot, const char *owner)`

## `src/runtime/wubu_dos_emu.h`

- `wubu_dos_emu_load_com(WubuDosEmu *e, const uint8_t *data, size_t size)`
- `wubu_dos_emu_load_exe(WubuDosEmu *e, const uint8_t *data, size_t size)`
- `wubu_dos_emu_run(WubuDosEmu *e, uint64_t max_steps)`
- `wubu_dos_emu_text(const WubuDosEmu *e, char *out, size_t out_size)`

## `src/runtime/wubu_dos_proc.h`

- `wubu_dos_proc_send_key(WubuDosProc *p, const char *key)`
- `wubu_dos_proc_write_ctl(WubuDosProc *p, const void *buf, size_t len)`
- `wubu_dos_proc_status_text(const WubuDosProc *p, char *out, size_t out_size)`

## `src/runtime/wubu_dxvk_conf.h`

- `dxvk_conf_read(const char *path, char *out, size_t size)`
- `dxvk_conf_set_key(char *buf, size_t bufsz, const char *key, const char *value)`
- `dxvk_conf_get_key(const char *buf, const char *key, char *out, size_t size)`
- `dxvk_conf_build_ui(const DxvkConfigUI *ui, char *buf, size_t bufsz)`

## `src/runtime/wubu_edr.h`

- `edr_analytics_set_enabled(bool on)`
- `edr_analytics_enabled(void)`
- `edr_recent_events(EdrEventView *out, int max, int min_type, int max_type)`

## `src/runtime/wubu_effect.h`

- `wubu_effect_apply(const WubuEffectStack *s, uint32_t *px, int w, int h)`

## `src/runtime/wubu_exec.h`

- `wubu_exec(const void *data, size_t size, const char *filename)`
- `wubu_exec_linux_elf(const void *elf_data, size_t elf_size)`
- `wubu_exec_win_pe(const void *pe_data, size_t pe_size)`
- `wubu_wine_configure_env(WubuCt *ct, const char *wine_prefix)`
- `wubu_exec_holyc(const char *source, size_t source_size)`
- `wubu_exec_c(const char *source, size_t source_size)`
- `wubu_exec_shell(const char *script, size_t script_size)`
- `wubu_exec_python(const char *script, size_t script_size)`
- `wubu_exec_wasm(const void *wasm_data, size_t wasm_size)`
- `wubu_vsl_init(void)`
- `wubu_vsl_shutdown(void)`
- `wubu_vsl_active(void)`
- `wubu_vsl_run(const char *cmd)`

## `src/runtime/wubu_fs_util.h`

- `wubu_fs_rm_rf(const char *path)`

## `src/runtime/wubu_game_session.h`

- `wubu_game_session_end(void)`

## `src/runtime/wubu_gc.h`

- `wubu_gc_free(void *ptr)`
- `wubu_gc_root_add(void *ptr)`
- `wubu_gc_collect(void)`

## `src/runtime/wubu_gdpr_age.h`

- `wubu_gdpr_age_verify(void)`
- `wubu_gdpr_age_check(void)`
- `wubu_gdpr_age_of_consent(void)`

## `src/runtime/wubu_gpu_backend.h`

- `wubu_gpu_backend_vram_mb(void)`

## `src/runtime/wubu_guest_root.h`

- `wubu_guest_root(char *out, size_t outsz)`

## `src/runtime/wubu_gvfs.h`

- `wubu_gvfs_is_in_flight(wubu_gvfs_state_t s)`
- `wubu_gvfs_is_terminal(wubu_gvfs_state_t s)`
- `wubu_gvfs_handle_init(wubu_gvfs_handle_t *h, uint64_t id)`

## `src/runtime/wubu_gvfs_mount.h`

- `wubu_gvfs_mount_table_init(wubu_gvfs_mount_table_t *t)`
- `wubu_gvfs_mount_contains(const wubu_gvfs_mount_t *m, const char *path)`

## `src/runtime/wubu_gvfs_path.h`

- `wubu_gvfs_path_is_safe(const char *path)`

## `src/runtime/wubu_holyd.h`

- `wubu_holyd_daemon_stop(WubuHoly *d)`
- `wubu_holyd_shutdown(WubuHoly *d)`
- `wubu_holyd_set_pointer_handler(wubu_holyd_pointer_fn fn)`

## `src/runtime/holyc_eval.h`

- `wubu_holyd_agi_init(void)`
- `wubu_holyc_eval(const char *src, char *out, size_t out_size)`
- `wubu_holyc_agent_eval(const char *src, char *out, size_t out_size)`

## `src/runtime/wubu_hook.h`

- `wubu_hook_chain_init(wubu_hook_chain_t *c)`

## `src/runtime/wubu_host_exec.h`

- `wubu_ct_destroy(WubuCt *ct)`
- `wubu_ct_start(WubuCt *ct)`
- `wubu_ct_wait(WubuCt *ct)`
- `wubu_ct_kill(WubuCt *ct, int sig)`
- `wubu_ct_state(WubuCt *ct)`

## `src/runtime/wubu_hwdetect.h`

- `wubu_hw_detect(wubu_hw_t *hw)`
- `wubu_hw_kernel_choice(const wubu_hw_t *hw)`

## `src/runtime/wubu_image.h`

- `wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size)`
- `wubu_manifest_from_json(const char *json, WubuImageManifest *manifest)`
- `wubu_manifest_save(const WubuImageManifest *manifest, const char *path)`
- `wubu_manifest_load(const char *path, WubuImageManifest *manifest)`
- `wubu_arch_name_register(WubuArch arch, const char *name)`
- `wubu_os_name_register(WubuOS os, const char *name)`

## `src/runtime/wubu_image_manifest.h`

- `wubu_manifest_compute_id(WubuImageManifest *manifest)`
- `wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size)`
- `wubu_manifest_from_json(const char *json, WubuImageManifest *manifest)`
- `wubu_manifest_save(const WubuImageManifest *manifest, const char *path)`
- `wubu_manifest_load(const char *path, WubuImageManifest *manifest)`

## `src/runtime/wubu_kernel_xchg.h`

- `wubu_kernel_register(const wubu_kernel_ops_t *ops)`
- `wubu_kernel_set_our_schema(int major)`
- `wubu_kernel_our_schema(void)`
- `wubu_kernel_schema_ok(const char *name, int our_schema)`
- `wubu_kernel_select(const char *name)`
- `wubu_kernel_swap(const char *name, char *prev, size_t prev_n)`
- `wubu_kernel_xchg_dump(char *buf, size_t n)`

## `src/runtime/wubu_logind.h`

- `wubu_logindd_add_session(wubu_logindd_t *login, const wubu_logindd_session_t *s)`
- `wubu_logindd_power(wubu_logindd_t *login, wubu_logindd_power_op_t op)`

## `src/runtime/wubu_netlink.h`

- `netlink_addr_add(const char *iface, const char *ip_with_cidr)`

## `src/runtime/wubu_ns_background.h`

- `wubu_ns_publish_background(wubu_session_boot_t *boot)`
- `wubu_ns_publish_login(wubu_logindd_t *login)`
- `wubu_ns_publish_auth(wubu_auth_t *auth)`

## `src/runtime/wubu_ns_bridge.h`

- `wubu_ns_bridge_create(const char *ns_root)`
- `wubu_ns_publish_bottle(const WubuBottle *b, const char *name)`
- `wubu_ns_bottle_action(WubuBottle *b, const char *action)`
- `wubu_ns_publish_snapshots(WubuSnapshotManager *mgr, const char *container_id)`

## `src/runtime/wubu_ns_dram.h`

- `wubu_ns_publish_dram(int elem_size, unsigned replicas)`

## `src/runtime/wubu_ns_kernels.h`

- `wubu_ns_kernels_select(const char *name)`

## `src/runtime/wubu_ns_kv.h`

- `wubu_kvfs_namespace_init(void)`
- `wubu_ns_kv_touch(int tick)`

## `src/runtime/wubu_ns_steaminput.h`

- `wubu_ns_steaminput_report(const char *hex)`

## `src/runtime/wubu_ns_worldspace.h`

- `wubu_ns_publish_worldspace(void)`

## `src/runtime/wubu_ntsync.h`

- `wubu_ntsync_wait(wubu_ntsync_t *ke, uint64_t key, int64_t timeout_ms)`
- `wubu_ntsync_release(wubu_ntsync_t *ke, uint64_t key, int wait, int64_t timeout_ms)`

## `src/runtime/wubu_onboarding.h`

- `wubu_onboarding_load(const char *sentinel_path)`
- `wubu_onboarding_complete(const char *signature)`

## `src/runtime/wubu_pkg.h`

- `pkg_version_cmp(const char *a, const char *b)`
- `pkg_satisfies(const char *version, const char *constraint)`

## `src/runtime/wubu_pkgbuild.h`

- `wubu_pkgbuild_is_resolved(const wubu_pkgbuild_t *pb)`

## `src/runtime/wubu_pressure_vessel.h`

- `wubu_pv_launch(WubuPv *pv)`

## `src/runtime/wubu_proton.h`

- `wubu_proton_validate_pe(wubu_proton_t *p, const uint8_t *data, size_t size)`
- `wubu_proton_parse_pe(wubu_proton_t *p, const uint8_t *data, size_t size)`
- `wubu_proton_map_sections(wubu_proton_t *p, const uint8_t *data, size_t size)`
- `wubu_proton_translate_api(wubu_proton_t *p, const char *win32_name)`
- `wubu_proton_load_default_dlls(wubu_proton_t *p)`

## `src/runtime/wubu_proton2.h`

- `wubu_proton_dump(const WubuProtonManager *mgr)`
- `wubu_proton_verify_installation(const WubuProtonManager *mgr)`

## `src/runtime/wubu_proton_dxvk.h`

- `wubu_proton_dxvk_set_resolver(wubu_proton_dxvk_path_resolver r)`

## `src/runtime/wubu_ramdisk.h`

- `wubu_rd_destroy(WubuRamdisk *rd)`
- `wubu_rd_boot(WubuRamdisk *rd)`
- `wubu_rd_state(WubuRamdisk *rd)`
- `wubu_rd_usage_mb(WubuRamdisk *rd)`
- `wubu_rd_mount(WubuRamdisk *rd)`
- `wubu_rd_load(WubuRamdisk *rd)`
- `wubu_rd_unmount(WubuRamdisk *rd)`
- `wubu_rd_bootstrap_disk(WubuRamdisk *rd, const char *mirror)`
- `wubu_rd_install_to_disk(WubuRamdisk *rd, const char *disk_path)`
- `wubu_rd_snapshot(WubuRamdisk *rd, const char *output_path)`
- `wubu_rd_install(WubuRamdisk *rd, const char *packages)`
- `wubu_rd_set_ram_size(WubuRamdisk *rd, const char *size_str)`

## `src/runtime/wubu_realm.h`

- `wubu_realm_set_kernel_schema(wubu_realm_t *r, int kernel_major)`
- `wubu_realm_kernel_schema_ok(const wubu_realm_t *r)`

## `src/runtime/wubu_redsea.h`

- `wubu_redsea_both_endian_u32(const uint8_t *p, uint32_t *out)`

## `src/runtime/wubu_rim.h`

- `wubu_rim_perimeter(const WubuRim *r)`
- `wubu_rim_nearest_t(const WubuRim *r, float px, float py)`

## `src/runtime/wubu_runtime.h`

- `wubu_runtime_save(const wubu_runtime_t *rt, const char *path)`
- `wubu_runtime_load(wubu_runtime_t *rt, const char *path)`

## `src/runtime/wubu_sdr.h`

- `wubu_sdr_ticket_encode(const wubu_sdr_ticket_t *t, char *out, size_t n)`
- `wubu_sdr_ticket_decode(const char *in, wubu_sdr_ticket_t *out)`
- `wubu_sdr_ticket_is_valid(const wubu_sdr_ticket_t *t, uint64_t now)`
- `wubu_sdr_table_add(wubu_sdr_table_t *t, const wubu_sdr_route_t *r)`

## `src/runtime/wubu_sdr_session.h`

- `wubu_sdr_sessions_close(wubu_sdr_sessions_t *s, uint32_t vport)`

## `src/runtime/wubu_secmon.h`

- `wubu_secmon_attach(wubu_secmon_t *m, pid_t pid)`
- `wubu_secmon_poll(wubu_secmon_t *m)`
- `wubu_secmon_wait(wubu_secmon_t *m)`
- `wubu_secmon_set_kv_stream(wubu_secmon_t *m, bool on)`

## `src/runtime/wubu_selfimprove.h`

- `wubu_selfimprove_destroy(wubu_selfimprove_t *s)`
- `wubu_selfimprove_cycle(wubu_selfimprove_t *s)`
- `wubu_selfimprove_set_human_gate(wubu_selfimprove_t *s, bool require)`

## `src/runtime/wubu_session_boot.h`

- `wubu_session_boot_start_all(wubu_session_boot_t *boot)`

## `src/runtime/wubu_session_mgr.h`

- `wubu_session_mgr_shutdown(wubu_session_mgr_t *bg)`

## `src/runtime/wubu_slime.h`

- `wubu_slime_free(WubuSlime *s)`

## `src/runtime/wubu_spawn.h`

- `wubu_run_program(const char *file, char *const argv[], bool silent)`

## `src/runtime/wubu_spell.h`

- `wubu_spellbook_close(wubu_spellbook_t *b)`
- `wubu_spellbook_count(const wubu_spellbook_t *b)`
- `wubu_spellbook_cast(wubu_spellbook_t *b, const char *name)`
- `wubu_spellbook_error_count(const wubu_spellbook_t *b)`
- `wubu_spellbook_conflict_count(const wubu_spellbook_t *b)`
- `wubu_spellbook_unmet_count(const wubu_spellbook_t *b)`

## `src/runtime/wubu_spell_resolve.h`

- `wubu_spell_registry_add(wubu_spell_resolve *r, wubu_spellmod_t *m)`
- `wubu_spell_default_dir(char *out, size_t outsz)`
- `wubu_spell_scan(wubu_spell_resolve *r, const char *dir)`

## `src/runtime/wubu_spellmod.h`

- `wubu_spellmod_close(wubu_spellmod_t *m)`
- `wubu_spellmod_provides(const wubu_spellmod_t *m, const char *capability)`

## `src/runtime/wubu_spring.h`

- `wubu_spring_field_init(WubuSpringField *f, int cap)`
- `wubu_spring_field_free(WubuSpringField *f)`

## `src/runtime/wubu_steaminput.h`

- `wubu_si_parse_deck_report(const uint8_t *data, size_t size)`
- `wubu_si_parse_deck_sensors(const uint8_t *data, size_t size)`
- `wubu_si_set_lizard_mode(int enable)`
- `wubu_si_lizard_mode(void)`

## `src/runtime/wubu_steamrt.h`

- `wubu_steamrt_verify(const char *const *libs, size_t n, int check_host)`

## `src/runtime/wubu_svc_socket.h`

- `wubu_svc_socket_destroy(wubu_svc_socket_t *sock)`
- `wubu_svc_socket_poll(wubu_svc_socket_t *sock)`
- `wubu_svc_socket_accept(wubu_svc_socket_t *sock)`

## `src/runtime/wubu_system.h`

- `wubu_system_init(const char *store_path)`
- `wubu_system_commit(const char *label, char *out_id, int id_len)`
- `wubu_system_rollback(const char *snapshot_id)`
- `wubu_system_is_readonly(void)`
- `wubu_system_active_label(char *out_label, int label_len)`

## `src/runtime/wubu_trace.h`

- `wubu_trace_destroy(wubu_trace_store_t *t)`
- `wubu_trace_set_consent(wubu_trace_store_t *t, bool may_collect)`
- `wubu_trace_get_consent(const wubu_trace_store_t *t)`
- `wubu_trace_set_frozen(wubu_trace_store_t *t, bool frozen)`
- `wubu_trace_is_frozen(const wubu_trace_store_t *t)`
- `wubu_trace_mirror(const wubu_trace_store_t *t, const char *path)`

## `src/runtime/wubu_track.h`

- `wb_track_is_stopword(const char *term)`

## `src/runtime/wubu_verifier_bytropix.h`

- `wubu_bytropix_verifier_destroy(wubu_bytropix_verifier_t *v)`
- `wubu_bytropix_score(const wubu_trace_span_t *span, void *ud, bool *passed)`

## `src/runtime/wubu_vga.h`

- `wb_vga_console_init(wb_vga_console_t *c)`
- `wb_vga_console_putc(wb_vga_console_t *c, char ch)`
- `wb_vga_console_puts(wb_vga_console_t *c, const char *s)`
- `wb_vga_console_clear(wb_vga_console_t *c)`

## `src/runtime/wubu_vsl.h`

- `vsl_init(void)`
- `vsl_shutdown(void)`
- `vsl_active(void)`
- `vsl_info(char *buf, size_t buf_size)`
- `vsl_dump_state(void)`

## `src/runtime/wubu_xdg_autostart.h`

- `wubu_xdga_scan_dir(const char *dir, wubu_xdga_entry_t *out, int max)`
- `wubu_xdga_build_argv(const wubu_xdga_entry_t *entry, char **argv)`
- `wubu_xdga_launch(const wubu_xdga_entry_t *entry)`

## `src/runtime/wubu_zxload.h`

- `wubu_zxload_read(const char *path, uint8_t **_out, size_t *_sz)`

## `src/runtime/edr/edr_internal.h`

- `fnv1a(const char *key)`

## `src/runtime/wubu_txn/wubu_txn.h`

- `wubu_txn_commit(WubuTxn *t)`

## `src/runtime/win32k/wubu_ns_win32.h`

- `wubu_ns_win32_sync(void)`

## `src/runtime/win32k/wubu_win32k.h`

- `wubu_win32k_init(void)`
- `wubu_win32k_revoke(wubu_cap_token_t tok)`
- `wubu_win32k_gdi_addref(wubu_cap_token_t g, int32_t caller_pid)`
- `wubu_win32k_gdi_deref(wubu_cap_token_t g, int32_t caller_pid)`
- `wubu_win32k_gdi_acquire(wubu_cap_token_t g, int32_t caller_pid)`
- `wubu_win32k_gdi_release(wubu_cap_token_t g, int32_t caller_pid)`
- `wubu_win32k_gdi_mark_destroy(wubu_cap_token_t g, int32_t caller_pid)`

## `src/runtime/container/wubu_container_universal.h`

- `wu_container_register(const wubu_container_adapter_t *adapter)`
- `wu_container_close(wubu_container_universal_t *c)`

## `src/runtime/container/wubucontainer.h`

- `wubu_container_init(WubuContainerEngine **engine_out, const char *container_dir)`

## `src/runtime/vsl/vsl_driver.h`

- `vsl_activate_driver(int drv_id)`
- `vsl_deactivate_driver(int drv_id)`

## `src/runtime/vsl/vsl_elf.h`

- `vsl_elf_load(const void *elf_data, size_t elf_size)`

## `src/runtime/vsl/vsl_file.h`

- `vsl_open(const char *path, int flags, int mode)`
- `vsl_close(int fd)`
- `vsl_read(int fd, void *buf, size_t count)`
- `vsl_write(int fd, const void *buf, size_t count)`
- `vsl_lseek(int fd, int64_t offset, int whence)`

## `src/runtime/vsl/vsl_gpu_vulkan.h`

- `vsl_vulkan_find_memory_type(VkMemoryPropertyFlags required_flags)`

## `src/runtime/vsl/vsl_memory.h`

- `vsl_munmap(uint64_t addr, size_t size)`
- `vsl_brk(uint64_t new_brk)`

## `src/runtime/vsl/vsl_nt_bridge.h`

- `vsl_nt_handle_to_data(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle, uint64_t *out_data)`
- `vsl_nt_allocate_handle(vsl_nt_bridge_ctx_t *ctx, int vsl_fd, uint64_t styx_fid, nt_object_type_t type)`
- `vsl_nt_free_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle)`
- `vsl_nt_allocate_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`
- `vsl_nt_free_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`

## `src/runtime/vsl/vsl_nt_internal.h`

- `vsl_nt_contract_sweep(nt_contract_result_t *out, uint32_t cap)`
- `vsl_nt_atoms_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_job_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_io_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_vmem_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_process_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_thread_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_section_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_timer_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_sync_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_registry_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_token_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_probe(uint32_t num, wubu_nt_slot_state_t *state)`
- `vsl_nt_registration_reset(void)`
- `vsl_nt_misc_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_misc_w11_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_alpc_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_wnf_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_worker_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_enclave_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_ioring_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_partition_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_ktm_register(vsl_syscall_fn_t *tbl, int size)`
- `vsl_nt_export_modsym(void)`

## `src/runtime/vsl/vsl_process.h`

- `vsl_create_process(const void *elf_data, size_t elf_size)`
- `vsl_create_process_macho(const void *macho_data, size_t macho_size)`
- `vsl_create_process_any(const void *binary_data, size_t binary_size)`
- `vsl_destroy_process(uint32_t pid)`
- `vsl_list_processes(VSL_PROC *out, int max_count)`

## `src/runtime/vsl/vsl_shared.h`

- `vsl_send_cmd(uint64_t cmd, uint64_t arg)`

## `src/runtime/vsl/vsl_syscall.h`

- `vsl_syscall_dispatch(uint64_t num, uint64_t *regs)`
- `vsl_kernel_interchange_init(void)`

## `src/runtime/vsl/gpu/vulkan/wubu_vsl_vulkan.h`

- `wubu_vsl_vk_init(const WubuVkInstanceCreateInfo *create_info)`
- `wubu_vsl_vk_cleanup(void)`
- `wubu_vsl_vk_get_backend(void)`
- `wubu_vsl_vk_get_gpu_device_fd(VkPhysicalDevice phys_dev, int *fd)`
- `wubu_vsl_venus_get_context(VkInstance instance, void **venus_context)`
- `wubu_vsl_venus_set_frame_pacing(VkInstance instance, uint32_t max_frames)`
- `wubu_vsl_vk_check_extension(VkPhysicalDevice phys_dev, const char *ext_name)`
- `wubu_vsl_vk_set_debug_callback(WubuVkDebugCallback callback, void *user_data)`
- `wubu_vsl_vk_dump_capabilities(VkInstance instance)`

## `src/runtime/vsl/gpu/kern/wubu_vsl_gpu.h`

- `wubu_vsl_gpu_close(WubuVslGpuDevice *device)`
- `wubu_vsl_gpu_get_info(WubuVslGpuDevice *device, WubuGpuDeviceInfo *info)`
- `wubu_vsl_gpu_buffer_destroy(WubuVslGpuBuffer *buffer)`
- `wubu_vsl_gpu_buffer_get_info(WubuVslGpuBuffer *buffer, WubuGpuBufferInfo *info)`
- `wubu_vsl_gpu_buffer_map(WubuVslGpuBuffer *buffer, void **ptr)`
- `wubu_vsl_gpu_buffer_unmap(WubuVslGpuBuffer *buffer)`
- `wubu_vsl_gpu_buffer_export_dmabuf(WubuVslGpuBuffer *buffer)`
- `wubu_vsl_gpu_context_destroy(WubuVslGpuContext *context)`
- `wubu_vsl_gpu_context_submit(WubuVslGpuContext *context, const WubuGpuSubmitInfo *submit)`
- `wubu_vsl_gpu_fence_destroy(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_fence_wait(WubuVslGpuFence *fence, uint64_t timeout_ns)`
- `wubu_vsl_gpu_fence_reset(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_fence_get_value(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_fence_signal(WubuVslGpuFence *fence, uint64_t value)`
- `wubu_vsl_gpu_fence_export_sync_file(WubuVslGpuFence *fence)`
- `wubu_vsl_gpu_get_connectors(WubuVslGpuDevice *device, WubuGpuConnector **connectors)`
- `wubu_vsl_gpu_get_modes(WubuGpuConnector *connector, WubuGpuMode **modes)`
- `wubu_vsl_gpu_atomic_commit(WubuVslGpuDevice *device, const WubuGpuAtomicCommit *commit, bool test_only)`
- `wubu_vsl_gpu_dump_state(WubuVslGpuDevice *device)`

## `src/runtime/wubu_manifest/wubu_manifest.h`

- `wubu_manifest_emit(const wubu_manifest_t *m, const char *out_dir)`

## `src/runtime/wubu_manifest/wubu_manifest_internal.h`

- `wubu_json_parse_manifest(const char *json, size_t len, wubu_manifest_t *m)`

## `src/hosted/hosted.h`

- `hosted_init(hosted_state_t *state, int argc, char **argv)`
- `hosted_run(hosted_state_t *state)`
- `hosted_shutdown(hosted_state_t *state)`
- `hosted_set_mode(hosted_state_t *state, hosted_mode_t mode)`

## `src/hosted/hosted_agent_channel.h`

- `hosted_agent_channel_poll(void)`

## `src/hosted/hosted_display.h`

- `hosted_display_primary(const hosted_displays_t *ds)`

## `src/hosted/hosted_fs_persist.h`

- `fs_persist_load(int replace)`
- `fs_persist_save(void)`

## `src/hosted/hosted_internal.h`

- `styxfs_max_files(void)`
- `fs_find(const char *name)`
- `shm_buffer_create(shm_buffer_t *buf, int w, int h)`
- `shm_buffer_destroy(shm_buffer_t *buf)`
- `hosted_render_desktop(hosted_state_t *state)`
- `hosted_input_dispatch(void)`
- `hosted_pe_executor(const void *data, size_t size, const char *cmdline)`
- `hosted_run(hosted_state_t *state)`
- `hosted_shutdown(hosted_state_t *state)`
- `hosted_blit(hosted_state_t *state)`
- `hosted_set_mode(hosted_state_t *state, hosted_mode_t mode)`
- `hosted_kernel_ready(void)`
- `hosted_wm_has_windows(void)`

## `src/hosted/wubu_display.h`

- `wubu_gbm_destroy_device(wubu_gbm_device_t *gbm)`
- `wubu_gbm_bo_destroy(wubu_gbm_device_t *gbm, wubu_gbm_bo_t *bo)`
- `wubu_gbm_bo_get_stride(wubu_gbm_bo_t *bo)`
- `wubu_gbm_bo_get_handle(wubu_gbm_bo_t *bo)`

## `src/hosted/wubu_metal_audio.h`

- `wubu_alsa_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_alsa_shutdown(void)`
- `wubu_alsa_submit(const float *buf, int frames)`
- `wubu_alsa_cpu_load(void)`
- `wubu_pulse_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pulse_shutdown(void)`
- `wubu_pulse_submit(const float *buf, int frames)`
- `wubu_pulse_cpu_load(void)`
- `wubu_pipewire_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pipewire_shutdown(void)`
- `wubu_pipewire_submit(const float *buf, int frames)`
- `wubu_pipewire_cpu_load(void)`

## `src/hosted/wubu_metal_drm.h`

- `wubu_drm_init(int width, int height)`
- `wubu_drm_shutdown(void)`
- `wubu_drm_flip(void)`
- `wubu_drm_set_mode(int width, int height, int refresh_hz)`
- `wubu_drm_get_modes(int *widths, int *heights, int max)`

## `src/hosted/wubu_metal_internal.h`

- `wubu_evdev_find_device(const char *type, int *out_fd)`
- `wubu_evdev_init_all(void)`
- `wubu_evdev_shutdown(void)`
- `wubu_evdev_poll(void)`
- `wubu_evdev_key_down(uint32_t key)`
- `wubu_evdev_mouse_pos(int *x, int *y)`
- `wubu_alsa_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_alsa_shutdown(void)`
- `wubu_alsa_submit(const float *buf, int frames)`
- `wubu_alsa_cpu_load(void)`
- `wubu_pulse_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pulse_shutdown(void)`
- `wubu_pulse_submit(const float *buf, int frames)`
- `wubu_pulse_cpu_load(void)`
- `wubu_pipewire_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_pipewire_shutdown(void)`
- `wubu_pipewire_submit(const float *buf, int frames)`
- `wubu_pipewire_cpu_load(void)`
- `wubu_wsl2_disp_init(void)`
- `wubu_wsl2_audio_init(void)`
- `wubu_x11_init(int width, int height)`
- `wubu_x11_shutdown(void)`
- `wubu_x11_flip(void)`
- `wubu_x11_set_mode(int width, int height, int refresh_hz)`
- `vbe_init_fb(int width, int height)`
- `vbe_shutdown_fb(void)`
- `wubu_disp_init(int width, int height)`
- `wubu_disp_shutdown(void)`
- `wubu_disp_set_mode(int width, int height, int refresh_hz)`
- `wubu_disp_flip(void)`
- `wubu_disp_poll_events(void)`
- `wubu_disp_current(void)`
- `wubu_disp_force(WubuDispBackend backend)`
- `wubu_disp_get_modes(int *widths, int *heights, int max)`
- `wubu_disp_gaad_nearest(int w, int h, int *out_w, int *out_h)`
- `wubu_input_init(void)`
- `wubu_input_shutdown(void)`
- `wubu_input_poll(void)`
- `wubu_input_key_down(uint32_t key)`
- `wubu_input_mouse_pos(int *x, int *y)`
- `wubu_input_gamepads(char names[][64])`
- `wubu_audio_init(int sample_rate, int channels, int buffer_frames)`
- `wubu_audio_shutdown(void)`
- `wubu_audio_submit(const float *buf, int frames)`
- `wubu_audio_cpu_load(void)`
- `wubu_metal_init(int width, int height)`
- `wubu_metal_run(void)`
- `wubu_metal_shutdown(void)`

## `src/hosted/wubu_vk_effect.h`

- `wubu_vk_effect_free(WubuVkEffect *e)`

## `src/jit/jit.h`

- `jit_call0(JITFunc *fn)`
- `jit_call1(JITFunc *fn, int64_t a0)`
- `jit_call2(JITFunc *fn, int64_t a0, int64_t a1)`
- `jit_free_exec(void *ptr, size_t size)`
- `wubu_clear_cache(void *addr, size_t size)`
- `jit_unlock_exec(void *ptr, size_t size)`
- `jit_stats(const JITContext *ctx, JITStats *out)`

## `src/jit/jit_branch_profile.h`

- `jbp_fallthrough_is_hot(int id)`
- `jbp_branch_count(void)`
- `jbp_emit_taken_inc(unsigned char *buf, int id)`

## `src/jit/jit_internal.h`

- `minic_peek2(MinicLexer *l)`
- `minic_advance(MinicLexer *l)`
- `minic_expect(MinicLexer *l, MinicTokType t)`
- `minic_is_type(MinicTokType t)`
- `minic_type_registry_init(MinicTypeRegistry *r)`
- `minic_type_new(MinicTypeRegistry *r)`
- `minic_type_index(MinicTypeRegistry *r, MinicType *t)`
- `minic_type_layout(MinicTypeRegistry *r, MinicType *s)`
- `minic_type_member_offset(MinicTypeRegistry *r, int type_idx, const char *member)`
- `minic_type_member_size(MinicTypeRegistry *r, int type_idx, const char *member)`
- `minic_type_size(MinicTypeRegistry *r, int type_idx)`
- `minic_type_align(MinicTypeRegistry *r, int type_idx)`
- `minic_type_is_struct(MinicTypeRegistry *r, int type_idx)`
- `minic_type_is_ptr(MinicTypeRegistry *r, int type_idx)`
- `minic_type_is_array(MinicTypeRegistry *r, int type_idx)`
- `minic_type_elem(MinicTypeRegistry *r, int type_idx)`

## `src/jit/jit_minic_loop.h`

- `minic_loop_body_init(LoopBody *lb)`
- `minic_loop_invariant_count(LoopBody *lb)`

## `src/jit/wubu_arm64.h`

- `warm64_bl_reg(WArm64Enc *e, WArm64Reg rn)`
- `warm64_ret(WArm64Enc *e, WArm64Reg rn)`
- `warm64_patch_branch(WArm64Enc *e, size_t branch_pos, size_t target)`

## `src/jit/wubu_disasm.h`

- `wdisasm_dump(const uint8_t *code, size_t code_len, FILE *out)`

## `src/jit/wubu_wasm.h`

- `wasm_enc_init(WasmEnc *e, uint8_t *buf, size_t cap)`
- `wasm_enc_init_dynamic(WasmEnc *e, size_t initial_cap)`
- `wasm_enc_free(WasmEnc *e)`
- `wasm_emit(WasmEnc *e, uint8_t b)`
- `wasm_block_i64(WasmEnc *e)`
- `wasm_loop(WasmEnc *e)`
- `wasm_if(WasmEnc *e)`
- `wasm_else(WasmEnc *e)`
- `wasm_end(WasmEnc *e)`
- `wasm_return(WasmEnc *e)`
- `wasm_br(WasmEnc *e, uint32_t label)`
- `wasm_br_if(WasmEnc *e, uint32_t label)`

## `src/jit/wubu_x86.h`

- `wx86_patch_rel32(Wx86Enc *e, size_t patch_pos, size_t target_pos)`
- `wx86_movzx_byte_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp)`
- `wx86_xor_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src)`
- `wx86_zero_reg(Wx86Enc *e, Wx86Reg r)`
- `wx86_multi_nop(Wx86Enc *e, int bytes)`
- `wx86_add_rax_mem(Wx86Enc *e, Wx86Reg base, int32_t disp)`
- `wx86_lea_scaled_index(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, uint8_t scale)`
- `wx86_imul_reg_reg_imm32(Wx86Enc *e, Wx86Reg dst, Wx86Reg src, int32_t imm)`
- `wx86_imul_rax_rm(Wx86Enc *e, Wx86Reg rm)`
- `wx86_movnti_mem_reg(Wx86Enc *e, Wx86Reg base, int32_t disp, Wx86Reg src)`
- `wx86_test_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b)`
- `wx86_cmovcc_reg_reg(Wx86Enc *e, Wx86CC cc, Wx86Reg dst, Wx86Reg src)`
- `wx86_jmp_reg(Wx86Enc *e, Wx86Reg reg)`
- `wx86_setcc_r8(Wx86Enc *e, Wx86CC cc, Wx86Reg dst)`

## `src/jit/x86_regalloc.h`

- `xra_alloc(XRARegAlloc *ra, int vreg)`
- `xra_finalize(XRARegAlloc *ra)`
- `xra_callee_saved_list(const XRARegAlloc *ra, Wx86Reg *out, int max)`
- `xra_spill_store(XRARegAlloc *ra, int vreg, Wx86Enc *e)`
- `xra_mark_const(XRARegAlloc *ra, int vreg, int64_t val)`
- `xra_set_next_use(XRARegAlloc *ra, int vreg, int next_use)`
- `xra_evict_farthest(XRARegAlloc *ra, Wx86Enc *e)`
- `xra_alloc_evict(XRARegAlloc *ra, int vreg, Wx86Enc *e)`

## `src/audio/wubu_audio.h`

- `wubu_audio_engine_create(int sample_rate, int buffer_frames, int channels)`
- `wubu_audio_engine_destroy(void)`
- `wubu_audio_backend_stop(void)`
- `wubu_audio_backend_running(void)`
- `wubu_audio_backend_latency(void)`
- `wubu_audio_sink_open(void)`
- `wubu_audio_sink_close(void)`
- `wubu_audio_sink_int16(const int16_t *pcm, int n)`
- `wubu_glott_speak(int vowel, int ms, double f0)`
- `wubu_glott_vowel_count(void)`
- `wubu_glott_scale(double f0)`
- `wubu_furnace_init(int n_chips, const WubuChipType *chips)`
- `wubu_furnace_shutdown(void)`
- `wubu_sf2_load(const uint8_t *data, size_t size)`
- `wubu_sf2_load_file(const char *path)`
- `wubu_sf2_unload(void)`

## `src/audio/bigmac/bigmac_out.h`

- `bigmac_out_init(int sample_rate, int channels, const char *wav_path)`
- `bigmac_out_write(const float *frames, int nframes)`
- `bigmac_out_flush(void)`

## `src/audio/bigmac/include/wbus/wb_recorder.h`

- `wb_recorder_destroy(wb_recorder *r)`
- `wb_recorder_set_overdub(wb_recorder *r, int overdub)`

## `src/audio/bigmac/include/wbus/wb_unit.h`

- `wb_instrument_destroy(wb_instrument *inst)`

## `src/audio/bigmac/include/wbus/wbus.h`

- `wb_project_destroy(wb_project *p)`
- `wb_project_sequence_count(const wb_project *p)`
- `wb_project_active_index(const wb_project *p)`
- `wb_project_add_sequence(wb_project *p, const char *name)`
- `wb_project_remove_sequence(wb_project *p, int i)`
- `wb_project_set_active(wb_project *p, int i)`
- `wb_project_save(const wb_project *p, const char *path)`
- `wb_session_destroy(wb_session *s)`
- `wb_session_add_time_sig_change(wb_session *s, double pos_samples, int num, int den)`
- `wb_session_get_time_sig_at(wb_session *s, double pos_samples, int *num_out, int *den_out)`
- `wb_session_get_bar_start(const wb_session *s, int bar_number)`
- `wb_session_samples_to_beats(const wb_session *s, double samples)`
- `wb_session_beats_to_samples(const wb_session *s, double beats)`
- `wb_session_remove_track(wb_session *s, uint32_t idx)`
- `wb_session_move_track(wb_session *s, uint32_t idx, int delta)`
- `wb_session_add_note(wb_track *tr, double start, double dur, int pitch, int vel)`
- `wb_session_track_pan_automation(wb_track *tr, struct wb_param_track *pt)`
- `wb_session_remove_note(wb_track *tr, double start, int pitch)`
- `wb_session_set_active_lane(wb_session *s, int track, int lane)`
- `wb_session_comp_region(wb_session *s, int track, int src_lane, double t0, double t1)`
- `wb_session_create_folder(wb_session *s, const char *name, int parent_folder_idx)`
- `wb_session_remove_folder(wb_session *s, int folder_idx)`
- `wb_session_set_send(wb_session *s, int src_track, int dest_track, float level, int send_index)`
- `wb_engine_set_clap_host(wb_engine *e, struct wb_clap_host *h)`
- `wb_engine_set_session(wb_engine *e, wb_session *s)`
- `wb_engine_set_snap(wb_engine *e, int on)`
- `wb_engine_launch(wb_engine *e, int track, int clip_idx)`
- `wb_engine_stop_launch(wb_engine *e, int track)`
- `wb_engine_launched_clip(wb_engine *e, int track)`
- `wb_engine_set_insert_param(wb_engine *e, int track, int slot, int param, float value)`
- `wb_engine_get_master_meter(wb_engine *e, float *peak, float *rms)`
- `wb_session_snap_to_beat(double t, double bpm)`
- `wb_engine_get_master_lufs(wb_engine *e, float *lufs_st, float *true_peak)`
- `wb_engine_set_send_level(wb_engine *e, int src_track, int dst_track, float level)`
- `wb_swing_offset(double bpm, double swing, double pos)`
- `wb_engine_set_insert_sidechain(wb_engine *e, int track, int slot, int src_track)`
- `wb_engine_set_midifx(wb_engine *e, int track, int slot, wb_midifx_type type)`
- `wb_engine_record(wb_engine *e, int track, int clip_idx, int on, int overdub)`
- `wb_engine_render(wb_engine *e, wb_sample *out, uint32_t n)`
- `wb_engine_xruns(wb_engine *e)`
- `wb_inputring_destroy(wb_input_ring *r)`
- `wb_inputring_count(const wb_input_ring *r)`
- `wb_engine_begin_edit(wb_engine *e)`
- `wb_engine_end_edit(wb_engine *e)`
- `wb_engine_invalidate_render_cache(void)`
- `wb_clip_set_color(wb_clip *cl, float exposure, float saturation)`
- `wb_session_video_clip_at(wb_session *s, int track, double timeline_pos)`
- `wb_session_import_chain(wb_session *s, int track, const char *chain)`
- `wb_warp_destroy(wb_warp *w)`
- `wb_warp_add_marker(wb_warp *w, double src_sample, double dst_beat)`
- `wb_warp_remove_marker(wb_warp *w, int index)`
- `wb_warp_clear_markers(wb_warp *w)`
- `wb_warp_marker_count(const wb_warp *w)`
- `wb_warp_src_to_dst(const wb_warp *w, double src_sample)`
- `wb_warp_dst_to_src(const wb_warp *w, double beat)`
- `wb_warp_auto_warp(wb_warp *w, const double *beat_positions, int num_beats)`
- `wb_aaf_export(const wb_session *session, const char *path)`
- `wb_omf_export(const wb_session *session, const char *path)`
- `wb_session_spatial_enable(wb_session *s, int on)`
- `wb_spatial_destroy(void *sp)`
- `wb_spatial_set_position(void *sp, float azimuth, float elevation, float distance)`
- `wb_spatial_set_listener_orientation(void *sp, float yaw, float pitch, float roll)`
- `wb_spatial_process(void *sp, const wb_sample *in, wb_sample *out_l, wb_sample *out_r, uint32_t frames)`
- `wb_spatial_set_binaural(void *sp, int enable)`
- `wb_spatial_set_room(void *sp, float reverb_level, float room_size)`
- `wb_atmos_destroy(void *a)`
- `wb_atmos_set_position(void *a, int obj_id, float azimuth, float elevation, float distance)`
- `wb_atmos_set_object_gain(void *a, int obj_id, float gain)`
- `wb_atmos_get_object_count(const void *a)`
- `wb_score_note_to_staff(int midi_pitch, char *name_out, int cap, int *octave_out)`
- `wb_score_pitch_to_line(int midi_pitch)`
- `wb_score_staff_position(int midi_pitch)`
- `wb_score_render_measure(wb_note *notes, int note_count, char *text_out, int cap)`
- `wb_score_note_name(int midi_pitch, char *out, int cap)`
- `wb_score_is_accidental(int midi_pitch)`
- `wb_score_is_black_key(int midi_pitch)`
- `wb_session_add_chord(wb_session *s, double pos, int root, int type)`
- `wb_session_clear_chords(wb_session *s)`
- `wb_session_transform_notes(wb_session *s, int track, int clip, int mode)`
- `wb_session_update_offline(wb_session *s)`
- `wb_session_relink_bin(wb_session *s)`
- `wb_session_update_transitions(wb_session *s, struct wb_clip_edit_table *et)`
- `wb_tempo_detect(const wb_sample *audio, uint32_t frames, uint32_t sample_rate)`
- `wb_tempo_detect_confidence(void)`
- `wb_audio_to_midi_create(uint32_t sr)`
- `wb_audio_to_midi_destroy(wb_audio_to_midi *a)`
- `wb_audio_to_midi_set_threshold(wb_audio_to_midi *a, float threshold)`
- `wb_audio_to_midi_set_min_duration(wb_audio_to_midi *a, float min_dur_ms)`
- `wb_bg_render_poll(wb_bg_render *r, float *progress_out)`
- `wb_bg_render_wait(wb_bg_render *r, int timeout_ms)`
- `wb_bg_render_cancel(wb_bg_render *r)`
- `wb_bg_render_destroy(wb_bg_render *r)`
- `wb_melody_ai_set_range(void *ai, int min_note, int max_note)`
- `wb_chord_ai_destroy(wb_chord_ai *ai)`
- `wb_chord_ai_set_complexity(wb_chord_ai *ai, float complexity)`
- `wb_chord_ai_set_mood(wb_chord_ai *ai, int mood)`
- `wb_chord_ai_get_tension(const wb_chord_ai *ai)`
- `wb_session_create_link_group(wb_session *s, const int *track_indices, int num_tracks)`
- `wb_session_linked_move_clip(wb_session *s, int group_id, int track, int clip, double new_start)`
- `wb_session_linked_trim_clip(wb_session *s, int group_id, int track, int clip, double delta_head, double delta_tail)`
- `wb_session_get_link_group(const wb_session *s, int group_id, int *track_indices_out, int max_count)`
- `wb_restoration_denoise(const float *in, float *out, int n, float strength)`
- `wb_restoration_declick(const float *in, float *out, int n, float threshold)`
- `wb_restoration_declip(const float *in, float *out, int n, float threshold)`
- `wb_restoration_dehum(const float *in, float *out, int n, float hum_freq)`
- `wb_restoration_voice_isolate(const float *in, float *out, int n, float strength)`
- `wb_podcast_free(struct wb_podcast *pc)`
- `wb_podcast_set_voice_isolation_strength(void *pc, float strength)`
- `wb_podcast_process_voice(void *pc, const float *in, float *out, int n)`
- `wb_master_adv_destroy(void *m)`
- `wb_master_adv_set_input_gain(void *m, float db)`
- `wb_master_adv_set_output_gain(void *m, float db)`
- `wb_master_adv_set_loudness_target(void *m, float lufs)`
- `wb_master_adv_set_stereo_width(void *m, float width)`
- `wb_master_adv_set_bass_mono(void *m, int enable)`
- `wb_master_adv_get_loudness(const void *m)`
- `wb_master_adv_get_peak(const void *m)`
- `wb_pdc_destroy(wb_pdc *p)`
- `wb_pdc_set_latency(wb_pdc *p, int track, int samples)`
- `wb_pdc_get_delay(const wb_pdc *p, int track)`
- `wb_pdc_get_max_latency(const wb_pdc *p)`
- `wb_pdc_set_enabled(wb_pdc *p, int enabled)`
- `wb_pdc_is_enabled(const wb_pdc *p)`
- `wb_pdc_process(wb_pdc *p, wb_sample **buffers, int num_tracks, uint32_t frames)`
- `wb_cloud_save_project(const char *project_name, const wb_session *s)`
- `wb_cloud_list_projects(char **names_out, int max_count)`
- `wb_cloud_restore_version(const char *project_name, int version)`
- `wb_cloud_cleanup(const char *project_name, int keep_count)`
- `wb_subtitle_translate_init(wb_subtitle_translate *st, wb_session *session)`
- `wb_subtitle_translate_language(wb_subtitle_translate *st, const char *lang_code)`
- `wb_subtitle_translate_process(wb_subtitle_translate *st)`
- `wb_gpu_particle_render(void *ps, uint8_t *rgba_out, int w, int h)`
- `wb_stabilize2_process(void *s, uint8_t *frame_rgba, int width, int height)`
- `wb_stabilize2_set_crop(void *s, float crop_percent)`
- `wb_analysis_loudness(const float *audio, int n, float sr, float *lufs_out)`
- `wb_analysis_spectrum(const float *audio, int n, float *bins, int num_bins, float sr)`
- `wb_analysis_phase_correlation(const float *l, const float *r, int n, float *correlation)`

## `src/audio/bigmac/include/wbus/wbus_agent.h`

- `wb_agent_set_perf(struct wb_perf *p)`

## `src/audio/bigmac/include/wbus/wbus_agi.h`

- `wb_agi_destroy(wb_agi *a)`
- `wb_agi_tick(wb_agi *a, double dt)`

## `src/audio/bigmac/include/wbus/wbus_anim.h`

- `wb_anim_free(wb_anim *a)`
- `wb_anim_set_camera(wb_anim *a, float rx, float ry, float dist)`
- `wb_anim_parent(wb_anim *a, int child, int parent)`
- `wb_anim_render_frame(wb_anim *a, double t, uint8_t *out_rgba)`
- `wb_anim_get_rate(void)`
- `wb_anim_set_progress(void (*fn)(double frac, void *user), void *user)`
- `wb_anim_progress(double frac)`

## `src/audio/bigmac/include/wbus/wbus_bvh.h`

- `wb_bvh_duration(const wb_bvh *b)`
- `wb_bvh_sample(const wb_bvh *b, double t, float *frame_data)`

## `src/audio/bigmac/include/wbus/wbus_captions.h`

- `wb_captions_generate(wb_captions *c, const char *video_path)`
- `wb_captions_write_srt(const char *srt_path, const char *text, int duration_ms)`
- `wb_ass_extract_dialogue(const char *ass_path, wb_ass_line *out, int max)`
- `run_cmd(const char *cmd, const char *context)`

## `src/audio/bigmac/include/wbus/wbus_capture.h`

- `wb_caplog_destroy(wb_caplog *l)`
- `wb_caplog_count(const wb_caplog *l)`
- `wb_caplog_clear(wb_caplog *l)`
- `wb_launchrec_destroy(wb_launchrec *r)`
- `wb_launchrec_start(wb_launchrec *r, const struct wb_session *s)`
- `wb_launchrec_commit(wb_launchrec *r, struct wb_session *s)`

## `src/audio/bigmac/include/wbus/wbus_cgi.h`

- `wb_cgi_scene_get_zoom(const wb_cgi_scene *sc)`
- `wb_cgi_scene_tri_count(const wb_cgi_scene *sc)`

## `src/audio/bigmac/include/wbus/wbus_clap.h`

- `wb_clap_plugin_destroy(wb_clap_plugin *p)`
- `wb_clap_host_destroy(wb_clap_host *h)`

## `src/audio/bigmac/include/wbus/wbus_compositor.h`

- `wb_frame_free(wb_frame *f)`
- `wb_node_add_param_lane(wb_node *n, const char *name, wb_automation_lane *lane)`
- `wb_compositor_set_backend(wb_render_backend b)`
- `wb_compositor_get_backend(void)`
- `wb_frame_set_gpu(wb_frame *f, int gpu)`
- `wb_frame_get_gpu(const wb_frame *f)`
- `wb_compositor_set_quality(double q)`
- `wb_compositor_get_quality(void)`
- `wb_compositor_tile_size(void)`
- `wb_node_pull_request(wb_node *n, double t, int rx, int ry, int rw, int rh)`
- `wb_node_decode_is_requested(const wb_node *n)`
- `wb_node_decode_is_ready(const wb_node *n)`
- `wb_graph_auto_cache(wb_node *root, int max_frames)`
- `wb_node_get_kind(const wb_node *n)`
- `wb_node_effect_mesh_warp_set_stiffness(wb_node *n, float stiffness)`
- `wb_node_effect_mesh_warp_clear_pins(wb_node *n)`
- `wb_node_effect_motion_blur_set_shutter_angle(wb_node *n, float angle)`
- `wb_node_effect_motion_blur_set_shutter_phase(wb_node *n, float phase)`
- `wb_node_source_video_duration(const wb_node *n)`
- `wb_node_source_video_width(const wb_node *n)`
- `wb_node_source_video_height(const wb_node *n)`
- `wb_node_effect_text_animator_set_easing(wb_node *n, int ease_type)`
- `wb_transition_add(wb_node *trans, wb_node *child)`
- `wb_node_set_format(wb_node *n, int w, int h)`
- `wb_node_get_format(const wb_node *n, int *w, int *h)`
- `wb_node_destroy(wb_node *n)`
- `wb_node_graph_destroy(wb_node_graph *g)`
- `wb_node_graph_count(const wb_node_graph *g)`
- `wb_node_graph_input_of(const wb_node_graph *g, int i, int k)`
- `wb_node_graph_pos(const wb_node_graph *g, int i, float *x, float *y)`
- `wb_node_graph_param(const wb_node_graph *g, int i, double t)`
- `wb_compositor_metal_init(void)`
- `wb_compositor_metal_process_hdr(float *float_buf, int n_pixels, int mode)`
- `wb_stt_process_audio(wb_edit_graph *g, int track, int clip_idx)`
- `wb_stt_burn_subtitles(wb_edit_graph *g, int track, int clip_idx, const char *srt)`

## `src/audio/bigmac/include/wbus/wbus_compositor_pro.h`

- `wb_compositor_pro_add_node(void *c, int type)`
- `wb_comp_pro_set_scale(void *c, int node, float sx, float sy)`
- `wb_comp_pro_set_rotate(void *c, int node, float angle_deg)`
- `wb_comp_pro_set_contrast(void *c, int node, float c)`
- `wb_comp_pro_set_saturation(void *c, int node, float s)`
- `wb_compositor_pro_process(void *c, uint8_t *output_rgba)`

## `src/audio/bigmac/include/wbus/wbus_conv.h`

- `wb_conv_load_ir(wb_conv_inst *c, const float *ir_mono, int ir_len, int block_size)`
- `wb_conv_set_gain(wb_conv_inst *c, float gain)`

## `src/audio/bigmac/include/wbus/wbus_delivery.h`

- `wb_delivery_export_stems(wb_session *s, const char *dir)`
- `wb_delivery_chapters(const wb_session *s, char *buf, size_t cap)`

## `src/audio/bigmac/include/wbus/wbus_dsp.h`

- `wb_osc_reset(wb_osc *o)`
- `wb_osc_process(wb_osc *o, float inc, int waveform, float shape)`
- `wb_env_init(wb_env *e, float sr)`
- `wb_env_note_on(wb_env *e, float a, float d, float s, float r)`
- `wb_env_note_off(wb_env *e)`
- `wb_env_process(wb_env *e)`

## `src/audio/bigmac/include/wbus/wbus_edit.h`

- `wb_audio_mix(wb_edit_graph *g, float *buf, double start_time, int n_frames)`
- `wb_audio_mixer_set_channels(int channels)`
- `wb_edit_search_transcript(wb_edit_graph *g, const char *query)`
- `wb_edit_text_undo(wb_edit_graph *g)`

## `src/audio/bigmac/include/wbus/wbus_export_job.h`

- `wb_export_job_cancel(wb_export_job *j)`
- `wb_export_job_wait(wb_export_job *j)`
- `wb_export_job_reset(wb_export_job *j)`
- `wb_export_job_running(const wb_export_job *j)`

## `src/audio/bigmac/include/wbus/wbus_export_queue.h`

- `wb_export_queue_job_status(void *inst, int idx)`
- `wb_export_queue_job_progress(void *inst, int idx)`

## `src/audio/bigmac/include/wbus/wbus_fft.h`

- `wb_fft_destroy(wb_fft_plan *p)`
- `wb_fft_run(const wb_fft_plan *p, double *re, double *im, int invert)`
- `wb_fft_real(const wb_fft_plan *p, const double *x, double *re, double *im)`

## `src/audio/bigmac/include/wbus/wbus_hpss.h`

- `wb_hpss_destroy(wb_hpss *h)`

## `src/audio/bigmac/include/wbus/wbus_import.h`

- `wb_import_scan_dir(const char *dir, char out[][WB_IMPORT_PATH_MAX], int max)`

## `src/audio/bigmac/include/wbus/wbus_lufs.h`

- `wb_lufs_create(wb_lufs *l, double sr)`
- `wb_lufs_process(wb_lufs *l, const float *in, int n)`
- `wb_lufs_short_term_lufs(const wb_lufs *l)`
- `wb_lufs_integrated_lufs(const wb_lufs *l)`
- `wb_lufs_peak(const wb_lufs *l)`

## `src/audio/bigmac/include/wbus/wbus_mesh.h`

- `wb_mesh_free(wb_mesh *m)`
- `wb_mesh_vert_count(const wb_mesh *m)`
- `wb_mesh_tri_count(const wb_mesh *m)`
- `wb_mesh_emit(const wb_mesh *m, wb_rast_vertex *verts, wb_rast_tri *tris)`
- `wb_mesh_append(wb_mesh *dst, const wb_mesh *src)`
- `wb_mesh_write_obj(const wb_mesh *m, const char *obj_path)`
- `wb_mesh_morph_apply(wb_mesh *m, const wb_mesh *target, float t)`

## `src/audio/bigmac/include/wbus/wbus_midi.h`

- `wb_midi_enumerate(char (*names)[64], int max, int *out_count)`
- `wb_midi_close(wb_midi *m)`
- `wb_midi_open_output(wb_midi *m, const char *substr)`
- `wb_midi_capture(wb_midi *m, uint8_t *buf, int cap)`
- `wb_midi_capture_len(wb_midi *m)`
- `wb_midi_send(wb_midi *m, uint8_t status, uint8_t data1, uint8_t data2)`
- `wb_midi_send_sysex(wb_midi *m, const uint8_t *data, int len)`
- `wb_lp_mk2_note(int row, int col)`
- `wb_scale_contains(int scale_root, int scale_type, int note)`
- `wb_chord_tones(int scale_root, int scale_type, int mode, int out[8])`
- `wb_launchpad_classic_led(wb_midi *m, int row, int col, uint8_t color)`
- `wb_launchpad_classic_note(int row, int col)`
- `wb_launchpad_classic_clear(wb_midi *m)`
- `wb_lp_mk2_row_col_from_note(int note, int *row, int *col)`

## `src/audio/bigmac/include/wbus/wbus_midi_remote.h`

- `wb_midi_remote_process(wb_midi_remote *mr, int cc, float value)`

## `src/audio/bigmac/include/wbus/wbus_midifx.h`

- `wb_midifx_set_param(wb_midifx *m, int param, float value)`
- `wb_midifx_tick(wb_midifx *m, int ticks, wb_midifx_event *out, int maxout)`

## `src/audio/bigmac/include/wbus/wbus_mod.h`

- `wb_mod_free(wb_modstack *ms)`
- `wb_mod_add_mirror(wb_modstack *ms, int axis)`
- `wb_mod_add_wave(wb_modstack *ms, float amp, float waves)`
- `wb_mod_add_twist(wb_modstack *ms, float radians_total)`
- `wb_mod_add_solidify(wb_modstack *ms, float thickness)`
- `wb_mod_count(const wb_modstack *ms)`

## `src/audio/bigmac/include/wbus/wbus_ofx.h`

- `wb_ofx_plugin_free(wb_ofx_plugin *p)`
- `wb_ofx_plugin_set_param(wb_ofx_plugin *p, const char *name, double value)`

## `src/audio/bigmac/include/wbus/wbus_param_track.h`

- `wb_param_track_value_at(const wb_param_track *tr, double t)`
- `wb_param_track_set_extrapolate(wb_param_track *tr, int on)`

## `src/audio/bigmac/include/wbus/wbus_pattern.h`

- `wb_pattern_init(wb_pattern_state *ps, uint8_t pitch, double bpm)`
- `wb_pattern_set_step(wb_pattern_state *ps, int step, uint8_t vel)`
- `wb_pattern_clear_all(wb_pattern_state *ps)`

## `src/audio/bigmac/include/wbus/wbus_perf.h`

- `wb_perf_free(wb_perf *p)`
- `wb_perf_param(wb_perf *p, int deck, int which, float v)`
- `wb_perf_record_arm(wb_perf *p)`
- `wb_perf_record_stop(wb_perf *p)`
- `wb_perf_recording(const wb_perf *p)`
- `wb_perf_event_count(const wb_perf *p)`
- `wb_perf_deck_fired(const wb_perf *p, int deck)`
- `wb_perf_reset_for_replay(wb_perf *p)`
- `wb_perf_seek(wb_perf *p, double t)`
- `wb_perf_render_frame(wb_perf *p, uint8_t *out_rgba)`

## `src/audio/bigmac/include/wbus/wbus_perfclip.h`

- `wb_perfclip_free(wb_perfclip *pc)`

## `src/audio/bigmac/include/wbus/wbus_rast.h`

- `wb_rast_destroy(wb_rast_ctx *r)`
- `wb_rast_render(wb_rast_ctx *r, uint8_t *out_rgba)`
- `wb_rast_render_mt(wb_rast_ctx *r, uint8_t *out_rgba)`
- `wb_rast_set_zbuffer(wb_rast_ctx *r, int on)`

## `src/audio/bigmac/include/wbus/wbus_scenedesc.h`

- `wb_scenedesc_load(wb_anim *a, const char *path)`

## `src/audio/bigmac/include/wbus/wbus_sf2.h`

- `wb_sf2_free(wb_sf2 *s)`

## `src/audio/bigmac/include/wbus/wbus_sfx.h`

- `wb_sfx_explosion(float *out, int n, uint32_t sr, float dur)`
- `wb_sfx_pickup(float *out, int n, uint32_t sr)`
- `wb_sfx_powerup(float *out, int n, uint32_t sr, float dur)`

## `src/audio/bigmac/include/wbus/wbus_shadowbin.h`

- `wb_shadowbin_write(const wb_session *s, const char *path)`
- `wb_shadowbin_read(wb_session *s, const char *path)`
- `wb_shadowbin_path_for(const char *project_path, char *buf, size_t cap)`

## `src/audio/bigmac/include/wbus/wbus_sonogram.h`

- `wb_sonogram_process(wb_sonogram *sg, const float *audio, uint32_t frames)`
- `wb_sonogram_render(wb_sonogram *sg, uint8_t *rgba_out, int width, int height)`
- `wb_sonogram_get_spectral_centroid(const wb_sonogram *sg)`

## `src/audio/bigmac/include/wbus/wbus_spectral_fx.h`

- `wb_spectral_fx_set_param(void *sf, int param, float value)`
- `wb_spectral_fx_process(void *sf, wb_sample *out, const wb_sample *in, uint32_t frames)`

## `src/audio/bigmac/include/wbus/wbus_text_edit.h`

- `wb_text_edit_init(wb_text_edit *te, struct wb_session *session)`
- `wb_text_edit_delete_words(wb_text_edit *te, int start_word, int end_word)`
- `wb_text_edit_reorder_words(wb_text_edit *te, int word_index, int new_position)`
- `wb_text_edit_insert_pause(wb_text_edit *te, int word_index, float duration_sec)`
- `wb_text_edit_dead_air(wb_text_edit *te, float threshold_sec)`
- `wb_text_edit_um_ah_remove(wb_text_edit *te)`

## `src/audio/bigmac/include/wbus/wbus_transcript.h`

- `wb_transcript_free(wb_transcript *t)`
- `wb_transcript_word_at(const wb_transcript *t, double ms)`
- `wb_transcript_shift_from(wb_transcript *t, int start_idx, double delta_ms)`
- `wb_transcript_remove_range(wb_transcript *t, int i0, int i1)`
- `wb_transcript_write_srt(const wb_transcript *t, const char *srt_path)`
- `wb_transcript_parse_srt(wb_transcript *t, const char *srt_path)`

## `src/audio/bigmac/include/wbus/wbus_transitions_pro.h`

- `wb_transition_init(wb_transition *t, int type, int src_w, int src_h)`
- `wb_transition_set_param(wb_transition *t, int param, float value)`
- `wb_transition_destroy(wb_transition *t)`

## `src/audio/bigmac/include/wbus/wbus_tts.h`

- `wb_tts_destroy(wb_tts *t)`
- `wb_tts_speak_wav(wb_tts *t, const char *text, const char *wav_path)`
- `wb_tts_voice_count(wb_tts *t)`
- `wb_tts_set_voice(wb_tts *t, int index)`

## `src/audio/bigmac/include/wbus/wbus_vfx.h`

- `wb_lut3d_load(wb_lut3d *lut, const char *path)`
- `wb_lut3d_apply(const wb_lut3d *lut, uint8_t *rgba, int count)`

## `src/audio/bigmac/include/wbus/wbus_video.h`

- `wb_video_clip_init(wb_video_clip *c)`
- `wb_video_clip_free(wb_video_clip *c)`
- `wb_video_decoder_get_duration(wb_video_decoder *d)`
- `wb_video_decoder_get_width(wb_video_decoder *d)`
- `wb_video_decoder_get_height(wb_video_decoder *d)`
- `wb_session_split_video_clip(wb_session *s, int track, int clip, double split_pos)`
- `wb_session_export_edl(const wb_session *s, const char *edl_path)`
- `wb_session_export_fcpxml(const wb_session *s, const char *xml_path)`

## `src/audio/bigmac/include/wbus/wbus_voice_isolate.h`

- `wb_isolate_destroy(wb_isolate *iso)`
- `wb_isolate_process(wb_isolate *iso, const float *in, float *out, int frames)`

## `src/audio/bigmac/include/wbus/wbus_voice_polish.h`

- `wb_voice_polish_param_at(wb_voice_polish *vp, const char *param, double t)`
- `wb_voice_polish_set(wb_voice_polish *vp, const char *param, float value)`
- `wb_voice_polish_process(wb_voice_polish *vp, float *buf, uint32_t frames)`
- `wb_loudness_meter_destroy(wb_loudness_meter *m)`
- `wb_loudness_meter_reset(wb_loudness_meter *m)`
- `wb_loudness_meter_integrated(wb_loudness_meter *m)`
- `wb_loudness_meter_short_term(wb_loudness_meter *m)`

## `src/audio/bigmac/include/wbus/wbus_vst3.h`

- `wb_vst3_scan(const char* dir_path)`
- `wb_vst3_plugin_name(int i)`
- `wb_vst3_create(const char* name, uint32_t sample_rate)`
- `wb_vst3_get_param(void* inst, int param_index)`
- `wb_vst3_param_name(void* inst, int idx, char* out, int outsz)`
- `wb_vst3_set_param(void* inst, int param_index, float value)`
- `wb_vst3_slot_get(int track, int slot)`

## `src/audio/bigmac/include/wbus/wbus_warp.h`

- `wb_warp_add_marker(wb_warp *w, double src_sample, double dst_beat)`
- `wb_warp_src_to_dst(const wb_warp *w, double src_sample)`
- `wb_warp_dst_to_src(const wb_warp *w, double beat)`
- `wb_warp_auto_warp(wb_warp *w, const double *beat_positions, int num_beats)`

## `src/audio/bigmac/include/wbus/wbus_workspace.h`

- `wb_workspace_set(wb_workspace *ws, wb_workspace_tier t)`
- `wb_workspace_set_unlocked(wb_workspace *ws, wb_workspace_tier t, int on)`
- `wb_workspace_unlocked(wb_workspace *ws, wb_workspace_tier t)`
- `wb_workspace_audio_active(wb_workspace *ws)`
- `wb_workspace_video_active(wb_workspace *ws)`
- `wb_workspace_fusion_active(wb_workspace *ws)`
- `wb_workspace_cgi_active(wb_workspace *ws)`
- `wb_workspace_agi_active(wb_workspace *ws)`
- `wb_workspace_perf_active(wb_workspace *ws)`

## `src/audio/bigmac/tools/wb_dsp_simd.h`

- `vec_exp_4(_mm_mul_ps(db, scale))`

## `src/audio/glott/wubuglott_artic.h`

- `wg_artic_get(const wg_artic *a, int key)`
- `wg_artic_set(wg_artic *a, int key, double x)`
- `wg_artic_to_array(const wg_artic *a, double *out, int cap)`
- `wg_artic_from_array(wg_artic *a, const double *in, int n)`
- `wg_body_mix_resolve(wg_body_mix m)`

## `src/audio/glott/wubuglott_geom.h`

- `wg_v2_make(a.x / l, a.y / l)`
- `wg_v2_make(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t)`
- `wg_v2_make(c.x + r * cos(ang), c.y + r * sin(ang))`
- `wg_v2_make(c.x + dx * co - dy * s, c.y + dx * s + dy * co)`
- `wg_catmull_rom(const wg_v2 *pts, int n, int per, wg_v2 *out, int out_cap)`
- `wg_tangent_angle(wg_v2 c, double r, wg_v2 p, int upper, double *out_angle)`

## `src/audio/glott/wubuglott_glottis.h`

- `wg_glottis_init(wg_glottis *g, double fs)`
- `wg_glottis_reset(wg_glottis *g)`
- `wg_glottis_next(wg_glottis *g, double dp)`
- `wg_glottis_cycle_phase(const wg_glottis *g)`
- `wg_glottis_shape(double ph, double oq, double skew, double sharp)`

## `src/audio/glott/wubuglott_network.h`

- `wg_network_free(wg_network *net)`
- `wg_network_snap_to_targets(wg_network *net)`
- `wg_network_begin_block(wg_network *net, int samples)`

