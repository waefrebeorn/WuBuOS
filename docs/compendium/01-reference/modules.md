<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Modules
> Generated 2026-11-18 05:20 UTC -- recursive src/ walk, 3950 modules.

| Tree | Module | Lines | Depends on | Purpose |
|------|--------|------:|------------|---------|
| `apps/` | `app_canvas` | 383 | apps.h, stdio.h, stdlib.h, string.h, vbe.h, window_chrome.h, wm.h, wubu_canvas.h | app_canvas.c  --  WuBu Canvas: in-shell image editor binding Binds the real layered wubu_canvas engi |
| `apps/` | `app_explorer` | 84 | apps.h, explorer.h, stdlib.h, string.h, vbe.h, wm.h, wubu_theme.h | app_explorer.c  --  WuBuOS File Manager: in-shell Explorer binding Binds the real, Win98/XP-class ex |
| `apps/` | `apps` | 429 | apps.h, bonzi.h, calc.h, cmd.h, comfy.h, control.h, dirent.h, dos_window.h, edit | apps.c  --  Single App Registry Implementation ONE data-driven table (g_app_defs[]) is the source of |
| `apps/` | `apps_gap_test` | 87 | egojar.h, music.h, notes.h, stdio.h, stdlib.h, string.h, todo.h | apps_gap_test.c -- the completed desktop apps test (Notes / Todo / Music). These three apps were col |
| `apps/` | `apps_gap_test_stub` | 27 | stdlib.h, wm.h | apps_gap_test_stub.c -- the WM stubs for the apps-gap test. notes.c/todo.c/music.c bind DosGuiWindow |
| `apps/` | `apps_test` | 885 | apps.h, calc.h, control.h, editor.h, egojar.h, egojar_net.h, music.h, notepad.h, | apps_test.c  --  WuBuOS DosGui Apps Test Suite Tests for built-in Win98-style apps: - Task Manager ( |
| `apps/` | `apps_test_stubs` | 162 | apps.h, stdarg.h, stddef.h, stdint.h, wm.h, wubu_compat_db.h, wubu_notify.h, wub | apps_test_stubs.c -- no-op link support for apps_test. apps.c is the app dispatcher: it references m |
| `apps/` | `apps_weak_test` | 99 | apps.h, stdio.h, string.h | apps_weak_test.c -- the registry in a link WITHOUT VWS. launch_vws_designer is declared weak because |
| `apps/` | `apps_wet` | 142 | apps.h, dirent.h, egojar.h, stdbool.h, stdio.h, stdlib.h, string.h | apps_wet.c -- discovering programs that are files. A program is a FILE: the applet directory is the  |
| `apps/` | `bonzi` | 236 | apps.h, bonzi.h, ctype.h, stdio.h, stdlib.h, string.h, vbe.h, window_chrome.h, w | bonzi.c -- Bonzi Buddy: desktop AGI agent persona (WuBuOS human interface). Real windowed agent: lau |
| `apps/` | `calc` | 385 | calc.h, calc_internal.h, math.h, stdio.h, stdlib.h, string.h, vbe.h, window_chro | calc.c  --  Calculator App (Standard / Scientific / Programmer / Graphing) Real windowed engine: lau |
| `apps/` | `calc_math` | 46 | calc.h, math.h | calc_math.c -- Calculator math evaluation (self-contained). calc_apply_op / calc_apply_func: pure do |
| `apps/` | `calc_test` | 140 | calc.h, math.h, stdio.h, stdlib.h, string.h | calc_test.c  --  WuBuOS Calculator behavioral tests Exercises the real calculation engine (no GUI re |
| `apps/` | `calc_test_support` | 53 | calc.h, stdlib.h, wm.h, wubu_theme.h | calc_test_support.c -- headless link support for the calculator unit test. The calculator's draw pat |
| `apps/` | `canvas_standalone` | 104 | egojar.h, input.h, stdbool.h, stdio.h, stdlib.h, string.h, vbe.h, wm.h, wubu_can | canvas_standalone.c -- standalone Linux entry point for the WuBuOS image editor (the real, layered w |
| `apps/` | `cmd` | 353 | cmd.h, egojar.h, pty.h, stdio.h, stdlib.h, string.h, vbe.h, window_chrome.h, wm. | Scrollback screen: the terminal is a real pty shell, so it must show what the shell prints, not just |
| `apps/` | `cmd_test` | 80 | cmd.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h | cmd_test.c -- Real test for the CMD terminal engine (apps/cmd/cmd.c). Proves the engine does real wo |
| `apps/` | `cmd_test_stub` | 42 | stdint.h, string.h | cmd_test_stub.c -- Minimal stubs for the CMD terminal test. The test only exercises pty spawn/read/h |
| `apps/` | `comfy` | 390 | bonzi.h, comfy.h, math.h, stdio.h, stdlib.h, string.h, vbe.h, window_chrome.h, w | comfy.c -- Comfy: node-graph visual scripting for the AGI operator. Real node-graph engine: nodes ha |
| `apps/` | `control` | 187 | control.h, stdlib.h, string.h, vbe.h, window_chrome.h, wm.h, wm_internal.h, wubu | control.c  --  Control Panel (9 tabs) - minimal stub |
| `apps/` | `control_test` | 164 | control.h, stdio.h, stdlib.h, string.h, vbe.h, wm.h, wm_internal.h, wubu_setting | control_test.c  --  WuBuOS Control Panel Test Suite Cell 395: Win98-style settings panel. Verifies t |
| `apps/` | `editor` | 285 | editor.h, stdlib.h, string.h, vbe.h, window_chrome.h, wm.h, wm_internal.h, wubu_ | editor.c  --  GUI adapter over the real WubuEditor engine Before this file was rewritten, "Editor" i |
| `apps/` | `edr_dash` | 240 | stdio.h, stdlib.h, string.h, vbe.h, window_chrome.h, wm.h, wm_internal.h, wubu_e | edr_dash.c -- EDR Activity Dashboard (the disclosure surface) This is the user-facing half of WuBuOS |
| `apps/` | `first_run_test` | 171 | apps.h, getting_started.h, stdio.h, string.h, wubu_shell_greeter.h, wubu_walkthr | first_run_test.c -- the first-boot surface. A new user boots into a desktop with no map.  The greete |
| `apps/` | `getting_started` | 20 | getting_started.h, wubu_walkthrough.h | getting_started.c -- the "hello world" path. This used to be a plain text document in a window. It i |
| `apps/` | `music` | 192 | dirent.h, egojar.h, egojar_net.h, music.h, stdio.h, stdlib.h, string.h, vbe.h, w | music.c  --  My Seed Music (a real player, the synthesized engine). A genuine music player inside a  |
| `apps/` | `notepad` | 396 | guest_file.h, notepad.h, stdlib.h, string.h, vbe.h, window_chrome.h, wm.h, wm_in | notepad.c  --  My Seed Notepad (Win98-style text editor, real engine) Genuine multi-line text editin |
| `apps/` | `notes` | 212 | dirent.h, egojar.h, egojar_net.h, notes.h, stdio.h, stdlib.h, string.h, vbe.h, w | notes.c  --  My Seed Notes (a real notes app, persisted). Genuine note management inside a DosGui wi |
| `apps/` | `regedit` | 136 | regedit.h, stdlib.h, string.h, vbe.h, wm.h, wubu_theme.h | regedit.c  --  Windows Registry Editor Clone - minimal stub |
| `apps/` | `repl` | 152 | gui_dbuf.h, holyd.h, repl.h, stdio.h, stdlib.h, string.h, vbe.h, window_chrome.h | repl.c  --  My Seed HolyC JIT REPL (runs inside GUI window) Uses the HolyC compiler (hd_eval) for ev |
| `apps/` | `tandem` | 186 | stdio.h, stdlib.h, string.h, tandem.h, vbe.h, wm.h, wm_internal.h, wubu_bonzi_st | tandem.c -- Tandem: the user+AGI shared desktop (the recursive learning loop made human). A real WuB |
| `apps/` | `test_tandem` | 68 | stdio.h, string.h, tandem.h, wubu_bonzi_study.h, wubu_psych.h | test_tandem.c -- the Tandem headless test: the user+AGI loop. Exercises the tandem timing loop (tick |
| `apps/` | `taskmgr` | 73 | apps.h, cmd.h, egojar.h, stdio.h, stdlib.h, string.h, taskmgr.h, wm.h | taskmgr.c -- the Task Manager entry point. The Task Manager IS the wubulens project (Waefrebeorn/wub |
| `apps/` | `todo` | 199 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, todo.h, vbe.h, window_chrom | todo.c  --  My Seed Todo (a real todo list, persisted). Genuine task management: a list loaded from  |
| `apps/` | `tutorial_test` | 171 | stddef.h, stdint.h, stdio.h, string.h, wubu_tutorial.h | tutorial_test.c -- the tutorial host, actually running the shipped scripts. A tutorial that does not |
| `apps/` | `vws_designer` | 129 | stdio.h, stdlib.h, string.h, vws_lua.h, wet.h, wet_lua.h, wm.h | vws_designer.c -- launch the VWS form designer as a WuBuOS window. The designer is a .wet applet wri |
| `apps/` | `vws_designer_test` | 147 | stdio.h, string.h, vws_designer.h, vws_lua.h, wet.h | vws_designer_test.c -- launch the VWS designer the way the shell does. The claim under test is the c |
| `apps/` | `wet_applet` | 274 | stdio.h, stdlib.h, string.h, wb_lv.h, wet.h, wet_applet.h, wet_lua.h, wubu_effec | wet_applet.c -- launch a .wet applet as a WuBuOS window. The framework owns the widget tree, the lay |
| `apps/` | `wet_applet_test` | 354 | stdio.h, stdlib.h, string.h, vws_templates.h, wet.h, wet_applet.h, wet_lua.h, wi | wet_applet_test.c -- behaviour gates for the .wet applet bridge. Proves the seam: a .wet script load |
| `apps/` | `wubu_apps2_test` | 356 | egojar.h, stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_codec.h, wubu_editor. | wubu_apps_test.c  --  Tests for Editor, Canvas, and Codec Cell 396/397/398: Notepad++ editor, Photos |
| `apps/` | `wubu_canvas_blend` | 52 | wubu_canvas.h | wubu_canvas_blend.c -- Canvas blend-compositing subsystem (self-contained). blend_channel + wubu_ble |
| `apps/` | `wubu_canvas_draw` | 226 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_canvas_internal.h | wubu_canvas_draw.c -- WuBuOS canvas: pixel drawing tools, color pick, and selection. Self-contained: |
| `apps/` | `wubu_canvas_filter` | 128 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_filter.c -- WuBuOS canvas: per-layer pixel filters (blur, sharpen, edge, invert, thresho |
| `apps/` | `wubu_canvas_io` | 658 | egojar.h, stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_image_codec_internal. | wubu_canvas_io.c  --  WuBuOS Image Editor: File I/O backend Cell 397 (I/O split 2026-07-09): PNG/BMP |
| `apps/` | `wubu_canvas_io_ppm` | 81 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_io_ppm.c -- Canvas PPM format save/load (self-contained). wubu_cv_save_ppm (P6 binary) + |
| `apps/` | `wubu_canvas_layers` | 191 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_layers.c -- WuBuOS canvas: create/destroy + layer ops + compositing. Self-contained: dep |
| `apps/` | `wubu_canvas_plugin` | 40 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_plugin.c -- WuBuOS canvas: plugin registration / run / unregister. Self-contained: depen |
| `apps/` | `wubu_canvas_transform` | 171 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_transform.c -- WuBuOS canvas: viewport (zoom/pan) and geometric layer transforms (resize |
| `apps/` | `wubu_canvas_undo` | 114 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_canvas_internal.h | wubu_canvas_undo.c -- WuBuOS canvas: undo/redo history + snapshot hook. Self-contained: owns the und |
| `apps/` | `wubu_codec` | 493 | egojar.h, egojar_net.h, errno.h, stdio.h, stdlib.h, string.h, wubu_codec.h, wubu | wubu_codec.c  --  WuBuOS Codec Layer Implementation Cell 398: FFmpeg CLI wrapper + optional libav li |
| `apps/` | `wubu_editor` | 468 | stdio.h, stdlib.h, string.h, wubu_editor.h | wubu_editor.c  --  WuBuOS Code Editor Implementation Cell 396: Tabbed editor with syntax HL, find/re |
| `apps/` | `wubu_editor_bookmark` | 55 | wubu_editor.h | wubu_editor_bookmark.c -- Editor bookmark subsystem (self-contained). wubu_ed_bookmark_toggle/next/p |
| `apps/` | `wubu_editor_find` | 136 | string.h, wubu_editor.h | wubu_editor_find.c -- WuBuOS editor find/replace (split from wubu_editor.c) Self-contained subsystem |
| `apps/` | `wubu_editor_macro` | 42 | stdlib.h, string.h, wubu_editor.h | wubu_editor_macro.c -- WuBuOS editor: keyboard macro record/playback. Extracted from wubu_editor.c ( |
| `apps/` | `wubu_editor_selection` | 146 | stdlib.h, string.h, wubu_editor.h | wubu_editor_selection.c -- WuBuOS editor selection + clipboard (split from wubu_editor.c) Self-conta |
| `apps/` | `wubu_editor_undo` | 130 | stdlib.h, string.h, wubu_editor.h | wubu_editor_undo.c -- WuBuOS editor undo/redo engine (split from wubu_editor.c) Self-contained subsy |
| `apps/` | `wubu_image_codec` | 110 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_image_codec_internal.h | wubu_image_codec.c -- Self-contained image-codec leaf for WuBuOS canvas. Pure, dependency-free codec |
| `apps/` | `wubu_petdex` | 139 | stdio.h, stdlib.h, string.h, time.h, vbe.h, window_chrome.h, wm.h, wubu_petdex.h | wubu_petdex.c -- the petdex, as a window. WuBuOS's pets are deterministic pixel art (tools/pet_gen.p |
| `apps/` | `wubu_tutorial` | 246 | apps.h, stdio.h, stdlib.h, string.h, wb_lv.h, wet.h, wet_lua.h, wubu_tutorial.h | wubu_tutorial.c -- the tutorial host. A tutorial is a .wet applet written in Lua, so a tutorial abou |
| `apps/` | `wubu_walkthrough` | 223 | stdio.h, string.h, vbe.h, window_chrome.h, wm.h, wubu_walkthrough_art.h | wubu_walkthrough.c -- the first-run walkthrough, with the official art. getting_started.c is a docum |
| `apps/` | `cell_csv` | 151 | cell_csv.h, ctype.h, stdio.h, stdlib.h, string.h | cell_csv.c -- CSV / TSV import & export for the wubucell spreadsheet model. See cell_csv.h. RFC 4180 |
| `apps/` | `cell_eval` | 138 | cell_internal.h, value_util.h | WuBuOffice -- apps/wubucell/cell_eval Formula evaluation: the wubucell_book is the resolver context. |
| `apps/` | `cell_io` | 195 | cell_internal.h | WuBuOffice -- apps/wubucell/cell_io Package assembly (wubucell_assemble) and teardown (wubucell_free |
| `apps/` | `cell_model` | 366 | cell_internal.h | WuBuOffice -- apps/wubucell/cell_model Workbook/sheet/cell model + XML rendering for worksheets and  |
| `apps/` | `cell_read` | 332 | cell_internal.h, cell_read.h, ctype.h, parser.h, reader.h, stdio.h, stdlib.h, st | cell_read.c -- read an .xlsx into a wubucell_book. SAX over wubuxml_parse for xl/sharedStrings.xml ( |
| `apps/` | `entry` | 2 |  |  |
| `apps/` | `main` | 46 | cell.h, stdio.h, stdlib.h, style.h | bar chart of the costs |
| `apps/` | `style` | 157 | stdlib.h, string.h, style.h | custom numFmtIds start at 164 |
| `apps/` | `conv_bridge` | 301 | conv_bridge.h, stdio.h, stdlib.h, string.h | conv_bridge.c -- cross-family model transforms for WuBuOffice convert. See conv_bridge.h. Clean-room |
| `apps/` | `conv_map` | 456 | assemble.h, cell.h, cell_csv.h, cell_read.h, conv_bridge.h, conv_map.h, doc_md.h | conv_map.c -- unified format conversion. See conv_map.h. Clean-room C11. Bridges every supported for |
| `apps/` | `model_from_json` | 149 | json.h, model_from_json.h, stdio.h, stdlib.h, string.h | model_from_json.c -- inverse of model_json.c. See model_from_json.h. Clean-room C11; reuses wubujson |
| `apps/` | `agent_main` | 20 | json.h, stdio.h, stdlib.h, string.h, wubudoc.h | wubudoc_agent main -- wubuOS unified document bus. NDJSON protocol over stdin/stdout. Ingest ANY sup |
| `apps/` | `doc_md` | 300 | ctype.h, doc_md.h, stdio.h, stdlib.h, string.h | doc_md.c -- Markdown & HTML serializers for the WuBuOffice document model. See doc_md.h. Clean-room  |
| `apps/` | `doc_rtf` | 91 | doc_rtf.h, stdio.h, string.h | doc_rtf.c -- Rich Text Format (RTF 1.x) writer. See doc_rtf.h. Clean-room C11, no third-party code. |
| `apps/` | `epub` | 196 | doc_md.h, epub.h, stdio.h, stdlib.h, string.h, zip.h | epub.c -- EPUB 3 writer over dm_doc. See epub.h. Clean-room C11 over wubuzip + wubudoc's shared HTML |
| `apps/` | `model_json` | 124 | model_json.h, stdio.h, string.h | model_json.c -- JSON serializers for the three WuBuOffice models. See model_json.h. Clean-room C11,  |
| `apps/` | `docmodel` | 250 | docmodel.h, parser.h, stdlib.h, string.h | docmodel.c — reconstruct a WordprocessingML document model from word/document.xml so wubuedit can re |
| `apps/` | `edit` | 124 | assemble.h, cell.h, cell_read.h, docmodel.h, docx_text.h, edit.h, reader.h, show | Structure-preserving round-trip for each supported format: parse the document part into our model, t |
| `apps/` | `entry` | 2 |  |  |
| `apps/` | `main` | 51 | stdio.h, stdlib.h, string.h, woff.h, wubufont.h | wubufont main -- sfnt (TTF/OTF) -> SVG <font> document. Usage: wubufont_cli <in.ttf> [sample] > out. |
| `apps/` | `main` | 272 | fontbank.h, fontdir.h, gauntlet.h, latin1.h, page_compose.h, stdio.h, stdlib.h,  | wubugauntlet_cli -- OCR robustness battery for the multi-font bank. Usage: wubugauntlet_cli [--ppm N |
| `apps/` | `doc_bin` | 184 | cfb.h, legacy.h, legacy_internal.h, string.h | doc_bin.c -- legacy Word (.doc) reader -> dm_doc. A .doc is a CFB container with a "WordDocument" st |
| `apps/` | `legacy_doc_write` | 167 | cfb_write.h, docmodel.h, legacy.h, legacy_internal.h, stdio.h, stdlib.h, string. | legacy_doc_write.c -- encode a legacy .doc (Word binary) from dm_doc. Produces a Word-97 (.doc) file |
| `apps/` | `legacy_ppt_write` | 127 | cfb_write.h, legacy.h, legacy_internal.h, show.h, stdio.h, stdlib.h, string.h | legacy_ppt_write.c -- encode a legacy .ppt (PowerPoint binary) from wubushow_pres. Produces a minima |
| `apps/` | `legacy_xls_write` | 206 | cell.h, cfb_write.h, legacy.h, legacy_internal.h, stdio.h, stdlib.h, string.h | legacy_xls_write.c -- encode a BIFF8 (.xls) workbook from wubucell_book. Strategy: emit a minimal bu |
| `apps/` | `ppt_bin` | 182 | cfb.h, legacy.h, legacy_internal.h, string.h | ppt_bin.c -- legacy PowerPoint (.ppt) reader -> wubushow_pres. A .ppt is a CFB container with a "Pow |
| `apps/` | `xls_biff` | 308 | cfb.h, legacy.h, legacy_internal.h, string.h | xls_biff.c -- legacy Excel (.xls, BIFF8) reader -> wubucell_book. A .xls is a CFB container with a " |
| `apps/` | `entry` | 2 |  |  |
| `apps/` | `main` | 182 | draw.h, input.h, note_controller.h, screen.h, stdio.h, stdlib.h, string.h, term. | wubunote -- native tabbed terminal text editor (Notepad++-class). Open any UTF-8/text file, edit it  |
| `apps/` | `note_controller` | 245 | draw.h, note_controller.h, stdio.h, stdlib.h, string.h | note_controller.c -- pure wubunote interaction logic. |
| `apps/` | `main` | 115 | fontbank.h, recognize.h, stdio.h, stdlib.h, string.h, wubufont.h, wubuocr.h | wubuocr main -- image (Netpbm) -> structured document JSON. The image-ingestion front-end for wubuOS |
| `apps/` | `odf_body` | 175 | odf_body.h, stdio.h, stdlib.h, string.h | odf_body.c -- shared ODF content-body emitters. See odf_body.h. Clean-room C11. Single source of tru |
| `apps/` | `odf_flat` | 106 | odf.h, odf_body.h, stdio.h, stdlib.h, string.h | odf_flat.c -- Flat OpenDocument (.fodt / .fods / .fodp) writer + reader. Flat ODF is a single, self- |
| `apps/` | `odf_pkg` | 70 | odf.h, stdio.h, stdlib.h, string.h, zip.h | odf_pkg.c -- shared ODF package assembler. See odf.h. Clean-room C11 over the raw wubuzip writer. |
| `apps/` | `odf_pres` | 122 | odf.h, odf_body.h, parser.h, reader.h, stdio.h, stdlib.h, string.h | odf_pres.c -- OpenDocument Presentation (.odp) writer + reader. See odf.h. Clean-room C11. Model: wu |
| `apps/` | `odf_sheet` | 138 | odf.h, odf_body.h, parser.h, reader.h, stdio.h, stdlib.h, string.h | odf_sheet.c -- OpenDocument Spreadsheet (.ods) writer + reader. See odf.h. Clean-room C11. Model: wu |
| `apps/` | `odf_text` | 184 | odf.h, odf_body.h, parser.h, reader.h, stdio.h, stdlib.h, string.h | odf_text.c -- OpenDocument Text (.odt) writer + reader. See odf.h. Clean-room C11. Model: dm_doc. Bo |
| `apps/` | `main` | 52 | stdio.h, stdlib.h, string.h | shift argv so subcommands see their own args |
| `apps/` | `wubuoffice` | 41 | errno.h, stdio.h, stdlib.h, string.h, unistd.h | wubuoffice.c -- unified launcher (UI parity: one name for the whole suite). The unified GUI shell is |
| `apps/` | `autocomp` | 94 | autocomp.h, ctype.h, doc.h, stdlib.h, string.h | autocomp.c -- opaque auto-completion engine (see autocomp.h). Ported out of view_editor.c so the edi |
| `apps/` | `bkmk` | 54 | bkmk.h, stdlib.h, string.h | bkmk.c -- opaque line-bookmark set (see bkmk.h). Ported out of view_editor.c so the editor no longer |
| `apps/` | `codefold` | 52 | codefold.h, doc.h, lex.h, stdlib.h, string.h | codefold.c -- opaque code-folding + function-list state (see codefold.h). Ported out of view_editor. |
| `apps/` | `dialog` | 68 | dialog.h, stdio.h, stdlib.h, string.h | dialog.c -- opaque modal text-input dialog (see dialog.h). |
| `apps/` | `doccmd_core` | 616 | a11y.h, aislot.h, caption.h, chart.h, cite.h, col.h, crdt.h, csv.h, doccmd.h, dr | doccmd.c -- opaque document-editing command module (see doccmd.h). Ported out of view_doc.c so the D |
| `apps/` | `doccmd_wave1` | 433 | a11y.h, aislot.h, caption.h, cite.h, col.h, crdt.h, csv.h, doccmd.h, dyslexia.h, | doccmd_col_demo -- append/resolve a thread on the comment store. Used to exercise the wubucol engine |
| `apps/` | `doccmd_wave2` | 424 | doccmd.h, stdio.h, stdlib.h, string.h, wubu3d.h, wubuanimation.h, wububasic.h, w | shared cell accessor for string-array rows (sort/filter) |
| `apps/` | `findbar` | 172 | doc.h, findbar.h, regex.h, search.h, stdlib.h, string.h | findbar.c -- opaque find/replace engine (see findbar.h). Ported out of view_editor.c so the editor v |
| `apps/` | `gotoline` | 52 | gotoline.h, stdlib.h, string.h | gotoline.c -- opaque "Go to line" mini-prompt (see gotoline.h). Ported out of view_editor.c; the edi |
| `apps/` | `hive` | 225 | hive.h, json.h, stdio.h, stdlib.h, string.h | hive.c — data-driven UI configuration ("the hive"). Loads a declarative JSON template (toolbar / men |
| `apps/` | `macro` | 106 | macro.h, stdio.h, stdlib.h, string.h | macro.c -- opaque macro record/playback engine (see macro.h). Ported out of view_editor.c; the edito |
| `apps/` | `main` | 648 | SDL.h, dialog.h, hive.h, macro.h, palette.h, pasteplain.h, plugin.h, settings.h, | wubuos -- unified WuBuOffice GUI shell (the "whole suite + Notepad++" front end). One SDL2 window ho |
| `apps/` | `palette` | 118 | ctype.h, palette.h, stdlib.h, string.h | palette.c -- UI-29 command palette logic. Case-insensitive subsequence match ("fzf-lite"): every que |
| `apps/` | `plugin` | 163 | dirent.h, dlfcn.h, plugin.h, stdio.h, stdlib.h, string.h | plugin.c -- wubuos plugin loader implementation. Portable dlopen-based loader. No Windows support is |
| `apps/` | `badabi_plugin` | 20 | stdlib.h, wuos_plugin.h | badabi_plugin.c -- negative-test plugin: reports a WRONG ABI version so the loader must reject it. N |
| `apps/` | `sample_plugin` | 49 | stdlib.h, string.h, wuos_plugin.h | sample_plugin.c -- reference wubuos plugin. Builds to sample_plugin.so. Demonstrates the full ABI: i |
| `apps/` | `test_autocomp` | 74 | autocomp.h, doc.h, stdio.h, stdlib.h, string.h | test_autocomp.c -- headless unit test for the extracted opaque AutoComp engine (no GUI/SDL). Verifie |
| `apps/` | `test_bkmk` | 48 | assert.h, bkmk.h, stdio.h | test_bkmk.c -- headless unit test for the extracted opaque BkMk engine. Pure logic (no Doc/GUI): tog |
| `apps/` | `test_codefold` | 56 | codefold.h, doc.h, lex.h, stdio.h, string.h | test_codefold.c -- headless unit test for the extracted opaque CodeFold engine. Verifies fold toggle |
| `apps/` | `test_dialog` | 54 | dialog.h, stdio.h, string.h | test_dialog.c -- headless unit test for the opaque Dialog module. |
| `apps/` | `test_doccmd` | 461 | doccmd.h, model.h, stdio.h, stdlib.h, string.h | test_doccmd.c -- headless unit test for the extracted opaque doccmd module. Verifies structural inse |
| `apps/` | `test_findbar` | 84 | assert.h, doc.h, findbar.h, stdio.h, stdlib.h, string.h | test_findbar.c -- headless unit test for the extracted opaque FindBar engine. Verifies find/replace  |
| `apps/` | `test_font` | 53 | stdio.h, stdlib.h, string.h, wuos_font.h | test_font.c -- headless unit test for the INT-15 font enumeration/picker. Verifies FreeType init + f |
| `apps/` | `test_gotoline` | 71 | assert.h, gotoline.h, stdio.h, string.h | test_gotoline.c -- headless unit test for the extracted opaque GotoLine engine. Verifies open/active |
| `apps/` | `test_hive` | 83 | hive.h, stdio.h, stdlib.h, string.h | test_hive.c — verify the data-driven hive template parses and exposes the expected toolbar / menus / |
| `apps/` | `test_macro` | 81 | macro.h, stdio.h, string.h | test_macro.c -- headless unit test for the extracted opaque Macro engine. Verifies record (chars/ret |
| `apps/` | `test_motion` | 39 | math.h, stdio.h, wuos_motion.h | test_motion.c — easing + tween correctness. |
| `apps/` | `test_plugin_abi` | 46 | dlfcn.h, plugin.h, stdio.h, stdlib.h, string.h, wuos_plugin.h | test_plugin_abi.c -- headless verification of the wubuos plugin C ABI. Exercises both the happy path |
| `apps/` | `test_shell_ui` | 72 | palette.h, stdio.h, string.h, toast.h | test_shell_ui.c -- headless checks for the shell's pure-logic UI modules: toast queue (UI-33) and co |
| `apps/` | `test_view` | 108 | autosave.h, cell.h, model.h, palette.h, settings.h, stdio.h, stdlib.h, string.h, | Headless smoke test: create + render every view (real engines, no GUI). Also exercises file open (do |
| `apps/` | `test_view_cell` | 170 | cell.h, model.h, palette.h, settings.h, stdio.h, stdlib.h, string.h, toc.h, wuos | test_view_cell.c -- interactive spreadsheet checks: navigation, editing, CSV/XLSX round-trips, refer |
| `apps/` | `test_view_docfile` | 131 | model.h, palette.h, settings.h, stdio.h, stdlib.h, string.h, toc.h, unistd.h, wu | test_view_docfile.c -- markdown -> Document view round-trip: sidebar structure, wubuscript fields, s |
| `apps/` | `test_view_editor` | 487 | autosave.h, cell.h, model.h, palette.h, settings.h, stdio.h, stdlib.h, string.h, | test_view_editor.c -- file-open round-trips (markdown -> Document, code -> Editor), settings view, a |
| `apps/` | `test_view_ocr_doc` | 135 | model.h, palette.h, settings.h, stdio.h, stdlib.h, string.h, toc.h, wuos.h | test_view_ocr_doc.c -- OCR view interactivity, document-view open+find, plugin ABI host check. Split |
| `apps/` | `test_wuos_toolbar` | 80 | hive.h, stdio.h, string.h, wuos_font.h, wuos_toolbar.h | test_wuos_toolbar.c -- headless checks for the shell's formatting toolbar (wuos_toolbar). Verifies t |
| `apps/` | `toast` | 45 | stdlib.h, string.h, toast.h | toast.c -- UI-33 toast queue. FIFO of copied strings with per-message TTL. |
| `apps/` | `view_cell` | 569 | cell.h, cell_csv.h, cell_internal.h, cell_read.h, ctype.h, stdio.h, stdlib.h, st | view_cell.c -- Spreadsheet cell view: renders a real wubucell workbook grid AND is genuinely interac |
| `apps/` | `view_compare` | 140 | diff.h, stdio.h, stdlib.h, string.h, wuos.h, wuos_file.h, wuos_font.h, wuos_them | view_compare.c -- Compare view: diffs two files via WuBuPad's DONE Myers LCS engine (src/diff) and r |
| `apps/` | `view_doc` | 423 | a11y.h, chart.h, ctype.h, doccmd.h, draw.h, math.h, model.h, rast.h, script.h, s | view_doc.c -- Document view: renders a wubumodel page via wurender AND is interactive. Given a path  |
| `apps/` | `view_doc_render` | 523 | model.h, rast.h, settings.h, shape.h, stdio.h, stdlib.h, string.h, ublayout.h, v | view_doc_render.c -- document view rendering (page layout, objects, link boxes, TOC pane), split fro |
| `apps/` | `view_editor` | 454 | SDL.h, autocomp.h, autosave.h, bkmk.h, codefold.h, ctype.h, doc.h, docs.h, encod | view_editor.c -- Notepad++-parity editor view (WuBuPad core + lexer). Hosts the REAL WuBuPad documen |
| `apps/` | `view_editor_keys` | 300 | SDL.h, autocomp.h, doc.h, findbar.h, gotoline.h, lex.h, macro.h, settings.h, std | view_editor_keys.c -- keyboard handling for the editor view, split from view_editor.c. Macro playbac |
| `apps/` | `view_editor_render` | 449 | SDL.h, codefold.h, doc.h, findbar.h, lex.h, settings.h, spell.h, stdio.h, stdlib | view_editor_render.c -- editor view rendering (text, caret, line numbers, token coloring) split from |
| `apps/` | `view_editor_textops` | 163 | bkmk.h, codefold.h, doc.h, docs.h, encode.h, lex.h, stdio.h, stdlib.h, string.h, | view_editor_textops.c -- editor text/session operations split from view_editor.c (no monoliths): EOL |
| `apps/` | `view_ocr` | 316 | fontbank.h, image.h, page_compose.h, png.h, stdio.h, stdlib.h, string.h, wubufon | view_ocr.c -- OCR view: runs the real wubuocr pipeline and is interactive. The recognized blocks are |
| `apps/` | `view_settings` | 160 | settings.h, stdio.h, stdlib.h, string.h, wuos.h, wuos_font.h, wuos_theme.h | view_settings.c -- Settings view (UI-25): a real, interactive preferences surface (not a stub). It e |
| `apps/` | `view_slide` | 301 | hive.h, pptx_write.h, stdio.h, stdlib.h, string.h, wuos.h, wuos_font.h, wuos_the | view_slide.c -- Slide view: an EDITABLE, SAVABLE presentation slide (title + bullets + a bar chart). |
| `apps/` | `viewshot` | 221 | SDL.h, dialog.h, macro.h, palette.h, plugin.h, settings.h, shape.h, stdio.h, std | viewshot.c -- headless WuBuOffice (wubuos) screenshot tool. Composes the SAME full window the live s |
| `apps/` | `wuos_file` | 31 | stdio.h, stdlib.h, string.h, wuos_file.h | wuos_file.c -- minimal file read/write helpers for the GUI shell. |
| `apps/` | `wuos_font` | 309 | dirent.h, ft2build.h, math.h, stat.h, stdio.h, stdlib.h, string.h, strings.h, wu | wuos_font.c -- shared FreeType text raster helper (see wuos_font.h). |
| `apps/` | `wuos_frame` | 560 | SDL.h, dialog.h, hive.h, layout.h, macro.h, palette.h, settings.h, stdio.h, stdl | wuos_frame.c -- one full frame of the wubuos shell: tab strip, menu bar, toolbar, sidebar, active vi |
| `apps/` | `wuos_motion` | 45 | wuos_motion.h | wuos_motion.c — easing + tween implementation. |
| `apps/` | `wuos_shell_a11y` | 138 | hive.h, stdio.h, stdlib.h, string.h, wuos.h, wuos_shell_internal.h, wuos_toolbar | wuos_shell_a11y.c -- H4: shell accessibility tree over JSON. Emits the ENTIRE shell UI as structured |
| `apps/` | `wuos_shell_events` | 301 | dialog.h, pasteplain.h, settings.h, stdlib.h, string.h, wuos_shell_internal.h | wuos_shell_events.c -- KEYDOWN dispatch for the wubuos shell: modal dialog capture, command-palette  |
| `apps/` | `wuos_toolbar` | 79 | hive.h, stdlib.h, string.h, wuos.h, wuos_font.h, wuos_toolbar.h | WUOS_KEY_STYLE_* / WUOS_KEY_INSERT_* sentinels |
| `apps/` | `test_wubupad_bridge` | 47 | stdio.h, stdlib.h, string.h, wubupad_bridge.h | test_wubupad_bridge.c -- verify WuBuOffice reuses WuBuPad's editor core (Phase E cross-repo bridge). |
| `apps/` | `wubupad_bridge` | 103 | doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_find.h, ui_headless.h, wubupad_brid | wubupad_bridge.c -- cross-repo bridge: reuse WuBuPad's from-scratch editor core (Doc + UI headless + |
| `apps/` | `wubupad_edit` | 45 | stdio.h, stdlib.h, string.h, wubupad_bridge.h | wubupad_edit.c -- CLI proving WuBuOffice reuses WuBuPad's editor core. Usage: wubupad_edit <file>    |
| `apps/` | `pdf_text` | 169 | pdf_internal.h, stdint.h, stdlib.h, string.h | pdf_text.c -- WinAnsi encoding, Helvetica metrics, and word wrapping for the wubupdf writer. See pdf |
| `apps/` | `pdf_write` | 170 | pdf.h, pdf_internal.h, stdio.h, stdlib.h, string.h | pdf_write.c -- lay out a dm_doc into pages and serialize a valid PDF file. See pdf.h / pdf_internal. |
| `apps/` | `entry` | 2 |  |  |
| `apps/` | `main` | 91 | docx_text.h, reader.h, stdio.h, stdlib.h, string.h | wuburead: open any OOXML file and dump its parts + extracted text. Dispatches by which parts are pre |
| `apps/` | `read_epub` | 161 | docmodel.h, parser.h, reader.h, readers.h, stdio.h, stdlib.h, string.h | read_epub.c -- EPUB reader -> dm_doc for WuBuOffice convert. EPUB is a ZIP container holding XHTML c |
| `apps/` | `read_html` | 219 | docmodel.h, parser.h, readers.h, stdio.h, stdlib.h, string.h | read_html.c -- HTML reader -> dm_doc for WuBuOffice convert. HTML is tag-based; we reuse the project |
| `apps/` | `read_rtf` | 152 | docmodel.h, readers.h, stdio.h, stdlib.h, string.h | read_rtf.c -- RTF reader -> dm_doc for WuBuOffice convert. RTF is a token stream (not XML): groups d |
| `apps/` | `entry` | 2 |  |  |
| `apps/` | `main` | 22 | show.h, stdio.h, stdlib.h |  |
| `apps/` | `show_io` | 156 | package.h, show_internal.h | WuBuOffice -- apps/wubushow/show_io Package assembly (wubushow_assemble) and teardown (wubushow_free |
| `apps/` | `show_model` | 72 | show_internal.h | WuBuOffice -- apps/wubushow/show_model Presentation model + per-slide XML rendering. The data model  |
| `apps/` | `show_read` | 229 | parser.h, reader.h, show.h, show_internal.h, show_read.h, stdio.h, stdlib.h, str | show_read.c -- read a .pptx into a wubushow_pres. See show_read.h for the contract. Self-contained:  |
| `apps/` | `agent_main` | 32 | agent.h, stdio.h, stdlib.h, string.h | wubusvg_agent main -- wubuOS NDJSON agent for vector documents. Reads one JSON command per line on s |
| `apps/` | `main` | 82 | stdio.h, stdlib.h, string.h, wubusvg.h | wubusvg main -- ingest an SVG file into the document model and regurgitate it as well-formed SVG. Th |
| `apps/` | `controller` | 245 | controller.h, stdio.h, string.h | controller.c -- pure wubuview interaction logic (viewport + tabs). |
| `apps/` | `docflat` | 165 | docflat.h, json.h, stdio.h, stdlib.h, string.h | docflat.c -- flatten a normalized JSON model to readable plain text. |
| `apps/` | `entry` | 2 |  |  |
| `apps/` | `main` | 264 | controller.h, docflat.h, draw.h, input.h, screen.h, stdio.h, stdlib.h, string.h, | wubuview -- native terminal document viewer with multi-tab support. Open ANY format wubudoc supports |
| `apps/` | `assemble` | 98 | assemble.h, package.h, stdio.h, stdlib.h, string.h, word.h, word_internal.h | Static numbering definition: abstractNum #0 = bullet, #1 = decimal; two concrete <w:num> instances ( |
| `apps/` | `entry` | 10 | string.h | subcommand: spell-check a text file (wubuword spell <file> [dict]) |
| `apps/` | `main` | 46 | assemble.h, stdio.h, stdlib.h, string.h, word.h | wubuword: emit a richer .docx (headings, bold, table) from scratch. |
| `apps/` | `word_doc` | 111 | stdlib.h, string.h, word_internal.h | WuBuOffice -- apps/wubuword/word_doc WordprocessingML document model + paragraph API + serialization |
| `apps/` | `word_spell` | 66 | spell.h, stdio.h, stdlib.h, string.h | word_spell.c -- spell-check subcommand for wubuword. Wires the dependency-free wubuspell engine into |
| `apps/` | `word_table` | 71 | word_internal.h | WuBuOffice -- apps/wubuword/word_table Table API for the WordprocessingML builder. A table is a sequ |
| `apps/` | `main` | 42 | model.h, stdio.h, stdlib.h, wubupng.h, wuburender.h | wubuwordview -- offscreen wubumodel_doc -> PNG renderer. Makes the office suite VISIBLE: renders a r |
| `apps/` | `main` | 135 | SDL.h, model.h, stdio.h, stdlib.h, string.h, wuburender.h | wubuwordwin -- interactive WuBuWord window (the office suite, LIVE). This is the real, usable face o |
| `apps/` | `CMakeCCompilerId` | 880 |  | cv-qualifiers did not exist in K&R C |
| `apps/` | `gpu_blas_stub` | 60 | gpu_blas.h | gpu_blas_stub.c -- CPU-only fallback for the GPU BLAS API. Compiled instead of gpu_blas.cu when WITH |
| `apps/` | `gru_gpu` | 209 | gpu_blas.h, gru.h, gru_layout.h, math.h, stdio.h, stdlib.h, string.h | gru_gpu.c -- GPU-matmul-accelerated GRU (C11, CUDA via gpu_blas). Faithful mirror of gru.c: the sequ |
| `apps/` | `test_wubu3d` | 39 | math.h, stdio.h, stdlib.h, wubu3d.h | REAL engine: rotate + project the cube; front/back verts must diverge on screen (perspective), provi |
| `apps/` | `wubu3d` | 97 | math.h, stdlib.h, string.h, wubu3d.h | rotated snapshot, size == nv |
| `apps/` | `a11y` | 267 | a11y.h, math.h, model.h, stdarg.h, stdio.h, stdlib.h, string.h | a11y.c -- dependency-free C11 accessibility auditor (see a11y.h). |
| `apps/` | `a11yannounce` | 30 | a11yannounce.h, stdlib.h, string.h | a11yannounce.c -- screen-reader announcement queue. See a11yannounce.h. |
| `apps/` | `test_a11yannounce` | 25 | a11yannounce.h, stdio.h, stdlib.h, string.h | test_a11yannounce.c |
| `apps/` | `a11ytree` | 65 | a11ytree.h, model.h, stdio.h, stdlib.h, string.h | a11ytree.c -- UI -> accessibility-tree serializer. See a11ytree.h. |
| `apps/` | `test_a11ytree` | 34 | a11ytree.h, model.h, stdio.h, stdlib.h, string.h | test_a11ytree.c -- needs wubumodel to build a doc. |
| `apps/` | `aislot` | 70 | aislot.h, stdio.h, stdlib.h, string.h | aislot.c -- offline AI assist hook. See aislot.h. Built-in fallback is a deterministic, offline heur |
| `apps/` | `test_aislot` | 39 | aislot.h, stdio.h, stdlib.h, string.h | test_aislot.c |
| `apps/` | `test_wubuanimation` | 47 | stdio.h, stdlib.h, string.h, wubuanimation.h | REAL engine: progress must interpolate over time. |
| `apps/` | `wubuanimation` | 73 | math.h, stdlib.h, string.h, wubuanimation.h | Ease function per animation kind. Input p in [0,1]; output in [0,1]. |
| `apps/` | `autosave` | 193 | autosave.h, errno.h, fcntl.h, signal.h, stat.h, stdio.h, stdlib.h, string.h, tim | autosave.c -- dependency-free C11 autosave + crash-recovery (see autosave.h) |
| `apps/` | `wububase` | 134 | stdio.h, stdlib.h, string.h, wububase.h | wububase.c -- shared internal utilities (see wububase.h). Clean C11. |
| `apps/` | `test_wububasic` | 75 | stdio.h, stdlib.h, string.h, wububasic.h | simple LET + PRINT |
| `apps/` | `wububasic` | 467 | ctype.h, stdio.h, stdlib.h, string.h, wububasic.h | current line index |
| `apps/` | `caption` | 30 | caption.h, stdlib.h, string.h | caption.c -- figure/table caption side-table. See caption.h. |
| `apps/` | `test_caption` | 20 | caption.h, stdio.h, string.h | test_caption.c |
| `apps/` | `cfb` | 284 | cfb.h, stdlib.h, string.h | wubucfb -- MS-CFB / OLE2 compound-file container reader. See cfb.h. Layout recap (little-endian thro |
| `apps/` | `cfb_write` | 240 | cfb_write.h, stdlib.h, string.h | cfb_write.c -- MS-CFB container writer. See cfb_write.h. Clean-room C11. Fixed 512-byte sectors, 64- |
| `apps/` | `chart` | 282 | chart.h, eval.h, funcs.h, math.h, stdarg.h, stdio.h, stdlib.h, string.h, value_u | chart.c -- dependency-free C11 chart renderer (see chart.h). |
| `apps/` | `cite` | 75 | cite.h, stdio.h, stdlib.h, string.h | cite.c -- bibliography / citation store. See cite.h. |
| `apps/` | `test_cite` | 24 | cite.h, stdio.h, stdlib.h, string.h | test_cite.c |
| `apps/` | `col` | 93 | col.h, stdlib.h, string.h | col.c -- opaque comment-thread store. See col.h. |
| `apps/` | `test_col` | 30 | col.h, stdio.h, string.h | test_col.c -- headless comment-thread store test. |
| `apps/` | `test_wubuconnector` | 38 | stdio.h, stdlib.h, string.h, wubuconnector.h | REAL engine: route produces a valid 3-point elbow from A to B. |
| `apps/` | `wubuconnector` | 57 | stdlib.h, string.h, wubuconnector.h | exit source right-center |
| `apps/` | `crdt` | 256 | crdt.h, stdio.h, stdlib.h, string.h | crdt.c -- node-sequence CRDT (op-based, LWW-merge). See crdt.h. Sequence of Items kept sorted by (la |
| `apps/` | `test_crdt` | 68 | crdt.h, stdio.h, stdlib.h, string.h | test_crdt.c -- headless convergence + move/delete test for the CRDT. |
| `apps/` | `csv` | 53 | csv.h, stdio.h, stdlib.h, string.h | csv.c -- CSV parser. See csv.h. |
| `apps/` | `test_csv` | 21 | csv.h, stdio.h, string.h | test_csv.c |
| `apps/` | `test_wubudiff` | 35 | stdio.h, stdlib.h, string.h, wubudiff.h | identical |
| `apps/` | `wubudiff` | 101 | stdio.h, stdlib.h, string.h, wubudiff.h | split text into lines (own allocations; caller frees each + the array). A trailing newline does NOT  |
| `apps/` | `agent` | 361 | agent.h, b64.h, json.h, stdlib.h, string.h, wubudoc.h | agent.c -- NDJSON agent protocol for the wubudoc facade (see agent.h). One JSON object per line on s |
| `apps/` | `b64` | 72 | b64.h, stdlib.h, string.h | b64.c -- base64 (RFC 4648) codec (see b64.h). Self-contained C11. |
| `apps/` | `wubudoc` | 30 | doc_access.inc, doc_create.inc, doc_detect.inc, doc_ingest.inc, doc_session.inc, | wubudoc.c -- unity aggregator for the unified document ingestion + creation facade (wubuOS protocol  |
| `apps/` | `draw` | 153 | draw.h, stdarg.h, stdio.h, stdlib.h, string.h, wububase.h, wubusvg.h | draw.c -- dependency-free C11 vector drawing model (see draw.h). |
| `apps/` | `test_wubudropcap` | 23 | stdio.h, stdlib.h, wubudropcap.h | invalid range rejected |
| `apps/` | `wubudropcap` | 25 | wubudropcap.h |  |
| `apps/` | `dyslexia` | 23 | dyslexia.h, stdlib.h, string.h | dyslexia.c -- dyslexia-friendly mode config. See dyslexia.h. |
| `apps/` | `test_dyslexia` | 21 | dyslexia.h, math.h, stdio.h, string.h | test_dyslexia.c |
| `apps/` | `test_wubuencrypt` | 58 | stdio.h, stdlib.h, string.h, wubuencrypt.h | NIST SP 800-38A F.2.1 CBC-AES128 known-answer test. |
| `apps/` | `wubuencrypt` | 259 | stdlib.h, string.h, wubuencrypt.h | ---------------- SHA-256 ---------------- |
| `apps/` | `epub` | 182 | epub.h, stdarg.h, stdio.h, stdlib.h, string.h, wububase.h, zip.h | epub.c -- dependency-free C11 EPUB3 writer (see epub.h). |
| `apps/` | `eqnum` | 43 | eqnum.h, model.h, stdio.h, stdlib.h, string.h | eqnum.c -- sequential equation numbering. See eqnum.h. |
| `apps/` | `test_eqnum` | 33 | eqnum.h, model.h, stdio.h, string.h | test_eqnum.c -- needs wubumodel to build a doc with FIELD (equation) nodes. |
| `apps/` | `exp` | 475 | deflate.h, exp.h, stdio.h, stdlib.h, string.h, wubufont.h | exp.c -- exporters. See exp.h. All formats are serialized by walking the laid-out pages of a wubulay |
| `apps/` | `exp_png` | 8 | exp_png.h, wubupng.h | exp_png.c -- selection/region -> PNG export. See exp_png.h. |
| `apps/` | `test_exp_png` | 23 | exp_png.h, stdint.h, stdio.h, stdlib.h, string.h | test_exp_png.c |
| `apps/` | `test_wubufilter` | 47 | stdio.h, stdlib.h, string.h, wubufilter.h | sales 10,5,30,7,15 -> in [6,15]: row0(10), row3(7), row4(15) |
| `apps/` | `wubufilter` | 102 | ctype.h, errno.h, stdlib.h, string.h, wubufilter.h | case-insensitive substring |
| `apps/` | `fmtpaint` | 60 | fmtpaint.h, model.h, stdlib.h, string.h | fmtpaint.c -- format painter. See fmtpaint.h. |
| `apps/` | `test_fmtpaint` | 46 | fmtpaint.h, model.h, stdio.h, string.h | test_fmtpaint.c -- format painter over real wubumodel styles. |
| `apps/` | `focus` | 22 | focus.h, stdlib.h | focus.c -- visible focus indicator config. See focus.h. |
| `apps/` | `test_focus` | 19 | focus.h, stdio.h | test_focus.c |
| `apps/` | `font_subset` | 300 | font_subset.h, stdio.h, stdlib.h, string.h, wubufont.h | font_subset.c -- N1: sfnt glyph subsetting for PDF FontFile2 embedding. Produces a valid subset sfnt |
| `apps/` | `odttf` | 63 | odttf.h, stdlib.h, string.h | odttf.c -- H5: embedded-font de-obfuscation for DOCX (word/fonts/*.odttf). ODTTF = a TrueType font w |
| `apps/` | `woff` | 245 | deflate.h, inflate.h, stdio.h, stdlib.h, string.h, woff.h, wubufont.h | woff.c -- clean-room WOFF 1.0 reader + writer. Native C11, no deps. Reuses wubuzip (raw DEFLATE) for |
| `apps/` | `wubufont` | 47 | font_cmap.inc, font_glyf_decode.inc, font_name.inc, font_parse.inc, font_raster. | wubufont.c -- unity aggregator for the clean-room SFNT/TrueType module. This file is intentionally t |
| `apps/` | `form` | 36 | form.h, stdlib.h, string.h | form.c -- PDF form-field data model. See form.h. |
| `apps/` | `test_form` | 21 | form.h, stdio.h, string.h | test_form.c |
| `apps/` | `ast` | 46 | ast.h, stdlib.h, string.h | WuBuOffice -- wubuformula/ast AST node constructors/destructor. |
| `apps/` | `eval` | 265 | eval.h, math.h, parser.h, registry.h, stdio.h, stdlib.h, string.h | WuBuOffice -- wubuformula/eval AST evaluator. Recurses into cell references through the resolver, ex |
| `apps/` | `datefin` | 189 | math.h, registry.h, stdlib.h, string.h, time.h, value_util.h | WuBuOffice -- wubuformula/functions/datefin Date/time and financial functions. Dates are Excel seria |
| `apps/` | `logic` | 85 | math.h, registry.h, stdlib.h, string.h, value_util.h | WuBuOffice -- wubuformula/functions/logic Logical functions: IF, IFERROR, AND, OR, NOT, IFS, SWITCH. |
| `apps/` | `lookup` | 179 | registry.h, stdlib.h, string.h, value_util.h | WuBuOffice -- wubuformula/functions/lookup Range-aware lookup and conditional-aggregate functions. T |
| `apps/` | `math` | 204 | math.h, registry.h, stdlib.h, string.h, value_util.h | WuBuOffice -- wubuformula/functions/math Aggregate and numeric functions: SUM, PRODUCT, AVERAGE, MIN |
| `apps/` | `registry` | 49 | registry.h, stdlib.h, string.h, value_util.h | WuBuOffice -- wubuformula/functions/registry Central function table. Category modules register into  |
| `apps/` | `stat` | 127 | math.h, registry.h, stdlib.h, string.h, value_util.h | WuBuOffice -- wubuformula/functions/stat Statistical functions: MEDIAN, MODE, STDEV, VAR, LARGE, SMA |
| `apps/` | `text` | 285 | ctype.h, registry.h, stdlib.h, string.h, strings.h, value_util.h | WuBuOffice -- wubuformula/functions/text Text functions: CONCATENATE, CONCAT, TEXTJOIN, LEFT, RIGHT, |
| `apps/` | `lexer` | 163 | ctype.h, lexer.h, stdlib.h, string.h | WuBuOffice -- wubuformula/lexer Formula tokenizer. Handles numbers, quoted strings, identifiers, cel |
| `apps/` | `parser` | 243 | ctype.h, parser.h, stdio.h, stdlib.h, string.h, value_util.h | WuBuOffice -- wubuformula/parser Clean-room, from-scratch (SLERM). Grammar (lowest precedence last): |
| `apps/` | `value` | 48 | stdlib.h, string.h, value.h | WuBuOffice -- wubuformula/value Formula value model + cell reference types. Clean-room, from-scratch |
| `apps/` | `value_util` | 120 | ctype.h, math.h, stdio.h, stdlib.h, string.h, strings.h, value_util.h | WuBuOffice -- wubuformula/value_util Shared value coercion / comparison helpers (single source of tr |
| `apps/` | `test_wubufreeze` | 30 | stdio.h, stdlib.h, wubufreeze.h | scroll to row 5 col 3: logical row 5 is visible at offset 3 |
| `apps/` | `wubufreeze` | 32 | wubufreeze.h | frozen region |
| `apps/` | `test_wubugallery` | 24 | stdio.h, stdlib.h, string.h, wubugallery.h |  |
| `apps/` | `wubugallery` | 64 | stdlib.h, string.h, wubugallery.h |  |
| `apps/` | `test_wubugoalseek` | 37 | math.h, stdio.h, stdlib.h, wubugoalseek.h | f(x) = x^2 - 2 -> root sqrt(2) ~1.4142 |
| `apps/` | `wubugoalseek` | 73 | math.h, stdlib.h, wubugoalseek.h | if not bracketing, scan for a bracket |
| `apps/` | `test_wubugrammar` | 37 | stdio.h, stdlib.h, wubugrammar.h | repeated word + misspelling + a-before-vowel + an-before-consonant |
| `apps/` | `wubugrammar` | 113 | ctype.h, stdio.h, stdlib.h, string.h, wubugrammar.h | built-in usage/misspelling pairs |
| `apps/` | `test_wubugridline` | 22 | stdio.h, stdlib.h, wubugridline.h |  |
| `apps/` | `wubugridline` | 20 | wubugridline.h |  |
| `apps/` | `hash` | 89 | hash.h, stdlib.h, string.h | hash.c -- SHA-256 (FIPS 180-4). See hash.h. |
| `apps/` | `test_hash` | 24 | hash.h, stdio.h, string.h | test_hash.c |
| `apps/` | `heading` | 41 | heading.h, model.h, stdlib.h | heading.c -- semantic heading-level enforcement. See heading.h. |
| `apps/` | `test_heading` | 31 | heading.h, model.h, stdio.h, string.h | test_heading.c -- needs wubumodel: nested sections get sequential levels. |
| `apps/` | `history` | 115 | history.h, stdio.h, stdlib.h, string.h | history.c -- opaque version-history store. See history.h. |
| `apps/` | `test_history` | 35 | history.h, stdio.h, stdlib.h, string.h | test_history.c -- headless version-history test. |
| `apps/` | `test_wubuhyperlink` | 37 | stdio.h, stdlib.h, string.h, wubuhyperlink.h | overwrite |
| `apps/` | `wubuhyperlink` | 72 | stdlib.h, string.h, wubuhyperlink.h |  |
| `apps/` | `test_wubuicon` | 28 | stdio.h, stdlib.h, string.h, wubuicon.h | overwrite |
| `apps/` | `wubuicon` | 51 | stdlib.h, string.h, wubuicon.h |  |
| `apps/` | `img2doc` | 87 | fontbank.h, img2doc.h, png.h, stdio.h, stdlib.h, string.h, wubufont.h, wubuocr.h | img2doc.c -- image -> OCR'd document bridge (see img2doc.h). |
| `apps/` | `imgops` | 138 | imgops.h, math.h, stdlib.h, string.h | imgops.c -- see imgops.h. Clean-room C11, O(area) per op. |
| `apps/` | `png` | 168 | inflate.h, png.h, stdlib.h, string.h | png.c -- clean-room PNG decoder (see png.h). |
| `apps/` | `test_wubuindex` | 29 | stdio.h, stdlib.h, string.h, wubuindex.h | pages: apple on 1,3; banana on 2 |
| `apps/` | `wubuindex` | 93 | ctype.h, stdlib.h, string.h, wubuindex.h | insert keeping entries sorted by term |
| `apps/` | `json` | 255 | ctype.h, json.h, stdio.h, stdlib.h, string.h | json.c -- minimal JSON parse + emit. Self-contained C11. |
| `apps/` | `lang` | 28 | lang.h, stdlib.h, string.h | lang.c -- per-node language attribute side-table. See lang.h. |
| `apps/` | `test_lang` | 20 | lang.h, stdio.h, string.h | test_lang.c |
| `apps/` | `layout` | 733 | model.h, shape.h, stdio.h, stdlib.h, string.h, ublayout.h | layout.c -- central text pipeline. See layout.h. Walks the opaque wubumodel tree, greedily word-wrap |
| `apps/` | `test_wubumailexport` | 30 | stdio.h, stdlib.h, string.h, wubumailexport.h |  |
| `apps/` | `wubumailexport` | 43 | stdio.h, stdlib.h, string.h, time.h, wubumailexport.h | size: headers + date + blank line + body |
| `apps/` | `test_wubumailmerge` | 32 | stdio.h, stdlib.h, string.h, wubumailmerge.h | unknown field left as-is |
| `apps/` | `wubumailmerge` | 94 | stdlib.h, string.h, wubumailmerge.h | placeholder: ${name} or {name} |
| `apps/` | `test_wubumasterdoc` | 28 | stdio.h, stdlib.h, string.h, wubumasterdoc.h |  |
| `apps/` | `wubumasterdoc` | 58 | stdlib.h, string.h, wubumasterdoc.h |  |
| `apps/` | `test_wubumasterslide` | 41 | stdio.h, stdlib.h, string.h, wubumasterslide.h | REAL engine: resolve a harmonized, drawable palette from the bg. |
| `apps/` | `wubumasterslide` | 59 | math.h, stdlib.h, string.h, wubumasterslide.h | sRGB luminance (0..1) of a 0..255 triple (linearized, correct WCAG math). |
| `apps/` | `math` | 316 | math.h, stdarg.h, stdio.h, stdlib.h, string.h, wububase.h, wubusvg.h | math.c -- dependency-free C11 math expression renderer (see math.h). |
| `apps/` | `doc_cache` | 165 | doc_cache.h, pthread.h, stat.h, stdlib.h, string.h | doc_cache.c — opaque, thread-safe parsed-document cache (ws07#1342). Keyed by (path, file mtime, fil |
| `apps/` | `load_async` | 167 | load_async.h, pthread.h, stdlib.h, string.h | load_async.c — background document loader (ws07#1334). A single worker thread drains the queue, runn |
| `apps/` | `model` | 409 | model_internal.h, stdlib.h, string.h | WS11 unified object model — core implementation (green baseline). v1 scope: doc lifecycle, node CRUD |
| `apps/` | `model_epub` | 372 | model.h, model_internal.h, parser.h, reader.h, stdio.h, stdlib.h, string.h | model_epub.c -- EPUB3 (.epub) import for the unified model (EXP-82 import half; the writer half ship |
| `apps/` | `model_io` | 277 | docx_document.h, docx_text.h, model_internal.h, package.h, reader.h, stdio.h, st | WS11 model -> OOXML writer (minimal, from-scratch). Emits a valid .docx (WordprocessingML) from the  |
| `apps/` | `model_odt` | 276 | model.h, model_internal.h, parser.h, reader.h, stdio.h, stdlib.h, string.h, zip. | model_odt.c -- OpenDocument Text (.odt) import/export for the unified model (DOC-78: ODF/ODT import- |
| `apps/` | `model_rtf` | 187 | model.h, stdio.h, stdlib.h, string.h | model_rtf.c -- Rich Text Format (.rtf) import for the unified model (closes the RTF *import* half of |
| `apps/` | `nesttab` | 66 | model.h, nesttab.h, stddef.h | nesttab.c -- nested tables builder/validator. See nesttab.h. |
| `apps/` | `test_nesttab` | 39 | model.h, nesttab.h, stdio.h | test_nesttab.c -- nested tables over real wubumodel. |
| `apps/` | `edit` | 251 | edit.h, stdio.h, stdlib.h, string.h | edit.c -- wubunote text-buffer model (pure, allocation-checked). |
| `apps/` | `test_wubunotebookbar` | 35 | stdio.h, stdlib.h, string.h, wubunotebookbar.h | REAL engine: geometry a renderer draws + hit-tests. |
| `apps/` | `wubunotebookbar` | 38 | stdlib.h, string.h, wubunotebookbar.h |  |
| `apps/` | `binarize` | 249 | binarize.h, math.h, stdlib.h, string.h | binarize.c -- Otsu global threshold + Sauvola adaptive threshold + despeckle + auto-crop (grayscale  |
| `apps/` | `components` | 136 | components.h, stdint.h, stdlib.h, string.h | components.c -- 8-connected component labeling via iterative flood fill. |
| `apps/` | `conv_gemm` | 107 | conv_gemm.h, math.h, omp.h | conv_gemm.c -- im2col/col2im + GEMM kernels for the conv engine. See conv_gemm.h. C11, no deps. Open |
| `apps/` | `convnet` | 315 | convnet.h, math.h, stdio.h, stdlib.h, string.h | convnet.c -- ultra-light conv front-end. See convnet.h. C11, no deps. Designed for SINGLE-CORE scala |
| `apps/` | `convnet3` | 624 | conv_gemm.h, convnet3.h, convnet3_internal.h, math.h, stdio.h, stdlib.h, string. | convnet3.c -- 3-stage ultra-light conv front-end (medium capacity). See convnet3.h. C11, no deps, si |
| `apps/` | `convnet3_cbam` | 99 | convnet3_internal.h, math.h, stdio.h, stdlib.h, string.h | convnet3_cbam.c -- CBAM attention (channel + spatial) for the CRNN convnet. Enabled via env CBAM=1 a |
| `apps/` | `crnn` | 493 | convnet3.h, crnn.h, ctc.h, gru.h, image.h, math.h, omp.h, rnn.h, stdio.h, stdlib | crnn.c -- CRNN recognizer (see crnn.h). |
| `apps/` | `crnn_transcribe` | 705 | binarize.h, crnn_transcribe.h, crnn_transcribe_internal.h, lexicon.h, math.h, st | crnn_transcribe.c -- page -> line segmentation -> CRNN -> docmodel JSON. See crnn_transcribe.h. C11, |
| `apps/` | `crnn_transcribe_pre` | 425 | crnn_transcribe_internal.h, math.h, stdio.h, stdlib.h, string.h | crnn_transcribe_pre.c -- page preprocessing + text helpers for the transcription pipeline: NFC latin |
| `apps/` | `ctc` | 199 | ctc.h, math.h, stdio.h, stdlib.h, string.h | ctc.c -- CTC forward-backward + gradient (Graves 2006), scalar C11. Numerically-stable LOG-SPACE imp |
| `apps/` | `dft` | 146 | dft.h, math.h, stdlib.h | dft.c -- 2D DFT compression + spectral analysis (see dft.h). Plain scalar C11, no deps. Direct 2D DF |
| `apps/` | `docfmt` | 595 | docfmt.h, json.h, stdio.h, stdlib.h, string.h, unistd.h, zip.h | docfmt.c -- OCR docmodel JSON -> alternate serializations. See docfmt.h |
| `apps/` | `font8x8` | 142 | font8x8.h | font8x8.c -- embedded 8x8 monochrome VGA bitmap font (U+0000..U+007F). Public Domain. Author: Marcel |
| `apps/` | `fontbank` | 281 | fontbank.h, math.h, stdio.h, stdlib.h, string.h, wubufont.h | fontbank.c -- multi-font zoning template bank + per-font voting. See fontbank.h. Builds zoning templ |
| `apps/` | `fontdir` | 118 | dirent.h, fontdir.h, stat.h, stdio.h, stdlib.h, string.h, strings.h | fontdir.c -- discover and load a whole directory tree of fonts (see fontdir.h). Clean C11, self-cont |
| `apps/` | `gauntlet` | 412 | gauntlet.h, lexicon.h, math.h, page_compose.h, stdio.h, stdlib.h, string.h, wubu | gauntlet.c -- OCR robustness battery (see gauntlet.h). Pure, deterministic corruptions over OcrImage |
| `apps/` | `goldplace` | 94 | goldplace.h, math.h, string.h | goldplace.c -- golden-ratio coordinate placement (see goldplace.h). Byte-identical golden math to Wu |
| `apps/` | `gru` | 222 | gru.h, gru_layout.h, math.h, stdio.h, stdlib.h, string.h | gru.c -- minimal scalar GRU, forward + BPTT (see gru.h). Single layer, optional bidirectional. Gates |
| `apps/` | `gru_layout` | 20 | gru_layout.h | gru_layout.c -- the ONE shared GRU weight-offset table. Single source of truth for the flat GRU weig |
| `apps/` | `image` | 195 | image.h, stdint.h, stdio.h, stdlib.h, string.h | image.c -- WuBuOCR grayscale page buffer + clean-room Netpbm decoder. |
| `apps/` | `latin1` | 54 | latin1.h, string.h | latin1.c -- Latin-1 (ISO-8859-1) glyph crowd (see latin1.h). Clean C11, self-contained. |
| `apps/` | `layout` | 205 | layout.h, stdlib.h, string.h | layout.c -- recursive XY-cut page segmentation into reading-order blocks. Algorithm (per region, aft |
| `apps/` | `lens` | 112 | image.h, lens.h, math.h, stdint.h, stdlib.h, string.h | lens.c -- Office-Lens-style document flattening (see lens.h). |
| `apps/` | `lexicon` | 254 | lexicon.h, stdio.h, stdlib.h, string.h | lexicon.c -- see lexicon.h. Dependency-free C11. |
| `apps/` | `mlp` | 368 | math.h, mlp.h, stdio.h, stdlib.h, string.h | mlp.c -- lightweight feed-forward classifier. See mlp.h. C11, no deps. |
| `apps/` | `ocl_gru` | 223 | cl.h, dlfcn.h, gru.h, gru_layout.h, math.h, opencl.h, stdio.h, stdlib.h, string. | ocl_gru.c -- OpenCL fallback (#98) for the GRU recurrent forward. Mirrors gru_fwd_dir() in gru.c EXA |
| `apps/` | `page_compose` | 257 | lexicon.h, math.h, page_compose.h, stdlib.h, string.h | page_compose.c -- synthetic document page composer (see page_compose.h). Clean C11, self-contained.  |
| `apps/` | `pdfsearch` | 239 | json.h, pdfsearch.h, stdio.h, stdlib.h, string.h | pdfsearch.c -- see pdfsearch.h. Clean-room searchable-PDF writer. |
| `apps/` | `png` | 151 | image.h, inflate.h, png.h, stdint.h, stdlib.h, string.h | png.c -- clean-room PNG decoder (see png.h). |
| `apps/` | `png_encode` | 13 | png_encode.h, stdlib.h, wubupng.h | png_encode.c -- thin wrapper over the shared wubupng encoder. Kept only to preserve the png_encode_g |
| `apps/` | `qr` | 664 | math.h, qr.h, stdint.h, stdio.h, stdlib.h, string.h | qr.c -- self-contained QR-code codec (encoder + decoder), clean C11, no deps. Scope: byte-mode QR co |
| `apps/` | `recognize` | 398 | font8x8.h, recognize.h, stdio.h, stdlib.h, string.h | recognize.c -- zoning feature extraction + 1-NN template glyph classifier. Two template sources, sam |
| `apps/` | `rnn` | 175 | math.h, rnn.h, stdlib.h, string.h | rnn.c -- minimal scalar LSTM, forward + BPTT (see rnn.h). Single layer, optional bidirectional. Gate |
| `apps/` | `stylebank` | 195 | math.h, stdio.h, stdlib.h, string.h, stylebank.h | stylebank.c -- 64MB multi-style conv expert bank. See stylebank.h. Plain C11, no deps, single-core s |
| `apps/` | `trainer` | 513 | convnet3.h, math.h, mlp.h, pthread.h, stdio.h, stdlib.h, string.h, trainer.h, un | trainer.c -- training orchestration for conv3+MLP. C11, no deps. |
| `apps/` | `unicode` | 90 | string.h, unicode.h | unicode.c -- generalized Unicode OCR token classes (see unicode.h). Clean C11, self-contained. |
| `apps/` | `wubuocr` | 303 | json.h, stdlib.h, string.h, wubuocr.h | wubuocr.c -- image -> structured document facade. |
| `apps/` | `zoning` | 142 | math.h, stdlib.h, string.h, zoning.h | zoning.c -- principled, interpretable glyph feature extraction. See zoning.h for the feature layout. |
| `apps/` | `docx_document` | 391 | docx_document.h, parser.h, stdlib.h, string.h, table_styles.h | docx_document.c — WordprocessingML document.xml -> unified model (ws05#0884 / ws05#082). Streaming S |
| `apps/` | `extract` | 296 | extract.h, parser.h, stdlib.h, string.h | extract.c — SAX-based plain-text extraction for OOXML documents. One streaming handler (driven by wu |
| `apps/` | `package` | 130 | package.h, rels_path.h, stdio.h, stdlib.h, string.h, zip.h | Build the rels part path for a given source. source == "" -> "_rels/.rels". Implemented in rels_path |
| `apps/` | `pptx_write` | 274 | package.h, pptx_write.h, reader.h, stdio.h, stdlib.h, string.h | pptx_write.c -- H13/H17: assemble a valid .pptx from the slide model. Emits [Content_Types].xml, rel |
| `apps/` | `reader` | 118 | docx_text.h, io_le.h, reader.h, rels_path.h, stdlib.h, string.h | parse .rels rooted at each part |
| `apps/` | `rels_path` | 20 | rels_path.h, string.h | "_rels/" |
| `apps/` | `table_styles` | 179 | parser.h, stdio.h, stdlib.h, string.h, table_styles.h | table_styles.c -- H6 implementation. See table_styles.h. Parses styles.xml via the existing wubuxml  |
| `apps/` | `pasteplain` | 31 | pasteplain.h, stdlib.h, string.h | pasteplain.c -- paste-plain stripper. See pasteplain.h. |
| `apps/` | `test_pasteplain` | 20 | pasteplain.h, stdio.h, stdlib.h, string.h | test_pasteplain.c |
| `apps/` | `pdf_extract` | 187 | inflate.h, pdf_extract.h, stdio.h, stdlib.h, string.h | pdf_extract.c -- clean-room PDF text extraction (see pdf_extract.h). |
| `apps/` | `pdfextract` | 25 | pdf_extract.h, pdfextract.h, stdio.h, stdlib.h | pdfextract.c -- PDF text import (EXP-91). See pdfextract.h. |
| `apps/` | `test_pdfextract` | 29 | pdfextract.h, stdio.h, stdlib.h, string.h | test_pdfextract.c |
| `apps/` | `pdfform` | 154 | form.h, pdfform.h, stdio.h, stdlib.h, string.h | pdfform.c -- PDF forms export (real AcroForm writer). See pdfform.h. Layout of the emitted PDF: obj  |
| `apps/` | `test_pdfform` | 50 | form.h, pdfform.h, stdio.h, stdlib.h, string.h | test_pdfform.c -- real AcroForm PDF writer. |
| `apps/` | `test_wubupivot` | 44 | stdio.h, stdlib.h, string.h, wubupivot.h | COUNT: each cell = number of rows in that slice |
| `apps/` | `wubupivot` | 99 | errno.h, stdlib.h, string.h, wubupivot.h | first pass: discover keys |
| `apps/` | `wubupng` | 95 | stdio.h, stdlib.h, string.h, wubupng.h, zlib.h | wubupng.c -- single correct PNG encoder (see wubupng.h). Clean C11. Reuses zlib's crc32 + compress ( |
| `apps/` | `test_wubuqr` | 29 | stdio.h, stdlib.h, string.h, wubuqr.h | top-left finder pattern should be present in first rows |
| `apps/` | `wubuqr` | 28 | qr.h, stdlib.h, string.h, wubuqr.h | The QR codec lives in wubuocr; expose it under the wubuqr module name. |
| `apps/` | `redact` | 39 | redact.h, stdlib.h, string.h | redact.c -- redaction set + redacted-text export. See redact.h. |
| `apps/` | `test_redact` | 20 | redact.h, stdio.h, stdlib.h, string.h | test_redact.c |
| `apps/` | `wuburender` | 317 | chart.h, ft2build.h, math.h, model.h, spell.h, stdio.h, stdlib.h, string.h, wubu | wuburender.c -- shared doc -> RGBA renderer (see wuburender.h). |
| `apps/` | `rtf` | 40 | rtf.h, stdio.h, stdlib.h, string.h | rtf.c -- RTF writer. See rtf.h. |
| `apps/` | `test_rtf` | 25 | rtf.h, stdio.h, stdlib.h, string.h | test_rtf.c |
| `apps/` | `test_wuburuler` | 33 | stdio.h, stdlib.h, wuburuler.h | REAL engine: resolve to PIXELS at 96 dpi so a ruler can draw it. |
| `apps/` | `wuburuler` | 33 | wuburuler.h | 1 inch default |
| `apps/` | `sandbox` | 59 | sandbox.h, stdlib.h, string.h | sandbox.c -- plugin capability sandbox. See sandbox.h. |
| `apps/` | `test_sandbox` | 28 | sandbox.h, stdio.h, string.h | test_sandbox.c |
| `apps/` | `test_wubuscenario` | 39 | stdio.h, stdlib.h, string.h, wubuscenario.h |  |
| `apps/` | `wubuscenario` | 79 | stdlib.h, string.h, wubuscenario.h | find existing |
| `apps/` | `scope` | 26 | scope.h, stdlib.h, string.h | scope.c -- table header-row scope side-table. See scope.h. |
| `apps/` | `test_scope` | 20 | scope.h, stdio.h, string.h | test_scope.c |
| `apps/` | `script` | 94 | ctype.h, eval.h, funcs.h, script.h, stdio.h, stdlib.h, string.h, value.h, value_ | script.c -- dependency-free C11 safe scripting host (see script.h). |
| `apps/` | `settings` | 243 | json.h, settings.h, stat.h, stdio.h, stdlib.h, string.h, types.h | settings.c -- see settings.h. Opaque config with JSON persistence. |
| `apps/` | `shape` | 114 | shape.h, stddef.h, string.h | shape.c -- see shape.h. Bounded Bidi visual reordering. |
| `apps/` | `test_wubusidebar` | 29 | stdio.h, stdlib.h, string.h, wubusidebar.h |  |
| `apps/` | `wubusidebar` | 60 | stdlib.h, string.h, wubusidebar.h |  |
| `apps/` | `sig` | 32 | hash.h, sig.h, string.h | sig.c -- HMAC-SHA256 document signature. See sig.h. |
| `apps/` | `test_sig` | 22 | hash.h, sig.h, stdio.h, string.h | test_sig.c |
| `apps/` | `test_wubusmartart` | 48 | stdio.h, stdlib.h, string.h, wubusmartart.h | REAL engine: layouts must produce boxes inside the frame, non-overlapping enough that each box has p |
| `apps/` | `wubusmartart` | 106 | math.h, stdlib.h, string.h, wubusmartart.h | root = node 0, centered top |
| `apps/` | `test_wubusolver` | 41 | math.h, stdio.h, stdlib.h, wubusolver.h | 2x + 3y = 8; 4x - y = 2  -> x=1, y=2 |
| `apps/` | `wubusolver` | 93 | math.h, stdlib.h, wubusolver.h | augmented copy with partial pivoting + back substitution |
| `apps/` | `test_wubusort` | 66 | stdio.h, stdlib.h, string.h, wubusort.h | stable heap ptr |
| `apps/` | `wubusort` | 86 | ctype.h, errno.h, stdlib.h, string.h, wubusort.h | stable bottom-up merge sort |
| `apps/` | `spell` | 280 | ctype.h, spell.h, stdint.h, stdio.h, stdlib.h, string.h | spell.c -- dependency-free C11 spell checker (see spell.h). |
| `apps/` | `spell_seed` | 39 | spell.h | spell_seed.c -- built-in common English word list (generated). A small, high-frequency seed so the c |
| `apps/` | `test_wubusubtotal` | 38 | stdio.h, stdlib.h, string.h, wubusubtotal.h | first-seen order A,B,C |
| `apps/` | `wubusubtotal` | 66 | errno.h, stdlib.h, string.h, wubusubtotal.h | find existing group |
| `apps/` | `agent` | 185 | agent.h, json.h, stdio.h, stdlib.h, string.h, wubusvg.h | agent.c -- wubuOS NDJSON protocol dispatcher over the verified SVG core. Self-contained C11; reuses  |
| `apps/` | `rast` | 167 | math.h, rast.h, stdio.h, stdlib.h, string.h, wubusvg.h | rast.c -- minimal SVG -> RGBA rasterizer (see rast.h). |
| `apps/` | `wubusvg` | 357 | parser.h, stdlib.h, string.h, wubusvg.h, xml.h | wubusvg.c -- clean-room SVG ingest + regurgitate. Native C11 + POSIX open_memstream. Reuses the wubu |
| `apps/` | `sync` | 130 | crdt.h, signal.h, stat.h, stdio.h, stdlib.h, string.h, sync.h, unistd.h | sync.c -- local-first CRDT store + shared lock. See sync.h. |
| `apps/` | `test_sync` | 51 | crdt.h, stat.h, stdio.h, stdlib.h, string.h, sync.h, unistd.h | test_sync.c -- headless local-first CRDT sync + shared lock test. |
| `apps/` | `test_wubuthesaurus` | 33 | stdio.h, stdlib.h, string.h, wubuthesaurus.h | case-insensitive |
| `apps/` | `wubuthesaurus` | 81 | ctype.h, stdlib.h, string.h, wubuthesaurus.h | find existing |
| `apps/` | `toc` | 183 | model.h, stdio.h, stdlib.h, string.h, toc.h, ublayout.h | toc.c -- DOC-54 table-of-contents generator. See toc.h. |
| `apps/` | `test_wubutransition` | 38 | stdio.h, stdlib.h, wubutransition.h | REAL engine: progress must blend from 0 -> 1 over the duration. |
| `apps/` | `wubutransition` | 45 | math.h, wubutransition.h | linear cross-fade (morph shares the same eased envelope) |
| `apps/` | `draw` | 243 | draw.h, stdlib.h, string.h | draw.c -- drawing primitives + pure word wrap. |
| `apps/` | `input` | 292 | input.h, stdlib.h, string.h | input.c -- pure byte-stream key decoder (no fd reads). |
| `apps/` | `screen` | 156 | screen.h, stdio.h, stdlib.h, string.h | screen.c -- off-screen cell grid + ANSI renderer. Pure C11, no TTY calls. |
| `apps/` | `term` | 92 | ioctl.h, stdlib.h, string.h, term.h, termios.h, unistd.h | term.c -- POSIX raw-mode terminal edge for wubutui. |
| `apps/` | `test_vars` | 24 | stdio.h, stdlib.h, string.h, vars.h | test_vars.c |
| `apps/` | `vars` | 74 | stdio.h, stdlib.h, string.h, vars.h | vars.c -- named document variables. See vars.h. |
| `apps/` | `test_watermark` | 22 | math.h, stdio.h, string.h, watermark.h | test_watermark.c |
| `apps/` | `watermark` | 27 | math.h, stdlib.h, string.h, watermark.h | watermark.c -- page watermark config. See watermark.h. |
| `apps/` | `parser` | 263 | parser.h, stdlib.h, string.h | parser.c — minimal streaming XML event parser (see parser.h). Single-pass, constant memory. Emits ST |
| `apps/` | `xml` | 96 | stdlib.h, string.h, xml.h | is the top element's start tag still unclosed ('>')? |
| `apps/` | `test_xps` | 24 | stdio.h, stdlib.h, string.h, xps.h | test_xps.c |
| `apps/` | `xps` | 109 | stdio.h, stdlib.h, string.h, xps.h | xps.c -- minimal XPS (store-only ZIP + FixedPage XML). See xps.h. Store-only ZIP: each entry = local |
| `apps/` | `bit` | 31 | bit.h | feed zero bits when we run off the end of the buffer |
| `apps/` | `bitw` | 83 | bitw.h, stdlib.h, string.h | Use a 64-bit accumulator so code bits (up to 25) merging with up to 7 buffered bits never overflow t |
| `apps/` | `block` | 120 | bit.h, block.h, huffman.h, stdlib.h, string.h | clamp to longest length code |
| `apps/` | `canon` | 128 | canon.h, stdlib.h, string.h | WuBuOffice -- wubuzip/canon Canonical Huffman code assignment (RFC 1951 3.2.2). Clean-room, from-scr |
| `apps/` | `crc` | 25 | crc.h, stdint.h |  |
| `apps/` | `deflate` | 302 | bitw.h, block.h, canon.h, deflate.h, fixedcode.h, limitcode.h, lz77.h, stdio.h,  | max input per DEFLATE block (bounds LZ77 memory) |
| `apps/` | `fixed` | 13 | fixed.h | 0..143   -> 8 bits |
| `apps/` | `fixedcode` | 38 | fixed.h, fixedcode.h, string.h | Compute canonical codes from the FIXED Huffman code *lengths* defined by RFC 1951 3.2.6 (via wubuzip |
| `apps/` | `huffman` | 57 | bit.h, huffman.h, string.h | Build a canonical Huffman decode table from code lengths (RFC 1951 3.2.2). The canonical assignment: |
| `apps/` | `inflate` | 66 | bit.h, block.h, fixed.h, huffman.h, inflate.h, stdlib.h | CMF |
| `apps/` | `limitcode` | 130 | limitcode.h, stdlib.h, string.h | WuBuOffice -- wubuzip/limitcode Length-limited Huffman code-length generator (package-merge algorith |
| `apps/` | `lz77` | 162 | lz77.h, stdlib.h, string.h | WuBuOffice -- wubuzip/lz77 Greedy + lazy LZ77 matcher for the DEFLATE encoder. Clean-room, from-scra |
| `apps/` | `reader` | 103 | inflate.h, io_le.h, reader.h, stdlib.h, string.h | store |
| `apps/` | `zip` | 143 | crc.h, deflate.h, stdio.h, stdlib.h, string.h, zip.h | local file header signature |
| `apps/` | `gradcheck_inorm` | 66 | convnet3.h, math.h, mlp.h, stdio.h, stdlib.h, string.h | gradcheck_inorm.c -- finite-difference gradient check for the instance-norm gamma/beta params (smoot |
| `apps/` | `test_a11y` | 100 | a11y.h, model.h, stdio.h, stdlib.h, string.h | test_a11y.c -- wubua11y acceptance test (doc + EPUB-parts audits). |
| `apps/` | `test_a11y_tables` | 90 | a11y.h, stdio.h, stdlib.h, string.h | test_a11y_tables.c -- hop 20: table accessibility checks (MS checker parity). Build a doc with a pro |
| `apps/` | `test_agent_verbs` | 74 | agent.h, stdio.h, stdlib.h, string.h, wubudoc.h | test_agent_verbs.c -- agent protocol verb coverage: find + structure. Research synthesis (SUPERIORIT |
| `apps/` | `test_autosave` | 82 | autosave.h, model.h, stdio.h, stdlib.h, string.h, unistd.h | test_autosave.c -- wubuautosave acceptance test (atomic snapshots + recovery) |
| `apps/` | `test_base` | 74 | assert.h, stdio.h, string.h, wububase.h | test_base.c -- unit tests for the shared wububase utilities. |
| `apps/` | `test_cell_cycle` | 120 | cell.h, cell_internal.h, stdio.h, stdlib.h, string.h | test_cell_cycle.c — regression guard for the circular-reference crash in the spreadsheet evaluator ( |
| `apps/` | `test_cell_edit` | 26 | cell.h, cell_internal.h, stdio.h, string.h | Verify that editing a value recomputes dependent formulas immediately. |
| `apps/` | `test_cell_kbd` | 44 | cell.h, stdio.h, stdlib.h, string.h | test_cell_kbd.c -- hop 21: spreadsheet keyboard conformance. Exercises the cell view's key handler:  |
| `apps/` | `test_cell_merge` | 60 | cell.h, cell_internal.h, cell_read.h, stdio.h, stdlib.h, string.h | test_cell_merge.c -- verify cell-merge (write + read-back round trip). wubucell_merge() records a re |
| `apps/` | `test_cell_read` | 81 | cell.h, cell_internal.h, cell_read.h, stdio.h, stdlib.h, string.h | xlsx read path: write a book (shared strings + a SUM formula with cached value), read it back into a |
| `apps/` | `test_chart` | 127 | chart.h, stdio.h, stdlib.h, string.h, wubusvg.h | test_chart.c -- wubuchart acceptance test (SVG chart rendering). |
| `apps/` | `test_cjk_fallback` | 48 | stdio.h, stdlib.h, string.h, wuos_font.h | test_cjk_fallback.c -- N4: on-screen CJK rasterization via font fallback. Draws CJK text through wuo |
| `apps/` | `test_cold_start` | 37 | assert.h, model.h, stdio.h, time.h | test_cold_start.c — ws07#1333: cold start must be fast. Times a from-scratch document creation + a m |
| `apps/` | `test_columns` | 104 | crnn.h, crnn_transcribe.h, image.h, stdio.h, stdlib.h, string.h | test_columns.c -- end-to-end multi-column detection. Generates 1/2/3-column synthetic pages and veri |
| `apps/` | `test_conformance` | 85 | assemble.h, cell.h, show.h, stdio.h, stdlib.h, string.h, word.h | Conformance gate: generate one file of each type with our own apps, then validate each against an IN |
| `apps/` | `test_conv_model` | 135 | conv_bridge.h, docmodel.h, model.h, stdio.h, stdlib.h, string.h | test_conv_model -- headless test for wubuconv_model_to_text. Builds a small wubumodel_doc with a top |
| `apps/` | `test_convert` | 185 | assemble.h, cell.h, conv_map.h, show.h, stdio.h, stdlib.h, string.h, word.h | test_convert -- unified format conversion matrix. Builds a docx, xlsx, pptx, md, csv, odt, ods, odp; |
| `apps/` | `test_convnet` | 135 | convnet.h, math.h, mlp.h, stdio.h, stdlib.h, string.h | test_convnet.c -- end-to-end test for the conv front-end + MLP stack. Build: cc -std=c11 -O2 -Isrc/w |
| `apps/` | `test_convnet3` | 133 | convnet3.h, math.h, mlp.h, omp.h, stdio.h, stdlib.h, string.h | tests/test_convnet3.c -- prove convnet3.c backprop is correct by training the full conv3+MLP end-to- |
| `apps/` | `test_crnn_transcribe` | 115 | crnn.h, crnn_transcribe.h, image.h, stdio.h, stdlib.h, string.h, unistd.h, wubuf | test_crnn_transcribe.c -- regression test for the CRNN line-transcribe document path (crnn_transcrib |
| `apps/` | `test_crnn_transcribe_full` | 91 | crnn.h, crnn_transcribe.h, image.h, stdio.h, stdlib.h, string.h, wubufont.h | test_crnn_transcribe_full.c -- regression test for the CRNN line-transcribe path with the full docum |
| `apps/` | `test_csv` | 78 | cell.h, cell_csv.h, stdio.h, stdlib.h, string.h | test_csv -- RFC 4180 CSV/TSV import + export round-trip for wubucell. Exercises the tricky cases: a  |
| `apps/` | `test_deflate` | 127 | deflate.h, inflate.h, stdio.h, stdlib.h, string.h, zlib.h | Round-trip: wubuzip_deflate() then wubuzip_inflate() must reproduce the input exactly. Also cross-ch |
| `apps/` | `test_deflate_roundtrip` | 50 | assert.h, deflate.h, inflate.h, stdio.h, stdlib.h, string.h | test_deflate_roundtrip.c — ws07#1338: we use OUR OWN dependency-free DEFLATE codec (wubuzip), not zl |
| `apps/` | `test_doc_cache` | 75 | doc_cache.h, model.h, stat.h, stdio.h, stdlib.h, string.h | test_doc_cache.c — ws07#1342: re-open is instant + never stale. Creates a REAL temp file so the (pat |
| `apps/` | `test_docfmt` | 60 | docfmt.h, json.h, stdio.h, stdlib.h, string.h | test_docfmt.c -- unit tests for the docmodel -> alternate-format serializers. |
| `apps/` | `test_docfmt2` | 55 | docfmt.h, json.h, stdio.h, stdlib.h, string.h | test_docfmt2.c -- unit test for TEI serialization + reading-order/confidence fields round-tripping t |
| `apps/` | `test_docx_cell_merge` | 87 | docx_document.h, model.h, stdio.h, stdlib.h, string.h | test_cell_merge.c -- H6b fidelity matrix row: merged cells. w:gridSpan (horizontal) and w:vMerge (ve |
| `apps/` | `test_draw` | 51 | draw.h, stdio.h, stdlib.h, string.h, wubusvg.h | test_draw.c -- wubudraw acceptance test (vector shapes -> SVG). |
| `apps/` | `test_drop` | 163 | input.h, png.h, stdio.h, stdlib.h, string.h, wubudoc.h | test_drop.c -- drag/drop + bracketed-paste ingestion. Covers the user's core requirement: "drag docu |
| `apps/` | `test_edit` | 135 | assemble.h, cell.h, cell_read.h, docmodel.h, docx_text.h, edit.h, reader.h, show | Structure-preserving round-trip for docx/xlsx/pptx: build a doc with a Heading1, a bold paragraph an |
| `apps/` | `test_epub` | 91 | epub.h, model.h, stdio.h, stdlib.h, string.h, unistd.h | test_epub.c -- wubuepub acceptance test (valid EPUB3 container). |
| `apps/` | `test_epub_model_roundtrip` | 138 | epub.h, model.h, stdio.h, stdlib.h, string.h, unistd.h | test_epub_model_roundtrip.c -- EPUB write -> unified-model load round trip. Proves the loader half ( |
| `apps/` | `test_exp` | 52 | exp.h, model.h, stdio.h, stdlib.h, string.h, ublayout.h | test_exp.c -- exporters consume the layout (EXP cluster). |
| `apps/` | `test_extract` | 118 | assemble.h, cell.h, docx_text.h, reader.h, show.h, stdio.h, stdlib.h, string.h,  | End-to-end extraction test: build a docx, an xlsx (shared strings), and a pptx, read each back throu |
| `apps/` | `test_fidelity_matrix` | 78 | docx_document.h, model.h, stdio.h, stdlib.h, string.h | test_fidelity_matrix.c -- F2: the fidelity matrix as a TESTED artifact. Matrix rows (feature x load/ |
| `apps/` | `test_flat_epub` | 124 | cell.h, docmodel.h, epub.h, odf.h, show.h, stdio.h, stdlib.h, string.h | test_flat_epub -- flat ODF (.fodt/.fods/.fodp) round trips + EPUB writer. Gate 1 (flat ODF): write e |
| `apps/` | `test_float_wrap` | 93 | docx_document.h, model.h, stdio.h, stdlib.h, string.h, ublayout.h | test_float_wrap.c -- H3 fidelity matrix row: floating images. wp:anchor with extent + wrapSquare mus |
| `apps/` | `test_font_subset` | 80 | font_subset.h, stdio.h, stdlib.h, string.h, wubufont.h | test_font_subset.c -- N1: glyph subsetting. Subset DejaVuSans to the codepoints used by a small docu |
| `apps/` | `test_fontbank` | 219 | fontbank.h, stdio.h, stdlib.h, string.h, wubufont.h, wubuocr.h | test_fontbank.c -- multi-font template bank recognition (end-to-end). Builds a font bank from every  |
| `apps/` | `test_foreign` | 90 | cell.h, cell_read.h, show.h, show_read.h, stdio.h, stdlib.h, string.h | test_foreign -- robustness gate: read OOXML files produced by INDEPENDENT tools (openpyxl, python-pp |
| `apps/` | `test_formula` | 400 | eval.h, funcs.h, math.h, registry.h, stdio.h, stdlib.h, string.h, value.h | WuBuOffice -- tests/test_formula Validates the wubuformula engine: parsing, evaluation, functions, r |
| `apps/` | `test_gauntlet` | 194 | fontbank.h, gauntlet.h, image.h, latin1.h, page_compose.h, stdio.h, stdlib.h, st | test_gauntlet.c -- OCR robustness battery (multi-font bank). Loads every available system font, buil |
| `apps/` | `test_gemm_nt` | 30 | gpu_blas.h, math.h, stdio.h, stdlib.h | NT check: C[MxN]=A[MxK]*B[NxK]^T |
| `apps/` | `test_gpu_blas` | 77 | gpu_blas.h, math.h, stdint.h, stdio.h, stdlib.h | test_gpu_blas.c -- verify the CUDA GEMM/conv match the CPU reference. Uses a deterministic LCG so it |
| `apps/` | `test_grabbag` | 66 | docx_document.h, model.h, stdio.h, stdlib.h, string.h | test_grabbag.c -- F1 fidelity doctrine: unknown constructs must be PRESERVED (WUBUMODEL_FOREIGN with |
| `apps/` | `test_gru_asan` | 30 | gpu_blas.h, gru.h, stdio.h, stdlib.h, string.h | vary T (like training) |
| `apps/` | `test_gru_cmp` | 77 | gpu_blas.h, gru.c, gru.h, math.h, stdint.h, stdio.h, stdlib.h | test_gru_cmp.c -- compares the GPU-matmul GRU (src/gpu/gru_gpu.c, linked as the real gru_* symbols i |
| `apps/` | `test_gru_gpu` | 88 | gpu_blas.h, gru.h, math.h, stdint.h, stdio.h, stdlib.h | test_gru_gpu.c -- finite-difference gradcheck of the GPU-matmul GRU. Proves gru_gpu.c forward + BPTT |
| `apps/` | `test_imageops` | 152 | binarize.h, image.h, math.h, stdio.h, stdlib.h, string.h | test_imageops.c -- unit tests for Sauvola binarization, despeckle, auto-crop and empty-page helpers  |
| `apps/` | `test_imgops` | 79 | image.h, imgops.h, stdio.h, stdlib.h, string.h | test_imgops.c -- unit tests for image preprocessing ops (imgops.c). |
| `apps/` | `test_inflate` | 42 | inflate.h, stdio.h, stdlib.h, string.h, zlib.h | Verify our from-scratch inflate decodes zlib-compressed data correctly. |
| `apps/` | `test_latin1` | 104 | fontbank.h, gauntlet.h, image.h, latin1.h, page_compose.h, stdio.h, stdlib.h, st | test_latin1.c -- Latin-1 tier (English-first, Latin second) for the multi-font bank. Verifies the us |
| `apps/` | `test_layout` | 101 | model.h, stdio.h, stdlib.h, string.h, ublayout.h | test_layout.c -- central pipeline check (model -> pages -> runs). |
| `apps/` | `test_legacy_pdf` | 205 | cell.h, docmodel.h, legacy.h, math.h, pdf.h, show.h, stdio.h, stdlib.h, string.h | test_legacy_pdf -- legacy binary readers (.xls/.doc/.ppt) + PDF writer. Two independent-oracle gates |
| `apps/` | `test_load_async` | 63 | load_async.h, model.h, pthread.h, stdio.h, string.h | test_load_async.c — ws07#1334: load off the UI thread, callback on the joining thread. Explicit CHEC |
| `apps/` | `test_math` | 75 | math.h, stdio.h, stdlib.h, string.h, wubusvg.h | test_math.c -- wubumath acceptance test (math expression -> SVG). |
| `apps/` | `test_md` | 100 | assemble.h, doc_md.h, docmodel.h, reader.h, stdio.h, stdlib.h, string.h, word.h | test_md -- Markdown import + Markdown/HTML export over the document model. Loop: write a Markdown fi |
| `apps/` | `test_mlp` | 58 | math.h, mlp.h, stdio.h, stdlib.h, string.h | test_mlp.c -- unit tests for the dependency-free MLP module. Build: cc -std=c11 -O2 -Isrc/wubuocr te |
| `apps/` | `test_model` | 62 | assert.h, model.h, stdio.h, string.h | WS11 model green-baseline smoke test. Covers: doc lifecycle, node create with stable id, RUN text, s |
| `apps/` | `test_model_io` | 50 | assert.h, docx_text.h, model.h, reader.h, stdio.h, stdlib.h, string.h | WS11 model -> OOXML round-trip: build a model, write .docx, read it back, and confirm the text survi |
| `apps/` | `test_model_io_roundtrip` | 146 | doc_cache.h, load_async.h, model.h, stat.h, stdio.h, stdlib.h, string.h | test_model_io_roundtrip.c — ws05#0885 + ws05#0884/#082 + ws07#1342/#1334. Structural round-trip: bui |
| `apps/` | `test_nfc` | 34 | crnn_transcribe.h, stdio.h, string.h | test_nfc.c -- unit test for Unicode NFC Latin composition (#96). Feeds combining-mark sequences and  |
| `apps/` | `test_ocr_realpage` | 98 | crnn.h, crnn_transcribe.h, image.h, ocr_render.h, stdio.h, stdlib.h, string.h, w | test_ocr_realpage.c -- regression test for the deskew + line-segmentation front door on a ROTATED mu |
| `apps/` | `test_odf` | 173 | cell.h, docmodel.h, odf.h, show.h, stdio.h, stdlib.h, string.h | test_odf -- OpenDocument (odt/ods/odp) write + read round-trips. For each format: build a model, wri |
| `apps/` | `test_odttf` | 62 | odttf.h, stdio.h, stdlib.h, string.h, wubufont.h | test_odttf.c -- H5: embedded font de-obfuscation. Round-trip: take a known 64-byte header, XOR the f |
| `apps/` | `test_pdf_geometry` | 70 | exp.h, model.h, stdio.h, stdlib.h, string.h, ublayout.h | test_pdf_geometry.c -- hop 8 fidelity row: geometry-aware PDF export. A model with a Heading1 + body |
| `apps/` | `test_pdf_i18n` | 67 | exp.h, model.h, stdio.h, stdlib.h, string.h, ublayout.h | test_pdf_i18n.c -- hop 14: non-Latin runs export through the Unicode (Type0/Identity-H + UTF-16BE he |
| `apps/` | `test_pdfua` | 50 | image.h, pdfsearch.h, stdio.h, stdlib.h, string.h, unistd.h | test_pdfua.c -- unit test for PDF/UA structure tree (#93) + language tag (#46/#94) plumbing through  |
| `apps/` | `test_png` | 117 | image.h, png.h, stdio.h, stdlib.h, string.h, wubupng.h, zlib.h | test_png.c -- validate the shared wubupng encoder independently. The encoder is proven correct again |
| `apps/` | `test_pptx_app_roundtrip` | 37 | pptx_write.h, stdio.h, stdlib.h, string.h | test_pptx_app_roundtrip.c -- hop 18 end-to-end: write a .pptx via the assembler, read it back via wu |
| `apps/` | `test_pptx_deck_cycle` | 42 | pptx_write.h, stdio.h, stdlib.h, string.h | test_pptx_deck_cycle.c -- N2 end-to-end: multi-slide deck round-trips through write_multi -> read_mu |
| `apps/` | `test_pptx_multi` | 71 | docx_text.h, package.h, pptx_write.h, reader.h, stdio.h, stdlib.h, string.h | test_pptx_multi.c -- hop 17: multi-slide pptx assembly. A 3-slide deck must produce sldIdLst with 3  |
| `apps/` | `test_pptx_roundtrip` | 59 | docx_text.h, package.h, pptx_write.h, reader.h, stdio.h, stdlib.h, string.h | test_pptx_roundtrip.c -- H13 fidelity row: slide deck round-trip. Write a .pptx via wubuoxml_pptx_wr |
| `apps/` | `test_reader` | 42 | assemble.h, reader.h, stdio.h, stdlib.h, string.h, word.h | Generate a docx, read it back with the OPC reader, and confirm we can extract the paragraph text. Ve |
| `apps/` | `test_readers` | 78 | assemble.h, conv_map.h, docmodel.h, readers.h, stdio.h, stdlib.h, string.h, word | test_readers -- reverse-direction readers for WuBuOffice convert. Builds a .docx with the wubuword g |
| `apps/` | `test_rtf_json` | 98 | assemble.h, cell.h, doc_md.h, doc_rtf.h, docmodel.h, model_json.h, reader.h, sho | test_rtf_json -- RTF writer + JSON serializers for all three models. Builds a doc model (via md impo |
| `apps/` | `test_run_toggle` | 79 | stdio.h, stdlib.h, string.h, view_doc_internal.h | test_run_toggle.c -- N3: direct character formatting (Ctrl+B/Ctrl+I surface). Toggle bold on a parag |
| `apps/` | `test_script` | 43 | script.h, stdio.h, stdlib.h, string.h | test_script.c -- wubuscript acceptance test (sandboxed formula host). |
| `apps/` | `test_settings` | 81 | assert.h, settings.h, stdio.h, string.h | test_settings.c -- wubusettings module check. |
| `apps/` | `test_shape` | 43 | shape.h, stdio.h, string.h | test_shape.c -- wubushape Bidi reorder check (INT-7). |
| `apps/` | `test_shortcut_conformance` | 82 | stdio.h, stdlib.h, string.h | test_shortcut_conformance.c -- U1: keyboard shortcut conformance gate. Research (Microsoft keyboard- |
| `apps/` | `test_spell` | 86 | spell.h, stdio.h, stdlib.h, string.h | test_spell.c -- wubuspell acceptance test (dependency-free spell checker). |
| `apps/` | `test_stylebank` | 54 | math.h, stdio.h, string.h, stylebank.h | test_stylebank.c -- smoke test for the 64MB multi-style expert bank. Verifies: create(56+8), slot ki |
| `apps/` | `test_table_styles` | 74 | stdio.h, stdlib.h, string.h, table_styles.h | test_table_styles.c -- H6 fidelity matrix row: table styles. A styles.xml with a basedOn chain + con |
| `apps/` | `test_tbl_styles_e2e` | 90 | docx_document.h, model.h, stdio.h, stdlib.h, string.h | test_tbl_styles_e2e.c -- H6d end-to-end: a docx (document.xml + styles.xml) whose table references G |
| `apps/` | `test_toc` | 464 | model.h, settings.h, stdio.h, stdlib.h, string.h, toc.h, ublayout.h, unistd.h | test_toc.c -- DOC-54 TOC generator + UXA-41 high-contrast settings. |
| `apps/` | `test_word_lists` | 100 | assemble.h, reader.h, stdio.h, stdlib.h, string.h, word.h | test_word_lists.c -- prove the WuBuWord list feature (bullet + numbered) produces a valid .docx: wor |
| `apps/` | `test_wubudoc` | 225 | json.h, stdio.h, stdlib.h, string.h, wubudoc.h, zip.h | test_wubudoc.c -- unified ingestion + creation facade (semantic engine). Verifies "treat as media, n |
| `apps/` | `test_wubuedit` | 139 | edit.h, stdio.h, stdlib.h, string.h | test_wubunote.c -- pure EditBuf model (Notepad++-class editing core). |
| `apps/` | `test_wubufont` | 206 | stdio.h, stdlib.h, string.h, woff.h, wubufont.h | test_wubufont.c -- validate the clean-room SFNT parser against a REAL font shipped with the system ( |
| `apps/` | `test_wubunote_ctrl` | 121 | input.h, note_controller.h, stdio.h, stdlib.h, string.h | test_wubunote_ctrl.c -- pure wubunote interaction controller. Tests the REAL event->state mapping in |
| `apps/` | `test_wubuocr` | 171 | binarize.h, components.h, image.h, layout.h, stdio.h, stdlib.h, string.h, wubuoc | test_wubuocr.c -- image->document pipeline: decode, Otsu, XY-cut reading order, connected components |
| `apps/` | `test_wubuocr_recognize` | 188 | font8x8.h, image.h, recognize.h, stdio.h, stdlib.h, string.h, wubuocr.h | test_wubuocr_recognize.c -- zoning + 1-NN glyph recognizer. The load-bearing assertion: render known |
| `apps/` | `test_wubusvg` | 226 | stdio.h, stdlib.h, string.h, wubusvg.h | test_wubusvg.c -- SVG ingest + regurgitate round-trip. Builds a document from a known SVG, asserts t |
| `apps/` | `test_wubusvg_agent` | 98 | agent.h, json.h, stdio.h, stdlib.h, string.h | test_wubusvg_agent.c -- NDJSON agent protocol for wubusvg. Drives the same dispatcher wubuOS uses an |
| `apps/` | `test_wubutui` | 308 | draw.h, input.h, screen.h, stdio.h, stdlib.h, string.h | test_wubutui.c -- pure TUI core: screen buffer, word wrap, key decode, draw. No TTY required: everyt |
| `apps/` | `test_wubuview` | 95 | docflat.h, stdio.h, stdlib.h, string.h | test_wubuview.c -- docflat model->text flattening (pure, no TTY). |
| `apps/` | `test_wubuview_ctrl` | 159 | controller.h, input.h, stdio.h, string.h | test_wubuview_ctrl.c -- pure interaction controller (viewport + tabs). |
| `apps/` | `test_wubuxml_parser` | 141 | parser.h, stdio.h, stdlib.h, string.h | test_wubuxml_parser.c — regression guard for ws05#0884 parser. The parser MUST emit element-END even |
| `apps/` | `test_wubuzip` | 31 | assert.h, stdio.h, string.h, zip.h | Known CRC-32 vectors (zlib-compatible). |
| `apps/` | `test_xlsample` | 35 | cell.h, cell_internal.h, cell_read.h, stdio.h | expect A1=14, B1=49, A3=7, B4=15 |
| `apps/` | `test_xlsx` | 50 | docfmt.h, reader.h, stdio.h, stdlib.h, string.h | test_xlsx.c -- unit test for docfmt_to_xlsx (Excel OOXML export). Builds a small docmodel JSON (a pa |
| `apps/` | `test_zoning` | 47 | math.h, stdio.h, stdlib.h, string.h, zoning.h | test_zoning.c -- unit tests for the ZoningExtractor module. Build: cc -std=c11 -O2 -Isrc/wubuocr tes |
| `apps/` | `bench_conv3` | 39 | convnet3.h, stdio.h, stdlib.h, string.h, time.h | bench_conv3.c -- measure forward+backward throughput of the im2col+GEMM conv engine. Pure compute, n |
| `apps/` | `bench_old` | 89 | stdio.h, stdlib.h, string.h, time.h | bench_old.c -- the OLD strided-gather conv (pre-im2col) implemented standalone, to A/B against bench |
| `apps/` | `bisect_pool2` | 62 | convnet3.h, math.h, stdio.h, stdlib.h | bisect_pool2.c -- test stage1 pool -> stage2 conv gradient chain (no stage3) |
| `apps/` | `bisect_pool_conv` | 61 | convnet3.h, math.h, stdio.h, stdlib.h | bisect_pool_conv.c -- test stage1 pool -> stage2 conv gradient chain |
| `apps/` | `charsetgen` | 69 | image.h, stdio.h, stdlib.h, string.h, wubufont.h | charsetgen.c -- for a given font, scan the WuBuOffice SCRIPT_RANGES and emit the codepoints that act |
| `apps/` | `conv_feat_test` | 64 | convnet3.h, math.h, stdio.h, stdlib.h, string.h | conv_feat_test.c -- test conv features for different EMNIST inputs. C11, no deps. Compile: cc -std=c |
| `apps/` | `conv_feats` | 39 | convnet3.h, stdio.h, stdlib.h, string.h | conv_feats.c -- load a trained conv3, dump its 256-d features for the test set to a text file (one r |
| `apps/` | `crnn_doc_demo` | 122 | crnn.h, image.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubufont.h | crnn_doc_demo.c -- END-TO-END document transcription: multi-line page image  ->  horizontal-projecti |
| `apps/` | `crnn_img_train` | 85 | crnn.h, image.h, math.h, stdio.h, stdlib.h, string.h, time.h | crnn_img_train.c -- end-to-end IMAGE training: proves conv trunk trains. Synthesizes line images: ea |
| `apps/` | `crnn_lex_train` | 149 | crnn.h, math.h, ocr_render.h, stdio.h, stdlib.h, string.h, time.h | crnn_lex_train.c -- REAL-WORD OCR training driven by a certifiable frequency lexicon (data/wordlists |
| `apps/` | `crnn_ocr_train` | 330 | crnn.h, image.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubufont.h | crnn_ocr_train.c -- REAL glyph OCR: render text via wubufont, train CRNN+CTC. Charset: 'A'..'Z' -> c |
| `apps/` | `crnn_photo_demo` | 115 | crnn.h, math.h, ocr_render.h, png.h, png_encode.h, stdio.h, stdlib.h, string.h,  | crnn_photo_demo.c -- FULL Office-Lens front door (multilingual, lexicon-driven): render real frequen |
| `apps/` | `crnn_reload` | 58 | crnn.h, image.h, stdio.h, stdlib.h, string.h, wubufont.h | crnn_reload.c -- load an existing saved model, decode a probe, re-save, reload, decode again. Isolat |
| `apps/` | `crnn_roundtrip` | 77 | crnn.h, image.h, stdio.h, stdlib.h, string.h, wubufont.h | crnn_roundtrip.c -- prove crnn_save/crnn_load fidelity using only the public API: load a font, creat |
| `apps/` | `crnn_train` | 75 | crnn.h, image.h, math.h, stdio.h, stdlib.h, string.h, time.h | crnn_train.c -- synthetic sequence-learning sanity test for the CRNN. We synthesize "line" sequences |
| `apps/` | `crnn_warp_train` | 204 | crnn.h, image.h, lens.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubufont.h | crnn_warp_train.c -- MATCHED-GEOMETRY OCR training. The photo inference path is: render -> perspecti |
| `apps/` | `ctc_gradcheck` | 58 | ctc.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | ctc_gradcheck.c -- finite-difference check of the REAL src/wubuocr/ctc.c Build: cc -std=c11 -O2 -I s |
| `apps/` | `ctc_test` | 157 | math.h, stdint.h, stdio.h, stdlib.h, string.h | ctc_test.c -- minimal CTC forward-backward + loss + gradient, standalone. Validates the algorithm th |
| `apps/` | `emnist_eval` | 255 | binarize.h, recognize.h, stdio.h, stdlib.h, string.h | emnist_eval.c -- train + evaluate the WuBuOCR lightweight zoning recognizer on the EMNIST Letters be |
| `apps/` | `emnist_infer` | 123 | math.h, mlp.h, stdio.h, stdlib.h, string.h, zoning.h | emnist_infer.c -- lightweight C11 OCR inference (dependency-free). Loads a wubu-trained MLP (see emn |
| `apps/` | `emnist_infer_conv` | 130 | convnet3.h, math.h, mlp.h, stdio.h, stdlib.h, string.h | emnist_infer_conv.c -- ultra-light conv3+MLP OCR inference (dependency-free). Loads conv3.wts (ConvN |
| `apps/` | `emnist_train` | 301 | math.h, mlp.h, stdio.h, stdlib.h, string.h, wubumath.h, zoning.h | emnist_train.c -- train the lightweight wubu OCR MLP on EMNIST Letters. Pipeline: load IDX corpus -> |
| `apps/` | `emnist_train_conv` | 353 | convnet.h, math.h, mlp.h, stdio.h, stdlib.h, string.h | emnist_train_conv.c -- train an ULTRA-LIGHT conv+MLP on EMNIST Letters. Pipeline: load IDX corpus -> |
| `apps/` | `emnist_train_conv3` | 437 | convnet3.h, math.h, mlp.h, pthread.h, stdio.h, stdlib.h, string.h, unistd.h | emnist_train_conv3.c -- end-to-end training of conv3 (3-stage MED front-end) + MLP on any 28x28 IDX  |
| `apps/` | `emnist_train_fast` | 293 | convnet3.h, math.h, mlp.h, pthread.h, stdio.h, stdlib.h, string.h, unistd.h | emnist_train_fast.c -- Training harness for conv3 + MLP on EMNIST letters. Fixes applied (2026-07-22 |
| `apps/` | `featprobe` | 43 | convnet3.h, math.h, stdio.h, stdlib.h, string.h | featprobe.c -- inspect conv3 feature statistics to detect INORM collapse. Build: cc -std=c11 -D_POSI |
| `apps/` | `fontcpgrep` | 41 | image.h, stdio.h, stdlib.h, string.h, wubufont.h | fontcpgrep.c -- for each font given, try rasterizing probe codepoints from several scripts; report w |
| `apps/` | `gen_pgmpage` | 77 | image.h, stdio.h, stdlib.h, string.h, time.h, wubufont.h | gen_pgmpage.c -- emit a synthetic multi-line Latin page as PGM (Netpbm), painted with the wubufont r |
| `apps/` | `gen_realpage` | 121 | image.h, math.h, ocr_render.h, png.h, png_encode.h, stdio.h, stdlib.h, string.h, | gen_realpage.c -- synthesize a multi-line "document" page as a PNG with photo-like distortions (rota |
| `apps/` | `gradcheck_conv3` | 103 | convnet3.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | gradcheck_conv3.c -- numeric vs analytic gradient check for convnet3_backward. IMPORTANT: a ReLU+max |
| `apps/` | `gradcheck_conv3_bisect` | 66 | convnet3.h, math.h, stdio.h, stdlib.h | gradcheck_conv3_bisect.c -- isolate the MED bug: stage1 pool -> stage2 conv |
| `apps/` | `gradcheck_conv3_med` | 75 | convnet3.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | gradcheck_conv3_med.c -- numeric vs analytic gradient check for CONV_MED with pooling. Tests the rea |
| `apps/` | `gradcheck_med` | 78 | convnet3.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | gradcheck_med.c -- numeric vs analytic gradient check for MED config (with pooling + hard ReLU). Tes |
| `apps/` | `gradcheck_pool` | 45 | convnet3.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | gradcheck_pool.c -- isolate the pooling+leaky backward path. Single conv stage WITH maxpool, leak=1  |
| `apps/` | `gradcheck_pool_valid` | 50 | convnet3.h, math.h, stdint.h, stdio.h, stdlib.h | gradcheck_pool_valid.c -- single-stage conv+pool backward, REAL (fdim>0), no INORM. Isolates the con |
| `apps/` | `gradcheck_real` | 77 | convnet3.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | gradcheck_real.c -- gradcheck conv backward with the EXACT training config: leak=0.1 (hard-ish ReLU) |
| `apps/` | `gru_test` | 85 | gru.h, math.h, stdio.h, stdlib.h, string.h | gru_test.c -- numerical-gradient check for GRU forward+backward (BPTT). Builds a small GRU, runs fwd |
| `apps/` | `image2doc` | 411 | conv_map.h, crnn.h, crnn_transcribe.h, docfmt.h, image.h, imgops.h, lens.h, lexi | image2doc.c -- OFFICE LENS: any photo (JPEG/PNG) -> editable document with spatial formatting. image |
| `apps/` | `mlp_gradcheck` | 61 | math.h, mlp.h, stdio.h, stdlib.h, string.h | mlp_gradcheck.c -- finite-difference vs analytic grad for mlp_backward_smooth. Loss = sum_c dfeat[c] |
| `apps/` | `mlp_real_test` | 57 | convnet3.h, mlp.h, stdio.h, stdlib.h, string.h | mlp_real_test.c -- does the MLP learn on REAL EMNIST conv features? Loads EMNIST, runs conv3_forward |
| `apps/` | `mlp_smoke` | 60 | math.h, mlp.h, stdio.h, stdlib.h | mlp_smoke.c -- does the optimizer actually MOVE weights? Two tests: A) real training path: accumulat |
| `apps/` | `mlp_train_feats` | 68 | math.h, mlp.h, stdio.h, stdlib.h, string.h | mlp_train_feats.c -- isolate the C MLP: train it on pre-extracted conv features (text dump) + labels |
| `apps/` | `ocl_gru_test` | 65 | gru.h, gru_layout.h, math.h, stdio.h, stdlib.h, string.h | ocl_gru_test.c -- verifies the OpenCL GRU forward (#98) matches the scalar CPU GRU forward exactly.  |
| `apps/` | `ocrcorpus` | 106 | binarize.h, dft.h, fontdir.h, image.h, json.h, math.h, stdio.h, stdlib.h, string | ocrcorpus.c -- generate a multi-script LABELED glyph corpus for training. The user's "wide good sour |
| `apps/` | `ocringest` | 342 | binarize.h, dft.h, fontdir.h, goldplace.h, image.h, json.h, math.h, page_compose | ocringest.c -- end-to-end coordinate-aware OCR ingestion demo driver. Ties the whole vision together |
| `apps/` | `ocrserve` | 179 | errno.h, in.h, inet.h, signal.h, socket.h, stdio.h, stdlib.h, string.h, unistd.h | ocrserve.c -- minimal dependency-free OCR micro-service. Wraps the image2doc pipeline over HTTP so o |
| `apps/` | `ocrwatch` | 77 | dirent.h, errno.h, stat.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wait.h | ocrwatch.c -- OCR watch-folder daemon. Polls a directory for new image files (jpg/png/pgm/bmp) and,  |
| `apps/` | `rnn_dx` | 47 | math.h, rnn.h, stdio.h, stdlib.h, string.h | rnn_dx.c -- minimal, unambiguous dx check. loss = sum_t ||h(t)||^2 (fwd only). |
| `apps/` | `rnn_test` | 112 | math.h, rnn.h, stdio.h, stdlib.h, string.h | rnn_test.c -- finite-difference check of LSTM forward+backward. We chain LSTM -> a small linear head |
| `apps/` | `test_convnet3_im2col` | 126 | convnet3.h, math.h, stdio.h, stdlib.h, string.h | test_convnet3_im2col.c -- verify the im2col+GEMM rewrite of convnet3 is NUMERICALLY equivalent to th |
| `apps/` | `test_grad_chain` | 68 | convnet3.h, math.h, stdio.h, stdlib.h, string.h | test_grad_chain.c -- full gradient-chain check for the im2col convnet3. Compares analytic gradient ( |
| `apps/` | `test_math` | 36 | crnn_transcribe.h, stdio.h, string.h | test_math.c -- verifies the math/equation line detector (#48). Feeds known math and prose strings an |
| `apps/` | `test_qr` | 114 | qr.h, stdio.h, stdlib.h, string.h | test_qr.c -- verifies the QR codec (#49): 1) encode -> module matrix -> decode (direct matrix round- |
| `apps/` | `train_conv3_clean` | 234 | convnet3.h, math.h, mlp.h, stdio.h, stdlib.h, string.h | train_conv3_clean.c -- GROUND-UP rebuild of the conv3+MLP EMNIST letters trainer. Design principles  |
| `apps/` | `train_mt` | 220 | convnet3.h, math.h, mlp.h, pthread.h, stdio.h, stdlib.h, string.h, time.h | train_mt.c -- MULTITHREADED (4-core) mini-batch data-parallel trainer for the conv3+MLP stack. Same  |
| `apps/` | `train_persample` | 136 | convnet3.h, math.h, mlp.h, stdio.h, stdlib.h, string.h, time.h | train_persample.c -- END-TO-END conv3+MLP trainer using the PROVEN per-sample SGD path (same as test |
| `apps/` | `trainer_cli` | 173 | stdio.h, stdlib.h, string.h, trainer.h, unistd.h | trainer_cli.c -- CLI for modular trainer |
| `apps/` | `main` | 119 | agent.h, doc.h, docs.h, smoke.h, stdio.h, stdlib.h, string.h, ui.h, ui_gfx.h, ui | wubupad.c -- the WuBuPad entry point. Two modes: 1. NDJSON agent mode (default, when stdin is not a  |
| `apps/` | `shot` | 92 | doc.h, docs.h, stdio.h, stdlib.h, string.h, ui.h, ui_gfx.h | shot.c -- headless WuBuPad GUI screenshot tool. Opens the real gfx backend (SDL2 + FreeType2) under  |
| `apps/` | `smoke` | 194 | autoindent.h, buffer.h, command.h, complete.h, diff.h, doc.h, encode.h, fuzzy.h, | smoke.c -- module-exercise smoke test for the wubupad binary. Calls at least one function from every |
| `apps/` | `CMakeCCompilerId` | 880 |  | cv-qualifiers did not exist in K&R C |
| `apps/` | `CMakeCCompilerId` | 880 |  | cv-qualifiers did not exist in K&R C |
| `apps/` | `agent` | 285 | agent.h, buffer.h, diff.h, doc.h, docs.h, encode.h, lex.h, search.h, stdio.h, st | agent.c -- wubuOS protocol dispatcher over the verified headless core. Self-contained C11; reuses Do |
| `apps/` | `autoindent` | 52 | autoindent.h, ctype.h, string.h | autoindent.c -- smart typing / auto-indent. See autoindent.h. |
| `apps/` | `test_autoindent` | 22 | autoindent.h, stdio.h, string.h | test_autoindent.c |
| `apps/` | `buffer` | 253 | buffer.h, stdlib.h, string.h | buffer.c -- piece-table implementation. See buffer.h. |
| `apps/` | `command` | 68 | command.h, stdlib.h, string.h | command.c -- command registry. See command.h. |
| `apps/` | `test_command` | 27 | command.h, stdio.h, string.h | test_command.c |
| `apps/` | `complete` | 117 | complete.h, ctype.h, lex.h, stdlib.h, string.h | complete.c -- word completion + symbol extraction (see complete.h). |
| `apps/` | `diff` | 111 | diff.h, stdio.h, stdlib.h, string.h | diff.c -- line diff via longest-common-subsequence (Wagner-Fischer style). Self-contained C11. Produ |
| `apps/` | `doc` | 332 | buffer.h, doc.h, stdlib.h, string.h | doc.c -- document model with linear undo/redo and cursor/selection. |
| `apps/` | `docs` | 121 | docs.h, encode.h, stdint.h, stdio.h, stdlib.h, string.h | docs.c -- multi-document session. Reuses Doc; owns metadata + ordering. |
| `apps/` | `encode` | 137 | encode.h, stdlib.h, string.h | encode.c -- encoding detect + convert to UTF-8. Self-contained C11. |
| `apps/` | `fuzzy` | 69 | ctype.h, fuzzy.h, string.h | fuzzy.c -- Atom-style fuzzy scorer. See fuzzy.h. |
| `apps/` | `test_fuzzy` | 27 | fuzzy.h, stdio.h, string.h | test_fuzzy.c |
| `apps/` | `json` | 233 | ctype.h, json.h, stdio.h, stdlib.h, string.h | json.c -- minimal JSON parse + emit. Self-contained C11. |
| `apps/` | `lex` | 419 | ctype.h, lex.h, stdlib.h, string.h | lex.c -- language registry + C and JSON lexers. Self-contained C11. |
| `apps/` | `mdpreview` | 136 | ctype.h, mdpreview.h, stdio.h, stdlib.h, string.h | mdpreview.c -- Markdown -> HTML. See mdpreview.h. |
| `apps/` | `test_mdpreview` | 37 | mdpreview.h, stdio.h, stdlib.h, string.h | test_mdpreview.c |
| `apps/` | `minimap` | 50 | ctype.h, minimap.h, stdlib.h, string.h | minimap.c -- code minimap. See minimap.h. |
| `apps/` | `test_minimap` | 21 | minimap.h, stdio.h, string.h | test_minimap.c |
| `apps/` | `multicursor` | 36 | multicursor.h, stdlib.h, string.h | multicursor.c -- multiple cursors. See multicursor.h. |
| `apps/` | `test_multicursor` | 31 | multicursor.h, stdio.h, stdlib.h, string.h | test_multicursor.c -- parallel insert over a buffer shim. |
| `apps/` | `palette` | 118 | fuzzy.h, palette.h, stdio.h, stdlib.h, string.h | palette.c -- command palette. See palette.h. |
| `apps/` | `test_palette` | 40 | command.h, palette.h, stdio.h, string.h | test_palette.c |
| `apps/` | `pkgmgr` | 197 | dirent.h, json.h, pkgmgr.h, stat.h, stdio.h, stdlib.h, string.h | pkgmgr.c -- package manager. See pkgmgr.h. |
| `apps/` | `test_pkgmgr` | 47 | pkgmgr.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h | test_pkgmgr.c -- uses a temp dir with a fake package.json. |
| `apps/` | `search` | 337 | ctype.h, search.h, stdint.h, stdio.h, stdlib.h, string.h | search.c -- literal search + Thompson NFA regex. Self-contained C11. Thompson NFA construction (Russ |
| `apps/` | `shape` | 208 | fribidi.h, ft2build.h, hb-ft.h, hb.h, shape.h, stdlib.h, string.h | shape.c -- HarfBuzz + FriBidi shaping (see shape.h). Implementation is compiled only when HarfBuzz a |
| `apps/` | `snippet` | 98 | snippet.h, stdlib.h, string.h | snippet.c -- snippets engine. See snippet.h. |
| `apps/` | `test_snippet` | 39 | snippet.h, stdio.h, stdlib.h, string.h | test_snippet.c -- exercises the snippet engine over a simple buffer shim. |
| `apps/` | `transform` | 137 | ctype.h, stdlib.h, string.h, strings.h, transform.h | transform.c -- text transforms: sort selected lines and case conversion. Headless, reusable by the a |
| `apps/` | `test_treeview` | 49 | stat.h, stdio.h, stdlib.h, string.h, treeview.h, unistd.h | test_treeview.c -- uses a temp dir tree + fake porcelain. |
| `apps/` | `treeview` | 162 | dirent.h, stat.h, stdio.h, stdlib.h, string.h, treeview.h | treeview.c -- project tree + git status. See treeview.h. |
| `apps/` | `ui` | 802 | complete.h, ctype.h, doc.h, docs.h, lex.h, stdio.h, stdlib.h, string.h, transfor | ui.c -- the UI controller: owns view state (scroll) and translates UI commands into opaque Doc mutat |
| `apps/` | `ui_atom` | 138 | command.h, fuzzy.h, palette.h, pkgmgr.h, stdio.h, stdlib.h, string.h, transform. | ui_atom.c -- Atom subsystem wiring for the WuBuPad UI controller. Owns the global CommandRegistry +  |
| `apps/` | `ui_find` | 162 | search.h, stdint.h, stdlib.h, string.h, ui_find.h | ui_find.c -- find/replace controller (see ui_find.h). |
| `apps/` | `ui_gfx` | 700 | SDL.h, ft2build.h, shape.h, stdio.h, stdlib.h, string.h, ui.h, ui_gfx.h, ui_them | ui_gfx.c -- real graphics backend for the UI layer (SDL2 + FreeType2). Owns only view rendering + in |
| `apps/` | `ui_headless` | 115 | stdlib.h, string.h, ui.h | ui_headless.c -- null/recording UI backend. Implements the UI_Backend vtable with no real display. E |
| `apps/` | `ui_macro` | 58 | stdlib.h, string.h, ui.h, ui_macro.h | ui_macro.c -- record + replay (see ui_macro.h). |
| `apps/` | `ui_theme` | 106 | stat.h, stdio.h, stdlib.h, string.h, types.h, ui_theme.h | ui_theme.c -- semantic theme palette + persistence (see ui_theme.h). Two token tables (dark / light) |
| `apps/` | `ui_tty` | 153 | ioctl.h, stdio.h, stdlib.h, string.h, termios.h, ui.h, unistd.h | ui_tty.c -- minimal curses-free terminal backend for the UI layer. Uses raw ANSI escapes (no ncurses |
| `apps/` | `test_agent` | 141 | agent.h, json.h, stdio.h, stdlib.h, string.h | test_agent.c -- exercise the wubuOS-facing protocol layer. Real assertions: ingest a document, regur |
| `apps/` | `test_atom` | 45 | command.h, doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_atom.h, ui_headless.h | test_atom.c -- integration: command palette drives the UI controller. |
| `apps/` | `test_complete` | 85 | complete.h, doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_headless.h | test_complete.c -- word completion + symbol/function list (Phase D, part 2). Completion + symbols te |
| `apps/` | `test_core` | 308 | buffer.h, diff.h, doc.h, docs.h, encode.h, lex.h, search.h, stdint.h, stdio.h, s | test_core.c -- exercise WuBuPad's headless editing core. Real assertions: piece-table correctness, u |
| `apps/` | `test_editor` | 89 | doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_headless.h, ui_macro.h | test_editor.c -- editor features (Phase D): EOL convert, column/block selection, macro record+replay |
| `apps/` | `test_find` | 54 | doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_find.h, ui_headless.h | test_find.c -- find/replace controller over the real Doc + search engine. Uses the headless backend  |
| `apps/` | `test_fold` | 62 | doc.h, stdio.h, string.h, ui.h, ui_headless.h | test_fold.c -- code folding (Phase D, part 3). Verifies fold toggle, is_folded query, unfold-all, bl |
| `apps/` | `test_lex_fold` | 55 | lex.h, stdio.h, string.h | test_lex_fold.c -- verify lex_folds + lex_symbols on sample C. |
| `apps/` | `test_lex_multi` | 42 | lex.h, stdio.h, string.h | test_lex_multi.c -- verify the config-driven lexers (py/js/css/sql/md). |
| `apps/` | `test_shape` | 76 | ft2build.h, shape.h, stdio.h, stdlib.h, string.h | test_shape.c -- text shaping (HarfBuzz + FriBidi) unit test. Verifies that the wubushape layer produ |
| `apps/` | `test_theme` | 76 | doc.h, docs.h, stdio.h, string.h, ui.h, ui_headless.h, ui_theme.h | test_theme.c -- theming + status string + tab switching (Phase C). Uses the headless backend (no dis |
| `apps/` | `test_transform` | 81 | doc.h, stdio.h, stdlib.h, string.h, transform.h | test_transform.c -- sort + case-transform correctness. |
| `apps/` | `test_ui` | 85 | assert.h, doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_headless.h | test_ui.c -- drive the editor core through the UI API on the headless backend. Scripts keystrokes an |
| `apps/` | `test_ui_gfx` | 62 | doc.h, stdio.h, stdlib.h, string.h, ui.h, ui_gfx.h | test_ui_gfx.c -- exercise the SDL2/FreeType graphics backend headlessly. In CI there is no display,  |
| `audio/` | `bigmac_out` | 259 | alloca.h, asoundlib.h, bigmac_out.h, error.h, simple.h, stdint.h, stdio.h, stdli | bigmac_out.c -- the host audio output backend for WuBuOS. See bigmac_out.h for the contract. Pulse - |
| `audio/` | `bigmac_out_test` | 85 | bigmac_out.h, math.h, stdio.h, string.h | bigmac_out_test.c -- behavior gate for the output backend. Proves the backend really opens something |
| `audio/` | `wb_3d_camera` | 409 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_3d_camera.c — 3D camera + lighting system R089: After Effects Advanced 3D renderer parity Virtual |
| `audio/` | `wb_3d_edit` | 356 | math.h, stdint.h, stdio.h, stdlib.h, string.h | wb_3d_edit.c — stereoscopic 3D editing for Vegas 10-style 3D workflows. Supports loading left+right  |
| `audio/` | `wb_3d_overlay` | 679 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | wb_3d_overlay.c — 3D Character Overlay System (R098). 60+ meta-layer abilities for YTP/YTPMV product |
| `audio/` | `wb_aaf_export` | 320 | stdarg.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | wb_aaf_export.c — AAF / OMF interchange export. Simplified but parseable interchange formats for mov |
| `audio/` | `wb_agent` | 1394 | math.h, stdio.h, stdlib.h, string.h, unistd.h, wb_internal.h, wbus.h, wbus_agent | wb_agent.c — headless agent command processor (R017 G9). Pure C11. Drives the editor via the real se |
| `audio/` | `wb_agi` | 94 | stdio.h, stdlib.h, string.h, wbus_agi.h | wb_agi.c — AGI control-surface bridge model (R043-G7). Task/orchestration state. Pure C11, stdlib on |
| `audio/` | `wb_ai_mix` | 500 | math.h, stdint.h, stdlib.h, string.h, wbus.h, wbus_ai_mix.h, wbus_dsp.h, wbus_ff | wb_ai_mix.c — AI-assisted mixing tools (Logic Mastering Assistant style). Analyzes audio and applies |
| `audio/` | `wb_analysis` | 219 | math.h, stdlib.h, string.h, wbus.h, wbus_fft.h | wb_analysis.c — broadcast/metering-style audio analysis tools. Pure C11, zero third-party. Reuses wb |
| `audio/` | `wb_anim` | 883 | math.h, stdio.h, stdlib.h, string.h, wbus_anim.h | wb_anim.c — keyframed 3D animation layer (R054). Per-object keyframe lists sampled with linear inter |
| `audio/` | `wb_arpeggiator` | 195 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_arpeggiator.c — MIDI arpeggiator. R077: Pattern-based note generation from held chords. Patterns: |
| `audio/` | `wb_arrange_ai` | 277 | stdint.h, stdlib.h, string.h, wbus.h | wb_arrange_ai.c — AI arrangement assistant. Auto-arranges clips into song structures (verse/chorus/b |
| `audio/` | `wb_assets` | 147 | dirent.h, stat.h, stdio.h, stdlib.h, string.h, wbus_assets.h, wbus_gltf.h | wb_assets.c — asset library index + cache (R057). |
| `audio/` | `wb_atmos` | 273 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_atmos.c — Dolby Atmos-style object-based spatial audio panner. Object-based binaural renderer: up |
| `audio/` | `wb_audio_color` | 273 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_audio_color.c — audio-reactive color grading for YTP music videos. R080: Maps audio features to c |
| `audio/` | `wb_audio_fx` | 373 | math.h, stdlib.h, string.h, wbus_edit.h, wbus_vst3.h | wb_audio_fx.c — audio effects DSP (R086). Per-clip audio effects for the edit graph: - 3-band EQ (lo |
| `audio/` | `wb_audio_mix` | 306 | math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wbus_edit.h | wb_audio_mix.c — audio mixing for video export (R085). Mixes multiple audio clips into a single PCM  |
| `audio/` | `wb_audio_mixer` | 199 | math.h, stdlib.h, string.h, wbus_edit.h | wb_audio_mixer.c — audio mixer for video editing (R086). Vegas 10 feature: per-track audio mixer wit |
| `audio/` | `wb_audio_mux` | 270 | avcodec.h, avformat.h, imgutils.h, math.h, mathematics.h, opt.h, stdio.h, stdlib | wb_audio_mux.c — audio muxing for video export (R085). Post-processes an MP4 file to add an AAC audi |
| `audio/` | `wb_audio_reactive` | 221 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_audio_reactive.c — audio-reactive video effect engine. R077: Maps audio features to visual parame |
| `audio/` | `wb_audio_reactive_node` | 148 | math.h, stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | wb_audio_reactive_node.c — audio-reactive video FX node (R084). Wraps wb_audio_reactive.c as a compo |
| `audio/` | `wb_audio_to_midi` | 218 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_audio_to_midi.c — monophonic audio-to-MIDI conversion. Converts a monophonic audio signal to MIDI |
| `audio/` | `wb_auto_captions` | 138 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_auto_captions.c — TikTok-style auto-caption generation. Generates word-by-word captions with timi |
| `audio/` | `wb_automation` | 185 | math.h, stdio.h, stdlib.h, string.h, wbus.h | wb_automation.c — automation envelopes (breakpoint curves over time). A lane owns an ordered list of |
| `audio/` | `wb_autoreframe` | 138 | stdint.h, stdlib.h, string.h, wbus.h | wb_autoreframe.c — intelligent video reframing (Premiere/AutoFlip style). Subject detection via sali |
| `audio/` | `wb_backend` | 170 | SDL.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_backend.h | wb_backend.c — audio backend: SDL2 audio device (realtime) and WAV file (offline). Both pull from th |
| `audio/` | `wb_bass_boost` | 115 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_bass_boost.c — bass boost / sub-bass enhancer. Meme/YTP essential: that earth-shaking low-end. Al |
| `audio/` | `wb_beat_grid` | 186 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | (wb_beat_grid defined in wbus_compositor.h) |
| `audio/` | `wb_beat_slicer` | 162 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_beat_slicer.c — beat-sliced audio chopping and rearrangement. R077: Essential for remix/mashup/me |
| `audio/` | `wb_beat_sync` | 208 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_beat_sync.c — beat-synced editing engine. Music video / meme editing: automatically cut, zoom, fl |
| `audio/` | `wb_bg_render` | 289 | errno.h, pthread.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wb_internal.h | wb_bg_render.c — background/offline rendering thread. Starts a pthread that runs the same engine ren |
| `audio/` | `wb_biquad_cascade_simd` | 157 | emmintrin.h, math.h, stdlib.h, string.h, wbus_biquad_cascade.h | wb_biquad_cascade_simd.c — SIMD biquad cascade (4 independent cascades in parallel). Two paths: 1. S |
| `audio/` | `wb_bitcrush` | 97 | math.h, stdlib.h, string.h, wb_unit.h | wb_bitcrush.c — R074 hop 147 (G-SF065): sample-rate decimation + bit-depth reduction for chiptune/lo |
| `audio/` | `wb_bleep` | 192 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_bleep.c — bleep censor engine for YTP comedy. R080: Classic YTP technique — replace "swear words" |
| `audio/` | `wb_bvh` | 646 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wbus_bvh.h | wb_bvh.c — BVH (Biovision Hierarchy) motion capture parser (R081). Parses the BVH format used by CMU |
| `audio/` | `wb_captions` | 456 | errno.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wbus_captions.h, wbus_tr | wb_captions.c — auto-captions pipeline for the video editor. Phase 1 (R011): whisper-cli subprocess  |
| `audio/` | `wb_capture` | 233 | math.h, stdlib.h, string.h, wbus.h, wbus_capture.h | wb_capture.c — G93 capture-quantize + G94 record-session-to-arrangement. Self-contained C11 module ( |
| `audio/` | `wb_cgi` | 169 | math.h, stdlib.h, string.h, wbus_cgi.h | wb_cgi.c — self-contained low-poly 3D-CGI scene model (R043-G7). Software pipeline: model rotate ->  |
| `audio/` | `wb_cgi_bands` | 112 | math.h, stdlib.h, string.h, wbus.h, wbus_anim.h, wbus_fft.h | wb_cgi_bands.c — R073 hop 61: frequency-band audio-reactive CGI. Extends the visualizer workflow: ea |
| `audio/` | `wb_cgi_react` | 144 | math.h, stdlib.h, wbus.h, wbus_anim.h | wb_cgi_react.c — R073 hop 55: audio-reactive CGI animation. Bridges the audio and 3D layers: samples |
| `audio/` | `wb_cgiexport` | 280 | stdio.h, stdlib.h, string.h, unistd.h, wbus_cgiexport.h, wbus_clip_edit.h, wbus_ | wb_cgiexport.c — CGI overlay export path (R058). Strategy: render the animation to a rawvideo RGBA s |
| `audio/` | `wb_char2d` | 477 | math.h, stdint.h, stdlib.h, string.h | wb_char2d.c — 2D character animation system (R077 Phase 1). Bone rigging with forward kinematics + i |
| `audio/` | `wb_chord_ai` | 323 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_chord_ai.c - AI chord progression generator (Markov-chain style). Generates chord progressions us |
| `audio/` | `wb_chord_detect` | 177 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_chord_detect.c — chord detection from MIDI notes. R077: Detect chords from MIDI note input. Algor |
| `audio/` | `wb_chorus` | 110 | math.h, stdlib.h, string.h, wb_unit.h | wb_chorus.c — stereo chorus + flanger-capable delay-line modulation. One or more short delay lines w |
| `audio/` | `wb_chroma_key` | 178 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_chroma_key.c — green screen / chroma key compositing. Meme essential: replace green background wi |
| `audio/` | `wb_chromakey` | 687 | math.h, stdarg.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_composit | wb_chromakey.c — Chroma key / green screen engine R093: AE Keylight parity + YTP cutout toolkit Pipe |
| `audio/` | `wb_chromakey_engine` | 130 | math.h, stdlib.h, string.h | wb_chromakey_engine.c — C11 engine-level chroma key processing Provides the functions that wb_video_ |
| `audio/` | `wb_clap` | 368 | dirent.h, dlfcn.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_clap.h | wb_clap.c — CLAP plugin host (self-contained). Loads .clap shared libraries via dlopen, finds the "c |
| `audio/` | `wb_clip_edit` | 181 | math.h, stdlib.h, wbus_clip_edit.h, wbus_param_track.h | wbus_clip_edit.c — R043 (G1/G2/G8/G9): clip-edit handle side-table. Self-contained: only depends on  |
| `audio/` | `wb_cloud` | 375 | dirent.h, errno.h, libgen.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unist | wb_cloud.c — cloud project sync with versioning. Local-filesystem backend: stores projects under ~/b |
| `audio/` | `wb_cloud_collab` | 114 | stdint.h, stdio.h, stdlib.h, string.h, wbus.h | wb_cloud_collab.c — cloud collaboration engine (CRDT-based multi-user sync). Real-time multi-user pr |
| `audio/` | `wb_cmd` | 6 | wbus_cmd.h | wb_cmd.c — command queue (declarations live in wb_cmd.h as inline). This file exists to anchor the t |
| `audio/` | `wb_color_grading` | 233 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | wb_color_grading.c — advanced color grading (DaVinci Resolve style). Lift/Gamma/Gain color wheels, s |
| `audio/` | `wb_comp` | 163 | math.h, stdlib.h, string.h, wbus.h | wb_comp.c — mono/stereo compressor/limiter. Feed-forward VCA-style compressor with soft-knee, attack |
| `audio/` | `wb_comp_simd` | 187 | emmintrin.h, math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_comp_simd.c — SIMD compressor/limiter (4 independent compressors in parallel). Processes 4 indepe |
| `audio/` | `wb_comping` | 105 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_comping.c — take lanes and comping system. R077: Non-destructive recording with take lanes. Data  |
| `audio/` | `wb_composite_ext` | 200 | math.h, stdlib.h, string.h, wbus_compositor.h | wb_composite_ext.c — Advanced compositing nodes for After Effects parity R088: adjustment layers, tr |
| `audio/` | `wb_composition` | 152 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_composition.c — Composition Timeline System (R103). Types defined in wbus_compositor.h — this fil |
| `audio/` | `wb_compositor` | 2788 | errno.h, math.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wb_ui.h | wb_compositor.c — pull-based RoI/RoD node compositor (R013 D1/D3). Pure C11. Recursive pull with ide |
| `audio/` | `wb_compositor_encode` | 462 | avcodec.h, avformat.h, imgutils.h, opt.h, stdint.h, stdio.h, stdlib.h, string.h, | wb_compositor_encode.c — compositor → H.264 encoder via libav (no ffmpeg CLI). R084 gap-analysis §2  |
| `audio/` | `wb_compositor_pro` | 197 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_compositor_pro.c — professional node compositor (Fusion/Nuke style). Node-based compositing with  |
| `audio/` | `wb_conv` | 362 | math.h, stdint.h, stdlib.h, string.h, wbus.h, wbus_conv.h, wbus_fft.h | wb_conv.c — convolution reverb (non-uniform partitioned overlap-save). Architecture (from R075 7-hop |
| `audio/` | `wb_conv_reverb` | 255 | math.h, stdint.h, stdlib.h, string.h, wbus.h, wbus_fft.h | wb_conv_reverb.c — hybrid convolution + algorithmic FDN reverb. Ableton Hybrid Reverb style: an FFT- |
| `audio/` | `wb_core` | 1375 | math.h, pthread.h, stdio.h, stdlib.h, string.h, wb_internal.h, wb_recorder.h, wb | wb_core.c — Big Mac DAW engine core. Architecture per R002 (study of Ardour + LMMS source): - Staged |
| `audio/` | `wb_csg` | 110 | stdlib.h, string.h, wbus_csg.h | wb_csg.c — R074 hop 188 (G-SF012): box subtraction via plane clipping. Each triangle is clipped agai |
| `audio/` | `wb_dark_arts` | 238 | math.h, stdint.h, stdlib.h, string.h | wb_dark_arts.c — YTP dark arts effects (R094c). Compression torture, infinite loop, Stare Down/Myste |
| `audio/` | `wb_datamosh` | 195 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_datamosh.c — datamosh / P-frame glitch effects for YTP. R80: Advanced YTP technique — simulate vi |
| `audio/` | `wb_deep_fry` | 120 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_deep_fry.c — advanced deep fry image/video effect. R077 H11: Meme essential — extreme saturation, |
| `audio/` | `wb_deesser` | 129 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_deesser.c — vocal de-esser (sibilance reduction). R077: Essential for podcast/voice polishing. Al |
| `audio/` | `wb_delay` | 83 | math.h, stdlib.h, string.h, wbus.h | wb_delay.c — stereo delay / echo with feedback, lowpass in loop. |
| `audio/` | `wb_delivery` | 221 | stat.h, stdio.h, stdlib.h, string.h, wbus_delivery.h | wb_delivery.c — delivery presets: loudnorm + chapters (R064). |
| `audio/` | `wb_drum_machine` | 278 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_drum_machine.c — TR-808/909-style analog drum synthesis. VST recreation: iconic drum machine voic |
| `audio/` | `wb_drum_rack` | 247 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wb_internal.h, wbus.h | wb_drum_rack.c — pad-based drum sampler (Ableton Drum Rack style). 64 pads (8x8 grid). Each pad is a |
| `audio/` | `wb_drum_simd` | 208 | g2_fm_simd.h, math.h, stdint.h, stdlib.h, string.h | wb_drum_simd.c — G5: SIMD-accelerated drum render. The drum path has sin() calls in kick (freq sweep |
| `audio/` | `wb_drums` | 154 | math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wb_unit.h | wb_drums.c — synthesis drum machine unit. Models kick/snare/low/mid/hi toms/closed/open hihat/clap v |
| `audio/` | `wb_dsp` | 30 | stdlib.h, string.h, wbus_plugin.h | wb_dsp.c — built-in plugin registry. |
| `audio/` | `wb_duck` | 97 | math.h, stdlib.h, string.h, wbus_duck.h | wb_duck.c — auto-ducking envelope generator (R063). |
| `audio/` | `wb_dvd_author` | 1396 | math.h, stat.h, stdint.h, stdio.h, stdlib.h, string.h, types.h | wb_dvd_author.c — DVD-Video / Blu-ray authoring engine R090: Vegas DVD Architect parity — proper DVD |
| `audio/` | `wb_dynamic_eq` | 173 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_dynamic_eq.c — dynamic EQ (frequency-dependent compression). R077: Apply EQ that responds to sign |
| `audio/` | `wb_dynamics_adv` | 224 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_dynamics_adv.c — advanced dynamics processor (Ableton Multiband Dynamics). Multiband compressor,  |
| `audio/` | `wb_edit` | 1238 | avcodec.h, avformat.h, imgutils.h, math.h, mathematics.h, opt.h, samplefmt.h, st | wb_edit.c — Video edit decision list (EDL) model (R084). Implements the edit graph: tracks, clips, t |
| `audio/` | `wb_edit_serialize` | 159 | stdio.h, stdlib.h, string.h, wbus_edit.h | wb_edit_serialize.c — edit graph save/load (R085). Serializes wb_edit_graph to a simple text format  |
| `audio/` | `wb_edit_undo` | 174 | stdio.h, stdlib.h, string.h, wbus_edit.h | wb_edit_undo.c — undo/redo for edit graph operations (R086). Snapshots the edit graph state before s |
| `audio/` | `wb_env` | 59 | wbus_dsp.h | wb_env.c — ADSR envelope generator. |
| `audio/` | `wb_eq` | 111 | math.h, stdlib.h, string.h, wb_unit.h | wb_eq.c — 4-band parametric EQ (zero-delay RBJ biquad cascade). Bands: lowshelf, two peaking, highsh |
| `audio/` | `wb_exciter` | 101 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_exciter.c — harmonic exciter / enhancer. R077: Adds harmonics for brightness, presence, "air". Al |
| `audio/` | `wb_export_job` | 90 | pthread.h, sched.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_export_job.h | wb_export_job.c — G38 (R072): one-slot background render queue. EXPORT spawns a pthread running wb_v |
| `audio/` | `wb_export_queue` | 226 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_edit.h, wbus_export_ | wb_export_queue.c — batch export queue. R078: Queue multiple export jobs and process them sequential |
| `audio/` | `wb_expression` | 104 | stdio.h, stdlib.h, string.h, wbus.h | src/wb_expression.c — expression maps for articulation management. G91: Cubase-style per-note articu |
| `audio/` | `wb_expressions` | 424 | math.h, stdio.h, stdlib.h, string.h, wbus_expressions.h | wb_expressions.c — After Effects-style expression engine R88: bytecode stack machine for procedural  |
| `audio/` | `wb_ffmpeg_filters` | 821 | math.h, stdarg.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_composit | wb_ffmpeg_filters.c — Advanced FFmpeg filter_complex command builder R092: Leverage ffmpeg's full fi |
| `audio/` | `wb_fft` | 86 | math.h, stdlib.h, string.h, wbus_fft.h | wb_fft.c — minimal self-contained radix-2 FFT (Cooley-Tukey). Pure C11, no third-party. Powers of tw |
| `audio/` | `wb_filter` | 65 | math.h, wbus_dsp.h | wb_filter.c — biquad filters (lowpass/highpass/bandpass/notch) via RBJ audio EQ cookbook. Used for s |
| `audio/` | `wb_fm` | 178 | math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wb_unit.h | wb_fm.c — FM synthesizer unit (2-operator, polyphonic). carrier pitch = note, modulator at ratio*fre |
| `audio/` | `wb_fm_g2` | 98 | g2_fm_simd.h, math.h, stdint.h, stdlib.h, string.h | wb_fm_g2.c — SIMD-accelerated FM render (G2). Processes FM_VOICES as 4 batches of 4 via SSE2 polynom |
| `audio/` | `wb_fm_g3` | 219 | g2_fm_simd.h, math.h, pthread.h, stdint.h, stdlib.h, string.h | wb_fm_g3.c — G3: dual-core MT FM render via pthreads. Design: snapshot voice state → spawn 2 threads |
| `audio/` | `wb_formant` | 186 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_formant.c — formant shifting for YTP voice character changes. R080: YTP essential — change voice  |
| `audio/` | `wb_freeze` | 158 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_freeze.c — track freeze / bounce-in-place. R077: CPU optimization — render track to audio, disabl |
| `audio/` | `wb_ftz` | 81 | TargetConditionals.h, emmintrin.h, stdint.h, xmmintrin.h | wb_ftz.c — Flush-To-Zero / Denormals-Are-Zero control. R077: Critical performance optimization for r |
| `audio/` | `wb_fuzz` | 145 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_fuzz.c — Electro-Harmonix Big Muff Pi fuzz emulation. VST recreation: iconic fuzz pedal with casc |
| `audio/` | `wb_gate` | 52 | math.h, stdlib.h, wb_internal.h | wb_gate.c — noise gate / expander, af1. Silences signal below a threshold; fast attack, adjustable r |
| `audio/` | `wb_gif_export` | 733 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_compositor.h, wbus_edit.h, wbu | wb_gif_export.c — Animated GIF export (R084 video edit graph). Pure-C11 GIF89a encoder with: - Globa |
| `audio/` | `wb_gltf` | 406 | math.h, stdio.h, stdlib.h, string.h, wbus_gltf.h | wb_gltf.c — minimal GLB (glTF 2.0) importer (R056). Container layout (per Khronos spec): u32 magic   |
| `audio/` | `wb_granular` | 314 | emmintrin.h, math.h, stdint.h, stdlib.h, string.h, wbus.h, wbus_dsp.h | wb_granular.c — granular synthesizer (G9 + G3 [R075] upgrade). Splits a loaded sample into short "gr |
| `audio/` | `wb_graphio` | 324 | stdio.h, stdlib.h, string.h, wbus_graphio.h, wbus_param_track.h, wbus_scenedesc. | wb_graphio.c — R074 hop 191 (G-SF080): compositor node-graph save/load. Serializes the editable grap |
| `audio/` | `wb_hdr_preview` | 323 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_hdr_preview.c — HDR preview output with tone mapping R090: HDR10/HLG/Dolby Vision parity Supports |
| `audio/` | `wb_hpss` | 161 | math.h, stdio.h, stdlib.h, string.h, wbus_fft.h, wbus_hpss.h | wb_hpss.c — R020-A Harmonic-Percussive Source Separation (pure C11). See wbus_hpss.h for the algorit |
| `audio/` | `wb_import` | 118 | dirent.h, stat.h, stdio.h, stdlib.h, string.h, strings.h, unistd.h, wb_internal. | wb_import.c — Wave 1 G01/G02: media scan + audio-file import. Pure C11 (POSIX dirent for the directo |
| `audio/` | `wb_input` | 103 | math.h, stdlib.h, string.h, wbus.h | wb_input.c — G05/G06: audio/MIDI input capture. A lock-free single-producer/single-consumer ring buf |
| `audio/` | `wb_kaleidoscope` | 164 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_kaleidoscope.c — kaleidoscope / mirror effect for YTP trippiness. R80: Classic YTP visual — mirro |
| `audio/` | `wb_karplus` | 129 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_karplus.c — Karplus-Strong physical modeling synthesis. N3 [R076]: plucked string synthesis via d |
| `audio/` | `wb_keyframes` | 192 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_keyframes.c — keyframe animation system. Professional video editing: animate any property over ti |
| `audio/` | `wb_keys` | 215 | stdint.h, string.h | wb_keys.c — keyboard shortcuts for video editing (R077 Phase 6). Professional video editing keyboard |
| `audio/` | `wb_ladder` | 112 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_ladder.c — Moog transistor ladder filter emulation. VST recreation: Minimoog-style 4-pole lowpass |
| `audio/` | `wb_lfo_sidechain` | 153 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_lfo_sidechain.c — LFO-driven sidechain ducking. R077: Rhythmic ducking without audio trigger — fo |
| `audio/` | `wb_light2d` | 425 | math.h, stdint.h, stdlib.h, string.h | wb_light2d.c — 2D lighting, shadows, and camera effects (R077 Phase 3). Normal-mapped 2D lighting, s |
| `audio/` | `wb_limiter` | 123 | math.h, stdlib.h, string.h, wbus_limiter.h | wb_limiter.c — R073 hop 37: lookahead brickwall limiter (two-pass). Pass 1 computes per-frame requir |
| `audio/` | `wb_linked_tracks` | 192 | stdlib.h, string.h, wbus.h | wb_linked_tracks.c — Ableton-style linked track group editing. A link group is a collection of track |
| `audio/` | `wb_lottie` | 289 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_lottie.c — Lottie JSON motion graphics renderer (After Effects/Lottie style). Parse Lottie JSON,  |
| `audio/` | `wb_lufs` | 111 | math.h, string.h, wbus.h, wbus_dsp.h, wbus_lufs.h | wb_lufs.c — G78: live K-weighted loudness meter (ITU-R BS.1770-4). Pure C11, zero third-party. Reuse |
| `audio/` | `wb_lut` | 254 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | wb_lut.c — 3D LUT import/export (.cube format). R077: Professional color grading LUT support. Suppor |
| `audio/` | `wb_lut_node` | 98 | stdio.h, stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | wb_lut_node.c — 3D LUT color grading node (R085). Wraps wb_lut3d_load/wb_lut3d_apply as a compositor |
| `audio/` | `wb_lyric_video` | 187 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_lyric_video.c — lyric video generator. R077 H15: Auto-generate timed lyric videos from transcript |
| `audio/` | `wb_macro_rack` | 399 | math.h, stdlib.h, string.h, wb_internal.h, wbus.h, wbus_dsp.h | wb_macro_rack.c — macro/parameter rack (Ableton Instrument Rack style). A chain of units (synth -> f |
| `audio/` | `wb_mask_node` | 473 | math.h, stdlib.h, string.h, wbus_compositor.h | wb_mask_node.c — Per-layer mask effect node (R090) Mask node: takes a source image (input 0) and an  |
| `audio/` | `wb_master_adv` | 448 | math.h, stdint.h, stdlib.h, string.h, wbus.h, wbus_lufs.h | wb_master_adv.c — advanced mastering suite (iZotope Ozone style). Chain: input gain → 4-band crossov |
| `audio/` | `wb_mastering_chain` | 192 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_mastering_chain.c — all-in-one mastering chain. R077: Serial DSP graph: EQ -> Multiband Comp -> S |
| `audio/` | `wb_mastering_pro` | 435 | math.h, stdint.h, stdlib.h, string.h, wbus.h, wbus_lufs.h | wb_mastering_pro.c — professional mastering chain (iZotope Ozone style). Chain: input gain → 4-band  |
| `audio/` | `wb_matte_fx` | 352 | math.h, stdint.h, stdlib.h, string.h | wb_matte_fx.c — Matte Painting + Cutout System (R108). Advanced compositing techniques for YTP/YTPMV |
| `audio/` | `wb_melody_ai` | 391 | stdint.h, stdlib.h, string.h, wbus.h | src/wb_melody_ai.c — AI melody composer. Generate melodies from scale, mood, and rhythm constraints. |
| `audio/` | `wb_melody_follow` | 274 | math.h, stdint.h, stdlib.h, string.h | wb_melody_follow.c — Melody-Following Pitch Mapper (R112). The key technique for YTPMV that sounds " |
| `audio/` | `wb_meme_sounds` | 365 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_meme_sounds.c — meme/YTP sound effect generator. Generates iconic meme sounds procedurally (no sa |
| `audio/` | `wb_mesh` | 513 | math.h, stdio.h, stdlib.h, string.h, wbus_mesh.h | wb_mesh.c — primitive mesh library + .obj loader (R053). Every primitive is built from the same grow |
| `audio/` | `wb_mesh_warp` | 316 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_mesh_warp.c — mesh warp / puppet tool effect node (After Effects parity). Divides the input into  |
| `audio/` | `wb_midi_chordgen` | 176 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_midi_chordgen.c — MIDI chord generator. R078 H1: Generate chords from single MIDI notes. Chord ty |
| `audio/` | `wb_midi_coremidi` | 442 | CoreFoundation.h, CoreMIDI.h, math.h, stdio.h, stdlib.h, string.h, strings.h, wb | wb_midi_coremidi.c — CoreMIDI input (macOS). Implements the portable wb_midi interface using CoreMID |
| `audio/` | `wb_midi_generators` | 225 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_midi_generators.c — MIDI generators (Ableton Live 12 style). Generate melodies, chords, and rhyth |
| `audio/` | `wb_midi_humanize` | 114 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_midi_humanize.c — MIDI humanization (timing/velocity randomization). R077: Add natural feel to qu |
| `audio/` | `wb_midi_remote` | 243 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_midi_remote.h | wb_midi_remote.c — MIDI remote control mapping. Ableton MIDI remote script style: map CC numbers to  |
| `audio/` | `wb_midi_scale` | 205 | stdlib.h, string.h, wbus.h | src/wb_midi_scale.c — MIDI scale quantizer. Snap incoming MIDI notes to a musical scale. Precomputes |
| `audio/` | `wb_midifx` | 161 | math.h, stdlib.h, string.h, wbus_midifx.h | wb_midifx.c — MIDI FX chain units (mf1). Stateless transforms (chord/transpose/velocity) act per-eve |
| `audio/` | `wb_mix_simd` | 139 | emmintrin.h, math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_mix_simd.c — SIMD mixer bus with constant-power pan laws. Sums N tracks to a master bus using SSE |
| `audio/` | `wb_mod` | 194 | math.h, stdlib.h, string.h, wbus_mod.h | wb_mod.c — non-destructive modifier stack (R055b). |
| `audio/` | `wb_mod_matrix` | 402 | math.h, stdlib.h, string.h, wbus.h | wb_mod_matrix.c — Bitwig-style modular modulation routing matrix. Routes modulation sources (LFOs, e |
| `audio/` | `wb_motion_blur` | 343 | math.h, stdlib.h, string.h, wbus_compositor.h | wb_motion_blur.c — directional motion blur effect node (R086). After Effects-style motion blur: trac |
| `audio/` | `wb_motion_overlay` | 134 | stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | wb_motion_overlay.c — motion tracking overlay node (R085). Wraps wb_motion_track as a compositor eff |
| `audio/` | `wb_motion_track` | 216 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_motion_track.c — point tracking via Lucas-Kanade optical flow. R077: Track feature points across  |
| `audio/` | `wb_mpe` | 188 | math.h, stdlib.h, string.h, wbus.h | src/wb_mpe.c — MIDI Polyphonic Expression (MPE) synthesizer. Per-note pitch bend, pressure, and timb |
| `audio/` | `wb_multiband` | 167 | math.h, stdlib.h, string.h, wb_internal.h, wbus_dsp.h | wb_multiband.c — 3-band multiband compressor (af1, final sub-item). Signal is split with two Linkwit |
| `audio/` | `wb_multicam` | 117 | stdio.h, stdlib.h, string.h, wbus_edit.h | wb_multicam.c — multi-camera editing (R086). Vegas 10 feature: sync multiple camera angles and switc |
| `audio/` | `wb_noise_gate` | 206 | math.h, stdlib.h, string.h, wb_internal.h | wb_noise_gate.c — Spectral noise gate for audio cleanup R087: AI noise removal (Camtasia/Premiere/Re |
| `audio/` | `wb_null_object` | 209 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_null_object.c — null objects + parenting for transform hierarchy R089: After Effects parity — nul |
| `audio/` | `wb_ofx` | 434 | math.h, ofxCore.h, ofxImageEffect.h, ofxMemory.h, ofxParam.h, ofxProperty.h, ofx | wb_ofx.c — minimal OpenFX host (R017 G4). Pure C11 + the OFX C headers. Implements: PropertySuite, P |
| `audio/` | `wb_ofx_plugin_builtin` | 134 | ofxCore.h, ofxImageEffect.h, ofxParam.h, ofxProperty.h, wbus_compositor.h | wb_ofx_plugin_builtin.c — a builtin OFX test plugin (R017 G4). Conforms to the OfxPlugin ABI and is  |
| `audio/` | `wb_osc` | 301 | g2_fm_simd.h, math.h, string.h, wbus_dsp.h | wb_osc.c — oscillators: sine/saw/square/triangle/noise/wavetable. Anti-aliased saw via poly-BLEP-lit |
| `audio/` | `wb_parallel_comp` | 113 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_parallel_comp.c — parallel compression (New York compression). R077: Blend compressed signal with |
| `audio/` | `wb_param_track` | 187 | math.h, stdlib.h, string.h, wbus_param_track.h | wb_param_track.c — keyframe animation track (R016 S2). Pure C11. Keys kept sorted by time. Bezier vi |
| `audio/` | `wb_particle` | 208 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_particle.c — particle system for audio-reactive visual effects. R077: GPU-style particle system r |
| `audio/` | `wb_particle_gpu` | 429 | emmintrin.h, math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_particle_gpu.c — GPU-accelerated particle system (SIMD/SSE2 CPU fallback). Real-time particle eff |
| `audio/` | `wb_pattern` | 50 | string.h, wbus_pattern.h | wb_pattern.c — R074 hop 179 (G-SF068): step pattern sequencer. Bar-synced 16-step triggering, determ |
| `audio/` | `wb_pdc` | 188 | stdlib.h, string.h, wbus.h | src/wb_pdc.c — plugin delay compensation (PDC). Automatic latency compensation for plugin chains. Ea |
| `audio/` | `wb_perf` | 316 | math.h, stdlib.h, string.h, wbus_anim.h, wbus_perf.h | wb_perf.c — live performance engine: decks, capture, deterministic replay (R065). State model: each  |
| `audio/` | `wb_perfclip` | 148 | stdio.h, stdlib.h, string.h, wbus_mesh.h, wbus_perfclip.h | wb_perfclip.c — performance recordings as nested timeline clips (R068). |
| `audio/` | `wb_phaser` | 122 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_phaser.c — phaser effect (allpass filter cascade with LFO). N5 [R076]: modulation effect — cascad |
| `audio/` | `wb_phoneme_seg` | 189 | math.h, stdint.h, stdlib.h, string.h | wb_phoneme_seg.c — Real-time phoneme/syllable segmentation (R114). Proper onset detection for speech |
| `audio/` | `wb_pitch_bend` | 160 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_pitch_bend.c — continuous pitch bending / sliding. R077: YTP/meme essential — smooth pitch slides |
| `audio/` | `wb_pitch_correct` | 202 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_pitch_correct.c — auto-tune style pitch correction. Melodyne-parity: snap detected pitch to neare |
| `audio/` | `wb_podcast` | 382 | math.h, stdlib.h, string.h, wbus_dsp.h | wb_podcast.c — podcast production workflow tools. Voice isolation (bandpass 300Hz–4kHz + spectral ga |
| `audio/` | `wb_precision` | 126 | stdlib.h, string.h, wbus_precision.h | wb_precision.c — Wave 2 lane B: G15 trim nudge, G16 razor, G66 drop modes. Pure C11 session math; no |
| `audio/` | `wb_prod_project` | 128 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_prod_project.c — YTPMV Production Project + Render Graph (R105). Types defined in wbus_compositor |
| `audio/` | `wb_project` | 210 | stdio.h, stdlib.h, string.h, wbus.h | wb_project.c — G69: multiple timelines (sequences) per project. A project owns up to WB_MAX_SEQUENCE |
| `audio/` | `wb_project_templates` | 273 | stdio.h, stdlib.h, string.h, wbus.h | wb_project_templates.c — project templates for quick session creation. R078 H10: Pre-built track lay |
| `audio/` | `wb_proxy` | 118 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | wb_proxy.c — proxy editing for 4K video on dual-core. R078 H4: Low-res preview, full-res export. Alg |
| `audio/` | `wb_quantize` | 163 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_quantize.c — smart quantize and groove templates. R077: MIDI quantization that preserves human fe |
| `audio/` | `wb_rast` | 899 | math.h, pthread.h, stdlib.h, string.h, wbus_rast.h | wb_rast.c — fixed-function software triangle rasterizer (R052). Techniques SLERM'd from CubeCoders/J |
| `audio/` | `wb_reaction` | 178 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_reaction.c — reaction video format (side-by-side split screen). R077: Combine two videos side-by- |
| `audio/` | `wb_realtime_preview` | 222 | math.h, pthread.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_composi | wb_realtime_preview.c — real-time GPU-accelerated playback preview R090: GPU playback parity with al |
| `audio/` | `wb_recorder` | 137 | stdio.h, stdlib.h, string.h, wb_recorder.h, wbus.h | wb_recorder.c — MIDI record-into-clip capture. Captures live MIDI note-on/note-off events into the e |
| `audio/` | `wb_restoration` | 481 | math.h, stdlib.h, string.h, wbus.h, wbus_fft.h | src/wb_restoration.c — audio restoration tools (iZotope RX style). Pure C11, zero third-party. Spect |
| `audio/` | `wb_restore` | 133 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_restore.c — audio restoration (de-click, de-clip, noise reduction). R077: Clean up damaged audio  |
| `audio/` | `wb_reverb` | 123 | stdlib.h, string.h, wbus.h | wb_reverb.c — Schroeder feedback-delay-network reverb (stereo). 4 parallel comb filters into 2 serie |
| `audio/` | `wb_sampler` | 102 | math.h, stdlib.h, string.h, wbus.h | wb_sampler.c — sample-playback instrument. Plays back a short PCM sample (loopable) at MIDI-note-tun |
| `audio/` | `wb_sampler_inst` | 504 | math.h, stdint.h, stdlib.h, string.h | wb_sampler_inst.c — Sampler Instrument System for YTPMV (R102). The core of YTPMV: load phoneme samp |
| `audio/` | `wb_sat_simd` | 128 | emmintrin.h, math.h, stdint.h, string.h, wbus.h | wb_sat_simd.c — SIMD saturation via polynomial tanh approximation. Replaces tanhf() with a vectorize |
| `audio/` | `wb_saturation` | 50 | math.h, stdlib.h, wb_internal.h | wb_saturation.c — analog-style saturation (waveshaper), af1. tanh soft-clip driven by a drive contro |
| `audio/` | `wb_scene_detect` | 135 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_scene_detect.c — scene/shot boundary detection. R077: Auto-detect cuts and scene changes in video |
| `audio/` | `wb_scenedesc` | 82 | stdio.h, stdlib.h, string.h, wbus_scenedesc.h | wb_scenedesc.c — R074 hop 171 (G-SF079): text scene description loader. Pure C11, stdio only. |
| `audio/` | `wb_score` | 225 | stdio.h, string.h, wbus.h | src/wb_score.c — MIDI to notation converter (Cubase score editor style). Converts MIDI pitches to st |
| `audio/` | `wb_screen_record` | 320 | CoreFoundation.h, CoreGraphics.h, CoreVideo.h, QuartzCore.h, pthread.h, stdint.h | wb_screen_record.c — macOS screen recording engine R089: Camtasia parity — screen + audio + cursor c |
| `audio/` | `wb_sdl_preview` | 349 | SDL.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_sdl_preview.c — SDL2 window for real-time video preview R090: Actually displays pixels on screen  |
| `audio/` | `wb_session` | 2822 | dirent.h, math.h, stdio.h, stdlib.h, string.h, unistd.h, wbus.h, wbus_clip_edit. | wb_session.c — session model: build, destroy, and a demo-song builder. A session is the editable arr |
| `audio/` | `wb_session_file` | 349 | stdio.h, stdlib.h, string.h, unistd.h, wbus.h | wb_session_file.c — save/load sessions in the .wbus text format. Grammar (matches wb_session_save ou |
| `audio/` | `wb_session_view` | 385 | stdlib.h, string.h, wbus.h | wb_session_view.c — Ableton-style session view clip launcher. Pure C11, zero third-party. Manages a  |
| `audio/` | `wb_sf2` | 254 | math.h, stdio.h, stdlib.h, string.h, wbus_sf2.h | wb_sf2.c — R074 hop 127 (G-SF062): minimal SoundFont 2 loader. Parses the RIFF container: INFO (skip |
| `audio/` | `wb_sfx` | 92 | math.h, stdlib.h, string.h, wbus_sfx.h | wb_sfx.c — R074 hop 150 (G-SF070): procedural game-SFX synthesizer. Laser (descending FM zap), explo |
| `audio/` | `wb_shadowbin` | 348 | stdio.h, stdlib.h, string.h, wbus_shadowbin.h | wb_shadowbin.c — the magic shadow bin: full edit state as portable JSON sidecar (R061). Write is ato |
| `audio/` | `wb_shape_node` | 262 | math.h, stdlib.h, string.h, wbus_compositor.h | wb_shape_node.c — Shape layer source nodes (After Effects parity) R088: vector primitive generators  |
| `audio/` | `wb_sidechain` | 205 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_sidechain.c — sidechain compression / pumping / ducking. R077 H4: Essential for modern music prod |
| `audio/` | `wb_smf` | 255 | stdio.h, stdlib.h, string.h, wbus_smf.h | wb_smf.c — R074 hop 115 (G-SF061): Standard MIDI File (SMF) loader. Parses type-0/1 .mid files into  |
| `audio/` | `wb_sonogram` | 404 | math.h, stdint.h, stdlib.h, string.h, wbus_fft.h, wbus_sonogram.h | wb_sonogram.c — real-time spectrogram + waveform display (iZotope RX style). R079: Advanced audio an |
| `audio/` | `wb_spatial_audio` | 368 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_spatial_audio.c — HRTF-based binaural 3D audio panner. R079: Spatial audio / Dolby Atmos-style po |
| `audio/` | `wb_spectral_edit` | 389 | math.h, stdlib.h, string.h, wbus_fft.h, wbus_spectral_edit.h | wb_spectral_edit.c — spectral audio repair tools (iZotope RX style). Pure C11, zero third-party. Use |
| `audio/` | `wb_spectral_fx` | 321 | math.h, stdlib.h, string.h, wbus_fft.h, wbus_spectral_fx.h | wb_spectral_fx.c — spectral effects (Ableton-style). Pure C11, zero third-party. Uses wb_fft for FFT |
| `audio/` | `wb_spectrum` | 162 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_spectrum.c — audio spectrum analyzer / visualizer. R77: Real-time frequency spectrum for music vi |
| `audio/` | `wb_speed_ramp` | 165 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_speed_ramp.c — smooth speed ramping / time remapping. R077 H17: Smooth speed changes for music vi |
| `audio/` | `wb_stabilize` | 155 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_video_stabilize.c — video stabilization (motion smoothing). R077: Digital video stabilization usi |
| `audio/` | `wb_stabilize2` | 354 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_stabilize2.c — video stabilization (phase-correlation motion estimation). R077+: DaVinci Resolve- |
| `audio/` | `wb_stem_export` | 164 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_stem_export.c — stem export (multitrack bounce). R077: Export individual tracks as separate audio |
| `audio/` | `wb_stem_split` | 88 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_stem_split.c — intelligent stem separation (Ableton 12.3 style). Pure DSP approach: multi-pass sp |
| `audio/` | `wb_stereo` | 233 | emmintrin.h, math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_stereo.c — mid-side stereo widening + Haas effect + M/S utility. N1 [R076]: spatial audio process |
| `audio/` | `wb_stereo_image` | 108 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_stereo_image.c — stereo imaging / mid-side processing. R077: Control stereo width, mid-side balan |
| `audio/` | `wb_stt_caption` | 430 | errno.h, math.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wb_ui.h, wbus_ca | wb_stt_caption.c — AI auto-caption / speech-to-text subtitle burn-in. Transcribes audio from a video |
| `audio/` | `wb_stutter` | 214 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_stutter.c — advanced stutter loop engine for YTP/meme editing. R077 H1: Stutter loop variations b |
| `audio/` | `wb_subtitle_translate` | 464 | ctype.h, stdio.h, string.h, wbus.h, wbus_transcript.h | wb_subtitle_translate.c — R079: multi-language subtitle translation engine. Dictionary-based caption |
| `audio/` | `wb_surround` | 171 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_surround.c — surround sound panning (5.1/7.1). R078: Professional surround mixing. Speaker layout |
| `audio/` | `wb_svg_import` | 489 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_svg_import.c — SVG file import to shape layers R090: After Effects SVG import parity Parses SVG X |
| `audio/` | `wb_synth` | 226 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_dsp.h, wbus_plugin.h | wb_synth.c — subtractive polyphonic synthesizer instrument. Implements the wbus plugin ABI. A handfu |
| `audio/` | `wb_synth_simd` | 347 | emmintrin.h, g2_fm_simd.h, math.h, stdint.h, stdlib.h, string.h, wb_biquad_simd. | wb_synth_simd.c — G4/G6: SIMD-accelerated synth render with oversampling. G4: Sine oscillator batchi |
| `audio/` | `wb_tape_stop` | 134 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_tape_stop.c — tape stop / vinyl brake / slow-down effect. R077: Essential for DJ transitions, mem |
| `audio/` | `wb_tempo_detect` | 152 | math.h, stdlib.h, string.h, wbus.h | wb_tempo_detect.c — autocorrelation-based BPM detection from an audio buffer. Pure C11, zero third-p |
| `audio/` | `wb_text_animate` | 203 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_text_animate.c — text animation system for kinetic typography. R077: Animated text overlays for m |
| `audio/` | `wb_text_animator` | 348 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_text_animator.c — per-character text animation effect node. After Effects parity: per-character a |
| `audio/` | `wb_text_based_edit` | 601 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wbus_edit.h | wb_text_based_edit.c — Text-based video editing (R086). Implements transcript-driven editing: parse  |
| `audio/` | `wb_text_edit` | 374 | ctype.h, math.h, stdlib.h, string.h, wbus.h, wbus_text_edit.h, wbus_video.h | wb_text_edit.c — text-based video editing via transcript (Descript/Resolve style). Pure C11. Edit th |
| `audio/` | `wb_text_templates` | 152 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_text_templates.c — text animation templates for titles/lower thirds. R077: Pre-built text animati |
| `audio/` | `wb_tga` | 60 | stdio.h, stdlib.h, string.h, wbus_tga.h | wb_tga.c — R074 hop 181 (G-SF014): minimal TGA loader. Supports uncompressed type-2 true-color 24/32 |
| `audio/` | `wb_time_sig` | 229 | stdlib.h, string.h, wbus.h | wb_time_sig.c — time signature map (changes over the timeline). Stores a sorted array of time signat |
| `audio/` | `wb_timestretch` | 281 | math.h, stdlib.h, string.h, wbus.h | wb_timestretch.c — G26: time-stretch / pitch-shift. WSOLA-style (Waveform-Similarity Overlap-Add) ti |
| `audio/` | `wb_track_folder` | 273 | stdlib.h, string.h, wbus.h | wb_track_folder.c — track folder hierarchy and advanced bus routing. Folders group tracks in the arr |
| `audio/` | `wb_transcript` | 248 | ctype.h, stdio.h, stdlib.h, string.h, wbus_transcript.h | wb_transcript.c — editable transcript model (R015 G6). Pure C11. |
| `audio/` | `wb_transient_shaper` | 102 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_transient_shaper.c — transient shaping (attack/sustain enhancement). R077 H8: SPL Differential En |
| `audio/` | `wb_transitions` | 352 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_transitions.c — video transition pack (20+ transitions). R077 H9: Professional transition effects |
| `audio/` | `wb_transitions_pro` | 182 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_transitions_pro.c — professional video transition pack. 20+ transition types: dissolve, wipe, sli |
| `audio/` | `wb_transport` | 95 | stdio.h, stdlib.h, string.h, wbus.h | wb_transport.c — sample-accurate timeline renderer. Reads the session's clips and schedules note-on/ |
| `audio/` | `wb_true_peak` | 129 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_true_peak_limiter.c — true peak limiter with intersample peak detection. R077: Essential for mast |
| `audio/` | `wb_tts` | 194 | math.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wb_internal.h, wbus_tts.h | wb_tts.c — legitimate, offline Text-to-Speech for Big Mac. Architecture (same as the caption engine) |
| `audio/` | `wb_tuner` | 157 | math.h, pthread.h, stdlib.h, string.h, wb_internal.h, wbus.h | wb_tuner.c — recursive self-improvement / feedback-fix loop. Circular loop: SENSE → ACT → MEASURE →  |
| `audio/` | `wb_ui_font` | 181 | SDL.h, stdio.h, string.h, wbus_compositor.h | wb_ui_font.c — embedded 5x7 bitmap font + SDL text renderer. Zero third-party: glyphs are hand-embed |
| `audio/` | `wb_undo` | 116 | stdlib.h, string.h, wbus.h | wb_undo.c — undo/redo via session snapshots. Before a mutation, the caller pushes the current sessio |
| `audio/` | `wb_unit` | 115 | stdlib.h, string.h, wb_internal.h, wb_unit.h | wb_unit.c — effect-unit registry + adapters over the built-in DSP. Wraps the existing comp/reverb/de |
| `audio/` | `wb_unit_clap` | 72 | stdio.h, stdlib.h, string.h, wb_internal.h, wb_unit.h, wbus_clap.h | wb_unit_clap.c — CLAP-plugin bridge as a wb_unit. Lets a loaded CLAP plugin be placed in any track's |
| `audio/` | `wb_vca` | 92 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_vca.c — VCA fader groups. R077: Master fader control over groups of channels. Algorithm: |
| `audio/` | `wb_vfx` | 671 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_vfx.h | wb_vfx.c — visual effects & compositing extensions (R077 Phase 2). Blend modes, 3D LUT loader (.cube |
| `audio/` | `wb_vfx_node` | 214 | math.h, stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | wb_vfx_node.c — wrap wb_vfx.c effects as compositor effect nodes (R077). Each factory creates a WB_N |
| `audio/` | `wb_vgm` | 377 | ctype.h, stdint.h, stdio.h, stdlib.h, string.h | wb_vgm.c — VGMusic.com scraper + MIDI parser (R096). Scrapes vgmusic.com for the most popular video  |
| `audio/` | `wb_vhs_effect` | 152 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_vhs_effect.c — VHS tracking errors and retro video degradation. R077 H12: VHS aesthetic — trackin |
| `audio/` | `wb_video` | 772 | SDL.h, avcodec.h, avformat.h, errno.h, fcntl.h, imgutils.h, signal.h, stdio.h, s | wb_video.c — FFmpeg video decode + 480p proxy + SDL2 preview for the video editor. Architecture (R00 |
| `audio/` | `wb_video_edit` | 433 | math.h, stdint.h, stdio.h, stdlib.h, string.h | wb_video_edit.c — video editing commands & auto-assembly (R077 Phase 4). Agent-driven video editing: |
| `audio/` | `wb_video_fx_pro` | 286 | math.h, stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | wb_video_fx_pro.c — professional video FX nodes (R085). Wraps existing modules as compositor effect  |
| `audio/` | `wb_video_node` | 162 | stdio.h, stdlib.h, string.h, wbus_compositor.h, wbus_video.h | wb_video_node.c — compositor source node wrapping wb_video_decoder. A WB_NODE_SOURCE that decodes a  |
| `audio/` | `wb_vocal_remove` | 71 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_vocal_remove.c — vocal removal via center channel extraction. R077: Remove vocals from stereo tra |
| `audio/` | `wb_vocal_synth` | 207 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_vocal_synth.c — formant synthesizer (vocal synthesis from phonemes). R078: Vocaloid/Logic-style f |
| `audio/` | `wb_voice_isolate` | 148 | math.h, stdlib.h, string.h, wbus_fft.h, wbus_voice_isolate.h | wb_voice_isolate.c — R018-D spectral voice isolation (RNNoise-style). Pipeline per channel: 1. Frame |
| `audio/` | `wb_voice_polish` | 442 | math.h, stdlib.h, string.h, wbus_dsp.h, wbus_voice_polish.h | wb_voice_polish.c — one-click voice-polish preset chain (R015 Tier 1). Chain: gate -> deesser -> com |
| `audio/` | `wb_vst3_editor` | 176 | SDL.h, stdio.h, stdlib.h, string.h, wbus_vst3.h | wb_vst3_editor.c — VST3 plugin editor window R091: Plugin editor UI with parameter knobs Provides a  |
| `audio/` | `wb_wah` | 195 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_wah.c — auto-wah / envelope filter for YTP funkiness. R080: Envelope-controlled bandpass — the cl |
| `audio/` | `wb_warp` | 366 | math.h, stdlib.h, string.h, wbus.h | wb_warp.c — R078: Ableton-style warp markers for elastic audio. Warp markers remap the source audio  |
| `audio/` | `wb_wav` | 251 | stdio.h, stdlib.h, string.h, wbus.h | wb_wav.c — WAV file reader/writer (16-bit PCM and 32-bit float). Pure C11, all ours. Standard RIFF/W |
| `audio/` | `wb_wavcache` | 106 | math.h, stdlib.h, string.h, wbus_wavcache.h | wb_wavcache.c — waveform LOD pyramid (R066). |
| `audio/` | `wb_wavetable` | 456 | math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_wavetable.c — wavetable synthesizer (Serum/Vital style). R078: Wavetable synthesis with morphing, |
| `audio/` | `wb_waveview` | 21 | wbus_waveview.h | wb_waveview.c — R074 hop 169 (G-SF077): waveform peak buckets for the arrangement view / stem inspec |
| `audio/` | `wb_workspace` | 90 | stdlib.h, string.h, wbus_workspace.h | wb_workspace.c — DaVinci-Resolve-style workspace/tier controller. See wbus_workspace.h. Self-contain |
| `audio/` | `wb_yin` | 155 | emmintrin.h, math.h, stdint.h, stdlib.h, string.h, wbus.h | wb_yin.c — YIN pitch detection algorithm. N2 [R076]: real-time pitch tracking for tuner/vocal analys |
| `audio/` | `wb_youtube_upload` | 472 | curl.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | wb_youtube_upload.c — YouTube direct upload via Data API v3 R090: Premiere Pro direct upload parity  |
| `audio/` | `wb_ytp` | 353 | math.h, stdint.h, stdlib.h, string.h | wb_ytp.c — YouTube Poop / meme editing engine (R077 Phase 5). YTP poopisms and meme editing techniqu |
| `audio/` | `wb_ytp_combo` | 335 | math.h, stdint.h, stdlib.h, string.h | wb_ytp_combo.c — YTP combination effects (R094f). Sex-O-Phone, Dance Rave, Meme Replacement, Paint J |
| `audio/` | `wb_ytp_fx` | 952 | math.h, stdint.h, stdlib.h, string.h | wb_ytp_fx.c — YTP/YTPMV effects engine (R094). Dark arts + poopisms + geometric warps + MIDI + autom |
| `audio/` | `wb_ytp_glitch` | 526 | math.h, stdint.h, stdlib.h, string.h | wb_ytp_glitch.c — Advanced Glitch Effects + Final YTP Gaps (R101). Closing the remaining gaps from R |
| `audio/` | `wb_ytp_remaining` | 224 | math.h, stdint.h, stdlib.h, string.h | wb_ytp_remaining.c — Final YTP gap closers (R094g). Sex-O-Phone, Dance Rave, Tennis Rally, Scramble+ |
| `audio/` | `wb_ytp_video_fx` | 430 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_ytp_video_fx.c — Video Poopisms Engine + YTPMV Pipeline (R099). Closing 30 gaps from the R094 gap |
| `audio/` | `wb_ytpmv` | 333 | math.h, stdint.h, stdlib.h, string.h | wb_ytpmv.c — YTPMV phoneme pipeline + YTP renderer (R094b). Phoneme extraction, pitch-to-note mappin |
| `audio/` | `wb_ytpmv_composite` | 316 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_composite.c — YTPMV multi-layer video compositing engine. Handles chroma key cutout, overla |
| `audio/` | `wb_ytpmv_engine` | 336 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_engine.c — YTPMV Production Engine (R124). The core engine that ties together: - Sample man |
| `audio/` | `wb_ytpmv_library` | 498 | math.h, stdint.h, stdio.h, stdlib.h, string.h | wb_ytpmv_library.c — YTPMV Source Library Manager (R135). Manages a library of source clips organize |
| `audio/` | `wb_ytpmv_lipsync` | 338 | stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_lipsync.c — C11 lip-sync/viseme engine for YTPMV (R131b). Maps ARPABET-style phonemes to 15 |
| `audio/` | `wb_ytpmv_machine` | 576 | math.h, stdint.h, stdlib.h, string.h | wb_ytpmv_machine.c — YTPMV production machine (R097). The missing pieces for world-class YTPMV produ |
| `audio/` | `wb_ytpmv_multi` | 437 | math.h, stdint.h, stdio.h, stdlib.h, string.h | wb_ytpmv_multi.c — Multi-Character YTPMV Engine (R131). Manages multiple characters, each with their |
| `audio/` | `wb_ytpmv_prod` | 222 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_prod.c — YTPMV Production Pipeline (R104). The actual workflow for making YTPMV: 1. Load so |
| `audio/` | `wb_ytpmv_renderer` | 258 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_renderer.c — YTPMV Beat-Synced Renderer + Relative Automation (R100). The final piece: ties |
| `audio/` | `wb_ytpmv_sync` | 425 | math.h, stdint.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_sync.c — YTPMV sync engine: audio-reactive video + lip sync (R095). The core YTPMV producti |
| `audio/` | `wb_ytpmv_vfx` | 475 | math.h, stdint.h, stdio.h, stdlib.h, string.h | wb_ytpmv_vfx.c — YTPMV Audio-Reactive Visual Effects Engine (R128). Generates ffmpeg filter strings  |
| `audio/` | `wb_ytpmv_vfx_pipeline` | 305 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | wb_ytpmv_vfx_pipeline.c — VFX Pipeline Integration (R132). Connects beat detection, VFX generation,  |
| `audio/` | `test_3d_camera` | 106 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_3d_camera.c — verify 3D camera + lighting system |
| `audio/` | `test_3d_edit` | 370 | assert.h, stdio.h, stdlib.h, string.h | test_3d_edit.c — test gate for wb_3d_edit.c Validates: - wb_node_source_stereo3d (source creation, p |
| `audio/` | `test_3d_overlay` | 241 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h, wbus_vfx.h | test_3d_overlay.c — 3D Character Overlay System tests (R098) |
| `audio/` | `test_aaf_export` | 228 | stdint.h, stdio.h, stdlib.h, string.h, wbus.h | test_aaf_export.c — AAF/OMF interchange export verification. Builds a 2-track session, exports to AA |
| `audio/` | `test_ai_mix` | 189 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_ai_mix.c — tests for the AI-assisted mixing tools. |
| `audio/` | `test_analysis` | 129 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_analysis.c — broadcast/metering audio analysis suite verification. Exercises wb_analysis_* with |
| `audio/` | `test_arrange_ai` | 125 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_arrange_ai.c — test AI arrangement assistant. |
| `audio/` | `test_atmos` | 185 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_atmos.c — Dolby Atmos-style spatial audio panner verification. Exercises wb_atmos_* with known  |
| `audio/` | `test_audio_color` | 83 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_audio_color.c — headless test of audio-reactive color grading |
| `audio/` | `test_audio_to_midi` | 301 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | test_audio_to_midi.c — gate test for wb_audio_to_midi. Verifies: create/destroy, sine wave pitch det |
| `audio/` | `test_autoreframe` | 57 | stdio.h, string.h, wbus.h | tests/test_autoreframe.c — test auto-reframe feature. |
| `audio/` | `test_beat_grid` | 151 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_beat_grid.c — Beat Grid Quantizer tests (R109) |
| `audio/` | `test_bg_render` | 291 | math.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wb_internal.h, wbus.h | tests/test_bg_render.c — background render API test suite. 1. Start a render — verify it begins 2. P |
| `audio/` | `test_bleep` | 78 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_bleep.c — headless test of bleep censor engine |
| `audio/` | `test_chord_ai` | 65 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_chord_ai.c — test AI chord progression generator. |
| `audio/` | `test_chromakey` | 194 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_chromakey.c — verify chroma key engine + ffmpeg chroma key wrappers |
| `audio/` | `test_clap_plugin` | 325 | math.h, stdbool.h, stdint.h, stdio.h, stdlib.h, string.h | test_clap_plugin.c — Minimal CLAP test plugin (.clap dylib). Exports TWO plugins via one factory: or |
| `audio/` | `test_cloud` | 199 | stdio.h, stdlib.h, string.h, unistd.h, wbus.h | test_cloud.c — gate test for wb_cloud (cloud project sync with versioning). Verifies: init, save/loa |
| `audio/` | `test_cloud_collab` | 51 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_cloud_collab.c — test cloud collaboration feature. |
| `audio/` | `test_color_grading` | 85 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_color_grading.c — test color grading feature. |
| `audio/` | `test_composition` | 85 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_composition.c — Composition Timeline System tests (R103) |
| `audio/` | `test_compositor_pro` | 61 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_compositor_pro.c — test professional compositor feature. |
| `audio/` | `test_cutout` | 108 | stdio.h, stdlib.h, string.h, unistd.h | test_cutout.c — Test cutout/rotoscope on real video |
| `audio/` | `test_datamosh` | 90 | assert.h, stdio.h, stdlib.h, string.h | test_datamosh.c — test gate for wb_datamosh.c |
| `audio/` | `test_drum_rack` | 284 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | test_drum_rack.c — gate test for wb_drum_rack (pad sampler). Verifies: create/destroy, load/trigger  |
| `audio/` | `test_dvd_author` | 104 | stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | test_dvd_author.c — verify DVD/Blu-ray authoring with proper navigation |
| `audio/` | `test_dvd_vm` | 187 | math.h, stat.h, stdio.h, stdlib.h, string.h, test_dvd_internal.h, unistd.h, wbus | test_dvd_vm.c — verify DVD Virtual Machine + game builder |
| `audio/` | `test_dynamics_adv` | 70 | math.h, stdio.h, stdlib.h, wbus.h | tests/test_dynamics_adv.c — test advanced dynamics feature. |
| `audio/` | `test_edit` | 750 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_internal.h, wbus_compositor.h, w | test_edit.c — test the video edit graph (R084) |
| `audio/` | `test_export` | 41 | stdio.h, stdlib.h, wbus_compositor.h | Check file exists |
| `audio/` | `test_export_queue` | 117 | stdint.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_edit.h, wbus_export_queue.h | test_export_queue.c — export queue wired to real node pipeline. Verifies: 1. Queue create/destroy |
| `audio/` | `test_expression` | 141 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_expression.c — headless test of expression maps / articulation management (G91). Verifies |
| `audio/` | `test_expressions` | 116 | math.h, stdio.h, stdlib.h, string.h, wbus_expressions.h | test_expressions.c — verify AE-style expression engine |
| `audio/` | `test_ffmpeg_filters` | 106 | stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ffmpeg_filters.c — verify advanced ffmpeg filter_complex wrappers |
| `audio/` | `test_formant` | 77 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_formant.c — headless test of formant shifter |
| `audio/` | `test_hdr_preview` | 82 | stdio.h, stdlib.h, string.h, wbus_compositor.h | test_hdr_preview.c — verify HDR preview |
| `audio/` | `test_kaleidoscope` | 83 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_kaleidoscope.c — headless test of kaleidoscope effect |
| `audio/` | `test_launchpad_mk2` | 222 | inttypes.h, math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wbus.h, wbus_mid | tests/test_launchpad_mk2.c — headless test of the Launchpad Mk2 driver + scale helpers + protocol-by |
| `audio/` | `test_linked_tracks` | 265 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_linked_tracks.c — linked track group editing tests. Verifies Ableton-style simultaneous m |
| `audio/` | `test_lottie` | 51 | stdio.h, string.h, wbus.h | tests/test_lottie.c — test Lottie renderer feature. |
| `audio/` | `test_macro_rack` | 274 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus.h | test_macro_rack.c — gate test for wb_macro_rack (Ableton Instrument Rack style). Verifies: create/de |
| `audio/` | `test_mask_node` | 188 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_mask_node.c — test cases for per-layer mask effect node (R090) |
| `audio/` | `test_master_adv` | 251 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_master_adv.c — gate test for wb_master_adv. Verifies: create/destroy, process, input/outp |
| `audio/` | `test_mastering_pro` | 298 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_mastering_pro.c — gate test for wb_mastering_pro. Verifies: create/destroy, process, inpu |
| `audio/` | `test_matte_fx` | 170 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_matte_fx.c — Matte Painting + Cutout System tests (R108) |
| `audio/` | `test_melody_ai` | 159 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_melody_ai.c — test AI melody composer feature. |
| `audio/` | `test_melody_follow` | 167 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_melody_follow.c — Melody-Following Pitch Mapper tests (R112) |
| `audio/` | `test_mesh_warp` | 321 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_mesh_warp.c — verification of mesh warp / puppet tool effect node. Tests: 1. No pins = identity |
| `audio/` | `test_metal_compositor` | 262 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_metal_compositor.c — verify Metal GPU compositor output matches CPU. Tests the GPU-accelerated  |
| `audio/` | `test_midi_generators` | 84 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_midi_generators.c — test MIDI generators feature. |
| `audio/` | `test_midi_remote` | 127 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_midi_remote.h | tests/test_midi_remote.c — verify wb_midi_remote mapping, process, save/load. Links only with build/ |
| `audio/` | `test_midi_scale` | 153 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_midi_scale.c — headless test of the MIDI scale quantizer. Verifies snapping, scale member |
| `audio/` | `test_mod_matrix` | 219 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_mod_matrix.c — modulation matrix tests. |
| `audio/` | `test_motion_blur` | 109 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_motion_blur.c — verify motion blur effect node (R086). Tests: 1. Node creation with default sam |
| `audio/` | `test_mpe` | 183 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_mpe.c — MPE (MIDI Polyphonic Expression) tests. |
| `audio/` | `test_null_object` | 91 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_null_object.c — verify null objects + parenting |
| `audio/` | `test_particle_gpu` | 214 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_particle_gpu.c — GPU particle system test suite. Tests: create/destroy, emit, update moti |
| `audio/` | `test_pdc` | 145 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_pdc.c — plugin delay compensation tests. |
| `audio/` | `test_podcast` | 285 | math.h, stdint.h, stdio.h, stdlib.h, string.h | tests/test_podcast.c — podcast production workflow test. Pure C11, standalone: only needs wb_podcast |
| `audio/` | `test_procedural` | 80 | stdio.h, stdlib.h, string.h, wbus_compositor.h | test_procedural.c — test procedural video generation (R084). Tests that compositor source nodes (tex |
| `audio/` | `test_project_templates` | 98 | assert.h, stdio.h, string.h, wbus.h | tests/test_project_templates.c — test project templates feature. |
| `audio/` | `test_realtime_preview` | 83 | stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | test_realtime_preview.c — verify real-time preview engine |
| `audio/` | `test_restoration` | 233 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_restoration.c — audio restoration suite test. Pure C11, standalone: only needs wb_restora |
| `audio/` | `test_sampler_inst` | 200 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_sampler_inst.c — Sampler Instrument + Piano Roll tests (R102) |
| `audio/` | `test_score` | 176 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_score.c — headless test of the MIDI-to-notation converter. Verifies note naming, octave c |
| `audio/` | `test_screen_record` | 59 | stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | test_screen_record.c — verify screen recording engine |
| `audio/` | `test_session_view` | 251 | assert.h, stdio.h, stdlib.h, string.h, wbus.h | test_session_view.c — gate test for wb_session_view (Ableton-style clip launcher). Verifies: create  |
| `audio/` | `test_sonogram` | 211 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_sonogram.h | tests/test_sonogram.c — sonogram / spectrogram visualization test. Pure C11, standalone: only needs  |
| `audio/` | `test_spatial_audio` | 226 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_spatial_audio.c — test HRTF binaural 3D audio panner. |
| `audio/` | `test_spectral_edit` | 286 | math.h, stdio.h, stdlib.h, string.h, wbus_spectral_edit.h | tests/test_spectral_edit.c — spectral audio repair tools test. Pure C11, standalone: only needs wb_s |
| `audio/` | `test_spectral_fx` | 259 | math.h, stdio.h, stdlib.h, string.h, wbus_spectral_fx.h | tests/test_spectral_fx.c — spectral effects gate test. Pure C11, standalone: only needs wb_spectral_ |
| `audio/` | `test_stabilize2` | 152 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_stabilize2.c — test video stabilization (phase correlation). |
| `audio/` | `test_stem_split` | 61 | math.h, stdio.h, wbus.h | tests/test_stem_split.c — test stem separation feature. |
| `audio/` | `test_stt_caption` | 121 | stdint.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_stt_caption.c — test harness for wb_stt_caption.c Tests the SRT parsing, caption lookup, and st |
| `audio/` | `test_subtitle_translate` | 195 | ctype.h, stdio.h, stdlib.h, string.h, wbus.h | test_subtitle_translate.c — R079 gate test for the subtitle translation engine. Tests: 1. Init with  |
| `audio/` | `test_surround_mix` | 340 | math.h, stdio.h, stdlib.h, string.h, wbus_edit.h | tests/test_surround_mix.c — 5.1 surround audio mixing verification. Tests wb_audio_mix_surround() to |
| `audio/` | `test_svg_import` | 78 | stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | test_svg_import.c — verify SVG import |
| `audio/` | `test_tempo_detect` | 100 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_tempo_detect.c — verify wb_tempo_detect on synthesized click trains. Links only with buil |
| `audio/` | `test_text_animator` | 205 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_text_animator.c — verify per-character text animation node. Tests: 1. Node creation with text i |
| `audio/` | `test_text_based_edit` | 215 | math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wbus_compositor.h, wbus_edit | test_text_based_edit.c — test text-based video editing (R086) |
| `audio/` | `test_text_edit` | 349 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wbus_transcript.h | test_text_edit.c — standalone test for text-based video editing. Tests the transcript manipulation l |
| `audio/` | `test_time_sig` | 211 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_time_sig.c — time signature changes feature tests. Verifies the sorted time-sig map, bar/ |
| `audio/` | `test_track_folder` | 266 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_track_folder.c — track folders and bus routing tests. Verifies folder hierarchy, child tr |
| `audio/` | `test_transitions_pro` | 84 | stdio.h, stdlib.h, string.h, wbus.h | tests/test_transitions_pro.c — test video transition pack. |
| `audio/` | `test_vgm` | 137 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_vgm.c — VGMusic MIDI parser tests (R096) |
| `audio/` | `test_video_fx_pro` | 69 | stdio.h, stdlib.h, string.h, wbus_compositor.h | test_video_fx_pro.c — test professional video FX nodes (R085) |
| `audio/` | `test_vocal_synth` | 114 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_vocal_synth.c — test vocal/formant synthesis feature. |
| `audio/` | `test_wah` | 115 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_wah.c — headless test of auto-wah / envelope filter |
| `audio/` | `test_wavetable` | 124 | math.h, stdio.h, stdlib.h, string.h, wbus.h | tests/test_wavetable.c — test wavetable synthesis feature. |
| `audio/` | `test_youtube_upload` | 60 | stdio.h, stdlib.h, string.h, wbus_compositor.h | test_youtube_upload.c — verify YouTube upload API |
| `audio/` | `test_ytp_combo` | 135 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytp_combo.c — YTP combination effects tests (R094f) |
| `audio/` | `test_ytp_fx` | 353 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h, wbus_param_track.h | test_ytp_fx.c — YTP effects engine tests (R094) |
| `audio/` | `test_ytp_glitch` | 153 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytp_glitch.c — Advanced Glitch Effects tests (R101) |
| `audio/` | `test_ytp_remaining` | 123 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytp_remaining.c — Final YTP gap closers (R094g) |
| `audio/` | `test_ytp_video_fx` | 290 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytp_video_fx.c — Video Poopisms + YTPMV Pipeline tests (R099) |
| `audio/` | `test_ytpmv_composite` | 181 | stdint.h, stdio.h, stdlib.h, string.h | tests/test_ytpmv_composite.c — YTPMV compositing engine tests. |
| `audio/` | `test_ytpmv_darkarts` | 185 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_darkarts.c — YTPMV pipeline + dark arts tests (R094b/c) |
| `audio/` | `test_ytpmv_library` | 137 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_library.c — Tests for YTPMV Source Library (R135) |
| `audio/` | `test_ytpmv_lipsync` | 172 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_lipsync.c — Lip-sync/viseme engine tests (R131b) |
| `audio/` | `test_ytpmv_machine` | 199 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_machine.c — YTPMV production machine tests (R097) |
| `audio/` | `test_ytpmv_multi` | 177 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_multi.c — Tests for Multi-Character YTPMV Engine (R131) |
| `audio/` | `test_ytpmv_prod` | 99 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_prod.c — YTPMV Production Pipeline tests (R104) |
| `audio/` | `test_ytpmv_renderer` | 131 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_renderer.c — YTPMV Renderer + Relative Automation tests (R100) |
| `audio/` | `test_ytpmv_sync` | 153 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_sync.c — YTPMV sync engine tests (R095) |
| `audio/` | `test_ytpmv_vfx` | 100 | math.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | test_ytpmv_vfx.c — Tests for YTPMV VFX Engine (R128) |
| `audio/` | `debug_clap` | 122 | dlfcn.h, stdio.h, stdlib.h, string.h | debug_clap.c — minimal dlopen test for bigmac-test.clap |
| `audio/` | `mk_burn` | 11 | stdio.h, wbus_captions.h | mk_burn.c — burn an SRT into a video via Big Mac's wb_captions_burn (ffmpeg subtitles filter). input |
| `audio/` | `mk_podcast` | 38 | stdio.h, stdlib.h, wb_internal.h, wbus_voice_polish.h | mk_podcast.c — Big Mac pipeline driver for a TTS podcast episode. Reads a raw WAV (mono/stereo), run |
| `audio/` | `mk_smb_ytpmv` | 478 | math.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | mk_smb_ytpmv.c — Super Mario Bros 1-1 YTPMV Producer (R113). Uses the actual SMB1OW.mid MIDI file fo |
| `audio/` | `mk_srt` | 52 | stdio.h, stdlib.h, string.h, wbus_transcript.h | mk_srt.c — build a Big Mac wb_transcript from Whisper word CSV, then export a real SRT via wb_transc |
| `audio/` | `mk_tts_podcast` | 49 | stdio.h, stdlib.h, string.h, wb_internal.h, wbus_tts.h, wbus_voice_polish.h | mk_tts_podcast.c — produce a podcast episode ENTIRELY with Big Mac's own code (no external/online TT |
| `audio/` | `mk_ytp` | 157 | math.h, stdio.h, stdlib.h, string.h, unistd.h | mk_ytp.c — YTP Production CLI (R107). Usage: mk_ytp <source_video.mp4> <output.mp4> [effects...] Eff |
| `audio/` | `mk_ytpmv` | 369 | math.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | mk_ytpmv.c — YTPMV Production CLI (R106). Usage: mk_ytpmv <source_audio.wav> <source_video.mp4> <out |
| `audio/` | `mk_ytpmv_bigmac` | 210 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | mk_ytpmv_bigmac.c — YTPMV Producer using Big Mac's sampler engine. Usage: mk_ytpmv_bigmac <events.tx |
| `audio/` | `mk_ytpmv_bigmac2` | 148 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_compositor.h | mk_ytpmv_bigmac2.c — YTPMV Producer using Big Mac's sampler engine. Usage: mk_ytpmv_bigmac2 <sample. |
| `audio/` | `mk_ytpmv_ffmpeg` | 437 | math.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wbus_compositor.h | mk_ytpmv_ffmpeg.c — YTPMV Producer using ffmpeg rubberband (R115). The CORRECT way to make YTPMV: 1. |
| `audio/` | `mk_ytpmv_sampler` | 424 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_compositor.h | mk_ytpmv_sampler.c — YTPMV Producer using Big Mac's sampler engine. Usage: mk_ytpmv_sampler <sample. |
| `audio/` | `test_agent` | 127 | stdio.h, stdlib.h, string.h, unistd.h, wb_internal.h, wbus.h, wbus_agent.h, wbus | test_agent.c — headless agent API (R017 G9) + G1/G5/G8 cross-check. Drives the editor via wb_agent_r |
| `audio/` | `test_agent_cgi` | 110 | stdio.h, stdlib.h, string.h, unistd.h, wbus_agent.h, wbus_assets.h | test_agent_cgi.c — R059: the AGI bridge drives the whole CGI pipeline through text commands. Proves: |
| `audio/` | `test_anim` | 102 | math.h, stdio.h, stdlib.h, string.h, wbus_anim.h | test_anim.c — verification of the R054 keyframed animation layer. Proves: sampling interpolates, obj |
| `audio/` | `test_anim2` | 94 | math.h, stdio.h, stdlib.h, string.h, wbus_anim.h | test_anim2.c — R055c verification: easing curves, camera keys, parenting. |
| `audio/` | `test_assets` | 88 | stat.h, stdio.h, stdlib.h, string.h, wbus_assets.h | test_assets.c — R057: asset library index/cache verification. Builds a synthetic kit tree (reusing t |
| `audio/` | `test_biquad4` | 89 | math.h, stdio.h, stdlib.h, wb_biquad_simd.h | test_biquad4.c — verify SIMD biquad matches scalar biquad |
| `audio/` | `test_biquad_cascade` | 153 | emmintrin.h, math.h, stdio.h, stdlib.h, string.h, wbus_biquad_cascade.h | test_biquad_cascade.c — gate test for wb_biquad_cascade_simd. Verifies: PFE correctness (cascade mat |
| `audio/` | `test_bvh` | 140 | math.h, stdio.h, stdlib.h, string.h, wbus_bvh.h | test_bvh.c — verify BVH parser (R081) |
| `audio/` | `test_captions` | 62 | stdio.h, stdlib.h, string.h, wbus_captions.h | test_captions.c — headless verification of the captions pipeline + G10 ASS styled-caption parser. Th |
| `audio/` | `test_cgi_agi` | 100 | math.h, stdio.h, string.h, wbus_agi.h, wbus_cgi.h | test_cgi_agi.c — R043-G7 gate: 3D-CGI scene model + AGI task bridge. |
| `audio/` | `test_cgiexport` | 80 | stdio.h, stdlib.h, string.h, wbus_cgiexport.h, wbus_mesh.h | test_cgiexport.c — R058 end-to-end: build an animation, export a video with the CGI overlay, ffprobe |
| `audio/` | `test_char2d` | 216 | math.h, stdio.h, stdlib.h, string.h | test_char2d.c — verify 2D character animation system |
| `audio/` | `test_clap` | 154 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_clap.h | test_clap.c — headless CLAP host verification (THE gate for plugin hosting). Loads the test .clap dy |
| `audio/` | `test_clip_edit` | 70 | math.h, stdio.h, wbus_clip_edit.h | test_clip_edit.c — R043 (G1/G2) clip-edit side-table gate. Verifies the opaque table: create/destroy |
| `audio/` | `test_comp_simd` | 130 | emmintrin.h, math.h, stdio.h, stdlib.h, string.h, wbus.h | test_comp_simd.c — gate test for wb_comp_simd (4-lane SIMD compressor). Verifies: create/destroy, SI |
| `audio/` | `test_compositor` | 2653 | math.h, stdio.h, stdlib.h, wb_ui.h, wbus.h, wbus_anim.h, wbus_cgi_bands.h, wbus_ | test_compositor.c — headless verification of keyframe tracks + pull node-graph compositor (R013 D1/D |
| `audio/` | `test_conv` | 197 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wbus_conv.h | test_conv.c — gate test for wb_conv (convolution reverb). Verifies: create/destroy, IR loading, part |
| `audio/` | `test_delivery` | 69 | math.h, stdio.h, stdlib.h, string.h, unistd.h, wbus.h, wbus_delivery.h | test_delivery.c — R064: delivery presets verification. Renders a session to WAV, measures its loudne |
| `audio/` | `test_drum_machine` | 105 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_drum_machine.c — gate test for wb_drum_machine (TR-808/909). Verifies: create/destroy, all voic |
| `audio/` | `test_drum_timing` | 141 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_internal.h, wb_unit.h, wbus.h | R076 FC1 drum timing harness — before (inline per-sample) vs after (hoisted). 8 drum voices, 512 fra |
| `audio/` | `test_dsp_simd_timing` | 76 | emmintrin.h, math.h, stdint.h, stdio.h, time.h, wb_dsp_simd.h | test_dsp_simd_timing.c — measure SIMD vs scalar DSP performance. Tests: sin, exp, log, tanh, pow, bi |
| `audio/` | `test_duck` | 75 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_duck.h | test_duck.c — R063: auto-ducking envelope verification. A voice track that SPEAKS for 1s, SILENT for |
| `audio/` | `test_export_e2e` | 109 | stdio.h, stdlib.h, string.h, wbus.h, wbus_video.h | test_export_e2e.c — headless end-to-end video export verification. Builds a session with a video tra |
| `audio/` | `test_export_mv` | 20 | stdio.h, wbus.h, wbus_agent.h, wbus_cgiexport.h |  |
| `audio/` | `test_fb1_timing` | 132 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_internal.h, wb_unit.h, wbus.h | test_fb1_timing.c — R076 FB1 measurement harness. Isolates the FM render path (wb_fm_render) and mea |
| `audio/` | `test_fc_timing` | 117 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_internal.h, wb_unit.h, wbus.h | test_fc_timing.c — R076 FC1 measurement harness: wb_osc_render phase-step precompute (linear/exponen |
| `audio/` | `test_fcpxml` | 82 | stdio.h, stdlib.h, string.h, wbus.h | test_fcpxml.c — R018-C FCPXML intent enrichment verification. Builds a session with a color-correcte |
| `audio/` | `test_fm_timing` | 123 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_internal.h, wb_unit.h, wbus.h | test_fm_timing.c — R076 FM render timing harness. Compares inline phase stepping vs precomputed phas |
| `audio/` | `test_fuzz` | 121 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_fuzz.c — gate test for wb_fuzz (Big Muff emulation). Verifies: create/drive, distortion audible |
| `audio/` | `test_g1` | 192 | math.h, stdint.h, stdio.h, stdlib.h, string.h, time.h, wbus.h | test_g1_lazyclear.c — R075 G1 measurement harness. Before/after signal for the lazy-clear track buff |
| `audio/` | `test_g2_fm_timing` | 125 | math.h, stdint.h, stdio.h, stdlib.h, string.h, time.h | test_g2_fm_timing.c — R076 G2: SIMD-accelerated FM sin cascade timing. Standalone harness: links ONL |
| `audio/` | `test_g3_fm_timing` | 208 | math.h, stdint.h, stdio.h, stdlib.h, string.h, time.h | test_g3_fm_timing.c — G3: dual-core MT FM render timing. Measures: scalar (1 thread) vs MT scalar (2 |
| `audio/` | `test_gltf` | 134 | math.h, stdio.h, stdlib.h, string.h, wbus_gltf.h, wbus_mesh.h | test_gltf.c — R056: GLB import round-trip. Hand-writes a VALID glTF 2.0 GLB container (header + JSON |
| `audio/` | `test_granular` | 54 | math.h, stdio.h, stdlib.h, string.h | test_granular.c — quick smoke test for granular synth |
| `audio/` | `test_hpss` | 57 | math.h, stdio.h, stdlib.h, string.h, wbus_hpss.h | test_hpss.c — verify R020-A harmonic-percussive source separation. |
| `audio/` | `test_karplus` | 102 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_karplus.c — gate test for wb_karplus (Karplus-Strong). Verifies: pluck produces sound, pitch fr |
| `audio/` | `test_keys` | 84 | stdint.h, stdio.h, string.h | test_keys.c — verify keyboard shortcut system |
| `audio/` | `test_ladder` | 116 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_ladder.c — gate test for wb_ladder (Moog ladder filter). Verifies: create/destroy, lowpass beha |
| `audio/` | `test_light2d` | 97 | math.h, stdint.h, stdio.h, stdlib.h, string.h | test_light2d.c — verify 2D lighting system |
| `audio/` | `test_loudness_meter` | 59 | math.h, stdio.h, stdlib.h, string.h, wbus_voice_polish.h | test_loudness_meter.c — R018-G live gated loudness meter verification. Feeds synthetic audio through |
| `audio/` | `test_mesh` | 120 | math.h, stdio.h, stdlib.h, string.h, wbus_mesh.h | test_mesh.c — verification of the R053 primitive mesh library + obj loader. Every primitive is rende |
| `audio/` | `test_mix_simd` | 105 | emmintrin.h, math.h, stdio.h, stdlib.h, string.h, wbus.h | test_mix_simd.c — gate test for wb_mix_simd (mixer bus + constant-power pan). Verifies: pan law corr |
| `audio/` | `test_mod` | 136 | math.h, stdio.h, stdlib.h, string.h, wbus_mod.h | test_mod.c — verification of the R055b modifier stack. Each modifier is applied and the RESULT is re |
| `audio/` | `test_mv` | 38 | stdio.h, wbus.h, wbus_agent.h, wbus_video.h | R071 debug: replicate the agent path exactly — command handler calls checkpoint BEFORE do_import_aud |
| `audio/` | `test_ofx` | 89 | math.h, stdio.h, stdlib.h, wbus.h, wbus_compositor.h, wbus_ofx.h, wbus_param_tra | test_ofx.c — headless verification of the minimal OpenFX host (R017 G4). Loads the builtin Brightnes |
| `audio/` | `test_perf` | 113 | stdint.h, stdio.h, stdlib.h, string.h, wbus_mesh.h, wbus_perf.h | test_perf.c — R065: live performance capture + deterministic replay. Proves: fire/fade/param events  |
| `audio/` | `test_perf_export` | 85 | stdio.h, stdlib.h, string.h, unistd.h, wbus.h, wbus_agent.h, wbus_cgiexport.h, w | test_perf_export.c — R070: perf-clip → video export passthrough. Freezes a recorded VJ set onto the  |
| `audio/` | `test_perf_freeze` | 66 | stdio.h, stdlib.h, wbus.h, wbus_agent.h, wbus_perf.h, wbus_perfclip.h, wbus_shad | test_perf_freeze.c — R068 end-to-end: recorded perf → timeline clip → agent `perf-freeze` command pr |
| `audio/` | `test_perfclip` | 85 | math.h, stdio.h, stdlib.h, wbus_perf.h, wbus_perfclip.h | test_perfclip.c — R068: perf recording → nested timeline clip. Records a tiny VJ set (fire A, crossf |
| `audio/` | `test_phaser` | 93 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_phaser.c — gate test for wb_phaser (allpass cascade + LFO). Verifies: create/destroy, process d |
| `audio/` | `test_pitch_correct` | 96 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_pitch_correct.c — gate test for wb_pitch_correct (auto-tune). Verifies: scale snapping, correct |
| `audio/` | `test_rast` | 108 | math.h, stdio.h, stdlib.h, string.h, wbus_rast.h | test_rast.c — headless verification of the R052 software rasterizer. Proves, by pixel inspection: -  |
| `audio/` | `test_sat_simd` | 119 | emmintrin.h, math.h, stdio.h, stdlib.h, string.h, wb_internal.h, wbus.h | test_sat_simd.c — gate test for wb_sat_simd (polynomial tanh saturation). Verifies: create/destroy,  |
| `audio/` | `test_shadowbin` | 125 | math.h, stdio.h, stdlib.h, string.h, wbus_mesh.h, wbus_shadowbin.h | test_shadowbin.c — R061: shadow bin round-trip. Build a session with video + audio + markers, write  |
| `audio/` | `test_stereo` | 144 | emmintrin.h, math.h, stdio.h, stdlib.h, string.h, wbus.h | test_stereo.c — gate test for wb_stereo (M/S widening + Haas). Verifies: M/S encode/decode correctne |
| `audio/` | `test_synth_timing` | 145 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_internal.h, wb_unit.h, wbus.h | R076 FC1 synth timing harness — before (inline per-sample inc divide) vs after (hoisted phase_step[] |
| `audio/` | `test_thumbnail` | 57 | stdio.h, stdlib.h, string.h, wbus_perf.h, wbus_perfclip.h | test_thumbnail.c — R069: performance deck thumbnails. Each deck renders a cached RGBA thumbnail when |
| `audio/` | `test_transcript` | 163 | math.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_transcript.h | test_transcript.c — headless verification of the editable transcript model (R015 G6): SRT parse -> w |
| `audio/` | `test_tts` | 71 | math.h, stdio.h, stdlib.h, string.h, wbus_tts.h | test_tts.c — verify the legitimate in-repo TTS engine (R020). Big Mac HOSTS Piper (VITS, Apache-2.0, |
| `audio/` | `test_vfx` | 141 | math.h, stdint.h, stdio.h, stdlib.h, string.h | test_vfx.c — verify VFX system |
| `audio/` | `test_video` | 150 | stdio.h, stdlib.h, string.h, wbus.h, wbus_video.h | test_video.c — headless tests for the video editor core (R008/R009/R016). Verifies Tier 0 (track-kin |
| `audio/` | `test_video_edit` | 181 | math.h, stdint.h, stdio.h, stdlib.h, string.h | test_video_edit.c — verify video editing system |
| `audio/` | `test_video_tools` | 58 | stat.h, stdio.h, stdlib.h, string.h, wbus.h, wbus_video.h | test_video_tools.c — headless verification of split-clip + auto-clip-shorts (R015 Tier 3, EDIT-tab ^ |
| `audio/` | `test_voice_isolate` | 96 | math.h, stdio.h, stdlib.h, string.h, wbus_fft.h, wbus_voice_isolate.h | test_voice_isolate.c — R018-D spectral voice isolation + FFT sanity. 1. FFT round-trip: real FFT ->  |
| `audio/` | `test_voice_polish` | 160 | math.h, stdio.h, stdlib.h, string.h, wbus_dsp.h, wbus_voice_polish.h | test_voice_polish.c — headless verification of the voice-polish preset chain (R015 Tier 1): gate ->  |
| `audio/` | `test_warp` | 260 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_warp.c — gate test for wb_warp (Ableton-style warp markers). Verifies: create/destroy, markers, |
| `audio/` | `test_wavcache` | 72 | math.h, stdio.h, stdlib.h, wbus_wavcache.h | test_wavcache.c — R066: waveform LOD pyramid verification. A known signal (full-scale square wave fo |
| `audio/` | `test_workspace` | 67 | stdio.h, string.h, wbus_workspace.h | test_workspace.c — R043 Fusion-style workspace/tier controller gate. Verifies: create/destroy, defau |
| `audio/` | `test_yin` | 116 | math.h, stdio.h, stdlib.h, string.h, wbus.h | test_yin.c — gate test for wb_yin (YIN pitch detection). Verifies: sine wave pitch detection, SIMD v |
| `audio/` | `test_ytp` | 147 | math.h, stdint.h, stdio.h, stdlib.h, string.h | test_ytp.c — verify YTP/meme editing engine |
| `audio/` | `vec_sin_test` | 287 | emmintrin.h, math.h, stdio.h, stdlib.h | vec_sin_4: 4-wide SSE2 sine for FM G2. Phases assumed in [0, 2π). Range-reduces via quadrant folding |
| `audio/` | `wb_asset_dl` | 286 | stdio.h, stdlib.h, string.h, unistd.h | wb_asset_dl.c — character asset downloader for YTP/3D poop (R082). Fetches sprites and 3D models fro |
| `audio/` | `wb_daw` | 6259 | SDL.h, math.h, stat.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wb_interna | wb_daw.c — Big Mac DAW application. SDL2 window with: - Transport bar: play/stop, BPM, bar:beat, tim |
| `audio/` | `wb_fm_g2` | 305 | emmintrin.h, math.h, stdint.h, stdio.h, stdlib.h, time.h | CEPHES-derived minimax coefficients for sin(x) on [-π/4,π/4]. Max error ~2.3e-7 (about 2^22 ULPs at  |
| `audio/` | `wb_midi_note_to_freq_stub` | 16 | math.h | wb_midi_note_to_freq_stub.c — standalone object providing only wb_midi_note_to_freq (needed by wb_fm |
| `audio/` | `wb_render` | 850 | errno.h, math.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wb_internal.h, w | wb_render.c — CLI offline render: build/load a session, render the whole song to a WAV file through  |
| `audio/` | `wb_selftest` | 4951 | math.h, stat.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wb_internal.h, wb | wb_selftest.c — headless engine self-test (THE GATE). Verifies transport, timeline scheduling, DSP u |
| `audio/` | `wb_ytp_director` | 302 | ctype.h, stdio.h, stdlib.h, string.h | wb_ytp_director.c — YTP Director: text description → full YTP edit (R083). This is the "brain" that  |
| `audio/` | `wubuglott_artic` | 131 | math.h, string.h, wubuglott_artic.h, wubuglott_math.h | wubuglott_artic.c -- the articulatory model. See wubuglott_artic.h. Slermed from chimera's anatomy/h |
| `audio/` | `wubuglott_artic_test` | 117 | egojar.h, math.h, stdio.h, stdlib.h, string.h, wubuglott_artic.h, wubuglott_math | wubuglott_artic_test.c -- behavior gate for the articulatory model. Not assert-nothing: checks the n |
| `audio/` | `wubuglott_decimator` | 104 | math.h, stdlib.h, wubuglott_decimator.h, wubuglott_math.h | wubuglott_decimator.c -- Kaiser-windowed sinc FIR decimator. See wubuglott_decimator.h. Slermed from |
| `audio/` | `wubuglott_decimator_test` | 125 | egojar.h, math.h, stdio.h, stdlib.h, wubuglott_decimator.h, wubuglott_math.h | wubuglott_decimator_test.c -- behavior gate for the FIR decimator. Not assert-nothing: checks the fi |
| `audio/` | `wubuglott_geom` | 178 | stdlib.h, wubuglott_geom.h, wubuglott_math.h | wubuglott_geom.c -- 2D geometry for the articulatory model. See wubuglott_geom.h. Slermed from chime |
| `audio/` | `wubuglott_geom_test` | 153 | egojar.h, math.h, stdio.h, stdlib.h, wubuglott_geom.h, wubuglott_math.h | wubuglott_geom_test.c -- behavior gate for the articulatory geometry. Not assert-nothing: checks the |
| `audio/` | `wubuglott_glottis` | 159 | wubuglott_glottis.h, wubuglott_math.h | wubuglott_glottis.c -- parametric vocal-fold source. See wubuglott_glottis.h for the model. Slermed  |
| `audio/` | `wubuglott_glottis_test` | 105 | egojar.h, math.h, stdio.h, string.h, wubuglott_glottis.h, wubuglott_math.h | wubuglott_glottis_test.c -- behavior gate for the vocal-fold source. Not assert-nothing: drives the  |
| `audio/` | `wubuglott_network` | 772 | math.h, stdlib.h, string.h, wubuglott_math.h, wubuglott_network.h | wubuglott_network.c -- branched airway acoustics. See wubuglott_network.h. Slermed from chimera-voca |
| `audio/` | `wubuglott_network_test` | 182 | egojar.h, math.h, stdio.h, stdlib.h, string.h, wubuglott_math.h, wubuglott_netwo | wubuglott_network_test.c -- behavior gate for the airway acoustics. Not assert-nothing: builds a rea |
| `audio/` | `wubuglott_voice` | 410 | math.h, stdlib.h, string.h, wubuglott_math.h, wubuglott_voice.h | wubuglott_voice.c -- the instrument. See wubuglott_voice.h. Slermed from chimera's human_voice.rs (r |
| `audio/` | `wubuglott_voice_test` | 179 | egojar.h, math.h, stdio.h, stdlib.h, string.h, wubuglott_math.h, wubuglott_voice | wubuglott_voice_test.c -- the instrument gate. Not assert-nothing: renders a real second of audio an |
| `audio/` | `test_wubu_audio_backend` | 79 | bigmac_out.h, egojar.h, math.h, stdio.h, stdlib.h, wubu_audio.h | test_wubu_audio_backend.c -- behavior gate for the device output path. Proves the whole chain the pr |
| `audio/` | `wubu_audio` | 69 | wubu_audio.h, wubu_audio_internal.h | wubu_audio.c  --  WuBuOS Audio Engine (Facade) Modular audio engine: chip emulations, furnace tracke |
| `audio/` | `wubu_audio_backend` | 100 | bigmac_out.h, egojar.h, stdio.h, wubu_audio.h, wubu_audio_internal.h | wubu_audio_backend.c -- the device output thread. The engine renders into a caller buffer; big-mac w |
| `audio/` | `wubu_audio_chips` | 408 | wubu_audio_internal.h | wubu_audio_chips.c  --  WuBuOS Chip Emulations All retro chip emulations (NES, GB, YM2612, SID, etc. |
| `audio/` | `wubu_audio_daw` | 210 | wubu_audio_internal.h | wubu_audio_daw.c  --  WuBuOS DAW Mixer (Ardour-style) Track management, bus routing, master processi |
| `audio/` | `wubu_audio_engine` | 365 | wubu_audio_internal.h | wubu_audio_engine.c  --  WuBuOS Audio Engine Main Engine lifecycle, process callback, global state.  |
| `audio/` | `wubu_audio_furnace` | 347 | wubu_audio_internal.h | wubu_audio_furnace.c  --  WuBuOS Furnace Tracker Chip tracker with pattern editor, Furnace-style. Ex |
| `audio/` | `wubu_audio_sf2` | 164 | wubu_audio_internal.h | wubu_audio_sf2.c  --  WuBuOS TinySoundFont SF2 Synthesis Simplified SF2 parser and sample playback e |
| `audio/` | `wubu_audio_sink` | 67 | bigmac_out.h, egojar.h, stdlib.h, wubu_audio.h | wubu_audio_sink.c -- the device sink for the sound system. Separate from wubu_audio_backend.c on pur |
| `audio/` | `wubu_audio_test` | 296 | assert.h, errno.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubu_math.h | wubu_audio_test.c  --  Tests for audio engine (DAW + Furnace + SF2 + AI) |
| `audio/` | `wubu_glott` | 117 | egojar.h, math.h, stdlib.h, string.h, wubu_audio.h, wubuglott_math.h, wubuglott_ | wubu_glott.c -- the WuBuGLOTT facade: make the voice speak out loud. WuBuGLOTT (src/audio/glott/) re |
| `audio/` | `wubu_glott_speak_test` | 123 | egojar.h, math.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubuglott_math.h, w | test_glott_speak.c -- the WuBuGLOTT product facade gate. Speaks every cardinal vowel through the dev |
| `bear/` | `bear_arena` | 82 | bear_arena.h, stdio.h | bear_arena.c  --  PufferC/BearRL Arena Allocator Implementation |
| `bear/` | `bear_cudnn` | 864 | bear_cudnn.h, cublas_v2.h, cuda_runtime.h, cudnn.h, math.h, stdint.h, stdlib.h,  | bear_cudnn.c  --  cuBLAS/cuDNN Wrapper Implementations HolyC-callable cuBLAS/cuDNN operations Compil |
| `bear/` | `bear_cudnn_cublas` | 220 | bear_cudnn.h, bear_simd.h, math.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `bear/` | `bear_cudnn_cuda` | 75 | bear_cudnn.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `bear/` | `bear_env` | 430 | bear_arena.h, bear_env.h, bear_env_internal.h, bear_simd.h, math.h, stdio.h, std | bear_env.c  --  PufferC/BearRL Vectorized Environment Implementation |
| `bear/` | `bear_env_npole` | 562 | bear_arena.h, bear_env.h, bear_env_internal.h, bear_simd.h, math.h, stdio.h, std | bear_env_npole.c -- WuBuOS BearRL N-Pole Cartpole (7-10 poles) Extracted from bear_env.c (monolith s |
| `bear/` | `bear_nn_ckpt` | 226 | bear_arena.h, bear_nn.h, stdio.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `bear/` | `bear_nn_policy` | 382 | bear_arena.h, bear_nn.h, bear_simd.h, math.h, stdio.h, stdlib.h, string.h | bear_nn_policy.c -- WuBuOS BearRL policy-network implementation (MLP/minGRU create, forward, sample, |
| `bear/` | `bear_nn_value` | 598 | bear_arena.h, bear_nn.h, math.h, stdio.h, stdlib.h, string.h | bear_nn_value.c -- WuBuOS BearRL value-network implementation (create, forward, orthogonal init, bac |
| `bear/` | `bear_opt` | 326 | bear_arena.h, bear_opt.h, bear_simd.h, math.h, stdlib.h, string.h | bear_opt.c  --  PufferC/BearRL Optimizer Implementation (Adam + Muon) |
| `bear/` | `bear_opt_test` | 151 | bear_arena.h, bear_opt.h, math.h, stdio.h, stdlib.h, string.h | bear_opt_test.c  --  BearRL Optimizer Closure Test (form≠function) Verifies that bear_optimizer_step |
| `bear/` | `bear_ppo_loss` | 241 | bear_arena.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h, stdio.h, s | bear_ppo_loss.c -- BearRL PPO loss computation, gradient clipping, and gradient application (extract |
| `bear/` | `bear_ppo_trainer` | 401 | bear_arena.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h, stdio.h, s | bear_ppo_trainer.c -- BearRL PPO trainer lifecycle, training iteration, and checkpoint save/load (ex |
| `bear/` | `bear_ppo_traj` | 291 | bear_arena.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h, stdio.h, s | bear_ppo_traj.c -- BearRL PPO trajectory buffers, GAE/V-Trace advantages, and minibatch sampler (ext |
| `bear/` | `bear_train` | 193 | bear_arena.h, bear_env.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h | bear_train.c  --  BearRL Training Entry Point Sovereign C11 RL training for N-Pole Cartpole (7-10 po |
| `bear/` | `bear_vulkan_soft` | 459 | bear_arena.h, bear_env.h, bear_nn.h, bear_ppo.h, bear_vulkan.h, math.h, stdio.h, | bear_vulkan_soft.c  --  BearRL Vulkan API Software Fallback (Pure C11) Implements the bear_vulkan.h  |
| `bear/` | `bear_train_chained` | 831 | alloca.h, bear_arena.h, bear_holo_opt.h, bear_mujoco.h, bear_nn.h, bear_opt.h, b | bear_train_chained.c — N-pole CHAINED cartpole training with knock-over reset Progressive curriculum |
| `bear/` | `bear_train_curriculum` | 128 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_mujoco.h, bear_nn.h, bear_ | bear_train_curriculum.c — Curriculum training 1→7 poles Progressive learning: each pole count uses p |
| `bear/` | `cartpole8_verify` | 381 | assert.h, math.h, stdio.h, stdlib.h, string.h | cartpole8_verify.c — Full C11 Verification of clearspring3/cartpole8 Solution Exact reimplementation |
| `bear/` | `record_solved` | 341 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_nn.h, bear_opt.h, bear_ppo | record_solved.c — Record video of already-solved CartPole using working trained model |
| `bear/` | `test_cartpole_env` | 73 | bear_arena.h, bear_env.h, math.h, stdio.h, stdlib.h | Quick CartPole environment sanity check |
| `bear/` | `test_chained_reward` | 194 | bear_arena.h, bear_mujoco.h, bear_nn.h, bear_ppo.h, egojar.h, math.h, stdint.h,  | Debug training: print rewards from chained_env_step |
| `bear/` | `test_env_reward` | 52 | bear_arena.h, bear_env.h, bear_nn.h, math.h, stdio.h, stdlib.h | Test env step reward computation |
| `bear/` | `test_gaad_update` | 152 | bear_arena.h, bear_gaad.h, bear_nn.h, bear_opt.h, bear_ppo.h, math.h, stdint.h,  | Test if GAAD updates weights after backward pass |
| `bear/` | `test_holo_opt` | 151 | bear_holo_opt.h, math.h, stdio.h, stdlib.h | Test holographic optimizer with bytropix benchmarks |
| `bear/` | `test_knockover_reset` | 200 | bear_arena.h, bear_mujoco.h, math.h, stdint.h, stdio.h, stdlib.h, string.h, time | test_knockover_reset.c — Test the knock-over reset strategy Start upright, kick to fall, observe fal |
| `bear/` | `test_knockover_standalone` | 188 | bear_mujoco.h, math.h, stdint.h, stdio.h, stdlib.h, string.h, time.h | test_knockover_standalone.c — Test knock-over reset without PPO |
| `bear/` | `test_knockover_train` | 394 | alloca.h, bear_arena.h, bear_mujoco.h, bear_nn.h, bear_opt.h, bear_ppo.h, math.h | test_knockover_train.c — Minimal PPO training with knock-over reset Uses the working BearRL API |
| `bear/` | `test_mingru` | 77 | bear_arena.h, bear_nn.h, stdio.h, stdlib.h, time.h | act |
| `bear/` | `test_physics` | 88 | bear_arena.h, bear_env.h, math.h, stdio.h |  |
| `bear/` | `test_policy_forward` | 66 | bear_arena.h, bear_env.h, bear_nn.h, math.h, stdint.h, stdio.h, stdlib.h | Quick test: do random weights produce non-zero actions? |
| `bear/` | `verify_cartpole8` | 282 | assert.h, math.h, stdio.h, stdlib.h, string.h | C Verification of 8-pole Cartpole Solve (from clearspring3/cartpole8) This re-implements the exact v |
| `bear/` | `verify_gaad_bug` | 39 | bear_arena.h, bear_gaad.h, stdio.h, stdlib.h, time.h | Quick test: GAAD Poincaré projection is O(n²) - inside the per-param loop |
| `bear/` | `curriculum_train` | 233 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_nn.h, bear_opt.h, bear_ppo | curriculum_train.c — Full 1→20 Curriculum with Training + Video Uses proven training from bear_cartp |
| `bear/` | `train_cartpole_gaad` | 601 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_nn.h, bear_opt.h, bear_ppo | train_cartpole_gaad.c — CartPole 1-10 Solver using GAAD Optimizer + Geometric Encoder Clean producti |
| `bear/` | `train_cartpole_gaad_v2` | 556 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_nn.h, bear_opt.h, bear_ppo | train_cartpole_gaad_v2.c — CartPole 1-10 Solver using GAAD Optimizer + Geometric Encoder Proper patt |
| `bear/` | `train_cartpole_gaad_video` | 478 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_nn.h, bear_opt.h, bear_ppo | train_cartpole_gaad_video.c — Full 1-20 Curriculum with Video Capture Renders frames to PPM, encodes |
| `bear/` | `train_curriculum` | 64 | alloca.h, bear_arena.h, bear_env.h, bear_gaad.h, bear_mujoco.h, bear_nn.h, bear_ | Minimal curriculum trainer with expert swing-up for 1-pole Builds on working bear_train_chained from |
| `bear/` | `train_gaad_cartpole` | 194 | GAAD_Encoder.h, bear_arena.h, bear_env.h, bear_gae.h, bear_optimizer.h, bear_pol |  |
| `brain/` | `simplehash` | 421 | math.h, stdio.h, stdlib.h, string.h, time.h | simplehash.c — Pure C port of SimpleHashV1-V3 (WuBu HashMind encoder) No JAX, no numpy, no Python. P |
| `brain/` | `check_chars` | 37 | stdio.h, tokenizer.h | Check what codes each ASCII char maps to |
| `brain/` | `debug_check` | 75 | hashmind_data.h, hashmind_model.h, stdio.h, string.h, tokenizer.h | Check for nan in logits |
| `brain/` | `debug_train_check` | 100 | hashmind_data.h, hashmind_model.h, math.h, stdio.h, stdlib.h, string.h, tokenize | Load training_sample.txt |
| `brain/` | `gradient_check` | 99 | hashmind_model.h, math.h, rolling_hash.h, stdio.h, tokenizer.h | gradient_check.c — Compare analytical backward pass against numerical gradients |
| `brain/` | `hashmind_data` | 118 | hashmind_data.h, hashmind_model.h, rolling_hash.h, stdio.h, stdlib.h, string.h,  | We need n - HASH_WINDOW + 1 training examples (each predicts next char) |
| `brain/` | `hashmind_model` | 488 | hashmind_model.h, math.h, rolling_hash.h, stdio.h, stdlib.h, time.h | hashmind_model.c — Full forward + backward pass for HashMind transformer Architecture: Embed → N×(At |
| `brain/` | `param_map` | 88 | hashmind_model.h, nn_ops.h, stdio.h, tokenizer.h | gradient_check.c — Identify failing weight by index |
| `brain/` | `quick_test` | 68 | hashmind_data.h, hashmind_model.h, math.h, stdio.h, stdlib.h, string.h, tokenize | Just do a quick small test |
| `brain/` | `rolling_hash` | 37 | rolling_hash.h |  |
| `brain/` | `test_check` | 64 | hashmind_model.h, rolling_hash.h, stdio.h, string.h, tokenizer.h | Single forward + loss check |
| `brain/` | `tokenizer` | 61 | tokenizer.h | Maps all printable ASCII (32-126) + \n(10) + \t(9) |
| `brain/` | `train` | 188 | hashmind_data.h, hashmind_model.h, stdio.h, stdlib.h, string.h, time.h, tokenize | train.c — HashMind Training Entry Point (pure C) Usage: ./train [options] --data  <file>   Training  |
| `brain/` | `wubu_math` | 120 | stdlib.h, string.h, wubu_math.h | wubu_math.c — Pure C implementation of WuBu hyperbolic math Implements Möbius addition and gyration  |
| `brain/` | `tailslayer-trefi-probe` | 335 | getopt.h, math.h, mman.h, sched.h, stdint.h, stdio.h, stdlib.h, string.h, time.h | trefi_probe.c — Detect periodic DRAM refresh (tREFI) jitter via clflush timing Probes a single addre |
| `brain/` | `wubu_kv_runtime` | 0 |  |  |
| `brain/` | `quants` | 1193 | arch-fallback.h, assert.h, float.h, ggml-common.h, ggml-cpu-impl.h, ggml-quants. |  |
| `brain/` | `bench` | 1094 | bench.h, cublas_v2.h, cuda_runtime.h |  |
| `brain/` | `dequant_iq2_xxs` | 198 | stdint.h, string.h, wubu_fp16.h | IQ2_XXS block-level operations for on-the-fly dequant dot product. The block format matches llama.cp |
| `brain/` | `gaad_nesting_llm` | 124 | gaad_nesting_llm.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `gguf_reader` | 2064 | gguf_reader.h, iq2s_grid.inc, iq3s_grid.inc, iq3xxs_grid.inc, math.h, mman.h, st |  |
| `brain/` | `kv_paged_attention` | 617 | assert.h, cuda_runtime.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_model.h | kv_paged_attention.c — PagedAttention KV Cache Manager (vLLM Style) Implements block-based KV cache  |
| `brain/` | `lfm2_attn` | 116 | lfm2_attn.h, lfm2_math.h, math.h, stdlib.h, string.h | lfm2_attn.c -- LFM2.5 GQA attention + RoPE + KV cache (C11, self-contained). |
| `brain/` | `lfm2_conv` | 98 | lfm2_conv.h, lfm2_math.h, stdlib.h, string.h | lfm2_conv.c -- LFM2.5 gated depthwise-causal-conv block (C11, self-contained). |
| `brain/` | `lfm2_ffn` | 41 | lfm2_ffn.h, lfm2_math.h, math.h, stdio.h, stdlib.h | lfm2_ffn.c -- LFM2.5 SwiGLU feed-forward (C11, self-contained). |
| `brain/` | `lfm2_forward` | 335 | gguf_reader.h, lfm2_attn.h, lfm2_conv.h, lfm2_ffn.h, lfm2_forward.h, lfm2_math.h | lfm2_forward.c -- LFM2.5 forward orchestrator (C11, self-contained). |
| `brain/` | `lfm2_load` | 473 | dirent.h, gguf_reader.h, lfm2_load.h, safetensors_reader.h, stat.h, stdio.h, std | lfm2_load.c -- LFM2.5 loader (C11, self-contained). MODIFIED (2026-08-07): loads BOTH safetensors sh |
| `brain/` | `lfm2_math` | 36 | gguf_reader.h, lfm2_math.h, math.h, stdint.h | lfm2_math.c -- self-contained numeric primitives (C11, <math.h> only). |
| `brain/` | `qlearner` | 74 | math.h, qlearner.h, stdlib.h, string.h | qlearner.c — Q-Learning LR Controller (meta-optimizer) Tunes the learning rate based on training dyn |
| `brain/` | `quantized_dot_generic` | 1284 | arm_neon.h, assert.h, gguf_reader.h, immintrin.h, math.h, smmintrin.h, stdint.h, | Non-256-divisible activation dim (e.g. WuBu-35M d_model=448). Q8_K block layout can't represent a pa |
| `brain/` | `quantized_matmul` | 723 | assert.h, gguf_reader.h, immintrin.h, math.h, omp.h, stdint.h, stdio.h, stdlib.h | quantized_matmul.c — Generic Q8_K-based quantized matrix multiplication. Self-contained: all vec_dot |
| `brain/` | `quantized_matmul_fixed` | 96 | gguf_reader.h, immintrin.h, stddef.h, stdint.h |  |
| `brain/` | `rsgd` | 105 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h | rsgd.c — Riemannian SGD optimizer for Poincaré ball parameters RSGD allows gradient-based optimizati |
| `brain/` | `safetensors_reader` | 489 | fcntl.h, math.h, mman.h, safetensors_reader.h, stat.h, stdint.h, stdio.h, stdlib | safetensors_reader.c -- HuggingFace safetensors loader (C11, self-contained). Parses the JSON header |
| `brain/` | `safetensors_writer` | 88 | safetensors_writer.h, stdio.h, stdlib.h, string.h | safetensors_writer.c -- write safetensors files (F32). Layout: [ uint64 LE header_len ][ header JSON |
| `brain/` | `thread_pool` | 94 | omp.h, stdio.h, stdlib.h, string.h, thread_pool.h |  |
| `brain/` | `tile_manager` | 203 | assert.h, cuda_runtime.h, stdio.h, stdlib.h, string.h, wubu_turboquant.h | tile_manager.c — Framebuffer-Style KV Tile Manager Treats KV cache like GPU framebuffer: - Tiles = 6 |
| `brain/` | `wubu` | 856 | gpu_wubu.h, math.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu.h, w | wubu.c -- WuBu-35M in C11. THE MUSTARD SEED: our own base model. (LICENSE at the repo root). Designe |
| `brain/` | `wubu1_block_bridge` | 87 | string.h, wubu1_block.h, wubu_model.h, wubu_moe.h, wubu_ssm.h | wubu1_block_bridge.c — lossless conversion: loaded wubu_layer_t → canonical wubu1_block_t. The WuBu1 |
| `brain/` | `wubu_4kv` | 301 | math.h, string.h, wubu_4kv.h | wubu_4kv.c -- 4-bit KV-cache quantization (SAW-INT4 / TurboQuant approach). Kevin-Bacon convergence  |
| `brain/` | `wubu_acq` | 44 | math.h, wubu_acq.h | wubu_acq.c -- Acquisition functions (FF02). C11. Convergence (EI / UCB / PI 7-hop): - EI(x) = (μ-f*) |
| `brain/` | `wubu_active` | 42 | math.h, string.h, wubu_active.h | wubu_active.c -- Active Learning (uncertainty sampling / QBC) (FF05). C11. Convergence (active learn |
| `brain/` | `wubu_actor_critic` | 41 | math.h, string.h, wubu_actor_critic.h | wubu_actor_critic.c -- Actor-Critic with TD advantage (GG03). C11. Convergence (actor-critic / TD ad |
| `brain/` | `wubu_adapter_compat` | 72 | string.h, wubu_adapter.h, wubu_adapter_compat.h | wubu_adapter_compat.c — compatibility shims for adapter versions Bridges between adapter API version |
| `brain/` | `wubu_adapter_registry` | 119 | stdlib.h, string.h, wubu_adapter.h | wubu_adapter_registry.c — adapter registry (the slot map) A simple slot map: name → adapter*. Suppor |
| `brain/` | `wubu_affinity` | 87 | sched.h, stdio.h, stdlib.h, string.h, sysinfo.h, unistd.h, wubu_affinity.h, wubu | wubu_affinity.c — CPU/NUMA affinity + hugepage helpers (Areas J/K, items 89-100). C11, self-containe |
| `brain/` | `wubu_agentauth` | 47 | stdlib.h, string.h, wubu_agentauth.h | wubu_agentauth.c -- Inter-agent message authentication (AG07). C11. Convergence (ASI07 insecure inte |
| `brain/` | `wubu_agentic_kv` | 106 | math.h, stdlib.h, string.h, wubu_agentic_kv.h | wubu_agentic_kv.c -- Hybrid scheduler + multimodal/agentic KV (S06/U01/U02/U03/U04/U05). C11. Conver |
| `brain/` | `wubu_agentic_mem` | 50 | math.h, string.h, wubu_agentic_mem.h | wubu_agentic_mem.c -- AGI-OS agentic memory (AE01-AE04). C11. Convergence (TeleMem/HiMem/Redis 2026  |
| `brain/` | `wubu_agentic_os` | 62 | math.h, stdlib.h, string.h, wubu_agentic_os.h | wubu_agentic_os.c -- AGI-OS agentic runtime governance (AD01-AD04). C11. Convergence (AgentCgroup 20 |
| `brain/` | `wubu_agentid` | 70 | string.h, wubu_agentid.h | wubu_agentid.c -- Inter-agent identity + zero-trust auth (DD03). C11. Convergence (SPIFFE/SVID + zer |
| `brain/` | `wubu_agi` | 193 | stdio.h, stdlib.h, string.h, wubu_agi.h | wubu_agi.c -- the AGI loop (WuBu's recursive learning machine). The loop (one iteration): 1. observe |
| `brain/` | `wubu_align` | 170 | math.h, string.h, wubu_align.h | wubu_align.c -- Preference alignment + unlearning (Theme IM). C11. |
| `brain/` | `wubu_ambig` | 54 | stdio.h, stdlib.h, string.h, wubu_ambig.h, wubu_user_sim.h | wubu_ambig.c -- the clarification behavior (AC-D10). |
| `brain/` | `wubu_amoeba` | 379 | math.h, stdio.h, stdlib.h, string.h, wubu_amoeba.h | wubu_amoeba.c -- THE AMOEBA: the diagnostic self-evolving model. THE HIVE IS THE BODY. The user: "th |
| `brain/` | `wubu_arena` | 122 | stdio.h, stdlib.h, string.h, wubu_arena.h | wubu_arena.c -- the kernel's single allocator of record. Theory/07: one arena, bump-allocated; slabs |
| `brain/` | `wubu_attn_gate` | 56 | math.h, stdlib.h, string.h, wubu_attn_gate.h | wubu_attn_gate.c -- Attention-sink-free gated attention (doc 011). Conditional gating: gate = σ(W_ga |
| `brain/` | `wubu_attn_kernels` | 49 | math.h, stdlib.h, wubu_attn_kernels.h | wubu_attn_kernels.c -- Attention-side fused kernels (P11 / P15 / O20). C11. Convergence (int2 KV + f |
| `brain/` | `wubu_attn_tune` | 71 | math.h, stdint.h, stdlib.h, string.h, wubu_attn_tune.h | wubu_attn_tune.c -- Attention/dispatch auto-tuners (L06 / N19 / O11). Self-contained C11. Convergenc |
| `brain/` | `wubu_attnres` | 77 | assert.h, math.h, stdlib.h, string.h, wubu_attnres.h | wubu_attnres.c — Attention Residuals (AttnRes, Kimi K3) (Round-4 #413/#414/#419). C11, self-containe |
| `brain/` | `wubu_audio` | 127 | math.h, string.h, wubu_audio.h | wubu_audio.c -- Audio encoder: mel-spectrogram + real FFT from scratch (CC02). C11. Convergence (lib |
| `brain/` | `wubu_awq` | 123 | math.h, stdlib.h, string.h, wubu_awq.h | wubu_awq.c -- AWQ activation-aware weight quantization (doc B05). Source: Lin et al., "AWQ: Activati |
| `brain/` | `wubu_backend` | 34 | wubu_backend.h | wubu_backend.c — CPU-only backend stub. Provides the wubu_backend_t vtable for CPU-only builds. GPU  |
| `brain/` | `wubu_backend_cuda` | 131 | cuda_runtime.h, wubu_backend.h, wubu_kvfs.h, wubu_model_gpu.h | wubu_backend_cuda.c — CUDA backend for wubu_backend_t vtable. Populates the vtable with CUDA functio |
| `brain/` | `wubu_backprop` | 1231 | gpu_wubu.h, math.h, stdio.h, stdlib.h, string.h, wubu_activations.h, wubu_backpr | wubu_backprop.c -- the REAL backward pass + REAL Muon for the WuBu seed (12 layers, 7 Q heads / 1 KV |
| `brain/` | `wubu_bandit` | 61 | math.h, stdlib.h, string.h, wubu_bandit.h | wubu_bandit.c -- Thompson Sampling / multi-armed bandits (FF06). C11. Convergence (Thompson sampling |
| `brain/` | `wubu_bear_mlp` | 163 | math.h, stddef.h, wubu_bear_mlp.h | wubu_bear_mlp.c  --  BearRL MLP Policy Forward (CPU, C11 fallback). Self-contained CPU port of WuBuO |
| `brain/` | `wubu_bf16_gemv` | 76 | cpuid.h, immintrin.h, math.h, stdlib.h, string.h, wubu_bf16_gemv.h, wubu_fp16.h | wubu_bf16_gemv.c -- AVX512-BF16 GEMV decode kernel (P09), with runtime dispatch + F32 fallback. C11. |
| `brain/` | `wubu_bft` | 87 | string.h, wubu_bft.h | wubu_bft.c -- Byzantine Fault Tolerance consensus (DD01). C11. Convergence (BFT + multi-round voting |
| `brain/` | `wubu_bi` | 133 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_backprop.h, wubu_bi.h | wubu_bi.c -- Block Importance oracle (ShortGPT, arXiv:2403.03853). BI(l) = mean over tokens of ||h_{ |
| `brain/` | `wubu_blueprint` | 82 | stdio.h, stdlib.h, string.h, wubu_blueprint.h, wubu_contracts.h | wubu_blueprint.c -- the EXECUTABLE BLUEPRINT (see the header). The lineage becomes a real constraint |
| `brain/` | `wubu_bo` | 42 | math.h, string.h, wubu_acq.h, wubu_bo.h, wubu_gp.h | wubu_bo.c -- Bayesian Optimization loop (FF03). C11. Convergence (BO loop / sample efficiency 7-hop) |
| `brain/` | `wubu_bonzi` | 181 | math.h, string.h, wubu_bonzi.h | wubu_bonzi.c -- Bonzi Buddy companion core (Theme JE). C11. |
| `brain/` | `wubu_bonzi2` | 151 | math.h, stdio.h, string.h, wubu_bonzi2.h | wubu_bonzi2.c -- the companion frontier, complete (JE). C11. |
| `brain/` | `wubu_bridge` | 130 | math.h, wubu_bridge.h | wubu_bridge.c -- Cross-resource bridges (Theme JF, first batch). C11. |
| `brain/` | `wubu_bridge2` | 134 | math.h, string.h, wubu_bridge2.h | wubu_bridge2.c -- the cross-resource bridges frontier, complete (JF). C11. Agnostic: a bridge-table  |
| `brain/` | `wubu_cache_advice` | 103 | assert.h, stdlib.h, string.h, wubu_cache_advice.h | wubu_cache_advice.c — ML-advice / competitive cache eviction (Round-2 #111/#112). C11, self-containe |
| `brain/` | `wubu_canvas` | 145 | math.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas.c -- THE VHF CANVAS (research/066): the latent space as a coordinate-addressable field,  |
| `brain/` | `wubu_capacity_wall` | 105 | math.h, wubu_capacity_wall.h | wubu_capacity_wall.c -- Decode I/O capacity/roofline predictor (N12/N11). Convergence (I/O survey 20 |
| `brain/` | `wubu_capgate` | 84 | stdio.h, stdlib.h, string.h, wubu_capgate.h | wubu_capgate.c -- CAPABILITY-GATED SPECIALIST SPAWNING (see the header). Prevents unbounded agent sp |
| `brain/` | `wubu_capzero` | 93 | stdint.h, string.h, wubu_capzero.h | wubu_capzero.c -- Capability/Zero-Trust kernel for the AGI-OS (AF02-AF04). C11. Convergence (Agentic |
| `brain/` | `wubu_causal` | 191 | math.h, stdio.h, stdlib.h, string.h, wubu_causal.h | wubu_causal.c -- Causal + neuro-symbolic substrate for AGI-OS (AW01-AW10). C11. Convergence (7-hop K |
| `brain/` | `wubu_cegis` | 70 | math.h, string.h, wubu_cegis.h | wubu_cegis.c -- Counterexample-guided inductive synthesis (EE03). C11. Convergence (CEGIS / Sketch / |
| `brain/` | `wubu_chunked_prefill` | 111 | stdlib.h, string.h, wubu_chunked_prefill.h | wubu_chunked_prefill.c -- Chunked prefill + disaggregated prefill/decode (doc D03/D04). Source: vLLM |
| `brain/` | `wubu_cla` | 72 | assert.h, math.h, stdlib.h, string.h, wubu_cla.h | wubu_cla.c — Cross-Layer Attention KV sharing (Round-3 #223/#224/#227). C11, self-contained. CLA red |
| `brain/` | `wubu_codec` | 229 | math.h, stdlib.h, string.h, wubu_codec.h | wubu_codec.c -- THE UNIVERSAL CODEC (research/067): the Zephyr-HD audio<->image transform in C11. Th |
| `brain/` | `wubu_codeexec` | 71 | stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_codeexec.h, wubu_spawn.h | wubu_codeexec.c -- Code exec verifier → feeds loopguard (AX07). C11. Convergence (code exec verifier |
| `brain/` | `wubu_codesynth` | 70 | unistd.h, wubu_codesynth.h, wubu_spawn.h | wubu_codesynth.c -- spec→C11 source generator (AX10). C11. Convergence (program synthesis 7-hop: HEU |
| `brain/` | `wubu_coherence_reward` | 90 | math.h, stdlib.h, string.h, wubu_coherence_reward.h | wubu_coherence_reward.c — coherence-based reward for KV-FS training The training reward IS the coher |
| `brain/` | `wubu_colonel` | 140 | stdio.h, stdlib.h, string.h, wubu_agentic_os.h, wubu_colonel.h | wubu_colonel.c -- the LIVE COLONEL bridge (see wubu_colonel.h). The trust boundary between the Brain |
| `brain/` | `wubu_compress` | 226 | math.h, string.h, wubu_compress.h | wubu_compress.c -- the context-compression frontier (IY). C11. |
| `brain/` | `wubu_compress2` | 218 | math.h, string.h, wubu_compress2.h | wubu_compress2.c -- the context-compression frontier, complete (IY). C11. |
| `brain/` | `wubu_contbatch` | 45 | string.h, wubu_contbatch.h | wubu_contbatch.c -- Continuous batching (iterative-level scheduling) (HH04). C11. Convergence (Orca  |
| `brain/` | `wubu_continuous_batching` | 323 | assert.h, stdlib.h, string.h, wubu_continuous_batching.h, wubu_paged_kv.h, wubu_ | wubu_continuous_batching.c — Iteration-level scheduling for LLM decode. C11, self-contained. Impleme |
| `brain/` | `wubu_contracts` | 143 | math.h, stdio.h, stdlib.h, string.h, wubu_contracts.h | wubu_contracts.c -- the RUNTIME CONTRACTS (see the header). Self-modification bounded by invariants, |
| `brain/` | `wubu_coord` | 87 | stdlib.h, string.h, wubu_coord.h | wubu_coord.c -- Concurrent-agent coordination (AH01-AH04). C11. Convergence (CoAgent MTPO serializab |
| `brain/` | `wubu_credit` | 41 | stdlib.h, wubu_credit.h | wubu_credit.c -- Turn-level credit assignment (AH12). C11. Convergence (TRACE: verifier-anchored tur |
| `brain/` | `wubu_credit_sft` | 31 | wubu_credit_sft.h | wubu_credit_sft.c -- the credit-assignment SFT mask. The doctrine (Orchard): a trajectory that never |
| `brain/` | `wubu_cross_attn` | 306 | math.h, stdlib.h, string.h, wubu_cross_attn.h | wubu_cross_attn.c — Cross-attention for multimodal fusion Implements cross-attention decode where Q  |
| `brain/` | `wubu_ctx_manage` | 69 | math.h, stdlib.h, wubu_ctx_manage.h | wubu_ctx_manage.c -- Context-window + dispatch auto-managers (L16 elastic context / N07 tiered-cache |
| `brain/` | `wubu_ctxvm` | 66 | math.h, stdlib.h, string.h, wubu_ctxvm.h | wubu_ctxvm.c -- AGI-OS context virtual-memory hierarchy (AF08-AF10). C11. Convergence (LLM context a |
| `brain/` | `wubu_cuda_graph` | 69 | assert.h, cuda_runtime.h, stdlib.h, string.h, wubu_cuda_graph.h | wubu_cuda_graph.c — CUDA-graph decode capture + partial param update (Area E, items E.41/E.42/E.43/E |
| `brain/` | `wubu_db_cross` | 112 | math.h, stdlib.h, string.h, wubu_db_cross.h | wubu_db_cross.c -- Cross-discipline DB -> KV engine mappings (O14 / O18 / O19). C11, no third-party  |
| `brain/` | `wubu_dbstate` | 59 | stdlib.h, string.h, wubu_dbstate.h | wubu_dbstate.c -- the DB-state reward verifier. |
| `brain/` | `wubu_dedup` | 74 | stdlib.h, string.h, wubu_dedup.h | wubu_dedup.c -- the rolling-hash duplicate-window scanner. A polynomial rolling hash over the window |
| `brain/` | `wubu_delta_net` | 118 | assert.h, math.h, stdlib.h, string.h, wubu_delta_net.h | wubu_delta_net.c — Gated-DeltaNet recurrent state (3:1 hybrid linear attention). C11, self-contained |
| `brain/` | `wubu_deltanet` | 95 | stdlib.h, string.h, wubu_deltanet.h | wubu_deltanet.c -- the Gated-DeltaNet linear mixer (research 008). |
| `brain/` | `wubu_dense_ffn` | 74 | gguf_reader.h, math.h, stdlib.h, string.h, wubu_dense_ffn.h | wubu_dense_ffn.c — dense SwiGLU FFN for hybrid models (C11). |
| `brain/` | `wubu_density_planner` | 182 | stdio.h, stdlib.h, string.h, wubu_density_planner.h | wubu_density_planner.c — information density planning (AN23-core) The AGI's resource allocator. Deci |
| `brain/` | `wubu_dequant_fp4` | 136 | math.h, stdint.h, stdlib.h, string.h, wubu_dequant_fp4.h | wubu_dequant_fp4.c — C11 row-level dequantization for OCP MXFP4 and NVFP4 microscaling 4-bit formats |
| `brain/` | `wubu_dequant_nf4` | 61 | stdint.h, string.h, wubu_dequant_nf4.h | wubu_dequant_nf4.c — NF4 (Normal Float 4) dequantization for wubuwizard. Self-contained C11 module.  |
| `brain/` | `wubu_der` | 50 | math.h, string.h, wubu_activations.h, wubu_der.h | wubu_der.c -- Dark Experience Replay (BB07). C11. |
| `brain/` | `wubu_dgm` | 88 | stdio.h, stdlib.h, string.h, time.h, wubu_dgm.h | wubu_dgm.c -- DGM empirical gate + regression test runner (AX01). C11. Convergence (7-hop KB sweep:  |
| `brain/` | `wubu_diag` | 323 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_diag.h, wubu_train.h | wubu_diag.c -- THE HIVE DIAGNOSTIC SYSTEM (research/056, INDEX AN08). The hive is not just where the |
| `brain/` | `wubu_diagnosis` | 467 | fcntl.h, math.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_diagnosis.h | wubu_diagnosis.c -- the CLOSED CONTROL SYSTEM (see wubu_diagnosis.h). The loop becomes the primary r |
| `brain/` | `wubu_dims` | 62 | wubu_dims.h | wubu_dims.c -- runtime dimension global for wubuwizard. See wubu_dims.h. The loader sets WUBU_DIMS f |
| `brain/` | `wubu_dims_gpu_stub` | 6 | wubu_dims.h | CPU-only stub for the GPU dims sync symbol so CPU builds/tests link without the CUDA toolkit. The re |
| `brain/` | `wubu_dims_stub` | 14 | safetensors_reader.h | wubu_dims_stub.c — stubs for the safetensors probe, so the byte-budget test can link wubu_runtime_di |
| `brain/` | `wubu_distill` | 59 | math.h, string.h, wubu_distill.h | wubu_distill.c -- Knowledge distillation (BB04). C11. Convergence (DER++ / dark knowledge distillati |
| `brain/` | `wubu_dn2` | 67 | math.h, stdlib.h, string.h, wubu_dn2.h | wubu_dn2.c -- Gated DeltaNet-2 (S02) + ternary STE proxy (T04). C11. Convergence (Gated DeltaNet-2 d |
| `brain/` | `wubu_dqn` | 69 | math.h, string.h, wubu_dqn.h | wubu_dqn.c -- DQN / Q-learning (value-based) (GG05). C11. Convergence (Q-learning / DQN / experience |
| `brain/` | `wubu_dsa` | 167 | math.h, stdlib.h, wubu_dsa.h | wubu_dsa.c -- DeepSeek-Sparse-Attention-style coarse-to-fine block indexer (DSA indexer). Self-conta |
| `brain/` | `wubu_dsv4` | 160 | assert.h, math.h, stdlib.h, string.h, wubu_dsa.h, wubu_dsv4.h, wubu_hashrouter.h | wubu_dsv4.c — DeepSeek-V4 Flash layer port (C11, opaque, minimal includes) DeepSeek-V4-Flash-0731: 2 |
| `brain/` | `wubu_dsv4_layer` | 206 | stdio.h, stdlib.h, string.h, wubu_dsv4_layer.h, wubu_mla.h | wubu_dsv4_layer.c --- DeepSeek-V4 MLA attention bridge. Self-contained C11 module that bridges GGUF  |
| `brain/` | `wubu_eagle` | 180 | stdlib.h, string.h, wubu_eagle.h, wubu_model.h | wubu_eagle.c — EAGLE self-draft speculative decoding (G01). Pure C11. Uses the existing wubu_model_t |
| `brain/` | `wubu_early_exit` | 84 | math.h, stdlib.h, wubu_early_exit.h | wubu_early_exit.c — Early-exit + self-speculative verify (doc J03). See header. Self-contained C11. |
| `brain/` | `wubu_ecosystem` | 363 | math.h, stdlib.h, string.h, wubu_ecosystem.h, wubu_hive.h, wubu_mobius.h, wubu_m | wubu_ecosystem.c -- the Ecosystem of Spheres (THEORY/10). The colony: many small hyperbolic balls, e |
| `brain/` | `wubu_ecs` | 115 | stdlib.h, string.h, wubu_ecs.h | wubu_ecs.c — ECS-style component store for engine state (doc C06). C11. |
| `brain/` | `wubu_enc_fs` | 105 | stdio.h, stdlib.h, string.h, wubu_enc_fs.h | wubu_enc_fs.c -- THE ENCODER IS A MOUNT (AN16 G3 + research/064). The encoder does not re-implement  |
| `brain/` | `wubu_enc_h3` | 176 | stdio.h, stdlib.h, string.h, wubu_enc_h3.h, wubu_fp8.h, wubu_nvfp4.h, wubu_rotat | wubu_enc_h3.c — MiniMax H3 text encoder NVFP4 requant + ConvRot un-rotation The MiniMax H3 encoder s |
| `brain/` | `wubu_encoder` | 73 | stdlib.h, string.h, wubu_encoder.h | wubu_encoder.c -- the encoder registry (THE ENCODER SLOT, WB06/AN06). One slot, many encoders: text/ |
| `brain/` | `wubu_encoder_impl` | 170 | math.h, stdlib.h, string.h, wubu_audio.h, wubu_encoder_impl.h, wubu_imgenc.h | wubu_encoder_impl.c -- OUR OWN encoder (WB06: made, not imported). The single-encoder modality-agnos |
| `brain/` | `wubu_energy` | 162 | math.h, wubu_energy.h | wubu_energy.c -- Energy-aware inference (Theme IJ). C11. Pure deterministic models, no hardware acce |
| `brain/` | `wubu_epcap` | 16 | wubu_epcap.h | wubu_epcap.c -- the episode-length cap. |
| `brain/` | `wubu_equiv_check` | 49 | math.h, stdio.h, stdlib.h, wubu_equiv_check.h | wubu_equiv_check.c — Lightweight GEMV equivalence verifier (doc F02). C11. |
| `brain/` | `wubu_eval` | 26 | stdlib.h, wubu_eval.h, wubu_passk.h | wubu_eval.c -- the agentic eval harness. |
| `brain/` | `wubu_eval_qat` | 121 | math.h, stdlib.h, string.h, wubu_eval_qat.h | wubu_eval_qat.c -- Long-context eval harness (Z01-Z05) + QAT (AA01-AA04). C11. Convergence (NIAH-2 / |
| `brain/` | `wubu_events` | 95 | stdlib.h, string.h, wubu_events.h | wubu_events.c -- the COLONY RUN RECORDER (see the header). Append-only JSONL, fsynced per event, rea |
| `brain/` | `wubu_evict2026` | 202 | math.h, string.h, wubu_evict2026.h | wubu_evict2026.c -- KV-eviction frontier mechanisms (Theme IO). C11. |
| `brain/` | `wubu_evict2026b` | 168 | stdlib.h, string.h, wubu_evict2026b.h | wubu_evict2026b.c -- KV eviction frontier, batch 2 (Theme IO). C11. |
| `brain/` | `wubu_evict2026c` | 174 | math.h, string.h, wubu_evict2026c.h | wubu_evict2026c.c -- the KV-eviction frontier, final (IO). C11. |
| `brain/` | `wubu_evolve` | 72 | stdio.h, stdlib.h, string.h, wubu_evolve.h | wubu_evolve.c -- Self-evolution loop (AX06). C11. Convergence (DGM + self-improving agents 7-hop): - |
| `brain/` | `wubu_ewc` | 64 | math.h, string.h, wubu_ewc.h | wubu_ewc.c -- EWC consolidation (BB02). C11. Convergence (EWC + Fisher Information 7-hop): - BB02: E |
| `brain/` | `wubu_experibuf` | 65 | math.h, stdlib.h, string.h, wubu_experibuf.h | wubu_replay.c -- Experience replay buffer (BB01). C11. Convergence (experience replay + reservoir sa |
| `brain/` | `wubu_expert_allreduce` | 47 | math.h, stdlib.h, wubu_expert_allreduce.h | wubu_expert_allreduce.c — Wide-expert all-reduce reference (doc E06). C11. |
| `brain/` | `wubu_expert_choice` | 164 | math.h, stdlib.h, string.h, wubu_expert_choice.h | wubu_expert_choice.c -- Fine-grained MoE expert-choice routing (doc E05). Source: Zhou et al., "Brai |
| `brain/` | `wubu_fast_attn` | 748 | math.h, stdio.h, stdlib.h, string.h, wubu_fast_attn.h, wubu_polarquant.h | wubu_fast_attn.c — Zero-malloc, precomputed-RoPE, direct-cache GQA attention. The existing wubu_gqa_ |
| `brain/` | `wubu_flash_prefill` | 159 | math.h, stdlib.h, string.h, wubu_flash_prefill.h | wubu_flash_prefill.c -- Fused tiled prefill attention (doc H01). Source: Dao et al., "FlashAttention |
| `brain/` | `wubu_flashdecode` | 86 | math.h, omp.h, string.h, wubu_flashdecode.h | wubu_flashdecode.c -- FlashDecoding-style decode attention (doc 015). Self-contained C11. See header |
| `brain/` | `wubu_fmt` | 61 | stdio.h, string.h, wubu_fmt.h | wubu_fmt.c -- the format-constraint reward checker. |
| `brain/` | `wubu_fp8` | 106 | math.h, stdint.h, wubu_fp8.h | wubu_fp8.c — FP8 E4M3 / E5M2 emulation (doc B07). Pure C11, no 3rd-party. |
| `brain/` | `wubu_fraud` | 65 | string.h, wubu_fraud.h | wubu_fraud.c -- Fraud detection: outlier + dispute resolution (DD05). C11. Convergence (fraud detect |
| `brain/` | `wubu_freeenergy` | 74 | math.h, wubu_freeenergy.h | wubu_freeenergy.c -- Predictive coding / free energy / active inference (Theme IN). C11, determinist |
| `brain/` | `wubu_fs_dataset` | 237 | dirent.h, stat.h, stdio.h, stdlib.h, string.h, wubu_fs_dataset.h | wubu_fs_dataset.c — file-system dataset implementation Walks a directory tree, tokenizes each file,  |
| `brain/` | `wubu_fs_trainer` | 231 | math.h, stdlib.h, string.h, wubu_fs_trainer.h | wubu_fs_trainer.c -- Phase 8 of the KV-FS hive mind (AN21/AN22): the model trains ON the filesystem, |
| `brain/` | `wubu_fuzz` | 122 | ctype.h, string.h, wubu_fuzz.h | wubu_fuzz.c -- robustness / fuzzing frontier (Theme IX). C11. |
| `brain/` | `wubu_fuzz2` | 214 | ctype.h, math.h, string.h, wubu_fuzz2.h | wubu_fuzz2.c -- the robustness frontier, complete (IX). C11. |
| `brain/` | `wubu_gaad` | 513 | math.h, stdlib.h, string.h, wubu_gaad.h | wubu_gaad.c  --  WuBuOS Golden Aspect Adaptive Decomposition Cell 393: GAAD -- the universal resolut |
| `brain/` | `wubu_gamebud` | 116 | pthread.h, stdlib.h, string.h, syscall.h, time.h, unistd.h, wubu_gamebud.h | wubu_gamebud.c — Game-design frame-budget governor (doc "gamebud"). C11. |
| `brain/` | `wubu_gemm` | 432 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_gemm.h | wubu_gemm.c -- WuBuOS-agnostic GEMM, OUR OWN CPU backend. Design: cache-blocked GEMM (three-level ti |
| `brain/` | `wubu_gemma4_model` | 792 | float.h, gguf_reader.h, math.h, omp.h, stdio.h, stdlib.h, string.h, wubu_gemma4. | wubu_gemma4_model.c — Gemma 4 12B dual-head-dim ISWA forward pass. Port of llama.cpp gemma4 model, a |
| `brain/` | `wubu_gemv_dispatch` | 110 | stdio.h, stdlib.h, string.h, wubu_bf16_gemv.h, wubu_gemm.h, wubu_gemv_dispatch.h | wubu_gemv_dispatch.c -- the PER-FAMILY GEMV DISPATCH (see header). The precision ladder EXECUTES, it |
| `brain/` | `wubu_gemv_tune` | 120 | math.h, stdio.h, wubu_gemv_tune.h, wubu_roofline.h | wubu_gemv_tune.c -- Roofline-driven GEMV auto-tuner (see header). Pure C, routes through wubu_roofli |
| `brain/` | `wubu_generate` | 405 | math.h, stdlib.h, string.h, wubu_activations.h, wubu_capzero.h, wubu_ctxvm.h, wu | wubu_generate.c -- autoregressive generation + n-gram speculative decoding (doc 018 / K01). Self-con |
| `brain/` | `wubu_gguf_bridge` | 48 | gguf_reader.h, math.h, string.h, wubu_gguf_tensor.h |  |
| `brain/` | `wubu_gguf_header` | 479 | gguf_reader.h, math.h, mman.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_gguf_ |  |
| `brain/` | `wubu_gguf_names` | 388 | stdio.h, string.h, wubu_gguf_names.h | wubu_gguf_names.c — role-based GGUF tensor-name resolver. Every weight is a ROLE; every role has a c |
| `brain/` | `wubu_gguf_tensor` | 1520 | gguf_reader.h, iq2s_grid.inc, iq3s_grid.inc, iq3xxs_grid.inc, math.h, stdio.h, s |  |
| `brain/` | `wubu_gguf_tokenizer` | 148 | stdio.h, stdlib.h, string.h, wubu_gguf_tokenizer.h | wubu_gguf_tokenizer.c — GGUF-embedded tokenizer extraction (C11). Walks the GGUF KV section (after m |
| `brain/` | `wubu_gp` | 99 | math.h, string.h, wubu_gp.h | wubu_gp.c -- Gaussian Process regression (FF01). C11. Convergence (GP regression / RBF kernel / Chol |
| `brain/` | `wubu_gptq` | 121 | math.h, stdlib.h, string.h, wubu_gptq.h | wubu_gptq.c -- GPTQ second-order weight quantization (doc B06). Source: Frantar et al., "GPTQ: Accur |
| `brain/` | `wubu_graph` | 321 | math.h, stdlib.h, string.h, wubu_graph.h | ---- internal graph layout ------------------------------------------ |
| `brain/` | `wubu_gravity` | 209 | math.h, stdlib.h, string.h, wubu_gravity.h, wubu_std.h | wubu_gravity.c -- THE GRAVITY FIELD (research/060, INDEX AN12). The organizing force of the amoeba w |
| `brain/` | `wubu_grow` | 194 | stdio.h, stdlib.h, string.h, wubu_grow.h | wubu_grow.c -- the model-growth operator. |
| `brain/` | `wubu_grow_kv` | 149 | stdio.h, stdlib.h, string.h, wubu_grow_kv.h | wubu_grow_kv.c — the KV-space amoeba grow operator Grows KV blocks in the namespace toward under-coh |
| `brain/` | `wubu_h3_norm` | 194 | math.h, safetensors_reader.h, stdint.h, stdlib.h, string.h, wubu_dequant_nf4.h,  | wubu_h3_norm.c — MiniMax H3 hyperbolic neural network normalization. Self-contained C11 module. H3 a |
| `brain/` | `wubu_hadamard` | 51 | math.h, stdlib.h, string.h, wubu_hadamard.h | wubu_hadamard.c — Hadamard rotation for incoherent FP8 (doc H03). C11. |
| `brain/` | `wubu_harness` | 93 | stdio.h, stdlib.h, string.h, wubu_harness.h, wubu_trajcell.h | wubu_harness.c -- the SUSTAINED AUTONOMY TASK HARNESS (see the header). Fixed suite, deterministic s |
| `brain/` | `wubu_harness_file` | 109 | math.h, stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_codec.h, wubu_harness_f | wubu_harness_file.c — LIVE-FILE HARNESS TASKS (see the header). Real file work, real scores, traj ce |
| `brain/` | `wubu_hashrouter` | 80 | stdlib.h, wubu_hashrouter.h | wubu_hashrouter.c -- hash-based expert routing (DeepSeek V3.2/V4 style). No learned router: the expe |
| `brain/` | `wubu_hive` | 196 | stdlib.h, string.h, wubu_hive.h | wubu_hive.c -- THE HIVE: the AGI's memory structure (WuBu). Linked fixed blocks + skipfield + freeli |
| `brain/` | `wubu_hive_merge` | 83 | stdio.h, string.h, wubu_hive_merge.h, wubu_priority_store.h | wubu_hive_merge.c — MULTI-CHECKPOINT LINEAGE MERGE (see header). Federate without forking. |
| `brain/` | `wubu_hopfield` | 113 | math.h, string.h, wubu_hopfield.h | wubu_hopfield.c -- Modern Hopfield / associative memory (Theme IL). C11, deterministic, no third-par |
| `brain/` | `wubu_hopfield2` | 201 | math.h, string.h, wubu_hopfield2.h | wubu_hopfield2.c -- Hopfield frontier extensions (Theme IP). C11. |
| `brain/` | `wubu_hopfield3` | 218 | math.h, stdlib.h, string.h, wubu_hopfield3.h | wubu_hopfield3.c -- Hopfield frontier, batch 2 (Theme IP). C11. |
| `brain/` | `wubu_hopfield4` | 272 | math.h, stdio.h, string.h, wubu_hopfield4.h | wubu_hopfield4.c -- the associative-memory frontier, ABSOLUTE final (IP). C11. Implements the 26 rem |
| `brain/` | `wubu_hugepage` | 78 | mman.h, stdlib.h, string.h, unistd.h, wubu_hugepage.h | wubu_hugepage.c -- Hugepage-backed KV arena (O02). Convergence (OS paging cross-discipline 7-hop): K |
| `brain/` | `wubu_hwcaps` | 104 | cpuid.h, stdio.h, string.h, wubu_hwcaps.h | wubu_hwcaps.c — Runtime hardware capability detection (CPUID ladder). See header. Self-contained C11 |
| `brain/` | `wubu_hybrid` | 159 | math.h, string.h, wubu_hybrid.h | wubu_hybrid.c -- the hybrid-attention frontier (JA). C11. |
| `brain/` | `wubu_hyper` | 119 | math.h, string.h, wubu_hyper.h | wubu_hyper.c -- the WuBu hyperbolic layer (OUR math, Lean-verified). All formulas ported from MATH/l |
| `brain/` | `wubu_hyperbolic_output_proj` | 243 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_hyperbolic_output_proj. |  |
| `brain/` | `wubu_imgenc` | 89 | math.h, string.h, wubu_activations.h, wubu_imgenc.h | wubu_imgenc.c -- Vision encoder: ViT patch embedding from scratch (CC01). C11. |
| `brain/` | `wubu_integrate` | 115 | math.h, stdlib.h, string.h, wubu_integrate.h | wubu_integrate.c -- Runtime policy composer wiring the recursive-loop gap modules into the live deco |
| `brain/` | `wubu_invariant` | 53 | math.h, string.h, wubu_invariant.h | wubu_invariant.c -- Loop invariant discovery (EE05). C11. Convergence (invariant discovery / abstrac |
| `brain/` | `wubu_jpeg` | 397 | math.h, stdlib.h, string.h, wubu_jpeg.h | wubu_jpeg.c -- JPEG baseline decoder (ours: Huffman + IDCT from spec). ITU-T T.81 baseline sequentia |
| `brain/` | `wubu_kda` | 68 | assert.h, math.h, stdlib.h, string.h, wubu_kda.h | wubu_kda.c — Kimi Delta Attention (KDA) recurrent state (Round-4 #401/#405/#410). C11, self-containe |
| `brain/` | `wubu_kereq` | 52 | assert.h, math.h, stdlib.h, string.h, wubu_kereq.h | wubu_kereq.c — Kernel equivalence via abstract interpretation (Round-2 #121/#123). C11, self-contain |
| `brain/` | `wubu_kernel` | 663 | math.h, stdarg.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_activati | wubu_kernel.c — Hardware-agnostic kernel dispatch, CPU baseline. WASTE reference (https://github.com |
| `brain/` | `wubu_kernel_backends` | 74 | stdio.h, wubu_kernel.h | wubu_kernel_backends.c — Device backend registration. WASTE reference: "all accelerators acceptable" |
| `brain/` | `wubu_kernel_budget` | 133 | stddef.h, stdio.h, wubu.h, wubu_kernel_budget.h, wubu_kv_select.h | wubu_kernel_budget.c -- byte-budget compiler for the kernel-layer model. Computes the slab layout fr |
| `brain/` | `wubu_kv2026` | 120 | math.h, stdlib.h, string.h, wubu_kv2026.h | wubu_kv2026.c -- Fresh 2026 KV-cache methods (Q02/Q03/Q07/Q09/Q10). C11. Convergence (2026 KV-cache  |
| `brain/` | `wubu_kv2026b` | 112 | math.h, stdlib.h, string.h, wubu_kv2026b.h | wubu_kv2026b.c -- More 2026 KV-cache methods (Q01/Q04/Q05/Q06). C11. Convergence (CentroidKV / R-KV  |
| `brain/` | `wubu_kv2026c` | 95 | math.h, stdlib.h, string.h, wubu_kv2026c.h | wubu_kv2026c.c -- Remaining 2026 KV methods (Q11/Q19/R04/R05). C11. Convergence (DASH-KV / HeteroCac |
| `brain/` | `wubu_kv_adaptive` | 119 | math.h, stdlib.h, string.h, wubu_kv_adaptive.h | wubu_kv_adaptive.c -- Ecco entropy-aware adaptive KV compression (doc 001). Source: Cheng et al., "E |
| `brain/` | `wubu_kv_budget` | 77 | math.h, wubu_kv_budget.h | wubu_kv_budget.c -- Layer-wise KV budget + adaptive sink + scheme selector + footprint forecaster (L |
| `brain/` | `wubu_kv_cacheline` | 166 | assert.h, stdlib.h, string.h, wubu_kv_cacheline.h | wubu_kv_cacheline.c -- Cache-line-aligned KV page storage (doc C03/I03). Allocates K/V pages aligned |
| `brain/` | `wubu_kv_coherence_diag` | 154 | stdlib.h, string.h, wubu_kv_coherence_diag.h | wubu_kv_coherence_diag.c — post-forward KV coherence diagnose The diagnostic operator: after each fo |
| `brain/` | `wubu_kv_compress` | 108 | stdint.h, stdlib.h, string.h, wubu_kv_compress.h | wubu_kv_compress.c -- Attention-score-driven KV compression (L07 SnapKV / L09 CIA). Self-contained C |
| `brain/` | `wubu_kv_embedding` | 329 | math.h, stdio.h, stdlib.h, string.h, wubu_kv_embedding.h | wubu_kv_embedding.c — the KV-FS embedding bridge The file system IS the model's context. This module |
| `brain/` | `wubu_kv_evict` | 155 | stdlib.h, string.h, wubu_kv_evict.h | wubu_kv_evict.c — Priority-based KV eviction (doc A07b). See header for the policy. Self-contained C |
| `brain/` | `wubu_kv_hierarchy` | 131 | math.h, stdio.h, stdlib.h, string.h, wubu_kv_hierarchy.h, wubu_mobius.h | wubu_kv_hierarchy.c — hyperbolic KV namespace hierarchy Maps /kv/in/<path> into the Poincaré disk so |
| `brain/` | `wubu_kv_runtime` | 65 | wubu_kv_runtime.h, wubu_roofline.h | wubu_kv_runtime.c -- runtime KV-cache scheme dispatch. The KV cache read/write/alloc functions in wu |
| `brain/` | `wubu_kv_select` | 56 | string.h, wubu_kv_select.h | wubu_kv_select.c -- Roofline-driven KV/weight selector (see header). Pure C, routes through the test |
| `brain/` | `wubu_kv_semantic_router` | 76 | math.h, stdlib.h, string.h, wubu_kv_semantic_router.h | wubu_kv_semantic_router.c — Poincaré routing bias for attention Translates the hyperbolic KV hierarc |
| `brain/` | `wubu_kv_shell` | 177 | stdio.h, stdlib.h, string.h, wubu_kv_shell.h | wubu_kv_shell.c — shell command routing to KV/FS Exposes the KV filesystem as a shell-accessible nam |
| `brain/` | `wubu_kv_shield` | 43 | wubu_kv_shield.h | wubu_kv_shield.c -- Adversarial-robust KV access (L15 / F safety theme). Self-contained C11. Converg |
| `brain/` | `wubu_kv_shrink` | 126 | stdio.h, stdlib.h, string.h, wubu_kv_shrink.h | wubu_kv_shrink.c — prune dead KV regions Tracks per-region utilization over consecutive forward pass |
| `brain/` | `wubu_kv_styx` | 106 | errno.h, stdio.h, stdlib.h, string.h, wubu_kv_styx.h | wubu_kv_styx.c — KV-cache structured exposure for external inspection. This module is the self-conta |
| `brain/` | `wubu_kv_tier` | 347 | errno.h, fcntl.h, mman.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_kv | wubu_kv_tier.c -- KV cache multi-tier cold storage (doc 002). Three tiers on a single host: HOT  = e |
| `brain/` | `wubu_kv_tiering` | 249 | gguf_reader.h, stdlib.h, string.h, wubu_kv_tiering.h | wubu_kv_tiering.c — precision tiering for KV cache Tracks per-KV-path precision tier (F32→F16→Q8_K→Q |
| `brain/` | `wubu_kv_transfer` | 96 | errno.h, fcntl.h, mman.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_kv | wubu_kv_transfer.c — Localhost KV transfer layer (D05, doc 007/002). NIXL/UCX analog for a single ho |
| `brain/` | `wubu_kvcache_quant` | 143 | math.h, stdlib.h, string.h, wubu_kvcache_quant.h | wubu_kvcache_quant.c -- KIVI asymmetric quantization for KV cache. Research convergence (Kevin-Bacon |
| `brain/` | `wubu_kvfs` | 381 | stdio.h, stdlib.h, string.h, wubu_kvfs.h | wubu_kvfs.c — KV namespace layer (G1 implementation) Path-addressable KV cache. Every mount is a dir |
| `brain/` | `wubu_kvquant` | 96 | math.h, stdlib.h, string.h, wubu_kvquant.h | wubu_kvquant.c — KV-cache quantization (Area B of the 100-point plan). C11, self-contained. FP8 (e4m |
| `brain/` | `wubu_kvvq` | 125 | math.h, stdlib.h, string.h, wubu_kvvq.h | wubu_kvvq.c -- data-independent residual subvector VQ for KV (doc 014). Self-contained C11. See head |
| `brain/` | `wubu_latency` | 78 | stdlib.h, string.h, wubu_latency.h | wubu_latency.c -- AGI-OS latency-class scheduler (AF05-AF07). C11. Convergence (Agent-OS HRT/SRT/DT  |
| `brain/` | `wubu_latentmoe` | 66 | assert.h, math.h, stdlib.h, string.h, wubu_latentmoe.h | wubu_latentmoe.c — Stable LatentMoE (Kimi K3 / DeepSeek-V3 style) (Round-4 #423/#424/#430). C11, sel |
| `brain/` | `wubu_layer_skip` | 74 | math.h, stdlib.h, string.h, wubu_layer_skip.h | wubu_layer_skip.c -- Mixture-of-Depths / token-wise layer skip (doc 017). Per-token gate decides whe |
| `brain/` | `wubu_lfm` | 198 | math.h, stdlib.h, string.h, wubu_lfm.h, wubu_linear_attn.h | wubu_lfm.c — LFM2.5-2.6B hybrid attention layer (C11, opaque, minimal) Hybrid architecture: linear a |
| `brain/` | `wubu_linattn` | 153 | math.h, string.h, wubu_linattn.h | wubu_linattn.c -- linear-attention / SSM frontier (Theme IU). C11. |
| `brain/` | `wubu_linattn2` | 214 | math.h, string.h, wubu_linattn2.h | wubu_linattn2.c -- the linear-attention frontier, complete (IU). C11. |
| `brain/` | `wubu_lineage` | 132 | stdio.h, stdlib.h, string.h, wubu_lineage.h | wubu_lineage.c -- LINEAGE-AWARE FITNESS + EXTINCTION PRESSURE (see wubu_lineage.h). Evolutionary pre |
| `brain/` | `wubu_linear_attn` | 102 | math.h, stdlib.h, string.h, wubu_linear_attn.h | wubu_linear_attn.c -- Linear / recurrent attention hybrids (S01-S05, S07). C11. Convergence (Gated D |
| `brain/` | `wubu_lm_infinite` | 83 | stdlib.h, string.h, wubu_lm_infinite.h | wubu_lm_infinite.c -- Long-context memory mechanics (L13 / O07 / N20). C11. Convergence (LM-Infinite |
| `brain/` | `wubu_lmcache` | 205 | stdio.h, stdlib.h, string.h, wubu_lmcache.h | wubu_lmcache.c -- LMCache-style prefix+PD KV persistence (doc A06). Source: LMCache (arXiv:2510.0966 |
| `brain/` | `wubu_lookahead` | 34 | stdlib.h, wubu_lookahead.h | wubu_lookahead.c -- Lookahead / n-gram speculative helper (M06). C11. Convergence (lookahead decodin |
| `brain/` | `wubu_loopguard` | 73 | stdlib.h, string.h, time.h, wubu_loopguard.h | wubu_loopguard.c -- Missing-need guards for the AGI-OS (AG01/AG05/AG06/AG08). C11. Convergence (OWAS |
| `brain/` | `wubu_lora` | 102 | math.h, stdlib.h, string.h, wubu_lora.h | wubu_lora.c -- LoRA application (C11, self-contained, opaque). Merged form:  W' = W + scale * (B^T @ |
| `brain/` | `wubu_lruk` | 75 | stdlib.h, string.h, wubu_lruk.h | wubu_lruk.c -- LRU-k KV eviction (O01, cross-discipline DB buffer-pool 7-hop). Convergence (DB buffe |
| `brain/` | `wubu_masked_ce` | 59 | math.h, string.h, wubu_masked_ce.h | wubu_masked_ce.c -- the masked next-token CE. |
| `brain/` | `wubu_medusa` | 39 | string.h, wubu_medusa.h | wubu_medusa.c -- Medusa self-draft heads (parallel tree draft) (HH05). C11. Convergence (Medusa / se |
| `brain/` | `wubu_mega` | 47 | assert.h, math.h, stdlib.h, string.h, wubu_mega.h | wubu_mega.c — MEGA: Moving Average Equipped Gated Attention (Round-3 #233/#235/#236). C11, self-cont |
| `brain/` | `wubu_megakernel` | 227 | math.h, stdlib.h, string.h, wubu_activations.h, wubu_megakernel.h | wubu_megakernel.c — Photon 2.0 fused decode megakernel (C11, opaque, minimal) The megakernel fuses:  |
| `brain/` | `wubu_mem_budget` | 217 | stdio.h, stdlib.h, string.h, windows.h, wubu_mem_budget.h | wubu_mem_budget.c -- Memory budget calculator for OOM-proof inference. Self-contained C11. Reads /pr |
| `brain/` | `wubu_metacog` | 188 | math.h, string.h, wubu_metacog.h | wubu_metacog.c -- AGI metacognition (Theme JD). C11. |
| `brain/` | `wubu_metadiag` | 213 | math.h, stdio.h, stdlib.h, string.h, wubu_metadiag.h | wubu_metadiag.c -- DUAL-TIMESCALE DIAGNOSE (see the header). |
| `brain/` | `wubu_metagame` | 58 | stdlib.h, string.h, wubu_metagame.h | wubu_metagame.c -- Open-ended self-modifying agent archive (AH05/AH06/AH08/AH13). C11. Convergence ( |
| `brain/` | `wubu_metagame2` | 167 | math.h, string.h, wubu_metagame2.h | wubu_metagame2.c -- the metacognition frontier, complete (JD). C11. |
| `brain/` | `wubu_mhc` | 112 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_mhc.h | wubu_mhc.c — Manifold-Constrained Hyper-Connections (Round-3 #213/#214/#215/#216). C11, self-contain |
| `brain/` | `wubu_mhc_mh` | 128 | math.h, stdlib.h, string.h, wubu_mhc_mh.h | wubu_mhc_mh.c -- Multi-head Hyper-Connections (the 2512.24880 form). The KAHUNA's residual architect |
| `brain/` | `wubu_misc_gaps` | 104 | math.h, mman.h, stdlib.h, string.h, wubu_misc_gaps.h | wubu_misc_gaps.c -- Cross-discipline / OS / neuro gap closers (L05/O12/O13/O15/ P12/P13). C11, no th |
| `brain/` | `wubu_mix` | 64 | stdio.h, stdlib.h, string.h, wubu_mix.h | wubu_mix.c -- the weighted multi-stream corpus mixer. |
| `brain/` | `wubu_mla` | 198 | math.h, stdlib.h, string.h, wubu_mla.h | wubu_mla.c -- Multi-head Latent Attention (doc E02). Source: DeepSeek-AI, "DeepSeek-V2: A Strong, Ec |
| `brain/` | `wubu_mm_adapter` | 70 | math.h, string.h, wubu_mm_adapter.h | wubu_mm_adapter.c -- Multimodal adapter: align → quantize → KV token IDs. C11. Convergence (BLIP-2/Q |
| `brain/` | `wubu_mm_align` | 64 | math.h, string.h, wubu_imgenc.h, wubu_mm_align.h | wubu_mm_align.c -- Cross-modal alignment: vision/audio → text embedding space. C11. Convergence (CLI |
| `brain/` | `wubu_mm_kv` | 40 | string.h, wubu_mm_kv.h | wubu_mm_kv.c -- Positional KV integration. C11. Convergence (KV-cache prefix injection 7-hop: positi |
| `brain/` | `wubu_mmrope` | 123 | math.h, stdint.h, stdlib.h, string.h, wubu_mmrope.h | wubu_mmrope.c — 3D Multimodal RoPE implementation for MiniMax H3. H3-Omni-Transformer uses 3D MM-RoP |
| `brain/` | `wubu_mobius` | 517 | math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h |  |
| `brain/` | `wubu_mobius_gyrate` | 75 | math.h, stdlib.h, string.h, wubu_mobius.h | Optimized Möbius gyration using precomputed dot products. Instead of calling wubu_mobius_add 3 times |
| `brain/` | `wubu_mobius_linear` | 200 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_mobius_l |  |
| `brain/` | `wubu_mobius_new` | 124 | math.h, stdio.h, string.h, wubu_mobius.h | Closed-form Möbius gyration. gyr[x,y]z = z + 2c * (Az*x + Bz*y) / τ² where c = 1/R², σ_x = 1 - c||x| |
| `brain/` | `wubu_model` | 1848 | gguf_reader.h, immintrin.h, math.h, mtp_q8_cache.h, omp.h, stdio.h, stdlib.h, st | dense SwiGLU FFN (zero-copy blob path) |
| `brain/` | `wubu_model_adapter` | 414 | stat.h, stdio.h, stdlib.h, string.h, wubu_da_guard.h, wubu_model_adapter.h | wubu_model_adapter.c -- HF config.json -> wubuwizard dims (C11, self-contained, opaque). Hand-parses |
| `brain/` | `wubu_model_ckpt` | 40 | stdlib.h, string.h, wubu_model.h | wubu_model_ckpt.c — Model state save/restore for speculative decode rollback. Extracted from wubu_mo |
| `brain/` | `wubu_model_format` | 82 | stdlib.h, string.h, wubu_model_format.h | wubu_model_format.c — Format-agnostic model loading (ADR-002). The engine must not be coupled to a s |
| `brain/` | `wubu_model_format_gguf` | 75 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_model_format.h, wubu_tokens.h | wubu_model_format_gguf.c — GGUF adapter for the format-agnostic model loader (ADR-002). Wraps gguf_r |
| `brain/` | `wubu_model_format_onnx` | 298 | stdint.h, stdio.h, stdlib.h, string.h, wubu_model_format.h | wubu_model_format_onnx.c — ONNX format adapter (real, 2026-08-09). Parses the ONNX protobuf (ModelPr |
| `brain/` | `wubu_model_format_st` | 43 | stdlib.h, string.h, wubu_model_format.h | wubu_model_format_st.c — safetensors adapter (ADR-002). Demonstrates the format-agnostic layer works |
| `brain/` | `wubu_model_safetensors_bridge` | 761 | math.h, safetensors_reader.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unis | wubu_model_safetensors_bridge.c -- load HF safetensors Colonel models into wubuwizard's wubu_model_t |
| `brain/` | `wubu_model_scalable` | 320 | stdio.h, stdlib.h, string.h, wubu_model_scalable.h | wubu_model_scalable.c — fractal self-scaling foundation model The model is a self-similar tree of la |
| `brain/` | `wubu_moe` | 764 | assert.h, math.h, mtp_q8_cache.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ | the router slot: physics routing replaces the learned gate |
| `brain/` | `wubu_moe2` | 175 | math.h, stdlib.h, string.h, wubu_moe2.h | wubu_moe2.c -- the mixed-agents router (phase 2 of the WuBu model). |
| `brain/` | `wubu_moe_backward` | 375 | math.h, prctl.h, resource.h, stdbool.h, stdlib.h, string.h, wubu_activations.h,  | wubu_moe_backward.c — MoE backward pass (CPU) Full backprop through MoE: shared expert + top-k route |
| `brain/` | `wubu_moe_grouped` | 64 | assert.h, stdlib.h, string.h, wubu_moe_grouped.h | wubu_moe_grouped.c — MoE expert grouping / grouped-GEMM dispatch (Area D, items D.31/D.37/D.38). C11 |
| `brain/` | `wubu_moe_hyperbolic` | 325 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_activations.h, wubu_moe_hype |  |
| `brain/` | `wubu_moe_hyperbolic_backward` | 559 | gguf_reader.h, math.h, stdlib.h, string.h, wubu_mobius.h, wubu_moe_hyperbolic.h | Poincaré router backward pass (single-level + nested 2-level). Single-level backward: see wubu_poinc |
| `brain/` | `wubu_moe_rag` | 153 | math.h, stdlib.h, string.h, wubu_moe_rag.h | wubu_moe_rag.c -- MoE routing (X01-X06) + retrieval-augmented KV (Y01-Y04). C11. Convergence (Top-K  |
| `brain/` | `wubu_moeroute` | 71 | math.h, string.h, wubu_moeroute.h | wubu_moeroute.c -- MoE capacity routing + load balancing (HH03). C11. Convergence (Switch Transforme |
| `brain/` | `wubu_moondream` | 225 | math.h, stdio.h, stdlib.h, string.h, wubu_activations.h, wubu_moondream.h | wubu_moondream.c -- MoonDream 3 vision model core (agentic vision). C11. Self-contained C11 implemen |
| `brain/` | `wubu_more_spec` | 90 | math.h, stdlib.h, wubu_more_spec.h | wubu_more_spec.c -- Remaining speculative-decoding policy variants (M07/M08/M09/M10/M15/M17/M18/M19/ |
| `brain/` | `wubu_mtp` | 283 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_model.h | wubu_mtp.c — Multi-Token Prediction head (speculative decode). Extracted from wubu_model.c per ADR-0 |
| `brain/` | `wubu_multiteach` | 224 | math.h, stdlib.h, string.h, wubu_multiteach.h | wubu_multiteach.c — Multi-teacher distillation kernel (C11, opaque, minimal) Fused multi-teacher KL  |
| `brain/` | `wubu_mxfp4` | 150 | assert.h, math.h, stdlib.h, string.h, wubu_mxfp4.h | wubu_mxfp4.c — OCP Microscaling (MX) FP4/FP8 quant (Round-4 #433/#435/#437/#438). C11, self-containe |
| `brain/` | `wubu_nest` | 96 | math.h, string.h, wubu_nest.h | wubu_nest.c -- the WuBu Nesting transitions (層疊嵌套), phase 3. |
| `brain/` | `wubu_nested_ssm` | 454 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_nested_s | Nested SSM Forward Implementation Product of K Poincaré balls with K curvatures. Generalizes wubu_po |
| `brain/` | `wubu_nested_ssm_backward` | 1752 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_nested_s | Backward through Möbius scalar multiplication: z = r ⊗ x z[i] = tanh(r * artanh(||x||/R)) * x[i] / | |
| `brain/` | `wubu_neurom` | 120 | math.h, string.h, wubu_neurom.h | wubu_neurom.c -- neuromorphic / SNN frontier (Theme IW). C11. |
| `brain/` | `wubu_nf4` | 113 | math.h, stdint.h, string.h, wubu_nf4.h | wubu_nf4.c — NormalFloat 4-bit quantization implementation NF4 uses 16 quantization levels from norm |
| `brain/` | `wubu_ngram` | 68 | stdlib.h, string.h, wubu_ngram.h | wubu_ngram.c — N-gram drafter for speculative decoding (cascade & fallback). Pure C11, self-containe |
| `brain/` | `wubu_ngram_cascade` | 41 | stdlib.h, string.h, wubu_ngram.h, wubu_spec_cascade.h | wubu_ngram_cascade.c — N-gram cascade drafter (zero extra model). Pure C11, self-contained. Uses pro |
| `brain/` | `wubu_numerical_audit` | 146 | math.h, stdio.h, string.h, wubu_numerical_audit.h | wubu_numerical_audit.c -- Numerical-stability audit of dequant paths (doc F03). Systematically tests |
| `brain/` | `wubu_nvfp4` | 74 | math.h, stdlib.h, wubu_fp8.h, wubu_nvfp4.h | wubu_nvfp4.c — NVFP4 (E2M1 + microscaling) emulation (doc B08). C11. |
| `brain/` | `wubu_ooxml` | 160 | ctype.h, stdlib.h, string.h, wubu_ooxml.h, wubu_zip.h | wubu_ooxml.c -- Office text extraction (docx/xlsx/pptx, ours). ZIP + minimal XML scanning — no XML l |
| `brain/` | `wubu_ops` | 281 | immintrin.h, math.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_dims.h, wubu_op | wubu_ops.c — Low-level numerical operators (activations, norm, conv). Extracted from wubu_ssm.c (Str |
| `brain/` | `wubu_optim` | 135 | math.h, string.h, wubu_backprop.h, wubu_optim.h | wubu_optim.c — the unified optimizer dispatch (the plumbing). See include/wubu_optim.h for the desig |
| `brain/` | `wubu_orbits` | 213 | math.h, stdlib.h, string.h, wubu_orbits.h, wubu_std.h | wubu_orbits.c -- NESTED-SPHERE MEMORY (research/060, AN12 wave 3). The fractal stacking: an item's a |
| `brain/` | `wubu_paged_kv` | 95 | assert.h, stdlib.h, string.h, wubu_paged_kv.h | wubu_paged_kv.c — Paged KV-cache block manager (Area C, items 21-30). C11, self-contained. Implement |
| `brain/` | `wubu_pagedkv` | 67 | string.h, wubu_pagedkv.h | wubu_pagedkv.c -- Paged KV cache (block table, CoW, prefix share) (HH02). C11. Convergence (vLLM Pag |
| `brain/` | `wubu_parallel_spec` | 76 | math.h, stdlib.h, string.h, wubu_parallel_spec.h | wubu_parallel_spec.c -- Parallel speculative decoding (V01-V04) + length-gen PE (W01-W03). C11. Conv |
| `brain/` | `wubu_passk` | 25 | math.h, wubu_passk.h | wubu_passk.c -- the pass@k estimator. |
| `brain/` | `wubu_patch_embed` | 161 | stdint.h, stdlib.h, string.h, wubu_patch_embed.h | wubu_patch_embed.c — ViT patch embedding implementation. C11, self-contained, opaque struct. Impleme |
| `brain/` | `wubu_pd_serve` | 87 | math.h, stdlib.h, string.h, wubu_pd_serve.h | wubu_pd_serve.c -- Disaggregated prefill/decode serving (AB01-AB06) + dynamic compute / mixture-of-d |
| `brain/` | `wubu_pd_split` | 54 | assert.h, stdlib.h, string.h, wubu_pd_split.h | wubu_pd_split.c — Prefill/Decode disaggregation planner (Round-2 #131/#132). C11, self-contained. Sp |
| `brain/` | `wubu_pdf` | 214 | stdlib.h, string.h, wubu_pdf.h, zlib.h | wubu_pdf.c -- PDF text extraction (ours: object scan + FlateDecode). PDF = objects; stream objects c |
| `brain/` | `wubu_pim` | 122 | math.h, string.h, wubu_pim.h | wubu_pim.c -- processing-in-memory / near-memory frontier (Theme IS). |
| `brain/` | `wubu_pim2` | 218 | math.h, string.h, wubu_pim2.h | wubu_pim2.c -- the PIM/hardware frontier, complete (IS). C11. |
| `brain/` | `wubu_planediv` | 48 | stdlib.h, string.h, wubu_planediv.h | wubu_planediv.c -- Control/data-plane separation + poisoning divergence (AG02/AG03). C11. Convergenc |
| `brain/` | `wubu_plateau` | 30 | wubu_plateau.h | wubu_plateau.c -- the amoeba plateau trigger. |
| `brain/` | `wubu_plugin` | 64 | dlfcn.h, stdio.h, stdlib.h, wubu_plugin.h | wubu_plugin.c — Dynamic plugin loader for wubuwizard backends. Implements research 066-G1 (plugin ar |
| `brain/` | `wubu_png` | 171 | stdlib.h, string.h, wubu_png.h, zlib.h | wubu_png.c -- PNG decoder (ours, zlib inflate + filter reconstruction). Public format (RFC 2083), de |
| `brain/` | `wubu_poincare_gqa` | 326 | gguf_reader.h, math.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu_m | Dequant a [rows, cols] BF16/F16 matrix into F32 [cols, rows] (transposed), matching the forward's [I |
| `brain/` | `wubu_poincare_gqa_backward` | 963 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_poincare |  |
| `brain/` | `wubu_poincare_ssm_backward` | 391 | math.h, stdlib.h, string.h, wubu_mobius.h, wubu_ssm.h |  |
| `brain/` | `wubu_polar_pso` | 401 | math.h, stdlib.h, string.h, wubu_mobius.h, wubu_polar_pso.h, wubu_polarquant.h | wubu_polar_pso.c — Meta-compiled PolarQuant decode kernels Implements PSO-cached decode, procedural  |
| `brain/` | `wubu_polarquant` | 660 | math.h, stdint.h, stdlib.h, string.h, wubu_mobius.h, wubu_polarquant.h | wubu_polarquant.c — PolarQuant recursive polar decomposition Implements the recursive pairwise polar |
| `brain/` | `wubu_policy` | 75 | math.h, string.h, wubu_policy.h | wubu_policy.c -- Policy representation + baseline (GG02). C11. Convergence (policy gradient / baseli |
| `brain/` | `wubu_ppo` | 52 | math.h, string.h, wubu_ppo.h | wubu_ppo.c -- PPO clipped surrogate objective (GG04). C11. Convergence (PPO / clipped surrogate 7-ho |
| `brain/` | `wubu_precision_plan` | 149 | stdlib.h, string.h, wubu_precision_plan.h | wubu_precision_plan.c -- HETEROGENEOUS PRECISION (AM03, research/046). The ladder is a pure function |
| `brain/` | `wubu_pref` | 102 | math.h, stdio.h, stdlib.h, string.h, wubu_pref.h | wubu_pref.c -- the RLHF ORACLE (see wubu_pref.h). The Bradley-Terry online preference model: pairs f |
| `brain/` | `wubu_pref2` | 267 | math.h, string.h, wubu_pref2.h | wubu_pref2.c -- the preference-alignment frontier, complete (IQ). C11. |
| `brain/` | `wubu_prefix_cache` | 199 | assert.h, stdint.h, stdlib.h, string.h, wubu_prefix_cache.h | wubu_prefix_cache.c -- Automatic Prefix Caching implementation. Pure C11, self-contained. Uses FNV-1 |
| `brain/` | `wubu_priority` | 110 | stdio.h, stdlib.h, string.h, wubu_priority.h | wubu_priority.c -- the AGI's institutional memory (model-level). See wubu_priority.h. The store make |
| `brain/` | `wubu_priority_store` | 164 | stdio.h, stdlib.h, string.h, wubu_priority_store.h | wubu_priority_store.c -- the PRIORITY STORE (see the header). The diagnose consults it before the ne |
| `brain/` | `wubu_prover` | 76 | string.h, wubu_prover.h | wubu_prover.c -- Automated theorem proving: natural-deduction prover (EE04). C11. Convergence (Lean  |
| `brain/` | `wubu_prover2` | 80 | math.h, wubu_hyper.h, wubu_prover.h | wubu_prover2.c -- the math-RL verifier (phase 6): Lean-style proof-step checking in C11. The model p |
| `brain/` | `wubu_q4k_m` | 200 | math.h, stdint.h, stdlib.h, string.h, wubu_q4k_m.h | wubu_q4k_m.c — Q4_K_M block quantization (Area G, items G.63/G.64). C11, self-contained. Matches GGU |
| `brain/` | `wubu_q8` | 45 | math.h, stdlib.h, string.h, wubu_q8.h | wubu_q8.c — Q8_0 block quantization load/dequant (Area G, items G.61/G.62). C11, self-contained. Q8_ |
| `brain/` | `wubu_quant_selector` | 62 | math.h, wubu_quant_selector.h | wubu_quant_selector.c -- Adaptive quantization / precision selectors (N04 batch-size-aware, N05 cont |
| `brain/` | `wubu_quantkv` | 52 | math.h, stdlib.h, string.h, wubu_quantkv.h | wubu_quantkv.c -- KV cache quantization (INT8 group-wise) (HH06). C11. Convergence (INT8/FP8 KV quan |
| `brain/` | `wubu_qwen35` | 78 | stdio.h, string.h, wubu_qwen35.h | wubu_qwen35.c — the QWEN3.5 HYBRID loader role math (see header). GGUF-corrected 2026-08-09: the fus |
| `brain/` | `wubu_qwen35_forward` | 241 | math.h, stdlib.h, string.h, wubu_qwen35_forward.h | wubu_qwen35_forward.c — the QWEN3.5 HYBRID FORWARD (see header). The gated-attn branch (unfused) + t |
| `brain/` | `wubu_rambus` | 102 | stdlib.h, string.h, wubu_rambus.h | wubu_rambus.c — RDRAM-style interleaved KV memory controller (doc "rambus"). Interleaved banks + row |
| `brain/` | `wubu_recency` | 14 | math.h, wubu_recency.h | wubu_recency.c -- the recency-weighted sampling. |
| `brain/` | `wubu_reinforce` | 57 | math.h, string.h, wubu_policy.h, wubu_reinforce.h | wubu_reinforce.c -- REINFORCE policy gradient (GG01). C11. Convergence (REINFORCE / policy gradient  |
| `brain/` | `wubu_repetition` | 160 | math.h, stdlib.h, string.h, wubu_repetition.h | wubu_repetition.c -- repeat_penalty + DRY (C11, self-contained). Design: opaque wubu_rep_state_t hol |
| `brain/` | `wubu_resource` | 45 | stdlib.h, wubu_resource.h | wubu_resource.c -- At-home resource envelope (AH14/AH15). C11. Convergence (bandwidth-bound tok/s; V |
| `brain/` | `wubu_resources` | 82 | resource.h, stdio.h, string.h, time.h, wubu_resources.h | wubu_resources.c -- the RESOURCE LEDGER (see the header). getrusage + /proc/self/status (WSL/Linux). |
| `brain/` | `wubu_reverify` | 49 | string.h, wubu_reverify.h | wubu_reverify.c -- Closed-loop self-verification (EE07). C11. |
| `brain/` | `wubu_ring_attn` | 204 | math.h, omp.h, stdlib.h, string.h, wubu_ring_attn.h | wubu_ring_attn.c — Ring Attention + Star Attention implementation Chunked scan with LSE (log-sum-exp |
| `brain/` | `wubu_rollout` | 40 | math.h, stdlib.h, string.h, wubu_rollout.h | wubu_rollout.c -- the Balanced Adaptive Rollout allocation. |
| `brain/` | `wubu_roofline` | 67 | assert.h, math.h, stdlib.h, string.h, wubu_roofline.h | wubu_roofline.c — Roofline / B*-crossover auto-tuner (Round-2 item #101). C11, self-contained. Imple |
| `brain/` | `wubu_rope_prefetch` | 108 | math.h, string.h, wubu_rope_prefetch.h | wubu_rope_prefetch.c -- RoPE-aware KV cache prefetch (doc A10). Source: Meta Llama-2 inference optim |
| `brain/` | `wubu_rotate` | 61 | math.h, string.h, wubu_rotate.h | wubu_rotate.c -- Hadamard rotation for outlier suppression (doc 013). Self-contained C11. See header |
| `brain/` | `wubu_router` | 70 | stdlib.h, string.h, wubu_router.h | wubu_router.c -- the router registry (THE ROUTER SLOT). One slot, many physics. Routers register und |
| `brain/` | `wubu_router_physics` | 318 | math.h, stdlib.h, string.h, wubu_ecosystem.h, wubu_gravity.h, wubu_moe.h, wubu_m | wubu_router_physics.c -- the physics routers registered into the slot. THE ROUTER SLOT (Revolver Doc |
| `brain/` | `wubu_rsi` | 135 | math.h, string.h, wubu_rsi.h | wubu_rsi.c -- recursive self-improvement frontier (Theme IV). C11. |
| `brain/` | `wubu_runtime_dims` | 155 | safetensors_reader.h, stdio.h, string.h, wubu_runtime_dims.h | wubu_runtime_dims.c -- runtime dimensions for the WuBu-35M engine. The Revolver Doctrine (THEORY/06) |
| `brain/` | `wubu_safekern` | 51 | stdlib.h, wubu_safekern.h | wubu_safekern.c -- AGI-OS safety kernel (AF11-AF13). C11. Convergence (corrigibility / unfireable sa |
| `brain/` | `wubu_safetensors_model` | 133 | stdio.h, stdlib.h, string.h, wubu_safetensors_model.h | wubu_safetensors_model.c -- safetensors -> wubuwizard weight layout (C11). Maps HF Transformers tens |
| `brain/` | `wubu_safetensors_shard` | 189 | dirent.h, fnmatch.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu_saf | wubu_safetensors_shard.c -- multi-shard safetensors loader. See wubu_safetensors_shard.h. Self-conta |
| `brain/` | `wubu_sandbox_safekern` | 72 | stdio.h, stdlib.h, string.h, wubu_sandbox_safekern.h | wubu_sandbox_safekern.c -- Sandbox capability bridge → safekern (AX08). C11. Convergence (sandbox +  |
| `brain/` | `wubu_save` | 89 | safetensors_writer.h, stdio.h, stdlib.h, string.h, wubu.h | wubu_save.c -- save WuBu checkpoints as REAL safetensors. The DA pass found: we could read safetenso |
| `brain/` | `wubu_scale` | 348 | stdio.h, stdlib.h, string.h, time.h, unistd.h, windows.h, wubu_hwcaps.h, wubu_sc | wubu_scale.c -- SCALE-TO-FIT planner (THEORY/11). One checkpoint, any hardware: probe -> tier -> pla |
| `brain/` | `wubu_scheduler` | 151 | stdio.h, stdlib.h, string.h, wubu_scheduler.h | wubu_scheduler.c -- Stage 2 deterministic token scheduler. Research basis (Kevin-Bacon hop 6-7): Any |
| `brain/` | `wubu_sd_clip` | 691 | gguf_reader.h, math.h, omp.h, stdio.h, stdlib.h, string.h, wubu_activations.h, w | wubu_sd_clip.c -- CLIP text encoder + byte-level BPE tokenizer for the WuBu stable-diffusion engine  |
| `brain/` | `wubu_sd_ops` | 1538 | arm_neon.h, gguf_reader.h, immintrin.h, math.h, stdint.h, stdio.h, stdlib.h, str | wubu_sd_ops.c -- 2D conv / groupnorm / sample ops for the WuBu SD engine. C11, self-contained. Conv  |
| `brain/` | `wubu_sd_taesd` | 219 | math.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu_sd_ops.h, wubu_s | wubu_sd_taesd.c -- Tiny AutoEncoder for Stable Diffusion (TAESD) decoder. Architecture (madebyollin/ |
| `brain/` | `wubu_sd_unet` | 1190 | arm_neon.h, gguf_reader.h, immintrin.h, math.h, omp.h, stdio.h, stdlib.h, string | wubu_sd_unet.c -- LDM UNet forward for the WuBu stable-diffusion engine. SD 1.5 topology (Anything-V |
| `brain/` | `wubu_sd_vae` | 326 | gguf_reader.h, math.h, omp.h, stdio.h, stdlib.h, string.h, time.h, wubu_sd_ops.h | wubu_sd_vae.c -- LDM AutoencoderKL DECODER forward (SD 1.5 family). Topology (verified against the A |
| `brain/` | `wubu_seed` | 34 | wubu_seed.h | wubu_seed.c -- the deterministic environment seed (splitmix64). |
| `brain/` | `wubu_self_cascade` | 72 | math.h, stdlib.h, string.h, wubu_spec_cascade.h | wubu_self_cascade.c — Self-cascade drafter (small local model). Pure C11. Calls a provided small-mod |
| `brain/` | `wubu_selfimprove` | 122 | math.h, stdio.h, stdlib.h, string.h, wubu_rsi.h, wubu_selfimprove.h | wubu_selfimprove.c -- the TRACE/SPAN OPERATOR + the RSI mutation engine (see wubu_selfimprove.h). Cl |
| `brain/` | `wubu_semcons` | 68 | string.h, wubu_bft.h, wubu_semcons.h | wubu_semcons.c -- Semantic consensus: claim + verify + dispute (DD04). C11. Convergence (AgentChain  |
| `brain/` | `wubu_serve` | 115 | wubu_serve.h | wubu_serve.c -- multi-tenant serving scheduler (Theme IR). C11. |
| `brain/` | `wubu_serve2` | 275 | wubu_serve2.h | wubu_serve2.c -- the serving-scheduler frontier, complete (IR). C11. |
| `brain/` | `wubu_shrink` | 139 | stdlib.h, string.h, wubu_shrink.h | wubu_shrink.c -- the model-shrink operators (AM01: the symmetric amoeba). Mirrors wubu_grow.c exactl |
| `brain/` | `wubu_si` | 47 | math.h, string.h, wubu_si.h | wubu_si.c -- Synaptic Intelligence (BB06). C11. |
| `brain/` | `wubu_siglip` | 335 | math.h, stdio.h, stdlib.h, string.h, wubu_siglip.h | wubu_siglip.c — SigLIP vision encoder (SmolVLM2-2.2B mmproj) The SmolVLM2 mmproj carries a SigLIP-SO |
| `brain/` | `wubu_sindy` | 116 | math.h, string.h, wubu_sindy.h | wubu_sindy.c -- SINDy sparse identification of nonlinear dynamics (EE02). C11. Convergence (SINDy /  |
| `brain/` | `wubu_skill_train` | 75 | stdio.h, string.h, wubu_skill_train.h | wubu_skill_train.c — SKILL → TRAIN FEEDBACK (see the header). Experience → weights, not only experie |
| `brain/` | `wubu_skillcell` | 241 | stdio.h, stdlib.h, string.h, wubu_skillcell.h | wubu_skillcell.c -- the SKILL CURRICULUM (see the header). The colony learns skills from experience, |
| `brain/` | `wubu_smoothquant` | 63 | math.h, stdlib.h, string.h, wubu_smoothquant.h | wubu_smoothquant.c -- SmoothQuant activation outlier migration (doc 005). Self-contained C11. See he |
| `brain/` | `wubu_smt_check` | 173 | math.h, stdlib.h, string.h, wubu_smt_check.h | wubu_smt_check.c -- SMT-style equivalence checking of GEMV rewrites (doc F02). Source: Alive2 (Lopes |
| `brain/` | `wubu_soa` | 73 | stdlib.h, string.h, wubu_soa.h | wubu_soa.c -- Structure-of-Arrays activation tensor layout (doc I02/C02). Converts activation tensor |
| `brain/` | `wubu_sparse_attn` | 75 | stdlib.h, string.h, wubu_sparse_attn.h | wubu_sparse_attn.c -- Block-sparse attention pattern generators (L11 NSA / L12 MoBA). Self-contained |
| `brain/` | `wubu_spawn` | 96 | errno.h, fcntl.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_spawn.h | wubu_spawn.c — shell-free external program launcher. C11, self-contained (no god headers). Replaces  |
| `brain/` | `wubu_spawn_win` | 98 | stdio.h, stdlib.h, string.h, windows.h, wubu_spawn.h | wubu_spawn_win.c — Win32 implementation of the shell-free program launcher. Mirrors wubu_spawn.c's c |
| `brain/` | `wubu_spec_cascade` | 137 | math.h, stdlib.h, string.h, wubu_ngram.h, wubu_spec_cascade.h, wubu_spec_decode. | wubu_spec_cascade.c — Cascade speculative decoding (Area K). Pure C11, self-contained. Two cascade f |
| `brain/` | `wubu_spec_decode` | 263 | math.h, stdlib.h, string.h, wubu_spec_decode.h | wubu_spec_decode.c — Speculative decoding framework Implements lossless token acceleration via draft |
| `brain/` | `wubu_spec_tuner` | 79 | stdlib.h, wubu_spec_tuner.h | wubu_spec_tuner.c -- Speculative decode auto-tuner (N15 / M12 / M15). Convergence (self-speculative  |
| `brain/` | `wubu_spec_variants` | 67 | math.h, stdlib.h, wubu_spec_variants.h | wubu_spec_variants.c -- Speculative-decoding variants (M11/M13/M14/L14). C11. Convergence (EAGLE / M |
| `brain/` | `wubu_specdec` | 89 | math.h, string.h, wubu_specdec.h | wubu_specdec.c -- Speculative decoding: draft + verify + reject (HH01). C11. Convergence (Leviathan/ |
| `brain/` | `wubu_ssd_moe` | 221 | pthread.h, safetensors_reader.h, stddef.h, stdint.h, stdio.h, stdlib.h, string.h | wubu_ssd_moe.c -- SSD-paged MoE slot-bank (ds4-ssd technique). See include/wubu_ssd_moe.h. Self-cont |
| `brain/` | `wubu_ssm` | 2410 | assert.h, cuda_runtime.h, gguf_reader.h, immintrin.h, math.h, omp.h, stdio.h, st |  |
| `brain/` | `wubu_ssm_chunked` | 388 | math.h, omp.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | wubu_ssm_chunked.c — Chunked Gated DeltaNet recurrence This is the EXACT same recurrence as the sequ |
| `brain/` | `wubu_ssm_delta` | 327 | immintrin.h, math.h, omp.h, stdio.h, stdlib.h, string.h, wubu_ssm.h, wubu_ssm_ut | wubu_ssm_delta.c — Gated DeltaNet 3:1 hybrid linear attention. Contains the core recurrence: sequent |
| `brain/` | `wubu_ssm_scan` | 50 | assert.h, math.h, stdlib.h, string.h, wubu_ssm_scan.h | wubu_ssm_scan.c — Chunkwise selective-scan (Area F, items F.51/F.58). C11, self-contained. Parallel  |
| `brain/` | `wubu_ssm_utils` | 139 | gguf_reader.h, math.h, omp.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | wubu_ssm_utils.c — shared SSM/GQA helper functions. Extracted from wubu_ssm.c to keep module boundar |
| `brain/` | `wubu_ssm_workspace` | 110 | stdlib.h, string.h, wubu_ssm.h, wubu_ssm_workspace.h | wubu_ssm_workspace.c — per-SSM-layer scratch pool. Replaces the per-call 13xmalloc + 13xfree inside  |
| `brain/` | `wubu_stream_kv` | 104 | stdlib.h, string.h, wubu_stream_kv.h | wubu_stream_kv.c -- StreamingLLM attention-sink KV remapping (L01). Convergence (Kevin-Bacon 7-hop,  |
| `brain/` | `wubu_symbolic` | 90 | stdlib.h, string.h, wubu_symbolic.h | wubu_symbolic.c -- Symbolic rule engine + constraint checker (AW05, AW07). C11. Convergence (Prolog/ |
| `brain/` | `wubu_symreg` | 137 | math.h, stdio.h, stdlib.h, string.h, wubu_symreg.h | wubu_symreg.c -- Symbolic regression via genetic programming (EE01). C11. Convergence (PySR / AI-Fey |
| `brain/` | `wubu_synth` | 83 | stdio.h, stdlib.h, string.h, unistd.h, wubu_spawn.h, wubu_synth.h | wubu_synth.c -- Program synthesis: spec→C11 code gen (AX05). C11. Convergence (program synthesis 7-h |
| `brain/` | `wubu_sys2026` | 95 | math.h, stdlib.h, string.h, wubu_sys2026.h | wubu_sys2026.c -- System/scheduling 2026 KV methods (Q12/Q13/Q14/Q16/Q17/Q18/R02). C11. Policy cores |
| `brain/` | `wubu_sys_tune` | 66 | math.h, stdlib.h, unistd.h, wubu_sys_tune.h | wubu_sys_tune.c -- System / dispatch auto-tuners (L10 / N06 / N10 / O03). C11. Convergence (SeerAtte |
| `brain/` | `wubu_tandem` | 243 | pthread.h, sched.h, stdlib.h, string.h, unistd.h, wubu_gnu_compat.h, wubu_hwcaps | wubu_tandem.c — N64 RCP-style tandem pipeline (doc "tandem"). Two stages (A=prefill/RSP, B=decode/RD |
| `brain/` | `wubu_taskbd` | 72 | math.h, string.h, wubu_taskbd.h | wubu_taskbd.c -- Task boundary detection via OOD on tok/s (BB03). C11. Convergence (task boundary de |
| `brain/` | `wubu_tensor_store` | 932 | gguf_reader.h, math.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu_t | wubu_tensor_store.c -- the uniform tensor catalog (anti-waste interchange). One catalog over ALL for |
| `brain/` | `wubu_ternary` | 196 | math.h, string.h, wubu_ternary.h | wubu_ternary.c -- the ternary/1.58-bit quantization frontier (JC). C11. |
| `brain/` | `wubu_thread_spec` | 190 | pthread.h, sched.h, stdlib.h, string.h, unistd.h, wubu_gnu_compat.h, wubu_thread | wubu_thread_spec.c — CPU thread-specialization analog (doc H02). Two pinned thread pools (prefill /  |
| `brain/` | `wubu_threshsig` | 45 | string.h, wubu_bft.h, wubu_threshsig.h | wubu_threshsig.c -- Threshold signing (aggregate agent signatures) (DD02). C11. Convergence (thresho |
| `brain/` | `wubu_token` | 105 | math.h, string.h, wubu_token.h | wubu_token.c -- tokenization frontier (Theme IT). C11. |
| `brain/` | `wubu_token2` | 214 | math.h, string.h, wubu_token2.h | wubu_token2.c -- the tokenization frontier, complete (IT). C11. |
| `brain/` | `wubu_tokenizer` | 807 | ctype.h, gguf_reader.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_gguf_tokeniz |  |
| `brain/` | `wubu_tokenizer_hf` | 526 | stdio.h, stdlib.h, string.h, wubu_tokenizer_hf.h | wubu_tokenizer_hf.c -- HuggingFace BPE tokenizer loader for wubuwizard. Self-contained: embeds a tin |
| `brain/` | `wubu_toolreg` | 84 | stdio.h, stdlib.h, string.h, wubu_toolreg.h, wubu_trajcell.h | wubu_toolreg.c -- the DENY-BY-DEFAULT TOOL REGISTRY (see the header). Real agency without ambient ho |
| `brain/` | `wubu_tooluse` | 81 | stdio.h, stdlib.h, string.h, wubu_tooluse.h | wubu_tooluse.c -- MCP-compatible tool schema + dispatch (AX04). C11. Convergence (MCP/function-calli |
| `brain/` | `wubu_train` | 250 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_backprop.h, wubu_train.h | wubu_train.c -- the WuBu training core (the AGI brain-cluster loop). The wizard becomes a TRAINING e |
| `brain/` | `wubu_traj_grpo` | 90 | math.h, stddef.h, wubu_traj_grpo.h | wubu_traj_grpo.c -- the multi-turn trajectory-level GRPO (the Orchard recipe core). Group-relative a |
| `brain/` | `wubu_traj_sft` | 68 | stdlib.h, string.h, wubu_traj_sft.h | wubu_traj_sft.c -- the trajectory -> masked-observation SFT converter. The input is COPIED (never mo |
| `brain/` | `wubu_trajcell` | 120 | stdio.h, stdlib.h, string.h, wubu_trajcell.h | wubu_trajcell.c -- VERIFIED TOOL-TRAJECTORY CELLS (see the header). Tool use joins the hive's fitnes |
| `brain/` | `wubu_tst` | 234 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_tst.h |  |
| `brain/` | `wubu_ttc` | 75 | math.h, stdlib.h, string.h, wubu_ttc.h | wubu_ttc.c -- Test-time-compute + multi-agent KV (Q08/Q15/Q20/R01/R03). C11. Convergence (PolyKV / H |
| `brain/` | `wubu_turboquant` | 85 | stdio.h, stdlib.h, string.h, wubu_turboquant.h | wubu_turboquant.c — TurboQuant tile manager implementation Backend for wubu_turboquant.h: manages KV |
| `brain/` | `wubu_ubus` | 283 | math.h, omp.h, stdio.h, stdlib.h, string.h, time.h, wubu_ubus.h | wubu_ubus.c -- the U-Bus substrate (see wubu_ubus.h + the design doc). Backends: CPU scalar (always) |
| `brain/` | `wubu_uq` | 69 | math.h, string.h, wubu_uq.h | wubu_uq.c -- Uncertainty Quantification: bootstrap ensemble + conformal (FF04). C11. Convergence (co |
| `brain/` | `wubu_user_sim` | 73 | stdio.h, stdlib.h, string.h, wubu_user_sim.h | wubu_user_sim.c -- the tau-bench-style user-simulator harness. |
| `brain/` | `wubu_userfs` | 615 | dirent.h, stat.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubu_imgenc.h, wubu | wubu_userfs.c -- USER SPACE IN THE NAMESPACE (the user's files ARE the training data — every user ac |
| `brain/` | `wubu_uuid` | 165 | fcntl.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_uuid.h | wubu_uuid.c — UUIDv7 generation (RFC 9562) for session tracking. Pure C11 port from WuBuOS/src/runti |
| `brain/` | `wubu_value` | 49 | math.h, string.h, wubu_value.h | wubu_value.c -- Value iteration / Bellman backup (GG06). C11. Convergence (value iteration / dynamic |
| `brain/` | `wubu_vecsearch` | 289 | math.h, stdlib.h, string.h, wubu_vecsearch.h | wubu_vecsearch.c -- Vector substrate for AGI-OS (AV01-AV08). C11. Convergence (7-hop KB sweep: vecto |
| `brain/` | `wubu_verify` | 50 | stdio.h, string.h, wubu_verify.h | wubu_verify.c -- C11 type-check + invariant assertion gate (AX09). C11. Convergence (formal verifica |
| `brain/` | `wubu_video` | 138 | stdlib.h, string.h, wubu_jpeg.h, wubu_video.h | wubu_video.c -- Video frame extraction (ours: AVI container + MJPEG). AVI (RIFF) is a public contain |
| `brain/` | `wubu_vision` | 200 | math.h, string.h, wubu_vision.h | wubu_vision.c -- the multimodal vision frontier (JB). C11. |
| `brain/` | `wubu_vision_encoder` | 391 | math.h, stdio.h, stdlib.h, string.h, wubu_vision_encoder.h | wubu_vision_encoder.c — 3D ViT vision encoder port (Qwen3.6-35B-A3B) 27-layer Vision Transformer wit |
| `brain/` | `wubu_vision_moondream` | 534 | json.h, math.h, stdio.h, stdlib.h, string.h, wubu_vision_moondream.h | wubu_vision_moondream.c — Moondream3 SigLIP-style ViT forward pass in C Loads dumped f32 weights fro |
| `brain/` | `wubu_weight` | 103 | string.h, wubu_weight.h | gguf_raw_size returns 0 for types it does not know — try the per-element fallback for float storage. |
| `brain/` | `wubu_width` | 96 | stdlib.h, string.h, wubu_width.h | wubu_width.c -- the width expansion (Net2Net dynamic-dims refactor). The ZERO-PADDING identity: ever |
| `brain/` | `wubu_win` | 134 | fcntl.h, io.h, process.h, stdio.h, stdlib.h, string.h, windows.h, wubu_win.h | wubu_win.c — implementation of the Windows POSIX shims declared in wubu_win.h. Linked into the Windo |
| `brain/` | `wubu_wm_kv` | 109 | stdlib.h, string.h, wubu_wm_kv.h | wubu_wm_kv.c -- Bounded working-memory KV (O05) + online roofline sampler (N02) + per-layer compute  |
| `brain/` | `wubu_worldmodel` | 53 | math.h, stdlib.h, wubu_worldmodel.h | wubu_worldmodel.c -- Closed-loop world-model verify-replan (AG04). C11. Convergence (open-loop probl |
| `brain/` | `wubu_yarn` | 40 | assert.h, math.h, stdlib.h, string.h, wubu_yarn.h | wubu_yarn.c — YaRN NTK-aware RoPE extrapolation (Round-3 #241/#242). C11, self-contained. Extends a  |
| `brain/` | `wubu_zip` | 141 | stdlib.h, string.h, wubu_zip.h, zlib.h | wubu_zip.c -- ZIP reader (ours: central directory + zlib inflate). Public container (PKWARE APPNOTE) |
| `brain/` | `agent_gauntlet` | 256 | agent_gauntlet.h, stat.h, stdio.h, stdlib.h, string.h, time.h, wubu_edr.h, wubu_ | agent_gauntlet.c -- wubuwizard model-agnostic agent tool gauntlet. Design (see wubuwizard-multimodel |
| `brain/` | `gauntlet_run` | 51 | agent_gauntlet.h, stdio.h | gauntlet_run.c -- CLI driver for the wubuwizard agent tool gauntlet. Loads all four Colonel models ( |
| `brain/` | `test_gauntlet` | 46 | agent_gauntlet.h, stdio.h, stdlib.h | test_gauntlet.c -- regression gate for the agent tool gauntlet. Asserts (on the fixture fallback, no |
| `brain/` | `test_gdn_chunk` | 71 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_gdn_chunk.c — verify wubu_ssm_gdn_chunked against the verified sequential scalar recurrence. Th |
| `brain/` | `amoeba_lifecycle` | 134 | math.h, stdio.h, stdlib.h, string.h, wubu_amoeba.h, wubu_hive.h, wubu_moe2.h | amoeba_lifecycle.c -- the AMOEBA lifecycle runner. The full diagnostic loop on the real organs: 1. s |
| `brain/` | `api_server` | 795 | ctype.h, err.h, errno.h, fcntl.h, in.h, inet.h, math.h, signal.h, socket.h, ssl. | api_server.c — Local inference API server for wubuwizard OpenAI-compatible HTTP server for education |
| `brain/` | `attn2_dump` | 83 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | attn2_dump.c -- capture attn2 output for cond vs uncond ctx at t=500 (mid-schedule where the prompt  |
| `brain/` | `attn2_probe` | 99 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_ops. | attn2_probe.c -- compute attn2 (cross) output for cond vs empty ctx on a REAL intermediate. If outpu |
| `brain/` | `bench_512k` | 387 | cuda_runtime.h, gguf_reader.h, inttypes.h, math.h, stdio.h, stdlib.h, string.h,  | bench_512k.c — 512k Context Window Benchmark Tests true 512k context capabilities: - PagedAttention  |
| `brain/` | `bench_512k_comprehension` | 631 | cuda_runtime.h, gaad_nesting_llm.h, gguf_reader.h, inttypes.h, math.h, stdio.h,  | bench_512k_comprehension.c — 512k Context Comprehension Benchmark Implements RULER/LongBench v2 styl |
| `brain/` | `bench_512k_full` | 1534 | cuda_runtime.h, dirent.h, gaad_nesting_llm.h, gguf_reader.h, inttypes.h, math.h, | bench_512k_full.c — Comprehensive 512k Context Benchmark Suite Implements ALL major long-context ben |
| `brain/` | `bench_e2e` | 554 | bench.h, math.h, stdio.h, stdlib.h, string.h, time.h, unistd.h | bench_e2e.c — Phase 2.5 Final: All-40-Layer End-to-End Benchmark Loads all 40 layers (30 SSM + 10 GQ |
| `brain/` | `bench_gemm` | 101 | math.h, omp.h, stdio.h, stdlib.h, string.h, time.h, wubu_gemm.h | bench_gemm.c -- baseline + new-kernel GEMM speed/accuracy benchmark. Times the CURRENT scalar path ( |
| `brain/` | `bench_supremacy` | 194 | immintrin.h, math.h, stdio.h, stdlib.h, string.h, time.h | bench_supremacy.c — THE SPEED GAUNTLET. Proves WuBu GEMM kernels meet or beat competition. Self-host |
| `brain/` | `bench_thread` | 81 | math.h, stdio.h, stdlib.h, string.h, thread_pool.h, time.h |  |
| `brain/` | `build_vocab_clusters` | 168 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h | build_vocab_clusters.c — Cluster 248k output weight columns into groups. Output: centroids file + gr |
| `brain/` | `bytropix_server` | 1036 | errno.h, fcntl.h, in.h, inet.h, json.h, pthread.h, select.h, signal.h, socket.h, | bytropix_server.c — Local inference API server with persistent process support. Two modes: Normal (d |
| `brain/` | `cfg_diff` | 53 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | cfg_diff.c -- compare cond vs uncond eps pointwise at t=999. Working CFG: large differences (prompt  |
| `brain/` | `cfg_scan` | 63 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | cfg_scan.c -- cond vs uncond eps diff across the whole schedule. At t=999 (max noise) prompt influen |
| `brain/` | `check_actual_dims` | 46 | gguf_reader.h, stdio.h, stdlib.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_alignment` | 100 | gguf_reader.h, inttypes.h, stdio.h, stdlib.h, string.h | check_alignment.c — Verify cache-line alignment of GGUF weight tensors. DDR4 optimal burst is 64 byt |
| `brain/` | `check_blob` | 72 | gguf_reader.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_block_boundary` | 92 | gguf_reader.h, stdio.h, stdlib.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_block_size` | 47 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_ce` | 100 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `check_dequant` | 106 | cmath, cstdio, cstdlib, cstring, gguf_reader.h, llama.h | check_dequant.c — Verify IQ2_XXS dequant by comparing against ggml. Build: g++ -std=c++11 -O2 -I /ho |
| `brain/` | `check_dequant2` | 61 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `check_dims` | 45 | gguf_reader.h, stdio.h | Verify gguf_reader tensor dimensions |
| `brain/` | `check_emb` | 95 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_emb2` | 51 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_emb_vs_ref` | 86 | llama.h, math.h, stdio.h, stdlib.h, string.h | Dump the actual token embedding for BOS from llama.cpp model tensors |
| `brain/` | `check_embeddings` | 52 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_expert_boundary` | 51 | gguf_reader.h, stdio.h, stdlib.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_expert_full` | 87 | gguf_reader.h, math.h, stdio.h, stdlib.h, wubu_model.h |  |
| `brain/` | `check_f16_scales` | 90 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_file_emb` | 77 | ggml.h, llama.h, math.h, stdio.h, stdlib.h, string.h | Dump token_embd.weight *and* output.weight directly from llama.cpp |
| `brain/` | `check_file_vs_blob` | 83 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_final` | 114 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_final_norm` | 121 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_forward` | 65 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h |  |
| `brain/` | `check_ggml_types` | 40 | gguf_reader.h, stdio.h | Quick check: what GGML types are used for SSM weights? |
| `brain/` | `check_gqa_nan` | 68 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `check_indexing` | 164 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_iq2_blocks` | 111 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | check_iq2_blocks.c — IQ2 dequant verification Reads raw blocks from GGUF and verifies dequant output |
| `brain/` | `check_iq2_denormals` | 57 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_fp16.h |  |
| `brain/` | `check_moe_full` | 103 | gguf_reader.h, math.h, stdio.h, stdlib.h, wubu_model.h |  |
| `brain/` | `check_moe_offset` | 87 | gguf_reader.h, math.h, stdio.h, stdlib.h, wubu_model.h |  |
| `brain/` | `check_moe_types` | 34 | gguf_reader.h, stdio.h |  |
| `brain/` | `check_moe_weights` | 78 | gguf_reader.h, math.h, stdio.h, wubu_model.h |  |
| `brain/` | `check_output_type` | 46 | gguf_reader.h, stdio.h |  |
| `brain/` | `check_outw` | 71 | gguf_reader.h, llama.h, math.h, stdio.h, stdlib.h, string.h | Compare output.weight dequant between our reader and llama.cpp |
| `brain/` | `check_outw_cols` | 47 | math.h, stdio.h, wubu_model.h | Verify specific columns of output weight between our dequant and Python |
| `brain/` | `check_q4k_block` | 109 | stdint.h, stdio.h, stdlib.h, string.h, wubu_fp16.h |  |
| `brain/` | `check_q4k_raw` | 87 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `check_q5k_denormals` | 80 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_fp16.h | f16_to_f32: use wubu_fp16.h |
| `brain/` | `check_q5k_dequant` | 86 | cmath, cstdint, cstdio, cstdlib, cstring, ggml.h | Cross-check: dequant one Q5_K block using ggml's API and compare with ours. Build: g++ -O2 -I /home/ |
| `brain/` | `check_qweight_nan` | 47 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `check_raw_blocks` | 82 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `check_raw_boundary` | 73 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_read_methods` | 83 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_ref_emb` | 104 | llama.h, math.h, stdio.h, stdlib.h, string.h | Extract token embedding from llama.cpp for comparison |
| `brain/` | `check_router` | 104 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | check_router.c — Compare router computation between our code and reference. Dumps router logits for  |
| `brain/` | `check_ssm_a` | 75 | math.h, stdio.h, wubu_model.h |  |
| `brain/` | `check_ssm_out` | 41 | gguf_reader.h, stdio.h |  |
| `brain/` | `check_ssm_params` | 59 | gguf_reader.h, math.h, stdio.h |  |
| `brain/` | `check_tensor_dims` | 35 | gguf_reader.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `check_tensor_f32` | 41 | gguf_reader.h, stdio.h, stdlib.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `check_tensor_type` | 44 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `check_think_token` | 41 | gguf_reader.h, stdio.h, string.h, wubu_tokenizer.h |  |
| `brain/` | `check_tok_embd` | 116 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | check_tok_embd.c — Extract token_embd.weight from GGUF and compare with file Build: gcc -O2 -I inclu |
| `brain/` | `check_token` | 20 | stdio.h, wubu_tokenizer.h |  |
| `brain/` | `check_typesizes` | 17 | ggml.h, stdio.h |  |
| `brain/` | `check_weights` | 110 | math.h, stdio.h, wubu_model.h |  |
| `brain/` | `check_weights2` | 56 | math.h, stdio.h, wubu_model.h |  |
| `brain/` | `compare_dequant` | 59 | llama.h, math.h, stdio.h, stdlib.h, string.h | Compare Q5_K dequantization between our gguf_reader and llama.cpp. Both should produce identical flo |
| `brain/` | `compare_iq1m_dequant` | 47 | math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `compare_layer0` | 114 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | compare_layer0.c — Run BOS token through 1 layer, dump output from both our reader and llama.cpp. Co |
| `brain/` | `compare_logits` | 138 | ggml.h, gguf_reader.h, llama.h, math.h, stdio.h, stdlib.h, string.h, wubu_model. |  |
| `brain/` | `compare_moe_expert` | 134 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | compare_moe_expert.c — Compare CPU vs GPU MoE per-expert contributions. Uses the working wubu_moe_fo |
| `brain/` | `compare_moe_layer0` | 104 | gguf_reader.h, math.h, omp.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_mo | compare_moe_layer0.c — Compare MoE layer 0 output between our implementations. Runs one layer manual |
| `brain/` | `compare_outw` | 28 | gguf_reader.h, stdio.h, wubu_model.h | Save first token of output.weight from our dequant for comparison |
| `brain/` | `compare_quant_matmul_vs_sgemm` | 295 | ggml.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | compare_quant_matmul_vs_sgemm.c Load one expert's gate weights (IQ2_XXS), compute the expert forward |
| `brain/` | `compare_weights` | 86 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | compare_weights.c — Dump quantized weight from both our reader and llama.cpp. Build: gcc -O2 -I incl |
| `brain/` | `compare_weights_two_paths` | 86 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | compare_weights_two_paths.c Load expert 64's gate weights via TWO paths and compare. |
| `brain/` | `cross_probe` | 38 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_ops. | cross_probe.c -- verify the cross-attn k/v path: project ctx (cond vs empty) through to_k/to_v of at |
| `brain/` | `dbg_gguf` | 79 | stdint.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `debug_beta_layout` | 123 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, gguf_reader.h, math.h, stdio.h, std |  |
| `brain/` | `debug_ce` | 96 | gguf_reader.h, math.h, stdio.h, stdlib.h, wubu_model.h | debug_ce.c — Diagnose why CE loss is 6.6e10 Checks: output.weight Q4_K dequant range, a few vocab lo |
| `brain/` | `debug_iq2s` | 206 | gguf_reader.h, iq2s_grid.inc, math.h, stdint.h, stdio.h, stdlib.h, string.h, wub |  |
| `brain/` | `debug_moe` | 206 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `debug_moe_layer0` | 94 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu |  |
| `brain/` | `debug_q4k_block` | 58 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_fp16.h | Print first block raw bytes |
| `brain/` | `delta_sparsity` | 85 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h | delta_sparsity.c — Measure hidden state delta sparsity between consecutive tokens. If delta is spars |
| `brain/` | `dequant_check` | 89 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | Dequant first Q4_K block of output.weight and save for Python comparison |
| `brain/` | `dequant_compare` | 166 | ggml.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | dequant_compare.c — Compare wubuwizard dequant vs llama.cpp ggml dequant. Reads raw quantized data f |
| `brain/` | `dequant_ref` | 239 | cmath, cstdint, cstdio, cstdlib, cstring, ggml.h, vector | dequant_ref.c — Read raw GGUF data and dequantize using ggml. Build: g++ -std=c++11 -O2 -I /home/wub |
| `brain/` | `dequant_verify` | 203 | cassert, cmath, cstdint, cstdio, cstdlib, cstring, ggml.h | Verify Q5_K and Q6_K dequant by dequanting raw bytes using ggml. Build: g++ -O2 -I /home/wubu/llama. |
| `brain/` | `dgemma_quick_test` | 83 | gguf_reader.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h | dgemma_quick_test.c — Quick DiffusionGemma forward test |
| `brain/` | `diag_forward` | 145 | math.h, stat.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | diag_forward.c — Run bytropix single-token forward, dump per-layer hidden states Build: gcc -O3 -I i |
| `brain/` | `diff_models` | 33 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `direct_compare` | 95 | ggml.h, math.h, stdio.h, stdlib.h, string.h | direct_compare.c — Compare our Q5_K dequant vs ggml reference on SAME raw data. Reads raw bytes dire |
| `brain/` | `dump_all_dims` | 75 | gguf_reader.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `dump_all_weights` | 37 | gguf_reader.h, stdio.h, wubu_model.h | Dump ALL layer 0 weights as flat float files for Python comparison |
| `brain/` | `dump_diag` | 176 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Full model with per-layer diagnostics: dump output stats per layer |
| `brain/` | `dump_dims` | 119 | gguf_reader.h, stdio.h, stdlib.h |  |
| `brain/` | `dump_expert_ref` | 47 | cmath, cstdio, cstdlib, cstring, llama.h, vector |  |
| `brain/` | `dump_full_layers` | 108 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | dump_full_layers.c — Dump per-layer residuals for comparison with llama.cpp. Build: gcc -O2 -I inclu |
| `brain/` | `dump_full_logits` | 77 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Full model + MoE, dump logits for comparison |
| `brain/` | `dump_gate_raw` | 49 | gguf_reader.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | dump_gate_expert0.c — Dump first column of gate weights for expert 0 from our loaded F32 buffer. Pri |
| `brain/` | `dump_gguf_names` | 21 | gguf_reader.h, stdio.h | dump_gguf_names.c — list all tensor names in a GGUF (for loader name-mapping work). |
| `brain/` | `dump_hidden` | 42 | stdio.h, string.h, wubu_model.h | Simple embedding check after GPU init. |
| `brain/` | `dump_hidden_backup` | 160 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | dump_hidden.c — Compare CPU vs GPU hidden states per-layer to find error accumulation. GPU MoE enabl |
| `brain/` | `dump_hidden_consecutive` | 110 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | dump_hidden_consecutive.c — Check if GPU with MoE stays correct across multiple decode steps. Compar |
| `brain/` | `dump_hidden_layer` | 123 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | dump_hidden_layer.c — Compare CPU vs GPU hidden states per layer. Uses FORCE_CPU_MOE env var to togg |
| `brain/` | `dump_hidden_prefill` | 106 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | dump_hidden_prefill.c — Compare CPU vs GPU hidden states for prefill (N>1). Tests GPU GQA (active fo |
| `brain/` | `dump_intermediates` | 211 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Enhanced debug: dump ALL intermediates for SSM layer 0 with detailed prints |
| `brain/` | `dump_kv` | 76 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `dump_kv_fixed` | 56 | gguf_reader.h, stdint.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `dump_l0_full` | 124 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Dump ALL intermediates for SSM layer 0 for verification |
| `brain/` | `dump_layers` | 87 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | dump_layers.c — Run 1 token through our model, dump per-layer hidden states. Compares SSM vs GQA lay |
| `brain/` | `dump_layers_moe` | 104 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | dump_layers_moe.c — Dump per-layer residuals WITH MoE enabled. Uses wubu_model_forward (library path |
| `brain/` | `dump_llama_embd` | 89 | cstdio, cstdlib, cstring, llama.h, string, vector |  |
| `brain/` | `dump_llama_hidden` | 89 | common.h, cstdio, cstring, llama.h, string, vector | dump_llama_hidden.c — Use llama.cpp API to run model and dump logits. Build: gcc -O2 -I /home/wubu/l |
| `brain/` | `dump_llama_logits` | 132 | cmath, cstdio, cstdlib, cstring, llama.h, string, vector | dump_llama_logits.c — Use llama.cpp API to dump logits for a single BOS token. Build: g++ -std=c++11 |
| `brain/` | `dump_loaded_weights` | 67 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | Dump first expert's dequantized gate weight for comparison. Uses our gguf_reader to load and dequant |
| `brain/` | `dump_logits` | 99 | cstdio, cstdlib, cstring, llama.h, string, vector | dump_logits.c — Dump logits from llama.cpp for a given prompt. Build: g++ -O2 -I /home/wubu/llama.cp |
| `brain/` | `dump_mmproj` | 93 | gguf_reader.h, stdio.h, string.h | dump_mmproj.c — Read and verify MMProj vision merger GGUF dimensions Reads /models/qwen3.6-35b-mmpro |
| `brain/` | `dump_moe_for_python` | 172 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | dump_moe_for_python.c — Dump MoE weights + test input for Python comparison. Dumps dequantized exper |
| `brain/` | `dump_moe_layer0` | 157 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | tools/dump_moe_layer0.c Load Layer 0 of the model WITH MoE and dump ALL MoE intermediates. Single to |
| `brain/` | `dump_moe_magnitude` | 173 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wubu_ssm.h | Dump shared expert before and after gating. |
| `brain/` | `dump_moe_split` | 172 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wubu_ssm.h | Dump routed-only and shared-only MoE outputs for layer 0. |
| `brain/` | `dump_moe_types` | 27 | gguf_reader.h, stdint.h, stdio.h, string.h |  |
| `brain/` | `dump_nonlayer_tensors` | 23 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `dump_our_debug` | 85 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `dump_our_embd` | 54 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `dump_our_logits` | 64 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Dump our model's logits + last hidden state for a single BOS token |
| `brain/` | `dump_output_weight` | 203 | math.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_fp16.h | Q4_K dequant helper (replicated from wubuwizard gguf_reader.c) |
| `brain/` | `dump_outw_block` | 52 | gguf_reader.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | Save first 144 bytes of output.weight (one Q4_K block) + dequant values |
| `brain/` | `dump_per_layer` | 133 | dirent.h, gguf_reader.h, math.h, stat.h, stdio.h, stdlib.h, string.h, wubu_model | dump_per_layer.c — Dump CPU and GPU per-layer hidden states via DUMP_LAYER_DIR env. Compare cos-sim  |
| `brain/` | `dump_ref` | 120 | llama.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `dump_ref_debug` | 65 | llama.h, stdio.h, stdlib.h, string.h | Dump reference BOS embedding and output.weight from llama.cpp |
| `brain/` | `dump_ref_emb` | 73 | llama.h, stdio.h, stdlib.h, string.h | Dump the first embedding from both our loader and llama.cpp to verify embedding lookup produces iden |
| `brain/` | `dump_ref_layers` | 78 | cmath, cstdio, cstdlib, cstring, llama.h, vector | dump_ref_layers.c — Use llama.cpp to dump per-layer hidden states. Build: g++ -std=c++11 -O2 -I /hom |
| `brain/` | `dump_ref_logits` | 61 | llama.h, stdio.h, stdlib.h, string.h | Dump reference logits from llama.cpp for a single BOS token |
| `brain/` | `dump_reference_logits` | 48 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h | dump_reference_logits.c — Run reference model and save logits + per-layer dumps Build: gcc -O3 -I in |
| `brain/` | `dump_rmsnorm` | 158 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h | dump_rmsnorm.c — Verify RMSNorm: dump normed input, weights, and output. Build: gcc -O2 -I include - |
| `brain/` | `dump_router_type` | 20 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `dump_tensor_dims` | 39 | gguf_reader.h, stdio.h |  |
| `brain/` | `dump_tensor_our` | 137 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | dump_tensor_our.c — Dump a specific tensor dequantized by our gguf_reader. Build: gcc -O2 -I include |
| `brain/` | `dump_tensor_types` | 43 | gguf_reader.h, stdio.h |  |
| `brain/` | `dump_tensors` | 39 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `dump_weight_ref` | 96 | cmath, cstdio, cstdlib, cstring, llama.h | dump_weight_ref.c — Dump a model tensor via llama.cpp. Build: g++ -std=c++11 -O2 -I /home/wubu/llama |
| `brain/` | `dump_weights_debug` | 55 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h | Compare BOS embedding + first few weights between our model and llama.cpp |
| `brain/` | `dump_weights_l0` | 50 | gguf_reader.h, stdio.h, wubu_model.h | Save first 2048*16 floats of attn_qkv for layer 0 for comparison |
| `brain/` | `empty_prompt` | 31 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h | empty_prompt.c -- CLIP encode of "" vs a normal prompt: check stats. The uncond embedding must look  |
| `brain/` | `eps_bias` | 29 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_unet.h | eps_bias.c -- UNet eps per-channel mean for zero + noise inputs. A correct UNet: eps per-channel mea |
| `brain/` | `exp2poly_probe` | 53 | arm_neon.h, math.h, stdio.h | exp2poly_probe.c — measure NEON polynomial exp2 vs libm exp2f |
| `brain/` | `extract_and_map` | 168 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `final_means` | 63 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | final_means.c -- reproduce the sampler, print FINAL latent channel means. If means are ~0, the laten |
| `brain/` | `find_special_tokens` | 60 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_tokenizer.h | find_special_tokens.c — Find <|im_start|>, <|im_end|>, <|think|> token IDs by scanning the tokenizer |
| `brain/` | `gen_fixture_btl3_lora` | 67 | stdint.h, stdio.h, stdlib.h, string.h | gen_fixture_btl3_lora.c -- write a tiny BTL-3 LoRA adapter safetensors on top of the fixture base (D |
| `brain/` | `gen_fixture_safetensors` | 40 | stdio.h, stdlib.h, string.h | gen_fixture_safetensors.c -- write a tiny F32 safetensors file for tests. Tensors: "a" [2,3] = 1..6, |
| `brain/` | `gen_fixture_safetensors_model` | 92 | stdint.h, stdio.h, stdlib.h, string.h | gen_fixture_safetensors_model.c -- write a tiny F32 safetensors model using the REAL published HF te |
| `brain/` | `gen_noise` | 36 | math.h, stdint.h, stdio.h, stdlib.h | gen_noise.c — generate 4x64x64 gaussian noise with the SAME xorshift64 + Box-Muller as tools/txt2img |
| `brain/` | `gen_ref_logits` | 63 | llama.h, stdio.h, stdlib.h, string.h | Generate fresh reference logits from llama.cpp and show top tokens |
| `brain/` | `gen_text` | 388 | gguf_reader.h, gpu_output_proj.h, math.h, signal.h, stdbool.h, stdio.h, stdlib.h | gen_text.c — Text generation with optional GPU-accelerated output projection. CPU-only:  make gen_te |
| `brain/` | `gen_text_gemma4` | 72 | prctl.h, resource.h, stdio.h, stdlib.h, string.h, time.h, wubu_gemma4.h |  |
| `brain/` | `gen_text_mtp` | 358 | gguf_reader.h, math.h, signal.h, stdio.h, stdlib.h, string.h, time.h, wubu_model | gen_text_mtp.c — MTP speculative decode with verification. DRAFT_N=2 (blog: 83% acceptance at 2, 50% |
| `brain/` | `get_ref_hidden` | 63 | llama.h, math.h, stdio.h, stdlib.h | Get reference last hidden state from llama.cpp |
| `brain/` | `ggml_check` | 82 | cmath, cstdio, cstdlib, cstring, ggml-alloc.h, ggml-backend.h, ggml.h | Verify SSM forward by calling ggml operators directly (same as llama.cpp) Link against: -lggml-base  |
| `brain/` | `gpu_matmul_check` | 38 | gpu_wubu.h, stdio.h, stdlib.h, string.h | gpu_matmul_check.c -- verify GPU matmul == CPU matmul bit-for-bit. |
| `brain/` | `infer_moe` | 162 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_core_dumps.h, w | infer_moe.c — MoE inference engine Pre-loads all 40 layers' MoE weights quantized in RAM. Fast dequa |
| `brain/` | `infer_moe_lazy` | 396 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_core_dumps.h, w | infer_moe_lazy.c — Lazy MoE inference Key optimization: dequant ONLY the top-2 experts per token ins |
| `brain/` | `infer_poincare` | 133 | bench.h, cuda_kernels.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, tim | infer_poincare.c — Poincaré SSM inference engine Loads one SSM layer, runs GPU Poincaré forward, ben |
| `brain/` | `infer_text` | 1740 | bench.h, gguf_reader.h, math.h, omp.h, prctl.h, resource.h, signal.h, stdbool.h, | infer_text.c — Full text generation pipeline v2 Phase 1 (prefill): full forward over prompt tokens,  |
| `brain/` | `infer_text_gpu` | 635 | bench.h, gguf_reader.h, math.h, prctl.h, resource.h, signal.h, stdio.h, stdlib.h | infer_text_gpu.c — GPU-accelerated text generation v5 Chunked prefill + persistent KV cache + increm |
| `brain/` | `infer_unified` | 234 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_core_dumps.h, w | infer_unified.c — Unified 40-layer inference (SSM→GQA→MoE) Loads GGUF once, runs all 40 layers with  |
| `brain/` | `infer_vision` | 118 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_core_dumps.h, wubu_siglip.h, w | infer_vision.c — Vision encoder inference engine Loads an mmproj GGUF, processes an image, outputs e |
| `brain/` | `infer_vision_text` | 114 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_core_dumps.h, wubu_model.h, wu | infer_vision_text.c — Vision→Text pipeline integration (S5) Loads vision encoder + text model, runs  |
| `brain/` | `inspect_iq2s` | 109 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `inspect_model` | 73 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `inspect_model_types` | 56 | gguf_reader.h, stdio.h, stdlib.h |  |
| `brain/` | `l1_probe` | 49 | gguf_reader.h, math.h, stdio.h, stdlib.h, wubu_sd_ops.h | l1_probe.c -- compare sd_dequant (via linear_q) vs gguf_read_tensor_f32 for time_embed.0.weight: if  |
| `brain/` | `latent_means` | 70 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | latent_means.c -- run the sigma sampler, print final latent channel means + eps per-channel means at |
| `brain/` | `latent_structure` | 74 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | latent_structure.c -- dump the final DDIM latent and measure spatial autocorrelation. A correct late |
| `brain/` | `layer_analysis` | 74 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Layer-by-layer analysis: save hidden state after every layer |
| `brain/` | `layer_cos_sim` | 57 | math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `lfm2_gen` | 130 | math.h, stdio.h, stdlib.h, time.h, wubu_lfm2.h | lfm2_gen.c -- autoregressive generation for LFM2.5 (backup brain). Reads an initial token-ID sequenc |
| `brain/` | `lfm2_hidden` | 36 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_lfm2.h | lfm2_hidden.c — dump the final hidden state (post final norm, pre lm_head) for a prompt. Usage: lfm2 |
| `brain/` | `lfm2_logits` | 42 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_lfm2.h | lfm2_logits.c — dump top-k logits for a prompt using lfm2 forward. Usage: lfm2_logits <model.gguf> < |
| `brain/` | `lfm2_logits_for` | 39 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_lfm2.h | lfm2_logits_for.c — print logits for specific tokens, on a prompt. Usage: lfm2_logits_for <model.ggu |
| `brain/` | `lfm2_test` | 53 | math.h, stdio.h, stdlib.h, string.h, wubu_lfm2.h | lfm2_test.c -- verify LFM2.5 forward produces finite, sensible logits. Loads the safetensors dir, em |
| `brain/` | `list_tensors` | 25 | gguf_reader.h, stdio.h, string.h |  |
| `brain/` | `load_model_layer` | 276 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_core_dumps.h, wubu_mobi | Load Qwen3.6 SSM layer weights from GGUF file. Opens the model, finds all tensors for layer `layer_i |
| `brain/` | `logit_probe` | 45 | stdio.h, stdlib.h, string.h, wubu_model.h, wubu_tokenizer.h | top 5 |
| `brain/` | `merge_gguf` | 247 | gguf_reader.h, stdint.h, stdio.h, stdlib.h, string.h | merge_gguf.c — Merge GGUF shards (e.g. -00001-of-00003 ...) into ONE file. wubuwizard's gguf_open()  |
| `brain/` | `moondream_cli` | 94 | stdio.h, stdlib.h, string.h, wubu_moondream.h | moondream_cli.c -- MoonDream 3 operational runner (the agentic vision core). Loads a real image (PPM |
| `brain/` | `multiteach_selftest` | 168 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_multiteach.h | tools/test_multiteach.c — triple-DA test for wubu_multiteach P1 (correctness): KL divergence is non- |
| `brain/` | `out_block_types` | 28 | gguf_reader.h, stdio.h, string.h | out_block_types.c -- dump types of the out.* tensors in the UNet GGUF |
| `brain/` | `outbias` | 20 | gguf_reader.h, stdio.h, stdlib.h | outbias.c -- check out.2 conv bias + groupnorm output stats. If the groupnorm output is near-zero, e |
| `brain/` | `outblock_trace` | 80 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | outblock_trace.c -- trace the cond-vs-uncond signal through the 12 output blocks (after each resbloc |
| `brain/` | `plugin_onnx` | 71 | stdio.h, stdlib.h, string.h, wubu_model_format.h, wubu_plugin.h | plugin_onnx.c — Example dlopen plugin: ONNX format adapter. Demonstrates the plugin architecture (re |
| `brain/` | `pre_out_dump` | 75 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_ops.h, wubu_sd_unet. | pre_out_dump.c -- capture OUR pre-out activation (stage 3) on zero input and dump per-channel means/ |
| `brain/` | `probe_posstable` | 71 | math.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_model.h, wubu_model_adapter. | Probe: is the engine forward position-stable? Forward the same prefix of length L, once as T=L and o |
| `brain/` | `probe_ssm_bisect` | 69 | math.h, stdio.h, stdlib.h, string.h, wubu_dims.h, wubu_ssm.h | Bisect the SSM forward directly: build a deterministic pseudo-input x of shape [maxT, d_model], call |
| `brain/` | `profile_experts` | 171 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_moe.h | profile_experts.c — Profile expert selection stability. Measures: (1) overlap from token N to N+1, ( |
| `brain/` | `prune_gguf` | 215 | gguf_reader.h, inttypes.h, stdint.h, stdio.h, stdlib.h, string.h | prune_gguf.c — Prune non-essential experts from GGUF model. Uses gguf_reader to parse the input, the |
| `brain/` | `prune_gguf_experts` | 243 | gguf_reader.h, inttypes.h, stdint.h, stdio.h, stdlib.h, string.h | prune_gguf_experts.c — Prune non-essential expert weights from a GGUF. Keeps only top-N routed exper |
| `brain/` | `quick_compare` | 78 | gguf_reader.h, llama.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ | Compare final hidden states: ours vs reference, before output projection |
| `brain/` | `ref_forward` | 127 | llama.h, math.h, stdio.h, stdlib.h, string.h | ref_forward.c — Use libllama to run a single token forward, dump hidden state. Links against ~/llama |
| `brain/` | `repro_grow` | 128 | stdio.h, stdlib.h, string.h, wubu.h, wubu_backprop.h, wubu_grow.h, wubu_train.h | repro_grow.c -- the exact CLI growth sequence repro (the progressive deepening: pos_g = n_layers / 2 |
| `brain/` | `rng_match` | 60 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | rng_match.c -- replicate std::default_random_engine (libstdc++ = minstd_rand0, LCG a=16807 m=2147483 |
| `brain/` | `run_bos` | 75 | stdio.h, stdlib.h, string.h, wubu_core_dumps.h, wubu_model.h | run_bos.c — Single BOS forward pass (no tokenizer needed). Usage: DUMP_LAYER_DIR=/tmp/dump_layers_ou |
| `brain/` | `run_bos_comparison` | 55 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h | run_bos_comparison.c — Run our MoE path with BOS token and compare with ref. Build: gcc -O2 -I inclu |
| `brain/` | `run_ref_bos` | 66 | cmath, cstdio, cstdlib, cstring, llama.h, vector | run_ref_bos.c — Run llama.cpp with BOS-only, dump logits + final hidden. Build: g++ -std=c++11 -O2 - |
| `brain/` | `scan_expert_offsets` | 83 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `scan_weights` | 38 | gguf_reader.h, math.h, stdio.h, wubu_model.h | Scan model weights for NaN/Inf values that could corrupt output |
| `brain/` | `sd_eps_probe` | 90 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_unet.h | sd_eps_probe.c — run OUR C11 UNet on the same noise + t, dump eps .bin. Mirror of the sd.cpp oracle  |
| `brain/` | `sd_gemm_probe` | 46 | stdio.h, stdlib.h, string.h, time.h, wubu_sd_ops.h | sd_gemm_probe.c — isolate wubu_sd_matmul_nt_f16 throughput on the CM4. Measures the GEMM alone (no i |
| `brain/` | `sd_gemm_probe2` | 56 | stdio.h, stdlib.h, time.h, wubu_sd_ops.h | sd_gemm_probe2.c — JC sensitivity + vcvt cost for the F16 GEMM. Measures wubu_sd_matmul_nt_f16 acros |
| `brain/` | `single_block_test` | 157 | ggml.h, math.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_fp16.h | single_block_test.c — Test ONE Q5_K block in detail. Compares intermediate values (d, dmin, scales,  |
| `brain/` | `spot` | 36 | gguf_reader.h, stdio.h, stdlib.h, string.h | spot.c -- hand-dequant first 8 F16 of time_embed.2.weight and compare to gguf_read_tensor_f32; also  |
| `brain/` | `ssm_layer0_dump` | 91 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | tools/ssm_layer0_dump.c Run SSM Layer 0 of the model for a single token and dump intermediates. |
| `brain/` | `ssm_verify` | 63 | cassert, cmath, cstdio, cstdlib, cstring, ggml.h | SELF-CONTAINED SSM VERIFICATION Build: g++ -O2 -I /home/wubu/llama.cpp/ggml/include -o /tmp/ssm_veri |
| `brain/` | `stage_debug` | 157 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Dump intermediate states before and after each stage for one token |
| `brain/` | `stage_trace` | 71 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h, wubu_sd_unet | stage_trace.c -- trace where the cond-vs-uncond signal dies: stage 1 = middle-block input, 2 = after |
| `brain/` | `t_decode` | 29 | stdio.h, stdlib.h, string.h, wubu_tokenizer.h | t_decode.c — decode token ids printed by gen_text (stdout line) |
| `brain/` | `t_deq_compare` | 26 | gguf_reader.h, stdio.h, stdlib.h, string.h | t_deq_compare.c — dequantize the first N floats of a GGUF tensor with OUR gguf_dequantize and dump t |
| `brain/` | `t_ffn_res` | 18 | gguf_reader.h, stdio.h, wubu_gguf_names.h | t_ffn_res.c — does the resolver find the dense FFN roles for Qwen3.5? |
| `brain/` | `t_gguf_tok` | 28 | stdio.h, stdlib.h, wubu_tokenizer.h | t_gguf_tok.c — probe the GGUF-embedded tokenizer path. Usage: t_gguf_tok <model.gguf> <text> Prints  |
| `brain/` | `t_kvvals` | 60 | stdint.h, stdio.h, stdlib.h, string.h | t_kvvals.c — print values of selected GGUF KV pairs (little-endian). |
| `brain/` | `t_kvwalk` | 37 | stdint.h, stdio.h, stdlib.h, string.h | t_kvwalk.c — walk a GGUF's KV section, print key + type + size. |
| `brain/` | `t_lfmdecode` | 81 | gguf_reader.h, stdint.h, stdio.h, stdlib.h, string.h | t_lfmdecode.c — decode token ids from LFM2.5's GGUF vocab (byte-level BPE). Usage: t_lfmdecode <mode |
| `brain/` | `t_lfmres` | 19 | gguf_reader.h, stdio.h, wubu_gguf_names.h | t_lfmres.c — resolve roles for LFM layer 0, print what comes back |
| `brain/` | `t_offsets` | 19 | gguf_reader.h, stdio.h | t_offsets.c — print GGUF tensor names + our reader's data_offsets. |
| `brain/` | `t_q8cmp` | 50 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | t_q8cmp.c — compare quantized_matmul (F32 x) vs quantized_matmul_from_q8 (Q8_K x) on the REAL attn_q |
| `brain/` | `t_qkv_nan` | 48 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | t_qkv_nan.c — isolate: does quantized_matmul produce NaN on Qwen3.5-0.8B's real attn_qkv (Q8_0, [614 |
| `brain/` | `tailslayer` | 355 | bench.h, gguf_reader.h, math.h, stdbool.h, stdio.h, stdlib.h, string.h, time.h,  | tailslayer.c — Speculative decoding with N drafts, longest-valid-prefix. Approach: 1. From model out |
| `brain/` | `td` | 38 | stdio.h, stdlib.h, wubu_tokenizer.h |  |
| `brain/` | `td2` | 24 | stdio.h, wubu_tokenizer.h |  |
| `brain/` | `temb_probe` | 58 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_unet.h | temb_probe.c -- dump time embedding for t=0 vs t=999 |
| `brain/` | `temb_rms` | 54 | gguf_reader.h, math.h, stdio.h, stdlib.h | temb_rms.c -- full time-embed diagnostic: output rms + diff for t values |
| `brain/` | `test_256k` | 93 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_moe.h, wubu_ssm | test_256k.c — 256K context inference stress test Tests MoE router at full 256K, SSM up to moderate s |
| `brain/` | `test_256k_chunked` | 116 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_model_safetensor | test_256k_chunked.c — prove the FULL 256K (262144) context forward runs on a memory-limited box by c |
| `brain/` | `test_256k_context` | 547 | math.h, resource.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_ssm.h | test_256k_context.c — 256K context memory bottleneck analysis Tests KV cache, SSM states, and forwar |
| `brain/` | `test_256k_forward` | 118 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_model_safetensor | test_256k_forward.c -- REAL 256K-context forward verification. Two things are proven: 1) The 256K CO |
| `brain/` | `test_4kv` | 156 | math.h, stdio.h, stdlib.h, string.h, wubu_4kv.h | wubu_4kv test — verify round-trip accuracy of 4-bit KV quantization. Uses realistic KV-cache-like da |
| `brain/` | `test_512k_budget` | 52 | assert.h, stdio.h, stdlib.h, wubu_mem_budget.h | Test: 512K-context OOM-safe memory budget (doc: 512K cycles, no OOM). Verifies the budget calculator |
| `brain/` | `test_adapter` | 157 | stdio.h, stdlib.h, string.h, wubu_adapter.h, wubu_adapter_compat.h | test_adapter.c — tests for the hot-swappable AGI adapter framework Verifies: T1: register + lookup a |
| `brain/` | `test_adaptive_hotpath` | 106 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_kv_adaptive.h, wubu_model.h | Test: adaptive KV cache read/write through engine hot-path (doc 001). Verifies that kv_cache_write_h |
| `brain/` | `test_affinity` | 35 | assert.h, stdio.h, stdlib.h, string.h, wubu_affinity.h | Test: wubu_affinity (Areas J/K — CPU/NUMA pinning + hugepages). |
| `brain/` | `test_agentic_kv` | 51 | math.h, stdio.h, wubu_agentic_kv.h | test_agentic_kv.c -- S06/U01/U02/U03/U04/U05 verification. |
| `brain/` | `test_agentic_os_mem` | 64 | math.h, stdio.h, wubu_agentic_mem.h, wubu_agentic_os.h | test_agentic_os_mem.c -- AD01-AD04 + AE01-AE04 verification. |
| `brain/` | `test_agi_os_integration` | 85 | math.h, stdio.h, wubu_ctxvm.h, wubu_latency.h, wubu_safekern.h | test_agi_os_integration.c -- AF05-AF13 verification (latency / ctx-vm / safety). |
| `brain/` | `test_align` | 107 | math.h, stdio.h, string.h, wubu_align.h | test_align.c -- Theme IM: preference alignment + unlearning. |
| `brain/` | `test_ambig` | 53 | stdio.h, string.h, wubu_ambig.h, wubu_user_sim.h | test_ambig.c -- the clarification oracle (AC-D10): a missing required slot or an unparseable value m |
| `brain/` | `test_amoeba` | 127 | stdio.h, string.h, wubu_amoeba.h, wubu_hive.h, wubu_moe2.h, wubu_prover.h | test_amoeba.c -- THE AMOEBA test: the diagnostic self-evolving model. Verifies the core claim: the m |
| `brain/` | `test_amoeba_shrink` | 230 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_grow.h, wubu_runtime_dims.h, w | test_amoeba_shrink.c -- AM01 gate: the symmetric amoeba. Proves the body can now RETRACT a pseudopod |
| `brain/` | `test_arena` | 81 | assert.h, stdio.h, stdlib.h, string.h, wubu_arena.h | test_arena.c -- doc 006 triple-DA (arena allocator for per-request + KV). P1 correctness: sub-arena  |
| `brain/` | `test_attn_gate` | 37 | math.h, stdio.h, stdlib.h, string.h, wubu_attn_gate.h | Test: wubu_attn_gate (doc 011) — gated attention sink-free forward. |
| `brain/` | `test_attn_kernels` | 39 | math.h, stdio.h, wubu_attn_kernels.h | test_attn_kernels.c -- P11/P15/O20 verification. |
| `brain/` | `test_attn_tune` | 40 | math.h, stdio.h, wubu_attn_tune.h | test_attn_tune.c -- L06/N19/O11 verification. |
| `brain/` | `test_attnres` | 36 | assert.h, math.h, stdio.h, wubu_attnres.h | Test: wubu_attnres (Round-4 #413 — Attention Residuals cross-layer). |
| `brain/` | `test_awq` | 54 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_awq.h | Test AWQ activation-aware weight quantization (doc B05). |
| `brain/` | `test_ax` | 110 | stdio.h, stdlib.h, string.h, wubu_codeexec.h, wubu_dgm.h, wubu_evolve.h, wubu_sa | test_ax.c -- AX01, AX04-AX08 verification. |
| `brain/` | `test_axi` | 113 | stdio.h, stdlib.h, string.h, wubu_codeexec.h, wubu_codesynth.h, wubu_dgm.h, wubu | test_axi.c -- AX09-AX12 self-improvement + verify + codesynth tests. |
| `brain/` | `test_backprop` | 305 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_backprop.h, wubu_runtime_dims. | test_backprop.c -- the REAL backward pass + REAL Muon, verified the DA way: analytic gradients vs FI |
| `brain/` | `test_backward` | 493 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_ssm.h |  |
| `brain/` | `test_bear_mlp` | 185 | math.h, stdio.h, string.h, wubu_bear_mlp.h | test_bear_mlp.c — tests for the BearRL CPU MLP/Kernels port. Tests: MLP forward, GAE computation, Ca |
| `brain/` | `test_bf16_gemv` | 53 | math.h, stdio.h, stdlib.h, wubu_bf16_gemv.h | test_bf16_gemv.c -- P09 verification: AVX512-BF16 GEMV matches F32 reference. |
| `brain/` | `test_bi` | 71 | math.h, stdio.h, stdlib.h, string.h, wubu_bi.h | test_bi.c -- Block Importance oracle (ShortGPT, arXiv:2403.03853). DA oracle gates: 1. rank() return |
| `brain/` | `test_blueprint` | 62 | stdio.h, string.h, wubu_blueprint.h, wubu_hive.h | test_blueprint.c -- the EXECUTABLE BLUEPRINT gate (Phase 6: WB01 — "mark WB01 wired only when a trai |
| `brain/` | `test_bonzi` | 140 | math.h, stdio.h, string.h, wubu_bonzi.h | test_bonzi.c -- Theme JE: Bonzi Buddy companion core (batch 1). |
| `brain/` | `test_bonzi2` | 98 | math.h, stdio.h, string.h, wubu_bonzi2.h | test_bonzi2.c -- Theme JE complete: the companion frontier. |
| `brain/` | `test_bridge` | 105 | math.h, stdio.h, wubu_bridge.h | test_bridge.c -- Theme JF: cross-resource bridges (first batch). |
| `brain/` | `test_bridge2` | 98 | math.h, stdio.h, string.h, wubu_bridge2.h | test_bridge2.c -- Theme JF complete: the cross-resource bridges frontier. |
| `brain/` | `test_btl3_lora` | 60 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_model_safetensors_bridge | test_btl3_lora.c -- verify BTL-3 two-step LoRA orchestration: base load (wubu_model_init_safetensors |
| `brain/` | `test_bwd_gqa` | 291 | math.h, stdio.h, stdlib.h, time.h, wubu_ssm.h |  |
| `brain/` | `test_bwd_gqa_debug` | 211 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_bwd_gqa_fix_eps` | 104 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_bwd_gqa_simple` | 163 | math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_bwd_gqa_standalone` | 170 | math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_bwd_gqa_t1` | 131 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_bwd_gqa_t2` | 118 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_bwd_gqa_trace` | 180 | math.h, stdio.h, stdlib.h, string.h | same as above |
| `brain/` | `test_bwd_model` | 157 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_cache_advice` | 33 | assert.h, stdio.h, wubu_cache_advice.h | Test: wubu_cache_advice (Round-2 #111 — ML-advice eviction). |
| `brain/` | `test_canvas` | 114 | math.h, stdio.h, stdlib.h, string.h, wubu_canvas.h | test_canvas.c -- research/066 gate: THE VHF CANVAS in C11. The user's prior art (VHF encoder, AUDIO/ |
| `brain/` | `test_capacity_wall` | 61 | math.h, stdio.h, wubu_capacity_wall.h | test_capacity_wall.c -- N12/N11 decode-I/O capacity + roofline predictor. Verifies: KV bytes formula |
| `brain/` | `test_capgate` | 72 | stdio.h, string.h, wubu_capgate.h, wubu_hive.h | test_capgate.c -- the CAPABILITY GATE gate (roadmap #4: "prevents unbounded agent spawning and makes |
| `brain/` | `test_capzero` | 63 | math.h, stdio.h, string.h, wubu_capzero.h | test_capzero.c -- AF02 (deny-by-default), AF03 (mem crypt), AF04 (NHI) verification. |
| `brain/` | `test_causal_symbolic` | 96 | math.h, stdio.h, stdlib.h, string.h, wubu_causal.h, wubu_symbolic.h | test_causal_symbolic.c -- AW01-AW10 causal + neuro-symbolic verification. |
| `brain/` | `test_chat_template` | 124 | stdio.h, string.h, wubu_tokenizer.h |  |
| `brain/` | `test_chunked_prefill` | 65 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_chunked_prefill.h | Test: chunked prefill + disaggregated PD (doc D03/D04). |
| `brain/` | `test_chunked_ssm` | 123 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_chunked_ssm.c — verify chunked SSM matches sequential SSM |
| `brain/` | `test_chunked_ssm_error` | 93 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_chunked_ssm_error.c — Compare chunked vs sequential SSM recurrence. Build: make test_chunked_ss |
| `brain/` | `test_chunked_vs_seq` | 93 | math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_chunked_vs_seq.c — Debug chunked vs sequential SSM discrepancy. Tests at CS=1 where chunked sho |
| `brain/` | `test_cla` | 32 | assert.h, stdio.h, wubu_cla.h | Test: wubu_cla (Round-3 #223 — Cross-Layer Attention KV sharing). |
| `brain/` | `test_codec` | 112 | math.h, stdio.h, stdlib.h, string.h, wubu_codec.h, wubu_imgenc.h | test_codec.c -- research/067 gate: THE UNIVERSAL CODEC. The user's prior art: "I did a lot of work t |
| `brain/` | `test_col_dequant` | 46 | ggml-common.h, ggml-quants.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | Quick test: dequantize IQ2_XXS column 0 from model data. |
| `brain/` | `test_colonel` | 86 | stdio.h, string.h, wubu_colonel.h | test_colonel.c -- the LIVE COLONEL bridge gate (the user directive #5: "the Brain requests Body acti |
| `brain/` | `test_colonel_effect` | 105 | stdio.h, stdlib.h, string.h, wubu_agentic_os.h, wubu_colonel.h, wubu_hive.h, wub | test_colonel_effect.c — the COLONEL END-TO-END EFFECT PATH (AN47 #4: "prove specialists can do somet |
| `brain/` | `test_colony` | 150 | stdio.h, stdlib.h, string.h, wubu_amoeba.h, wubu_blueprint.h, wubu_contracts.h,  | test_colony.c — THE RELEASE GATE (AN47 #7: "make test_colony — harness floor + zero contract violati |
| `brain/` | `test_compress` | 104 | math.h, stdio.h, string.h, wubu_compress.h | test_compress.c -- Theme IY complete: the context-compression frontier. |
| `brain/` | `test_compress2` | 104 | math.h, stdio.h, string.h, wubu_compress2.h | test_compress2.c -- Theme IZ complete: the context-compression frontier. |
| `brain/` | `test_continual` | 116 | math.h, stdio.h, wubu_distill.h, wubu_ewc.h, wubu_experibuf.h, wubu_taskbd.h | test_continual.c -- BB01-BB04 verification. |
| `brain/` | `test_continuous_batching` | 137 | assert.h, stdio.h, stdlib.h, string.h, wubu_continuous_batching.h, wubu_schedule | Test: continuous (iteration-level) batching (doc 007). The scheduler runs 1 token per step for up to |
| `brain/` | `test_contracts` | 87 | math.h, stdio.h, string.h, wubu_contracts.h, wubu_hive.h | test_contracts.c -- the RUNTIME CONTRACTS gate (roadmap #5: "keeps self-modification bounded by some |
| `brain/` | `test_conv` | 72 | math.h, stdio.h, stdlib.h, string.h | Verify conv1d weight indexing against known reference pattern. |
| `brain/` | `test_cpu_timing` | 252 | cpu_timing.h, hedged_spec.h, math.h, mman.h, pthread.h, stdio.h, stdlib.h | test_cpu_timing.c — Test cycle-accurate timing utilities Build: gcc -O2 -I include -o test_cpu_timin |
| `brain/` | `test_credit_dbstate` | 67 | stdio.h, wubu_credit_sft.h, wubu_dbstate.h | test_credit_dbstate.c -- the credit-assignment SFT mask + the DB-state reward verifier (the Orchard  |
| `brain/` | `test_cross_attn` | 86 | math.h, stdio.h, stdlib.h, string.h, wubu_cross_attn.h | test_cross_attn.c — Cross-attention (multimodal fusion) correctness test Validates F32 and Q8 cross- |
| `brain/` | `test_ctx_manage` | 37 | math.h, stdio.h, wubu_ctx_manage.h | test_ctx_manage.c -- L16/N07/N14 verification. |
| `brain/` | `test_cuda_graph` | 28 | assert.h, stdio.h, wubu_cuda_graph.h | Test: wubu_cuda_graph (Area E — CUDA-graph decode planning/update). Exercises the CPU-side planning  |
| `brain/` | `test_cuda_kernels` | 378 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_db_cross` | 49 | math.h, stdio.h, wubu_db_cross.h | test_db_cross.c -- O14/O18/O19 verification. |
| `brain/` | `test_debt` | 104 | math.h, stdio.h, string.h, wubu_der.h, wubu_reverify.h, wubu_si.h | test_debt.c -- the research-debt close: Synaptic Intelligence (BB06), Dark Experience Replay (BB07), |
| `brain/` | `test_decode_path` | 337 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | test_decode_path.c — Full decode path verification Compares prefill(T=7, token 6) vs prefill(T=6) +  |
| `brain/` | `test_decoder_paths` | 44 | stdio.h, string.h | test_decoder_paths.c -- regression harness comparing Qwen/VLM decode variants. Build: make test_deco |
| `brain/` | `test_dedup` | 50 | stdio.h, stdlib.h, string.h, wubu_dedup.h | test_dedup.c -- the rolling-hash duplicate-window scanner: an embedded exact duplicate must be found |
| `brain/` | `test_delta_net` | 50 | assert.h, math.h, stdio.h, stdlib.h, wubu_delta_net.h | Test: wubu_delta_net (Round-3 #202 — Gated-DeltaNet recurrence). Verifies the recurrent state update |
| `brain/` | `test_deltanet` | 91 | math.h, stdio.h, stdlib.h, string.h, wubu_deltanet.h | test_deltanet.c -- the Gated-DeltaNet test (research 008, phase 5). Verifies: 1. the delta rule lear |
| `brain/` | `test_dense_ffn` | 65 | math.h, stdio.h, stdlib.h, string.h, wubu_dense_ffn.h | test_dense_ffn.c — dense SwiGLU FFN tests (C11, no deps). |
| `brain/` | `test_density_planner` | 191 | math.h, stdio.h, stdlib.h, string.h, wubu_density_planner.h, wubu_kv_shrink.h, w | test_density_planner.c — tests for density planning (AN23-core) Verifies: T1: high-density files are |
| `brain/` | `test_dequant` | 51 | gguf_reader.h, stdio.h, string.h, wubu_model.h | test_dequant: Dump first 32 floats from a specific tensor to compare against llama.cpp reference. Us |
| `brain/` | `test_diag` | 274 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_diag.h, wubu_hive.h, wubu_runt | test_diag.c -- THE HIVE DIAGNOSTIC SYSTEM test (research/056, AN08). The 6 DA oracles from the desig |
| `brain/` | `test_diagnosis` | 177 | stdio.h, stdlib.h, string.h, wubu_amoeba.h, wubu_diagnosis.h, wubu_hive.h, wubu_ | test_diagnosis.c -- the CLOSED CONTROL SYSTEM gate (the user directive: "the colony can notice its o |
| `brain/` | `test_disagg_prefill_decode` | 60 | assert.h, stdio.h, stdlib.h, string.h, wubu_continuous_batching.h | Test: D03 disaggregated prefill/decode (doc 007). Verifies: (1) prefill engine and decode engine run |
| `brain/` | `test_dn2` | 45 | math.h, stdio.h, wubu_dn2.h | test_dn2.c -- S02/T04 verification. |
| `brain/` | `test_dsa` | 243 | math.h, stdint.h, stdio.h, stdlib.h, wubu_dsa.h | test_dsa.c -- DSA indexer verification (coarse-to-fine block attention). Synthetic KV: 16 blocks x 8 |
| `brain/` | `test_dsv4` | 182 | math.h, stdio.h, stdlib.h, string.h, wubu_dsa.h, wubu_dsv4.h, wubu_hashrouter.h, | test_dsv4.c — tests for DeepSeek-V4 Flash hybrid layer port. Covers: hyper-connection residual, sink |
| `brain/` | `test_dsv4_layer` | 121 | stdio.h, stdlib.h, string.h, wubu_dsv4_layer.h | test_dsv4_layer.c — tests for the DSV4 MLA attention bridge (wubu_dsv4_layer). Covers: create/free,  |
| `brain/` | `test_eagle` | 80 | stdio.h, wubu_eagle.h | test_eagle.c — Test EAGLE self-draft speculative decoding. Structural/algorithmic tests (no model fi |
| `brain/` | `test_early_exit` | 44 | assert.h, math.h, stdio.h, wubu_early_exit.h | Test: J03 early-exit + self-speculative verify. Verifies: (1) early-exit triggers on low hidden delt |
| `brain/` | `test_ecosystem` | 198 | math.h, stdio.h, stdlib.h, string.h, wubu_ecosystem.h | test_ecosystem.c -- THEORY/10 validation gate: the Ecosystem of Spheres. Proves the physics router ( |
| `brain/` | `test_ecs` | 54 | assert.h, stdio.h, stdlib.h, string.h, wubu_ecs.h | Test: ECS component store (doc C06). register / snapshot / restore. |
| `brain/` | `test_ee` | 121 | math.h, stdio.h, wubu_cegis.h, wubu_invariant.h, wubu_prover.h, wubu_sindy.h, wu | test_ee.c -- EE01-EE06 verification. |
| `brain/` | `test_emb_parity` | 93 | gguf_reader.h, llama.h, math.h, stdio.h, stdlib.h, string.h | test_emb_parity.c — Compare token embedding from our gguf_reader vs llama.cpp. Build: gcc -O2 -Iincl |
| `brain/` | `test_enc_fs` | 127 | math.h, stdio.h, stdlib.h, string.h, wubu_enc_fs.h, wubu_encoder.h, wubu_encoder | test_enc_fs.c -- AN16 G3 gate: THE ENCODER IS A MOUNT. "We are already lifting heavy work." The enco |
| `brain/` | `test_enc_h3` | 149 | math.h, stdio.h, stdlib.h, string.h, wubu_enc_h3.h, wubu_fp8.h, wubu_nvfp4.h, wu | test_enc_h3.c — tests for MiniMax H3 encoder NVFP4 requant + ConvRot. Covers: unrotate (Hadamard sel |
| `brain/` | `test_encoder` | 155 | math.h, stdio.h, stdlib.h, string.h, wubu_encoder.h | test_encoder.c -- WB06/AN06 gate: THE ENCODER SLOT. Proves the modality-agnostic seam: encoders regi |
| `brain/` | `test_encoder_ours` | 127 | math.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubu_encoder.h, wubu_encoder_ | test_encoder_ours.c -- WB06 gate: OUR OWN encoder (made, not imported). The single-encoder modality- |
| `brain/` | `test_energy` | 100 | math.h, stdio.h, wubu_energy.h | test_energy.c -- Theme IJ: energy-aware inference (wubu_energy). Triple-DA: edge cases + monotonicit |
| `brain/` | `test_english_prompt` | 74 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_tokenizer |  |
| `brain/` | `test_epcap` | 34 | stdio.h, wubu_epcap.h | test_epcap.c -- the episode-length cap: the longest fitting prefix, the full-fits flag, the zero-cos |
| `brain/` | `test_eval` | 36 | stdio.h, stdlib.h, wubu_eval.h | test_eval.c -- the agentic eval harness: the DB-state verification composed with the pass@k and the  |
| `brain/` | `test_eval_qat` | 56 | math.h, stdio.h, wubu_eval_qat.h | test_eval_qat.c -- Z01-Z05 + AA01-AA04 verification. |
| `brain/` | `test_events` | 73 | stdio.h, stdlib.h, string.h, wubu_events.h | test_events.c — the COLONY RUN RECORDER gate (post-AN54 A1). Asserts: 1. events append as JSONL line |
| `brain/` | `test_evict2026` | 128 | math.h, stdio.h, string.h, wubu_evict2026.h | test_evict2026.c -- Theme IO: 2026 eviction frontier mechanisms. |
| `brain/` | `test_evict2026b` | 109 | math.h, stdio.h, string.h, wubu_evict2026b.h | test_evict2026b.c -- Theme IO batch 2: the frontier infrastructure. |
| `brain/` | `test_evict2026c` | 104 | math.h, stdio.h, string.h, wubu_evict2026c.h | test_evict2026c.c -- Theme IO remainder: the final KV-eviction frontier. |
| `brain/` | `test_expert_choice` | 53 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_expert_choice.h | Test: MoE expert choice routing (doc E05). |
| `brain/` | `test_expert_dequant` | 47 | ggml-common.h, ggml-quants.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | Test dequant of full expert 64 gate weights. |
| `brain/` | `test_f32_all` | 74 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_f32_fallback` | 141 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_f32_proj` | 62 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_fake_body` | 126 | stdio.h, stdlib.h, string.h, wubu_agentic_os.h, wubu_colonel.h, wubu_hive.h, wub | test_fake_body.c — the FAKE-BODY COLONEL INTEGRATION TEST (post- AN54 F33: "Colonel integration test |
| `brain/` | `test_fast_attn` | 154 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_fast_attn.h | test_fast_attn.c — Correctness + speed benchmark for wubu_fast_attn. Verifies numerical correctness  |
| `brain/` | `test_fast_attn_q8` | 115 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_fast_attn.h | test_fast_attn_q8.c — Q8 KV cache fast decode benchmark + correctness |
| `brain/` | `test_ff` | 158 | math.h, stdio.h, wubu_acq.h, wubu_active.h, wubu_bandit.h, wubu_bo.h, wubu_gp.h, | test_ff.c -- FF01-FF07 verification. |
| `brain/` | `test_flash_prefill` | 126 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_flash_prefill.h | Test: FlashAttention-style fused prefill (doc H01). Verifies that the tiled online-softmax attention |
| `brain/` | `test_flashdecode` | 86 | assert.h, math.h, stdio.h, stdlib.h, wubu_flashdecode.h | test_flashdecode.c -- doc 015 triple-DA. P1 correctness: FlashDecoding output == reference full-soft |
| `brain/` | `test_fmt` | 64 | stdio.h, string.h, wubu_fmt.h | test_fmt.c -- the format-constraint reward checker (the Atropos Answer-Format env): the binary forma |
| `brain/` | `test_fold_sincos8` | 69 | math.h, stdio.h, time.h, wubu_foldmath.h | test_fold_sincos8.c — the AVX2 8-wide folded sin/cos gate (ported from wuburvc, the sibling repo's C |
| `brain/` | `test_foldmath` | 121 | math.h, stdio.h, stdlib.h, time.h, wubu_foldmath.h | test_foldmath.c -- the folded sin/cos (Silas Lock's algorithm via Kaze Emanuar's "The Folded Polynom |
| `brain/` | `test_fp4_dequant` | 126 | math.h, stdio.h, string.h, wubu_dequant_fp4.h | test_fp4_dequant.c — verify MXFP4 and NVFP4 dequantization against known test vectors.  Build: gcc - |
| `brain/` | `test_fp8` | 66 | assert.h, math.h, stdio.h, stdlib.h, wubu_fp8.h | Test: FP8 E4M3/E5M2 emulation (doc B07). Round-trip + dot accuracy. |
| `brain/` | `test_freeenergy` | 69 | math.h, stdio.h, wubu_freeenergy.h | test_freeenergy.c -- Theme IN: predictive coding / free energy. |
| `brain/` | `test_fs_trainer` | 231 | math.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu.h, wubu_fs_dataset.h | test_fs_trainer.c -- AN21/AN22 Phase 8 gate: THE MODEL TRAINS ON THE FILESYSTEM. The user's directiv |
| `brain/` | `test_full_model` | 92 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Full model forward (40 layers, no MoE) with fresh embedding, compare vs ref |
| `brain/` | `test_full_moe` | 58 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_core_dumps.h, wubu_mode |  |
| `brain/` | `test_full_pipeline` | 637 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_moe.h, wubu_ssm | test_full_pipeline.c — Compare full 40-layer prefill vs decode for last token Tests: prefill T=7 → l |
| `brain/` | `test_fused` | 182 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, gguf_reader.h, math.h, stdio.h, std | Test: Fused SSM Forward (parallel scan) vs step-by-step GPU |
| `brain/` | `test_fused_vs_old` | 208 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, gguf_reader.h, math.h, stdio.h, std | Compare: Fused SSM Forward (parallel scan) vs step-by-step GPU (old delta_net_step) |
| `brain/` | `test_fuzz` | 79 | math.h, stdio.h, string.h, wubu_fuzz.h | test_fuzz.c -- Theme IX batch 1: the robustness/fuzzing frontier. |
| `brain/` | `test_fuzz2` | 65 | math.h, stdio.h, string.h, wubu_fuzz2.h | test_fuzz2.c -- Theme IX complete: the robustness frontier. |
| `brain/` | `test_fwd_simple` | 76 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_tokenizer |  |
| `brain/` | `test_gaad` | 217 | math.h, stdio.h, wubu_gaad.h | test_gaad.c — tests for the GAAD Golden Aspect Adaptive Decomposition. Port of WuBuOS/src/kernel/wub |
| `brain/` | `test_gemma4` | 145 | gguf_reader.h, gpu_gemma4.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_g | test_gemma4.c — Gemma 4 12B dual-head-dim ISWA test. CPU only: make test_gemma4 GPU: make test_gemma |
| `brain/` | `test_gemv_dispatch` | 89 | math.h, stdio.h, stdlib.h, string.h, wubu_gemm.h, wubu_gemv_dispatch.h, wubu_pre | test_gemv_dispatch.c — the PER-FAMILY GEMV DISPATCH gate (AN47 #2: "diagnose-driven precision change |
| `brain/` | `test_gemv_equivalence` | 157 | float.h, gguf_reader.h, math.h, stdio.h, stdlib.h, wubu_smoothquant.h | tools/test_gemv_equivalence.c -- Bounded kernel equivalence test (doc 009). Exhaustively test quanti |
| `brain/` | `test_gemv_int4` | 108 | math.h, stdio.h, stdlib.h, string.h, wubu_gemm.h, wubu_gemv_tune.h, wubu_safeten | test_gemv_int4.c -- B03: int4 weight GEMV vs fp32 oracle (triple-DA). Pass 1 correctness: cosine(int |
| `brain/` | `test_gemv_tune` | 112 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_gemm.h, wubu_gemv_tune.h, wu | test_gemv_tune.c -- Roofline-driven GEMV: tiled fp32 + int8 variants must match the scalar oracle (c |
| `brain/` | `test_generate_spec` | 124 | math.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_generate.h, wubu_model.h, wu | test_generate_spec.c -- doc 018 / K01 triple-DA (equivalence proof). P1 correctness (EQUIVALENCE): g |
| `brain/` | `test_gg` | 153 | math.h, stdio.h, wubu_actor_critic.h, wubu_dqn.h, wubu_policy.h, wubu_ppo.h, wub | test_gg.c -- GG01-GG07 verification. |
| `brain/` | `test_gguf_dequant` | 90 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_gguf_load` | 141 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | test_gguf_load.c — DeepSeek-V4 Config-I 3-split load gate Usage: test_gguf_load <path-to-part-00001. |
| `brain/` | `test_gguf_tq` | 108 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | test_gguf_tq.c — TurboQuant loader unit test (Q2_0 remap + TQ3_1S dequant) Builds a synthetic GGUF v |
| `brain/` | `test_gptq` | 76 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_gptq.h | Test GPTQ second-order weight quantization (doc B06). |
| `brain/` | `test_gpu` | 638 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, gguf_reader.h, math.h, stdio.h, std | Phase 2.5 GPU Test Loads 1 SSM layer from GGUF, runs forward pass on GPU using cuBLAS + CUDA kernels |
| `brain/` | `test_gpu_attn` | 423 | gpu_wubu.h, math.h, stdio.h, stdlib.h, string.h, time.h | test_gpu_attn.c -- the GPU attention tile vs the CPU reference (the bp's exact hybrid GQA math): the |
| `brain/` | `test_gpu_dequant` | 97 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_gpu_dequant2` | 101 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_gpu_dequant3` | 101 | gguf_reader.h, math.h, stdio.h, string.h |  |
| `brain/` | `test_gpu_iq1m` | 130 | gguf_reader.h, gpu_quant_matmul.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_gpu_iq1m_batched` | 129 | gguf_reader.h, gpu_quant_matmul.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_gpu_layers` | 327 | bench.h, cublas_v2.h, cuda_runtime.h, math.h, stdio.h, stdlib.h, string.h, wubu_ | test_gpu_layers.c — Test GPU-ified SSM and GQA projection matmuls Verifies that cuBLAS-backed matmul |
| `brain/` | `test_gpu_ns5` | 110 | gpu_wubu.h, math.h, stdio.h, stdlib.h, string.h, time.h | test_gpu_ns5.c -- the GPU NS5 vs the CPU reference on square, wide, and tall matrices. Caught the sq |
| `brain/` | `test_gpu_output_proj` | 103 | gguf_reader.h, gpu_output_proj.h, stdio.h, stdlib.h, string.h, time.h, wubu_mode | test_gpu_output_proj.c — Standalone test: compare CPU vs GPU output proj. Loads model, runs forward  |
| `brain/` | `test_gpu_poincare` | 133 | bench.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_gpu_poincare.c — Verify GPU Poincaré SSM forward produces valid output |
| `brain/` | `test_gqa_only` | 117 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_gravity` | 216 | math.h, stdio.h, string.h, wubu_gravity.h | test_gravity.c -- THE GRAVITY FIELD test (research/060, AN12). The DA oracles: 1. stable orbit: a ce |
| `brain/` | `test_grow` | 163 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_backprop.h, wubu_grow.h, wubu_ | test_grow.c -- the model-growth operator against the DA oracle: the FUNCTION-PRESERVING property (th |
| `brain/` | `test_grow_free` | 71 | stdio.h, stdlib.h, string.h, wubu.h, wubu_backprop.h, wubu_grow.h, wubu_train.h | alloc model blocks like wubu_train_cli does |
| `brain/` | `test_gyrate` | 74 | math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h | test_gyrate.c — Validate optimized gyration against reference 3-add version. |
| `brain/` | `test_h3_norm` | 131 | math.h, stdio.h, stdlib.h, string.h, wubu_dequant_nf4.h, wubu_h3_norm.h | test_h3_norm.c — runtime tests for the H3 hyperbolic normalization kernel. Verifies: - F32 path: H3  |
| `brain/` | `test_harness` | 67 | stdio.h, string.h, wubu_harness.h, wubu_hive.h | test_harness.c -- the TASK HARNESS gate (Phase 3: Pillars 11+20 — "the colony cannot pass by short-b |
| `brain/` | `test_harness_file` | 71 | math.h, stdio.h, stdlib.h, string.h, wubu_harness_file.h, wubu_hive.h | test_harness_file.c — the LIVE-FILE HARNESS gate (AN47 #3: "real user-space tasks so traj cells and  |
| `brain/` | `test_hashrouter` | 131 | stdio.h, stdlib.h, string.h, wubu_hashrouter.h | test_hashrouter.c -- standalone tests for wubu_hashrouter. Covers: (1) determinism, (2) top-k distin |
| `brain/` | `test_hh` | 156 | math.h, stdio.h, string.h, wubu_contbatch.h, wubu_medusa.h, wubu_moeroute.h, wub | test_hh.c -- HH01-HH07 verification. |
| `brain/` | `test_hive` | 113 | stdio.h, stdlib.h, string.h, wubu_hive.h | test_hive.c -- THE HIVE test: the AGI's memory structure (WuBu). Verifies the properties the user's  |
| `brain/` | `test_hive_merge` | 111 | stdio.h, string.h, wubu_diagnosis.h, wubu_hive_merge.h, wubu_priority_store.h | test_hive_merge.c — the MULTI-CHECKPOINT LINEAGE MERGE gate (AN47 #6: "the colony can federate witho |
| `brain/` | `test_hopfield` | 92 | math.h, stdio.h, wubu_hopfield.h | test_hopfield.c -- Theme IL: modern Hopfield / associative memory. |
| `brain/` | `test_hopfield2` | 102 | math.h, stdio.h, string.h, wubu_hopfield2.h | test_hopfield2.c -- Theme IP: Hopfield frontier extensions. |
| `brain/` | `test_hopfield3` | 129 | math.h, stdio.h, string.h, wubu_hopfield3.h | test_hopfield3.c -- Theme IP batch 2: the memory-systems engineering. |
| `brain/` | `test_hopfield4` | 190 | math.h, stdio.h, string.h, wubu_hopfield3.h, wubu_hopfield4.h | test_hopfield4.c -- Theme IP ABSOLUTE final: completes the 26 remaining gaps. |
| `brain/` | `test_host_dequant` | 121 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | Copy of the host dequant from gpu_gemma4_forward.cu |
| `brain/` | `test_hugepage` | 48 | stdio.h, string.h, wubu_hugepage.h | test_hugepage.c -- O02 Hugepage KV arena verification. Verifies: alloc returns usable writable memor |
| `brain/` | `test_hybrid` | 70 | math.h, stdio.h, string.h, wubu_hybrid.h | test_hybrid.c -- Theme JA complete: the hybrid-attention frontier. |
| `brain/` | `test_hyper` | 115 | math.h, stdio.h, stdlib.h, wubu_hyper.h | test_hyper.c -- verify the LEAN-PROVEN hyperbolic properties in C11. Each test corresponds to a form |
| `brain/` | `test_hyperbolic_output_proj` | 339 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_hyperbolic_output_proj. | test_hyperbolic_output_proj.c — Validate hyperbolic output projection. Tests: 1. Forward output is f |
| `brain/` | `test_indexing` | 72 | math.h, stdio.h, stdlib.h, string.h | Verify which weight indexing convention is correct. Tests against known reference: for simple 2x2 ma |
| `brain/` | `test_ingest_all` | 334 | math.h, stdio.h, stdlib.h, string.h, wubu_imgenc.h, wubu_jpeg.h, wubu_ooxml.h, w | test_ingest_all.c -- THE ALL-FORMATS GATE: images, video, office. The user's directive: "we need to  |
| `brain/` | `test_integrate` | 64 | math.h, stdio.h, wubu_integrate.h | test_integrate.c -- runtime policy composer verification (option c: exploit gaps). |
| `brain/` | `test_iq1m` | 59 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `test_iq2_block` | 112 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_iq2_dequant` | 273 | gguf_reader.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | test_iq2_dequant.c — IQ2_XXS dequant verification. Loads blk.0.ffn_gate_exps.weight from a GGUF mode |
| `brain/` | `test_iq2_xxs_dot` | 146 | gguf_reader.h, iq2xxs_grid_data.inc, math.h, stdio.h, stdlib.h, string.h, wubu_f | test_iq2_xxs_dot.c — Compare CPU vs GPU IQ2_XXS dot products for a single block. Build with GPU supp |
| `brain/` | `test_iq_dot` | 97 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_kat_decode_bank` | 78 | math.h, stdio.h, stdlib.h, string.h, wubu_ssd_moe.h | test_kat_decode_bank.c -- verify the ds4-ssd MoE decode bank pages real KAT-Coder 256-expert weights |
| `brain/` | `test_kda` | 36 | assert.h, math.h, stdio.h, stdlib.h, wubu_kda.h | Test: wubu_kda (Round-4 #401 — Kimi Delta Attention channel-wise decay). |
| `brain/` | `test_kereq` | 39 | assert.h, stdio.h, wubu_kereq.h | Test: wubu_kereq (Round-2 #121 — genuine symbolic equivalence prover). Tests the real abstract-inter |
| `brain/` | `test_kernel_dispatch` | 236 | math.h, stdio.h, stdlib.h, string.h, wubu_activations.h, wubu_kernel.h | test_kernel_dispatch.c — hardware-agnostic kernel dispatch tests |
| `brain/` | `test_kivi_roundtrip` | 97 | math.h, stdio.h, stdlib.h, string.h, wubu_kvcache_quant.h | test_kivi_roundtrip.c Sigma/Diagnostic: KIVI quant/dequant round-trip cosine >= 0.9999 on real Colon |
| `brain/` | `test_kv2026` | 50 | math.h, stdio.h, wubu_kv2026.h | test_kv2026.c -- Q02/Q03/Q07/Q09/Q10 verification. |
| `brain/` | `test_kv2026b` | 47 | math.h, stdio.h, wubu_kv2026b.h | test_kv2026b.c -- Q01/Q04/Q05/Q06 verification. |
| `brain/` | `test_kv2026c` | 45 | math.h, stdio.h, wubu_kv2026c.h | test_kv2026c.c -- Q11/Q19/R04/R05 verification. |
| `brain/` | `test_kv_adaptive` | 47 | assert.h, math.h, stdio.h, stdlib.h, wubu_kv_adaptive.h | Test: Ecco entropy-aware adaptive KV compression (doc 001). |
| `brain/` | `test_kv_budget` | 44 | math.h, stdio.h, wubu_kv_budget.h | test_kv_budget.c -- L18/L19/N03/N17 verification. |
| `brain/` | `test_kv_cache` | 580 | cuda_kernels.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu | test_kv_cache.c — KV cache for GQA attention Design: - K/V cached per GQA layer (post-RMSNorm K, raw |
| `brain/` | `test_kv_cache_integration` | 96 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_kv_select.h, wubu_model.h | Test: KV-cache quantization integration via kv_cache_read/write_head (the engine's real attention pa |
| `brain/` | `test_kv_cacheline` | 37 | assert.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_kv_cacheline.h | Test: cache-line-aligned KV page storage (doc C03/I03). |
| `brain/` | `test_kv_compress` | 48 | math.h, stdio.h, wubu_kv_compress.h | test_kv_compress.c -- L07/L09 verification. |
| `brain/` | `test_kv_diag` | 176 | stdio.h, string.h, wubu_grow_kv.h, wubu_kv_coherence_diag.h, wubu_kv_embedding.h | test_kv_diag.c — tests for the KV coherence diagnose cycle (Phase 7) Verifies the full metabolism lo |
| `brain/` | `test_kv_embedding` | 406 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_coherence_reward.h, wubu_fs_ | test_kv_embedding.c — tests for the KV-FS embedding bridge Verifies: T1: create + free (round-trip) |
| `brain/` | `test_kv_evict` | 55 | assert.h, stdio.h, stdlib.h, wubu_kv_evict.h | Test: A07b priority-based KV eviction (importance + LRU hybrid). Verifies: (1) important-but-stale b |
| `brain/` | `test_kv_evict_h2o` | 58 | stdio.h, stdlib.h, wubu_kv_evict.h | test_kv_evict_h2o.c -- L03 H2O heavy-hitter eviction verification. Verifies: cumulative attention ma |
| `brain/` | `test_kv_hierarchy` | 211 | math.h, stdio.h, string.h, wubu_kv_hierarchy.h, wubu_kv_semantic_router.h, wubu_ | test_kv_hierarchy.c — tests for hyperbolic KV namespace addressing Verifies: T1: root maps to center |
| `brain/` | `test_kv_shell` | 126 | stdio.h, stdlib.h, string.h, wubu_kv_shell.h, wubu_kvfs.h | test_kv_shell.c — tests for KV shell command routing (Phase 11) Verifies: T1: ls /kv/in/ lists encod |
| `brain/` | `test_kv_shield` | 48 | math.h, stdio.h, string.h, wubu_kv_shield.h | test_kv_shield.c -- L15 KVShield adversarial-robustness verification. |
| `brain/` | `test_kv_styx` | 46 | assert.h, stdio.h, stdlib.h, string.h, wubu_kv_styx.h | empty snapshot |
| `brain/` | `test_kv_tier` | 33 | assert.h, stdio.h, string.h, unistd.h, wubu_kv_tier.h | Write then read back |
| `brain/` | `test_kv_tier_evict` | 142 | assert.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_kv_tier.h | test_kv_tier_evict.c — Triple-DA test for KV tier eviction + cold storage. Tests: P1 correctness: bl |
| `brain/` | `test_kv_tiering` | 132 | stdio.h, stdlib.h, string.h, wubu_kv_embedding.h, wubu_kv_tiering.h, wubu_kvfs.h | test_kv_tiering.c — tests for KV precision tiering (Phase 9) Verifies: T1: all files start at F32 ti |
| `brain/` | `test_kv_transfer` | 43 | assert.h, stdio.h, stdlib.h, string.h, wubu_kv_transfer.h | Test: D05 localhost KV transfer layer (doc 007/002). Verifies: KV blocks written by a "prefill insta |
| `brain/` | `test_kvcache_quant` | 88 | assert.h, math.h, stdio.h, stdlib.h, wubu_kvcache_quant.h | Test: wubu_kvcache_quant (Roofline/DB/KIVI convergence -- KV quant). Pass 1 (correctness): q8_0 + KI |
| `brain/` | `test_kvfs` | 379 | assert.h, stdio.h, stdlib.h, string.h, time.h, wubu_kvfs.h | tools/test_kvfs.c — triple-DA test for wubu_kvfs P1 (correctness): create, mount, lookup, read, writ |
| `brain/` | `test_kvquant` | 39 | assert.h, math.h, stdio.h, stdlib.h, wubu_kvquant.h | Test: wubu_kvquant (Area B — KV-cache quantization). |
| `brain/` | `test_kvvq` | 111 | assert.h, math.h, stdio.h, stdlib.h, wubu_kvvq.h, wubu_rotate.h | test_kvvq.c -- doc 014 triple-DA (residual subvector VQ). P1 correctness: pack/unpack of indices is  |
| `brain/` | `test_latentmoe` | 45 | assert.h, math.h, stdio.h, stdlib.h, wubu_latentmoe.h | Test: wubu_latentmoe (Round-4 #423 — Stable LatentMoE 896/16 + shared). |
| `brain/` | `test_layer_debug` | 132 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_tokenizer |  |
| `brain/` | `test_layer_parity` | 188 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | test_layer_parity.c — Layer-by-layer hidden state comparison vs llama.cpp. Load model, run 1 token t |
| `brain/` | `test_layer_skip` | 73 | math.h, stdio.h, stdlib.h, string.h, wubu_layer_skip.h | Test MoD layer skip (doc 017). Tests two paths: 1. Gate < 0.5 → skip: y = x (passthrough, no F(x) co |
| `brain/` | `test_lfm` | 149 | math.h, stdio.h, stdlib.h, string.h, wubu_lfm.h | test_lfm.c — tests for LFM2.5-2.6B hybrid attention layer. Covers: create/free, linear attention (De |
| `brain/` | `test_lfm2_load_gguf` | 41 | lfm2_load.h, stdio.h, stdlib.h, string.h | test_lfm2_load_gguf.c — load the REAL LFM2.5-2.6B GGUF with our modified loader ("make it load whate |
| `brain/` | `test_linattn` | 102 | math.h, stdio.h, string.h, wubu_linattn.h | test_linattn.c -- Theme IU batch 1: the linear-attention frontier. |
| `brain/` | `test_linattn2` | 104 | math.h, stdio.h, string.h, wubu_linattn2.h | test_linattn2.c -- Theme IU complete: the linear-attention frontier. |
| `brain/` | `test_lineage` | 77 | stdio.h, string.h, wubu_lineage.h | test_lineage.c -- the LINEAGE + EXTINCTION gate (roadmap #1: evolutionary pressure with memory, not  |
| `brain/` | `test_linear_attn` | 44 | math.h, stdio.h, wubu_linear_attn.h | test_linear_attn.c -- S01/S03/S04/S05/S07 verification. |
| `brain/` | `test_lm_infinite` | 38 | math.h, stdio.h, wubu_lm_infinite.h | test_lm_infinite.c -- L13/O07/N20 verification. |
| `brain/` | `test_lmcache` | 79 | assert.h, math.h, stat.h, stdio.h, stdlib.h, string.h, wubu_lmcache.h | Test: LMCache prefix+PD KV persistence (doc A06). |
| `brain/` | `test_lookahead` | 35 | math.h, stdio.h, wubu_lookahead.h | test_lookahead.c -- M06 verification. |
| `brain/` | `test_loopguard_planediv` | 75 | stdio.h, string.h, wubu_loopguard.h, wubu_planediv.h | test_loopguard_planediv.c -- AG01/AG02/AG03/AG05/AG06/AG08 verification. |
| `brain/` | `test_lora` | 43 | math.h, stdio.h, wubu_lora.h | test_lora.c -- verify rank-r LoRA merge + forward against a reference matmul. |
| `brain/` | `test_lruk` | 38 | math.h, stdio.h, wubu_lruk.h | test_lruk.c -- O01 LRU-k verification. |
| `brain/` | `test_masked_ce` | 71 | math.h, stdio.h, stdlib.h, string.h, wubu_masked_ce.h | test_masked_ce.c -- the masked CE against the FD oracle: the masked positions get ZERO gradient, the |
| `brain/` | `test_medusa` | 70 | assert.h, stdio.h, stdlib.h, string.h, wubu_spec_decode.h | Test: MEDUSA guess heads (doc 012). MEDusa uses multiple independent "guess heads" at different laye |
| `brain/` | `test_mega` | 28 | assert.h, math.h, stdio.h, wubu_mega.h | Test: wubu_mega (Round-3 #233 — MEGA EMA+gated step). |
| `brain/` | `test_megakernel` | 112 | math.h, stdio.h, stdlib.h, string.h, wubu_megakernel.h | test_megakernel.c — tests for Photon 2.0 fused decode megakernel. Covers: create/free, fused decode  |
| `brain/` | `test_metacog` | 125 | math.h, stdio.h, string.h, wubu_metacog.h | test_metacog.c -- Theme JD: AGI metacognition (first batch). |
| `brain/` | `test_metadiag` | 130 | stdio.h, string.h, wubu_hive.h, wubu_metadiag.h | test_metadiag.c -- the DUAL-TIMESCALE DIAGNOSE gate (roadmap #3: continuous self-monitoring, not rea |
| `brain/` | `test_metagame2` | 88 | math.h, stdio.h, string.h, wubu_metagame2.h | test_metagame2.c -- Theme JD complete: the metacognition frontier. |
| `brain/` | `test_metagame2_resource` | 75 | stdio.h, stdlib.h, string.h, wubu_metagame2.h, wubu_resource.h | test_metagame2_resource.c -- AH07/AH09/AH10/AH11 (metagame2) + AH14/AH15 (resource). |
| `brain/` | `test_metagame_coord` | 76 | stdio.h, string.h, wubu_coord.h, wubu_credit.h, wubu_metagame.h | test_metagame_coord.c -- AH01-AH04 (coord) + AH05/AH06/AH08/AH13 (metagame) + AH12 (credit) verifica |
| `brain/` | `test_mhc` | 35 | assert.h, math.h, stdio.h, wubu_mhc.h | Test: wubu_mhc (Round-3 #213 — mHC identity + non-negativity). |
| `brain/` | `test_misc_gaps` | 45 | math.h, stdio.h, wubu_misc_gaps.h | test_misc_gaps.c -- L05/O12/O13/O15/P12/P13 verification. |
| `brain/` | `test_mix` | 55 | stdio.h, stdlib.h, string.h, wubu_mix.h | test_mix.c -- the weighted multi-stream mixer: the output must be a deterministic blend whose per-st |
| `brain/` | `test_mla` | 155 | math.h, stdio.h, stdlib.h, string.h, wubu_mla.h | test_mla.c — tests for Multi-Latent Attention (MLA) kernel. Verifies the wubu_mla module used by Dee |
| `brain/` | `test_mmrope` | 145 | math.h, stdio.h, stdlib.h, string.h, wubu_mmrope.h | test_mmrope.c — tests for 3D MM-RoPE kernel. Verifies: - init succeeds with divisible head_dim (12 - |
| `brain/` | `test_mobius_backward` | 129 | math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h | test_mobius_backward.c — Numerical verification of Möbius addition backward. Compares analytic gradi |
| `brain/` | `test_mobius_linear` | 101 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_mobius_l | test_mobius_linear.c — Validate Möbius linear layer forward + backward. |
| `brain/` | `test_model` | 48 | stdio.h, stdlib.h, wubu_core_dumps.h, wubu_model.h |  |
| `brain/` | `test_model_adapter` | 196 | assert.h, safetensors_reader.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_dims | test_model_adapter.c -- config-driven unit test for the model-agnostic core (wubu_model_adapter + sa |
| `brain/` | `test_model_config` | 117 | stat.h, stdio.h, stdlib.h, string.h, wubu_model_adapter.h | test_model_config.c -- verify the HF adapter derives the REAL model dims from config.json for the fo |
| `brain/` | `test_model_hwaccel` | 81 | assert.h, stdio.h, stdlib.h, string.h, wubu_gamebud.h, wubu_hwcaps.h, wubu_model | Test: wubu_model_wire_hwaccel() actually wires the HW stack into a real model. Loads the small fixtu |
| `brain/` | `test_model_moe` | 64 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h | test_model_moe.c — Run wubu_model_forward with MoE enabled, dump logits. Build: gcc -O2 -I include - |
| `brain/` | `test_moe` | 184 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_moe.h, wubu_ssm | test_moe.c — Test MoE forward for a single layer Loads one layer's MoE weights, runs 4 tokens throug |
| `brain/` | `test_moe2` | 68 | stdio.h, stdlib.h, string.h, wubu_moe2.h | test_moe2.c -- the mixed-agents router test (phase 2 of the WuBu model). Verifies: the router picks  |
| `brain/` | `test_moe_accum` | 218 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | test_moe_accum.c — Compare CPU vs GPU MoE error accumulation across layers. Runs one full model forw |
| `brain/` | `test_moe_dequant_direct` | 83 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h |  |
| `brain/` | `test_moe_grouped` | 33 | assert.h, stdio.h, wubu_moe_grouped.h | Test: wubu_moe_grouped (Area D — MoE expert grouping). |
| `brain/` | `test_moe_hyperbolic` | 353 | assert.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_moe.h, wubu_moe_hype | test_moe_hyperbolic.c — Test Nested MoE routing with Poincaré distance + hierarchy Tests: 1. Poincar |
| `brain/` | `test_moe_layer` | 130 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wu | test_moe_layer.c — Compare CPU vs GPU MoE output for a single layer. Tests layer 0 MoE with a fixed  |
| `brain/` | `test_moe_layer0` | 63 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | test_moe_layer0.c — Compare our MoE forward vs ggml for layer 0. Build: gcc -O2 -I include -o test_m |
| `brain/` | `test_moe_rag` | 76 | math.h, stdio.h, wubu_moe_rag.h | test_moe_rag.c -- X01-X06 + Y01-Y04 verification. |
| `brain/` | `test_moondream` | 123 | math.h, stdio.h, stdlib.h, string.h, wubu_moondream.h | test_moondream.c -- MoonDream 3 vision core: the agentic vision frontier. |
| `brain/` | `test_more_cores` | 86 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_equiv_check.h, wubu_expert_a | Test: B08 NVFP4, H03 Hadamard, A08/E02 MLA, E06 all-reduce, F02 equiv-check. All pure-CPU algorithm  |
| `brain/` | `test_more_spec` | 58 | math.h, stdio.h, wubu_more_spec.h | test_more_spec.c -- M07/M08/M09/M10/M15/M17/M18/M19/M20 verification. |
| `brain/` | `test_mtp_draft` | 91 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_m | test_mtp_draft.c — Verify MTP head loads and produces non-trash logits. Tests the code path without  |
| `brain/` | `test_mtp_load` | 126 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_multiconsensus` | 111 | stdio.h, string.h, wubu_agentid.h, wubu_bft.h, wubu_fraud.h, wubu_semcons.h, wub | test_multiconsensus.c -- DD01-DD07 verification. |
| `brain/` | `test_multimodal` | 116 | math.h, stdio.h, wubu_audio.h, wubu_imgenc.h, wubu_mm_adapter.h, wubu_mm_align.h | test_multimodal.c -- CC01-CC07 verification. |
| `brain/` | `test_mxfp4` | 47 | assert.h, math.h, stdio.h, stdlib.h, wubu_mxfp4.h | Test: wubu_mxfp4 (Round-4 #433 — OCP Microscaling FP4/FP8 round-trip). |
| `brain/` | `test_nan_logit` | 77 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h | test_nan_logit.c — Find NaN in output projection specifically |
| `brain/` | `test_nan_trace` | 131 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h | test_nan_trace.c — Find first NaN source in model forward |
| `brain/` | `test_nan_xrange` | 120 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h | test_nan_xrange.c — Check x range, skip MoE properly |
| `brain/` | `test_nest` | 116 | math.h, stdio.h, stdlib.h, string.h, wubu_nest.h | test_nest.c -- the WuBu Nesting transitions test (phase 3). Verifies the quaternion SO(4) rotation p |
| `brain/` | `test_nested_moe_router_backward` | 391 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_moe.h, wubu_moe_hyperbolic.h | test_nested_moe_router_backward.c — Validate Nested MoE 2-level backward pass. Tests: 1. Forward run |
| `brain/` | `test_nested_ssm` | 276 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_nested_s | Test Nested SSM forward pass. Tests: 1. K=1 with R=0.956 matches single Poincaré SSM output |
| `brain/` | `test_nested_ssm_backward` | 388 | float.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu | Test Nested SSM Backward pass. Tests BPTT through K independent Poincaré ball recurrences: 1. Forwar |
| `brain/` | `test_neurom` | 91 | math.h, stdio.h, string.h, wubu_neurom.h | test_neurom.c -- Theme IW batch 1: the neuromorphic frontier. |
| `brain/` | `test_nf4` | 80 | math.h, stdio.h, stdlib.h, string.h, wubu_nf4.h | test_nf4.c — NormalFloat 4-bit quantization correctness test Compares NF4 roundtrip vs Q8 and F32 fo |
| `brain/` | `test_nf4_debug` | 27 | math.h, stdio.h, wubu_nf4.h | Directly test quantize on known values |
| `brain/` | `test_nf4_dequant` | 100 | math.h, stdio.h, stdlib.h, string.h, wubu_dequant_nf4.h | test_nf4_dequant.c — runtime tests for NF4 dequantization. 22 test vectors covering: - All 16 NF4 co |
| `brain/` | `test_nf4_exact` | 11 |  |  |
| `brain/` | `test_ngram` | 88 | stdio.h, stdlib.h, string.h, wubu_ngram.h | test_ngram.c — Test n-gram speculative drafting. |
| `brain/` | `test_no_moe` | 52 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_numerical_audit` | 61 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_kv_adaptive.h, wubu_numerica | Test: numerical-stability audit of dequant paths (doc F03). |
| `brain/` | `test_one_layer` | 81 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Run only 1 layer (SSM layer 0) and compare logits |
| `brain/` | `test_one_ssm_backward` | 188 | gguf_reader.h, math.h, signal.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_mod | test_one_ssm_backward.c — Validate one SSM layer backward pass. Loads the real model, runs one SSM l |
| `brain/` | `test_ops` | 154 | math.h, stdio.h, stdlib.h, string.h, wubu_banner.h, wubu_ops.h | test_ops.c — Golden-file tests for the wubu_ops numerical primitives. Generated from research 066-D5 |
| `brain/` | `test_optim` | 177 | math.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_backprop.h, wubu_optim.h, wubu | test_optim.c — the unified-optimizer gate (research/069). Proves the optimizer SPLIT works end-to-en |
| `brain/` | `test_orbits` | 153 | math.h, stdio.h, string.h, wubu_hive.h, wubu_orbits.h | test_orbits.c -- NESTED-SPHERE MEMORY test (research/060, AN12 wave 3). The DA oracles: 1. write/rea |
| `brain/` | `test_orch` | 67 | stdio.h, stdlib.h, string.h, wubu_agi.h, wubu_hive.h | test_orch.c -- the specialist-cell orchestrator gate (the user directive #3: hierarchical planning — |
| `brain/` | `test_output_proj` | 50 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h |  |
| `brain/` | `test_paged_kv` | 41 | assert.h, stdio.h, wubu_paged_kv.h | Test: wubu_paged_kv (Area C — paged attention block manager). |
| `brain/` | `test_parallel_scan` | 376 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, gguf_reader.h, math.h, stdio.h, std | Test: Parallel SSM Scan Kernel vs CPU Reference Launches the ssm_parallel_scan_kernel, compares agai |
| `brain/` | `test_parallel_spec` | 50 | math.h, stdio.h, wubu_parallel_spec.h | test_parallel_spec.c -- V01-V04 + W01-W03 verification. |
| `brain/` | `test_passk` | 36 | math.h, stdio.h, wubu_passk.h | test_passk.c -- the pass@k estimator: the unbiased combinatorial values against the hand-computed re |
| `brain/` | `test_patch_embed` | 181 | math.h, stdio.h, stdlib.h, string.h, wubu_patch_embed.h | test_patch_embed.c — tests for ViT patch embedding kernel. Verifies: - Patch extraction + projection |
| `brain/` | `test_pd_serve` | 53 | math.h, stdio.h, wubu_pd_serve.h | test_pd_serve.c -- AB01-AB06 + AC01-AC03 verification. |
| `brain/` | `test_pd_split` | 42 | assert.h, math.h, stdio.h, wubu_pd_split.h | Test: wubu_pd_split (Round-2 #131 — PD disaggregation planner). |
| `brain/` | `test_pga_backward` | 164 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_poincare_gqa.h, wubu_ss | test_pga_backward.c — Validate Poincaré GQA backward produces non-zero weight gradients. Loads model |
| `brain/` | `test_pim` | 83 | math.h, stdio.h, string.h, wubu_pim.h | test_pim.c -- Theme IS batch 1: the PIM/near-memory frontier. |
| `brain/` | `test_pim2` | 86 | math.h, stdio.h, string.h, wubu_pim2.h | test_pim2.c -- Theme IS complete: the PIM/hardware frontier. |
| `brain/` | `test_plateau` | 41 | stdio.h, wubu_plateau.h | test_plateau.c -- the amoeba plateau trigger: a decreasing loss series is progress, a flat/rising on |
| `brain/` | `test_poincare_gqa` | 243 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_poincare_gqa.h, | test_poincare_gqa.c Verifies the Poincaré GQA forward pass: 1. Poincaré GQA produces non-NaN output |
| `brain/` | `test_poincare_kv_cache` | 208 | math.h, stdio.h, stdlib.h, string.h, wubu_poincare_gqa.h, wubu_ssm.h | test_poincare_kv_cache.c — Unit test for Poincaré GQA hyperbolic KV cache. |
| `brain/` | `test_poincare_router_backward` | 77 | math.h, stdio.h, stdlib.h, string.h, wubu_mobius.h, wubu_moe_hyperbolic.h | test_poincare_router_backward.c — Validate Poincaré router backward. |
| `brain/` | `test_polar_pso` | 176 | math.h, stdio.h, stdlib.h, string.h, wubu_polar_pso.h | test_polar_pso.c — PSO + procedural precache + Rambus serial bit reader Tests: roundtrip, attention  |
| `brain/` | `test_polarquant` | 129 | math.h, stdio.h, stdlib.h, string.h, wubu_polarquant.h | test_polarquant.c — PolarQuant recursive polar packed roundtrip |
| `brain/` | `test_polarquant_benchmark` | 160 | math.h, stdint.h, stdio.h, stdlib.h, string.h, time.h, wubu_polar_pso.h, wubu_po | test_polarquant_nobreakmark.c — Zero-malloc decode benchmark Measures cycles per token for malloc-fr |
| `brain/` | `test_polarquant_cache` | 144 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_polarquant.h | test_polarquant_cache.c — PolarQuant mixed-precision KV cache benchmark Tests: roundtrip accuracy, f |
| `brain/` | `test_polarquant_scale` | 227 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_polarquant.h | test_polarquant_scale.c — PolarQuant vs Q8 scaling benchmark Simulates decode attention at 256K and  |
| `brain/` | `test_pool` | 29 | stdio.h, stdlib.h, thread_pool.h, time.h |  |
| `brain/` | `test_pos_embd` | 50 | stdio.h, wubu_vision.h, wubu_vision_encoder.h | test_pos_embd.c — dump vision position embedding to find NaN source |
| `brain/` | `test_precision_plan` | 123 | stdio.h, stdlib.h, string.h, wubu_precision_plan.h | test_precision_plan.c -- AM03 gate: HETEROGENEOUS PRECISION. "Run on all hardware" is a precision pl |
| `brain/` | `test_pref` | 68 | stdio.h, string.h, wubu_pref.h | test_pref.c -- the RLHF ORACLE gate (the user directive #4: "give the oracle teeth"). Asserts: |
| `brain/` | `test_pref2` | 183 | math.h, stdio.h, string.h, wubu_pref2.h | test_pref2.c -- Theme IQ complete: the alignment frontier. |
| `brain/` | `test_prefill_vs_decode` | 381 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_ssm.h | test_prefill_vs_decode.c — Compare prefilled vs decoded GQA output for position np This tests whethe |
| `brain/` | `test_prefix_reuse` | 69 | assert.h, stdio.h, stdlib.h, string.h, wubu_prefix_cache.h | Test: cross-request prefix KV reuse (doc 010). Two requests share a 512-token prefix. The second req |
| `brain/` | `test_priority` | 89 | stdio.h, stdlib.h, string.h, wubu_priority.h | test_priority.c -- the priority store (AGI institutional memory). DA oracle gates: 1. init clears ev |
| `brain/` | `test_priority_store` | 77 | stdio.h, string.h, wubu_priority_store.h | test_priority_store.c -- the PRIORITY STORE gate (Phase 2 completion: "one checkpoint + one profile  |
| `brain/` | `test_probe_qwen` | 78 | math.h, stdio.h, stdlib.h, string.h, wubu_dims.h, wubu_model.h, wubu_model_safet | test_probe_qwen.c -- load real Qwen3.6-27B (MAX_LAYERS env) and dump layer-0 weight pointers + alloc |
| `brain/` | `test_proj_accuracy` | 74 | gguf_reader.h, math.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu_d | test_proj_accuracy.c -- verify quantized_matmul's projection math against an unambiguous scalar orac |
| `brain/` | `test_prover2` | 57 | stdio.h, wubu_prover.h | test_prover2.c -- the math-RL verifier test (phase 6). The verifier must ACCEPT the true theorems (t |
| `brain/` | `test_q2q3_dequant` | 258 | gguf_reader.h, math.h, stdio.h, string.h |  |
| `brain/` | `test_q2q3_dequant2` | 268 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_q4k_dequant` | 53 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h |  |
| `brain/` | `test_q4k_dequant2` | 55 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h |  |
| `brain/` | `test_q4k_dequant3` | 52 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_q5q6` | 93 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_q8` | 26 | assert.h, math.h, stdio.h, stdlib.h, wubu_q8.h | Test: wubu_q8 (Area G — Q8_0 lossless weight quant). |
| `brain/` | `test_q8k_pqv` | 139 | math.h, stdio.h, stdlib.h, string.h, wubu_fast_attn.h, wubu_polarquant.h | test_q8k_pqv.c — Hybrid Q8_K + PolarQuant_V attention correctness test. Validates the fast_attn deco |
| `brain/` | `test_quant_selector` | 37 | math.h, stdio.h, wubu_quant_selector.h | test_quant_selector.c -- N04/N05/N09 verification. |
| `brain/` | `test_quantized_matmul` | 80 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h |  |
| `brain/` | `test_qwen35` | 89 | stdio.h, string.h, wubu_qwen35.h | test_qwen35.c — the QWEN3.5 HYBRID loader role split gate (AN28 "build next": the fused-tensor role  |
| `brain/` | `test_qwen35_forward` | 119 | stdio.h, stdlib.h, string.h, wubu_qwen35_forward.h | test_qwen35_forward.c — the QWEN3.5 HYBRID FORWARD gate (AN28 "build next": the gated-attn branch wi |
| `brain/` | `test_real_load` | 95 | math.h, stdio.h, stdlib.h, wubu_dims.h, wubu_model.h, wubu_model_safetensors_bri | test_real_load.c -- load the REAL downloaded Agents-A1-4B Colonel checkpoint (multi-shard safetensor |
| `brain/` | `test_recency` | 35 | math.h, stdio.h, wubu_recency.h | test_recency.c -- the recency-weighted sampling: monotonic increase toward the fresh end, the base f |
| `brain/` | `test_regression` | 375 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_t | test_regression.c — Regression test suite comparing wubuwizard engine output against llama.cpp refer |
| `brain/` | `test_repetition` | 47 | math.h, stdio.h, wubu_repetition.h | test_repetition.c -- verify repeat_penalty + DRY suppression. |
| `brain/` | `test_resources` | 44 | stdio.h, string.h, wubu_resources.h | test_resources.c — the RESOURCE LEDGER gate (post-AN54 A6). Asserts: 1. the snapshot reads real RSS  |
| `brain/` | `test_ring_attn` | 42 | math.h, stdio.h, stdlib.h, wubu_ring_attn.h |  |
| `brain/` | `test_rollout` | 50 | stdio.h, string.h, wubu_rollout.h | test_rollout.c -- the Balanced Adaptive Rollout: the harder tasks get more of the budget, the counts |
| `brain/` | `test_roofline` | 40 | assert.h, math.h, stdio.h, wubu_roofline.h | Test: wubu_roofline (Round-2 #101 — B*-crossover auto-tuner). |
| `brain/` | `test_rope_adjacent` | 142 | math.h, stdio.h, stdlib.h, string.h | test_rope_adjacent.c — Verify adjacent-pair RoPE against split-half Two binary outputs: one with cor |
| `brain/` | `test_rope_layer0` | 134 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_rope_layer0.c — Dump Q before/after RoPE at layer 3 (first GQA layer) Compile: gcc -O2 -I inclu |
| `brain/` | `test_rope_prefetch` | 38 | assert.h, math.h, stdio.h, stdlib.h, wubu_rope_prefetch.h | Test: RoPE-aware KV prefetch (doc A10). |
| `brain/` | `test_rope_t2` | 87 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h |  |
| `brain/` | `test_rotate` | 93 | assert.h, math.h, stdio.h, stdlib.h, wubu_gemm.h, wubu_rotate.h | test_rotate.c -- doc 013 (QuaRot/Hadamard) triple-DA. P1 correctness: (W*H_P)*(H_P*x) == W*x exactly |
| `brain/` | `test_router` | 176 | stdio.h, stdlib.h, string.h, wubu_router.h | test_router.c -- THE ROUTER SLOT gate (Revolver Doctrine, THEORY/06). Proves the routing is AGNOSTIC |
| `brain/` | `test_router_stability` | 217 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_moe.h, wubu_ssm | test_router_stability.c — Measure router sensitivity to input perturbation. Loads only the router we |
| `brain/` | `test_rsgd` | 64 | gguf_reader.h, math.h, rsgd.h, stdio.h, stdlib.h, string.h | test_rsgd.c — Verify RSGD optimizer produces valid Poincaré ball output |
| `brain/` | `test_rsi` | 98 | math.h, stdio.h, string.h, wubu_rsi.h | test_rsi.c -- Theme IV batch 1: the recursive self-improvement frontier. |
| `brain/` | `test_runtime_dims` | 88 | stdio.h, stdlib.h, string.h, wubu.h, wubu_runtime_dims.h | test_wubu_runtime_dims.c -- the Revolver Doctrine + Aligned Rewrite gate (S1-S5, ADR005). Proves: 1. |
| `brain/` | `test_safetensors` | 30 | safetensors_reader.h, stdio.h, string.h | test_safetensors.c -- verify the safetensors reader on the fixture. |
| `brain/` | `test_scalable_model` | 175 | math.h, stdio.h, stdlib.h, string.h, wubu_model_scalable.h | test_scalable_model.c — tests for fractal self-scaling model (AN23) Verifies: T1: model builds regio |
| `brain/` | `test_scale_to_fit` | 238 | stdio.h, stdlib.h, string.h, wubu_scale.h | test_scale_to_fit.c -- THEORY/11 gate: one checkpoint, any hardware. The SAME checkpoint (fixed geom |
| `brain/` | `test_scheduler` | 47 | assert.h, stdio.h, stdlib.h, wubu_scheduler.h | Test: wubu_scheduler (Areas H/I — continuous batching + prefix cache). |
| `brain/` | `test_seed` | 58 | stdio.h, string.h, wubu_seed.h | test_seed.c -- the deterministic seed oracle (AC-E13): the same seed yields the same sequence and th |
| `brain/` | `test_self_cascade` | 27 | assert.h, stdio.h, stdlib.h, string.h, wubu_spec_cascade.h | Test: self-cascade drafter (doc 012/018). |
| `brain/` | `test_selfimprove` | 66 | stdio.h, string.h, wubu_hive.h, wubu_rsi.h, wubu_selfimprove.h | test_selfimprove.c -- the TRACE/SPAN OPERATOR + RSI mutation engine gate (closes P20: "wubu_selfimpr |
| `brain/` | `test_serve` | 105 | math.h, stdio.h, string.h, wubu_serve.h | test_serve.c -- Theme IR batch 1: the multi-tenant serving scheduler. |
| `brain/` | `test_serve2` | 112 | math.h, stdio.h, string.h, wubu_serve2.h | test_serve2.c -- Theme IR complete: the serving scheduler frontier. |
| `brain/` | `test_shexp` | 126 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_moe.h, wubu_ssm.h |  |
| `brain/` | `test_shexp_gate` | 47 | gguf_reader.h, math.h, stdio.h, wubu_moe.h |  |
| `brain/` | `test_siglip` | 57 | math.h, stdio.h, stdlib.h, string.h, wubu_siglip.h | test_siglip.c — load the SmolVLM2 mmproj, run a synthetic image, check output is finite and determin |
| `brain/` | `test_siglip_parity` | 47 | math.h, stdio.h, stdlib.h, string.h, wubu_siglip.h | test_siglip_parity.c — run the same cb checkerboard through our SigLIP encoder and print stats for o |
| `brain/` | `test_skill_train` | 51 | stdio.h, string.h, wubu_skill_train.h | test_skill_train.c — the SKILL → TRAIN FEEDBACK gate (AN47 #5: "close experience → weights, not only |
| `brain/` | `test_skill_train_real` | 75 | stdio.h, stdlib.h, string.h, wubu.h, wubu_runtime_dims.h, wubu_skill_train.h, wu | test_skill_train_real.c — the SKILL → TRAIN REAL-WEIGHTS gate (the milestone: "close experience → we |
| `brain/` | `test_skillcell` | 112 | stdio.h, string.h, wubu_hive.h, wubu_skillcell.h | test_skillcell.c -- the SKILL CURRICULUM gate (Phase 4: Pillar 17 — "the colony can add a skill from |
| `brain/` | `test_smoothquant` | 76 | assert.h, math.h, stdio.h, stdlib.h, wubu_smoothquant.h | test_smoothquant.c -- doc 005 triple-DA (SmoothQuant activation migration). P1 correctness: the Smoo |
| `brain/` | `test_smt_check` | 55 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_smt_check.h | Test: SMT-style GEMV equivalence checking (doc F02). |
| `brain/` | `test_soa` | 64 | assert.h, math.h, stdio.h, stdlib.h, string.h, wubu_soa.h | Test: SoA activation tensor layout (doc I02/C02). |
| `brain/` | `test_sparse_attn` | 46 | math.h, stdio.h, wubu_sparse_attn.h | test_sparse_attn.c -- L11/L12 verification. |
| `brain/` | `test_spec_cascade` | 38 | assert.h, stdio.h, stdlib.h, wubu_spec_cascade.h | Test: CAS-Spec adaptive deferral (doc 018). Validates the defer_threshold / adaptive-deferral path i |
| `brain/` | `test_spec_decode` | 59 | assert.h, math.h, stdio.h, stdlib.h, wubu_ngram.h, wubu_spec_decode.h | Test: wubu_spec_decode (Area A — speculative decoding). |
| `brain/` | `test_spec_tuner` | 37 | math.h, stdio.h, wubu_spec_tuner.h | test_spec_tuner.c -- N15/M12/N16 verification. |
| `brain/` | `test_spec_variants` | 37 | math.h, stdio.h, wubu_spec_variants.h | test_spec_variants.c -- M11/M13/M14/L14 verification. |
| `brain/` | `test_splitk` | 86 | math.h, stdio.h, stdlib.h, string.h, wubu_fast_attn.h | test_splitk.c — Split-K parallel decode correctness test. Compares wubu_fast_attn_decode_splitk agai |
| `brain/` | `test_ssd_moe` | 138 | math.h, safetensors_reader.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_ssd_mo | test_ssd_moe.c -- verify the ds4-ssd slot-bank pager. Builds a SYNTHETIC safetensors checkpoint (D=1 |
| `brain/` | `test_ssd_moe_real` | 205 | fcntl.h, math.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_ssd_moe.h | test_ssd_moe_real.c -- prove the ds4-ssd slot-bank on REAL KAT-256-expert weights with BOUNDED memor |
| `brain/` | `test_ssm_carry` | 92 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Test: SSM forward with T=6 + T=1 vs T=7 Verifies state carry consistency |
| `brain/` | `test_ssm_l0` | 154 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | Test SSM Layer 0 in isolation. Loads layer 0 SSM weights, computes embedding for token 9419, runs wu |
| `brain/` | `test_ssm_precision` | 89 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_ssm.h | test_ssm_precision.c Feed the reference's L0 hidden state into our SSM at L1. If cos-sim improves dr |
| `brain/` | `test_ssm_scan` | 24 | assert.h, math.h, stdio.h, stdlib.h, wubu_ssm_scan.h | Test: wubu_ssm_scan (Area F — chunkwise SSM scan). |
| `brain/` | `test_st_bridge` | 144 | math.h, stdio.h, stdlib.h, wubu_model.h, wubu_model_safetensors_bridge.h, wubu_r | test_st_bridge.c -- verify the safetensors->wubuwizard F32 bridge maps the REAL published HF tensor  |
| `brain/` | `test_step_bench` | 35 | stdio.h, stdlib.h, string.h, time.h, wubu.h, wubu_runtime_dims.h, wubu_train.h |  |
| `brain/` | `test_stream_kv` | 104 | stdio.h, stdlib.h, string.h, wubu_stream_kv.h | test_stream_kv.c -- StreamingLLM attention-sink remap verification (L01). Verifies: identity when di |
| `brain/` | `test_sys2026` | 49 | math.h, stdio.h, wubu_sys2026.h | test_sys2026.c -- Q12/Q13/Q14/Q16/Q17/Q18/R02 verification. |
| `brain/` | `test_sys_tune` | 37 | math.h, stdio.h, wubu_sys_tune.h | test_sys_tune.c -- L10/N06/N10/O03 verification. |
| `brain/` | `test_tandem_gamebud` | 114 | assert.h, stdio.h, stdlib.h, string.h, wubu_gamebud.h, wubu_hwcaps.h, wubu_rambu | Test: full hardware-acceleration stack (doc "tandem"/"rambus"/"gamebud"). Composes: hwcaps (SIMD det |
| `brain/` | `test_tensor_store` | 272 | gguf_reader.h, math.h, safetensors_reader.h, stdio.h, stdlib.h, string.h, wubu_t | test_tensor_store.c -- the uniform tensor catalog round-trip test. Proves the anti-waste interchange |
| `brain/` | `test_ternary` | 93 | math.h, stdio.h, string.h, wubu_ternary.h | test_ternary.c -- Theme JC complete: the ternary/1.58-bit frontier. |
| `brain/` | `test_text_fwd` | 48 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h | test_text_fwd.c — test model forward with text embeddings |
| `brain/` | `test_thread_spec` | 55 | assert.h, pthread.h, stdio.h, stdlib.h, wubu_thread_spec.h | Test: H02 CPU thread-specialization (prefill/decode pinned pools). Verifies: (1) jobs run on the rig |
| `brain/` | `test_tok` | 29 | stdio.h, wubu_tokenizer.h |  |
| `brain/` | `test_tok_debug` | 47 | stdio.h, stdlib.h, string.h, wubu_tokenizer.h |  |
| `brain/` | `test_tok_hf` | 23 | stdio.h, stdlib.h, string.h, wubu_tokenizer_hf.h |  |
| `brain/` | `test_token` | 87 | math.h, stdio.h, string.h, wubu_token.h | test_token.c -- Theme IT batch 1: the tokenization frontier. |
| `brain/` | `test_token2` | 99 | math.h, stdio.h, string.h, wubu_token2.h | test_token2.c -- Theme IT complete: the tokenization frontier. |
| `brain/` | `test_tokenizer` | 113 | stdio.h, stdlib.h, string.h, wubu_tokenizer.h |  |
| `brain/` | `test_tokenizer_simple` | 31 | stdio.h, string.h, wubu_tokenizer.h |  |
| `brain/` | `test_toolreg` | 62 | stdio.h, string.h, wubu_hive.h, wubu_toolreg.h | test_toolreg.c -- the DENY-BY-DEFAULT TOOL REGISTRY gate (Phase 5: Pillars 13/15 — "push real agency |
| `brain/` | `test_traj_grpo` | 100 | math.h, stdio.h, stdlib.h, string.h, wubu_traj_grpo.h | test_traj_grpo.c -- the trajectory-level GRPO against the FD oracle. The gradients MUST match the fi |
| `brain/` | `test_traj_sft` | 77 | stdio.h, string.h, wubu_traj_sft.h | test_traj_sft.c -- the masked-observation SFT converter: the masks must follow the Hermes/Orchard do |
| `brain/` | `test_trajcell` | 73 | stdio.h, string.h, wubu_hive.h, wubu_trajcell.h | test_trajcell.c -- the TOOL-TRAJECTORY CELL gate (roadmap #2: "closes the gap between the model talk |
| `brain/` | `test_tst` | 358 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_tst.h |  |
| `brain/` | `test_ttc` | 39 | math.h, stdio.h, wubu_ttc.h | test_ttc.c -- Q08/Q15/Q20/R01/R03 verification. |
| `brain/` | `test_ubus` | 92 | math.h, stdio.h, stdlib.h, string.h, wubu_ubus.h | test_ubus.c -- the U-Bus substrate: dispatch equality across backends, the roofline selector sanity, |
| `brain/` | `test_user_sim` | 60 | stdio.h, string.h, wubu_user_sim.h | test_user_sim.c -- the tau-bench-style user simulator: the react / verify / utter state machine agai |
| `brain/` | `test_userfs` | 299 | math.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubu_encoder.h, wubu_encoder_ | test_userfs.c -- THE USER SPACE IS THE TRAINING DATA (research/065). Regular user files (documents,  |
| `brain/` | `test_uuid` | 125 | stdio.h, stdlib.h, string.h, wubu_uuid.h | test_uuid.c — tests for UUIDv7 generation (RFC 9562). Tests: generation, format, monotonic increment |
| `brain/` | `test_vec_dot_types` | 65 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_vecsearch` | 108 | math.h, stdio.h, stdlib.h, string.h, wubu_vecsearch.h | test_vecsearch.c -- AV01-AV08 vector substrate verification. |
| `brain/` | `test_vision` | 102 | math.h, stdio.h, string.h, wubu_vision.h | test_vision.c -- Theme JB complete: the multimodal vision frontier. |
| `brain/` | `test_vision_real` | 107 | math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_vision.h, wubu_v | test_vision_real.c — Vision encoder with real screenshot image Loads raw float pixels from file, run |
| `brain/` | `test_vm_init` | 28 | stdio.h, wubu_vision_moondream.h | test_vm_init.c — Quick test for moondream weight loading. Build: gcc -O3 -I include -o test_vm_init  |
| `brain/` | `test_width` | 140 | stdio.h, stdlib.h, string.h, wubu_width.h | test_width.c -- the width expansion structural DA oracle: every expanded weight keeps the old block  |
| `brain/` | `test_wm_kv` | 51 | math.h, stdio.h, wubu_wm_kv.h | test_wm_kv.c -- O05/N02/N08 verification. |
| `brain/` | `test_worldmodel_agentauth` | 54 | math.h, stdio.h, string.h, wubu_agentauth.h, wubu_worldmodel.h | test_worldmodel_agentauth.c -- AG04 (world-model closed-loop) + AG07 (inter-agent auth). |
| `brain/` | `test_wubu_alignment` | 122 | stdio.h, wubu.h, wubu_runtime_dims.h | test_wubu_alignment.c -- Theory/08 gate: aligned geometry tiles evenly. Proves the redesigned 35M ge |
| `brain/` | `test_wubu_arch` | 73 | stdio.h, stdlib.h, string.h, wubu.h | test_wubu.c -- WuBu-35M: the mustard seed test. Loads the REAL released checkpoint, verifies the par |
| `brain/` | `test_wubu_kernel` | 118 | stdio.h, stdlib.h, string.h, wubu.h, wubu_arena.h, wubu_kernel_budget.h, wubu_kv | test_wubu_kernel.c -- the kernel-layer byte-budget gate (Theory/07). Proves the 35M engine's kernel  |
| `brain/` | `test_wubu_kv_stride` | 62 | stdio.h, stdlib.h, string.h, wubu_model.h | test_wubu_kv_stride.c -- the S6 gate: runtime KV context cap. The Revolver Doctrine says the KV cach |
| `brain/` | `test_wubu_mhc_mh` | 136 | math.h, stdio.h, stdlib.h, string.h, wubu_mhc_mh.h | test_wubu_mhc_mh.c -- multi-head hyper-connections (2512.24880 form). DA oracles: 1. identity init:  |
| `brain/` | `test_wubu_save` | 50 | stdio.h, stdlib.h, string.h, wubu.h, wubu_save.h | test_wubu_save.c -- the checkpoint round-trip test. Saves the loaded model as real safetensors, relo |
| `brain/` | `test_wubu_train` | 95 | stdio.h, stdlib.h, string.h, wubu.h, wubu_train.h | test_wubu_train.c -- the WuBu training core test (the AGI loop). Proves the training loop: the seed  |
| `brain/` | `test_wubu_wubu` | 88 | stdio.h, stdlib.h, string.h, wubu.h, wubu_moe2.h | test_wubu_wubu.c -- the WuBu mode test (blueprint phases 1-2 wired into the seed). Verifies: 1. mode |
| `brain/` | `test_yarn` | 30 | assert.h, math.h, stdio.h, wubu_yarn.h | Test: wubu_yarn (Round-3 #241 — YaRN NTK-aware extrapolation). |
| `brain/` | `test_zero_layers` | 83 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | Zero layers: just embedding + final norm + output projection |
| `brain/` | `tok_decode` | 19 | stdio.h, stdlib.h, wubu_tokenizer.h | tok_decode.c — decode token ids with a model's tokenizer. |
| `brain/` | `tok_encode` | 70 | stdio.h, stdlib.h, string.h, wubu_tokenizer.h | tok_encode.c — tokenize text and print the ids (verify the encoder). |
| `brain/` | `tok_ids` | 28 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h | tok_ids.c -- dump our BPE token IDs for the test prompt. |
| `brain/` | `tok_ids2` | 24 | wubu_sd_clip.c | tok_ids2.c -- compile the clip .c INLINE to access bpe_encode internals. |
| `brain/` | `tok_probe` | 28 | gguf_reader.h, stdio.h, stdlib.h, string.h, wubu_sd_clip.h | tok_probe.c -- print token IDs for the prompt and "". |
| `brain/` | `tok_test` | 20 | stdio.h, stdlib.h, wubu_tokenizer_hf.h | tok_test.c — encode a prompt with wubu_tokenizer_hf and dump token ids. |
| `brain/` | `tokenize_corpus` | 176 | stdint.h, stdio.h, stdlib.h, string.h, wubu_tokenizer.h | tokenize_corpus.c — Tokenize corpus_raw.txt using Qwen3.6 tokenizer and write binary uint32_t data f |
| `brain/` | `trace_nan` | 120 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_s |  |
| `brain/` | `train_backprop` | 300 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_core_dumps.h, w | train_backprop.c — Phase 3.5: Backpropagation training Full forward pass + CE loss + gradient w.r.t. |
| `brain/` | `train_gpu` | 1031 | bench.h, gguf_reader.h, math.h, omp.h, qlearner.h, rsgd.h, stdio.h, stdlib.h, st | train_gpu.c — Phase 3.5: GPU-Accelerated Training Hybrid: GPU for attention/SSM forward, CPU for nor |
| `brain/` | `train_integrated` | 707 | bench.h, gguf_reader.h, math.h, omp.h, qlearner.h, rsgd.h, stdio.h, stdlib.h, st | train_integrated.c — All 7 modules wired into single training pipeline. Usage:  ./train_integrated [ |
| `brain/` | `train_real` | 503 | cblas.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model. | train_real.c — Phase 3: Training Pipeline on Real Qwen3.6 Model Loads the 40-layer model + tokenizer |
| `brain/` | `train_stub` | 385 | math.h, stdio.h, stdlib.h, string.h, time.h | train_stub.c — Phase 3 Training Loop Verification A self-contained training stub that: 1. Creates a  |
| `brain/` | `ts_convert` | 57 | stdio.h, string.h, wubu_tensor_store.h | ts_convert.c — one-liner model-format interchange via the tensor store. ts_convert <in> <out> [safet |
| `brain/` | `tvar` | 63 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_unet.h | tvar.c — run our UNet on the SAME noise at several timesteps, print corr(x, eps) per the skill's dea |
| `brain/` | `txt2img_sd` | 344 | complex.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_sd_c | txt2img_sd.c -- end-to-end SD 1.5 txt2img on the wizard C11 engine. CLIP text encode -> DDIM (12 ste |
| `brain/` | `unet_corr` | 33 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_unet.h | unet_corr.c -- feed noise x, check corr(x, eps). Working UNet at t=999 predicts ~the noise itself: c |
| `brain/` | `unet_eps_dump` | 63 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_ops.h, wubu_sd_unet. | unet_eps_dump.c -- run OUR C11 UNet on the exact same input as the sd.cpp harness (noise file, sigma |
| `brain/` | `unet_mag` | 39 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_sd_unet.h | unet_mag.c -- feed pure noise (RMS 1) at t=999, check eps RMS. Correct UNet: eps ~ x (RMS ~1). Broke |
| `brain/` | `vae64` | 58 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_ops.h | vae64.c -- conv2d_q (conv_in 4->512) at the REAL 64x64 size, vs manual. Tiling T=8 runs 8 tiles — ca |
| `brain/` | `vae_conv_probe` | 74 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_ops.h | vae_conv_probe.c -- verify conv2d_q vs canonical for a REAL VAE conv (up.0.block.0.conv1: 256->128,  |
| `brain/` | `vae_mid_probe` | 46 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_ops.h, wubu_sd_vae.h | vae_mid_probe.c -- decode constant latent, dump mid output (64x64, 512ch) to check for stripes BEFOR |
| `brain/` | `vae_pattern` | 49 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_vae.h | vae_pattern.c -- decode constant latents to find spatial bugs. all=0.5 -> smooth mid color. all=1.0, |
| `brain/` | `vae_sanity` | 37 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_sd_vae.h | vae_sanity.c -- decode a ZERO latent (and a scaled-ones latent). Correct VAE: zeros -> flat gray (me |
| `brain/` | `verify_dequant` | 92 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `verify_emb` | 104 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | verify_emb.c — Verify token embedding lookup against reference. Loads token_embd.weight from GGUF, e |
| `brain/` | `verify_iq2s` | 74 | gguf_reader.h, math.h, stdio.h, stdlib.h |  |
| `brain/` | `verify_iq_dequant_vs_llamacpp` | 153 | ggml-common.h, ggml-quants.h, math.h, stdint.h, stdio.h, stdlib.h, string.h | verify_iq_dequant_vs_llamacpp.c Compare wubuwizard IQ2_XXS/IQ3_XXS dequant against llama.cpp's dequa |
| `brain/` | `verify_l1_weights` | 75 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | verify_l1_weights.c — Check that L1 MoE gate weights match between our gguf_reader and direct file r |
| `brain/` | `verify_moe_weights` | 108 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | Verify loaded MoE weights produce correct expert 64 output. Compare against direct GGUF raw file deq |
| `brain/` | `verify_phase26_fusions` | 329 | cublas_v2.h, cuda_kernels.h, cuda_runtime.h, gguf_reader.h, math.h, stdio.h, std | Verify Phase 26 fused kernels vs old cuBLAS+element-wise path. Tests: ssm_beta_alpha_fused_decode +  |
| `brain/` | `verify_q5k` | 151 | ggml.h, gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | verify_q5k.c — Verify Q5_K dequant against ggml reference. Build: cd /home/wubu/wubuwizard |
| `brain/` | `verify_raw_dequant` | 175 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | Quick verif: Read raw IQ2_XXS from GGUF, dequant, compute expert 64 output. Then compare against our |
| `brain/` | `verify_weights` | 87 | gguf_reader.h, llama.h, math.h, stdio.h, stdlib.h, string.h | Compare RMSNorm weights directly |
| `brain/` | `wstats` | 37 | gguf_reader.h, math.h, stdio.h, stdlib.h | wstats.c -- dump weight statistics for time_embed + emb_layers tensors |
| `brain/` | `wubu_anomaly` | 144 | math.h, stdio.h, stdlib.h, string.h | wubu_anomaly.c — the A8 ANOMALY DETECTOR (post-AN54 A8): scan one event log for the release-gate ant |
| `brain/` | `wubu_boot` | 159 | math.h, safetensors_writer.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_bi.h, wu | wubu_boot.c — THE COLONEL BOOT CORE EXTRACTOR (research/060, AN12). The smallest subset of the weigh |
| `brain/` | `wubu_cli` | 111 | stdio.h, stdlib.h, string.h, wubu.h, wubu_banner.h, wubu_tokenizer_hf.h | wubu_cli.c -- WuBu-35M operational runner (the mustard seed CLI). Loads the real released checkpoint |
| `brain/` | `wubu_dedup_cli` | 36 | stdio.h, stdlib.h, string.h, wubu_dedup.h | wubu_dedup_cli.c -- the corpus-health scanner: measures the exact duplicate-window rate of a .tok st |
| `brain/` | `wubu_endurance` | 399 | stdio.h, stdlib.h, string.h, time.h, wubu_amoeba.h, wubu_contracts.h, wubu_diagn | wubu_endurance.c — THE MULTI-HOUR ENDURANCE RUNNER (AN47 #1 done- definition: "a single CLI path tha |
| `brain/` | `wubu_hive_walk` | 91 | stdint.h, stdio.h, stdlib.h, string.h, wubu_diagnosis.h | wubu_hive_walk.c -- the HIVE WALK tool (the user directive #2: the agent can invoke 'show me the las |
| `brain/` | `wubu_live_learn` | 107 | stdint.h, stdio.h, stdlib.h, string.h, wubu.h, wubu_tokenizer_hf.h | wubu_live_learn.c -- the LIVE SPEED LEARNING loop (the kernel's brain-side). The feedback loop (the  |
| `brain/` | `wubu_qwen35_probe` | 177 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_qwen35.h, wubu_qwen35_f | wubu_qwen35_probe.c — the QWEN3.5 REAL-GGUF probe (the parity path's first step): load the ACTUAL Q8 |
| `brain/` | `wubu_replay` | 76 | stdio.h, stdlib.h, string.h, wubu_events.h | wubu_replay.c — the REPLAY VERIFIER (post-AN54 A7). C11. Given the hive + prio + the event log, RECO |
| `brain/` | `wubu_report` | 158 | stdio.h, stdlib.h, string.h, wubu_diagnosis.h, wubu_events.h, wubu_priority_stor | wubu_report.c — the HIVE-WALK REPORT GENERATOR (post-AN54 A2). One command: lineage top-K, extinctio |
| `brain/` | `wubu_runcompare` | 165 | math.h, stdio.h, stdlib.h, string.h | wubu_runcompare.c — the A7 RUN-COMPARE tool (post-AN54 A7): two event logs -> a delta report on the  |
| `brain/` | `wubu_timeseries` | 133 | math.h, stdio.h, stdlib.h, string.h, wubu_events.h | wubu_timeseries.c — the SUITE-SCORE TIME SERIES (post-AN54 A3). Reads <base>.events.jsonl and export |
| `brain/` | `wubu_tokenc` | 106 | stdint.h, stdio.h, stdlib.h, string.h, wubu_tokenizer_hf.h | wubu_tokenc.c -- the C11 corpus tokenizer (the AGI corpus pipeline). Reads text files (or stdin), to |
| `brain/` | `wubu_train_cli` | 613 | glob.h, immintrin.h, stat.h, stdint.h, stdio.h, stdlib.h, string.h, wubu.h, wubu | wubu_train_cli.c -- the AGI training loop runner (the seed grows). Reads uint16 token streams (.tok, |
| `brain/` | `gen_text_mtp` | 140 | gguf_reader.h, math.h, signal.h, stdio.h, stdlib.h, string.h, time.h, wubu_model | gen_text_mtp.c — Clean decode with model->save_last_hidden for h_39 capture. MTP draft + verify comm |
| `brain/` | `test_mtp_draft` | 91 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, time.h, wubu_model.h, wubu_m | test_mtp_draft.c — Verify MTP head loads and produces non-trash logits. Tests the code path without  |
| `brain/` | `test_mtp_load` | 126 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h |  |
| `brain/` | `test_q2q3_dequant` | 258 | gguf_reader.h, math.h, stdio.h, string.h |  |
| `brain/` | `compare_weights_two_paths` | 86 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | compare_weights_two_paths.c Load expert 64's gate weights via TWO paths and compare. |
| `brain/` | `dump_gate_raw` | 49 | gguf_reader.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | dump_gate_expert0.c — Dump first column of gate weights for expert 0 from our loaded F32 buffer. Pri |
| `brain/` | `dump_moe_split` | 172 | math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h, wubu_ssm.h | Dump routed-only and shared-only MoE outputs for layer 0. |
| `brain/` | `ssm_layer0_dump` | 91 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_ssm.h | tools/ssm_layer0_dump.c Run SSM Layer 0 of the model for a single token and dump intermediates. |
| `brain/` | `verify_l1_weights` | 75 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h | verify_l1_weights.c — Check that L1 MoE gate weights match between our gguf_reader and direct file r |
| `brain/` | `verify_moe_weights` | 108 | gguf_reader.h, math.h, stdio.h, stdlib.h, string.h, wubu_model.h, wubu_moe.h | Verify loaded MoE weights produce correct expert 64 output. Compare against direct GGUF raw file deq |
| `bridge/` | `bridge` | 61 | bridge.h, string.h | bridge.c  --  My Seed Bridge Implementation |
| `bridge/` | `bridge_test` | 207 | bridge.h, stdio.h, string.h | bridge_test.c  --  WuBuOS Bridge/DOS Flip Test Suite Cell 103: Tests DOS flip bridge wiring (Ctrl+Al |
| `bridge/` | `vbe_ws_bridge` | 417 | stdio.h, string.h, vbe_ws_bridge.h | vbe_ws_bridge.c  --  VBE ↔ WorldSim Render Bridge Implementation Cell 070: Wires WorldSim software r |
| `bridge/` | `vbe_ws_bridge_test` | 727 | stdio.h, stdlib.h, string.h, vbe_ws_bridge.h | vbe_ws_bridge_test.c  --  Test Suite for VBE ↔ WorldSim Render Bridge Cell 070: Tests the wiring bet |
| `bridge/` | `wubu_syscall` | 514 | egojar.h, egojar_net.h, errno.h, interrupt.h, stdio.h, string.h, styx.h, time.h, | wubu_syscall.c  --  WuBuOS HolyC Syscall Bridge Implementation Registers 25 TempleOS/ZealOS-compatib |
| `bridge/` | `wubu_syscall_test` | 127 | stdio.h, string.h, vbe.h, wm.h, wubu_syscall.h | wubu_syscall_test.c  --  WuBuOS Syscall Bridge Test Suite Tests the 26 TempleOS/ZealOS-compatible sy |
| `bridge/` | `wubu_syscall_vbe` | 58 | vbe.h, wubu_syscall.h | wubu_syscall_vbe.c -- VBE syscall handlers (self-contained). sys_vbe_*: thin wrappers translating in |
| `firmware/` | `fw_acpi` | 135 | fw.h, fw_acpi.h, fw_pci.h | fw_acpi.c  --  WuBuFW ACPI table discovery and publication. On a QEMU/x86 machine the firmware norma |
| `firmware/` | `fw_acpiload` | 166 | fw.h, fw_acpi.h, fw_fwcfg.h | fw_acpiload.c  --  Execute QEMU's etc/table-loader linker script. The loader is a sequence of fixed- |
| `firmware/` | `fw_agi` | 132 | fw.h, fw_agi_attest.h, fw_secureboot.h, fw_tpm.h | fw_agi.c  --  WuBuOS AGI OS kernel shim (firmware-resident microkernel). This is the firmware-side h |
| `firmware/` | `fw_ahci` | 277 | fw.h, fw_block.h, fw_pci.h | fw_ahci.c  --  WuBuFW AHCI (SATA) driver. Real port init + command list / FIS / PRDT DMA. AHCI is ho |
| `firmware/` | `fw_ata` | 221 | fw.h, fw_block.h | fw_ata.c  --  WuBuFW ATA PIO block driver (28/48-bit LBA). QEMU's default disk on -drive if=ide is a |
| `firmware/` | `fw_block` | 45 | fw.h, fw_block.h | fw_block.c  --  WuBuFW unified block device layer. fw_media.c used to read straight from ATA PIO, wh |
| `firmware/` | `fw_bs_mem` | 255 | fw.h | fw_bs_mem.c  --  Boot services: TPL, memory, events, timers, misc. |
| `firmware/` | `fw_bs_proto` | 283 | fw.h | fw_bs_proto.c  --  Boot services: protocol database + image services. |
| `firmware/` | `fw_con` | 119 | fw.h | fw_con.c  --  WuBuFW EFI_SIMPLE_TEXT_{INPUT,OUTPUT}_PROTOCOL thunks. These are the ms_abi wrappers o |
| `firmware/` | `fw_drivers` | 81 | fw.h, fw_block.h, fw_pci.h, fw_tpm.h | fw_drivers.c  --  WuBuFW driver manager and boot-stage measurement. Binds real drivers to the device |
| `firmware/` | `fw_e1000` | 263 | fw.h, fw_block.h, fw_pci.h | fw_e1000.c  --  WuBuFW Intel 8254x (e1000) Ethernet driver. QEMU's default NIC is exactly this part  |
| `firmware/` | `fw_fsproto` | 229 | fw.h | fw_fsproto.c  --  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL / EFI_FILE_PROTOCOL bound to a fw_volume. Read-onl |
| `firmware/` | `fw_fwcfg` | 150 | fw.h, fw_fwcfg.h | fw_fwcfg.c  --  QEMU fw_cfg interface and ACPI table installation. QEMU does not place ACPI tables i |
| `firmware/` | `fw_gop` | 161 | fw.h, fw_pci.h | fw_gop.c  --  WuBuFW Graphics Output Protocol over a linear framebuffer. A gaming-console-class firm |
| `firmware/` | `fw_guid` | 38 | efi.h | fw_guid.c  --  WuBuFW well-known EFI GUID definitions. |
| `firmware/` | `fw_handle` | 109 | fw.h | ever an image hands us. |
| `firmware/` | `fw_lib` | 246 | fw.h, stdarg.h | fw_lib.c  --  WuBuFW freestanding string/mem + console (serial + VGA text). The firmware links -nost |
| `firmware/` | `fw_main` | 202 | fw.h, fw_acpi.h, fw_agi.h, fw_block.h, fw_fwcfg.h, fw_pci.h, fw_rsa.h, fw_secure | fw_main.c  --  WuBuFW entry point and boot manager. Called from reset.S once the CPU is in long mode |
| `firmware/` | `fw_media` | 526 | fw.h, fw_block.h | fw_media.c  --  WuBuFW media stack: MBR/GPT partition scan + FAT12/16/32 read-only filesystem. A UEF |
| `firmware/` | `fw_mem` | 266 | fw.h | fw_mem.c  --  WuBuFW physical memory manager. A real UEFI implementation must hand the OS a coherent |
| `firmware/` | `fw_nvme` | 215 | fw.h, fw_block.h, fw_pci.h | fw_nvme.c  --  WuBuFW NVMe driver. Admin queue + one I/O queue pair, polled completions. Enough to i |
| `firmware/` | `fw_pci` | 257 | fw.h, fw_pci.h | fw_pci.c  --  WuBuFW PCI/PCIe enumeration and resource access. Every real driver (AHCI, NVMe, XHCI,  |
| `firmware/` | `fw_pcires` | 128 | fw.h, fw_pci.h | fw_pcires.c  --  PCI resource (BAR) assignment. On a real boot there is nobody to assign BARs: QEMU  |
| `firmware/` | `fw_pe` | 200 | fw.h | fw_pe.c  --  WuBuFW PE32+ (COFF) loader for EFI applications. Loads a BOOTX64.EFI: validates MZ/PE h |
| `firmware/` | `fw_rsa` | 242 | fw_rsa.h, string.h | fw_rsa.c -- WuBuFW RSA-2048 signature verification (PKCS#1 v1.5). The Secure Boot db is hash-of-cert |
| `firmware/` | `fw_rt` | 177 | fw.h | fw_rt.c  --  WuBuFW runtime services: time, variables, reset. Variables live in a fixed RAM store (n |
| `firmware/` | `fw_secureboot` | 563 | fw.h, fw_rsa.h, fw_secureboot.h, fw_sha256.h, string.h | fw_secureboot.c  --  WuBuFW authenticated image verification (DB/dbx). Measured boot alone does not  |
| `firmware/` | `fw_sha256` | 115 | fw.h, fw_tpm.h | fw_sha256.c  --  SHA-256 for WuBuFW (measured boot + image verification). FIPS 180-4. Self-contained |
| `firmware/` | `fw_shell` | 224 | fw.h, fw_block.h, fw_pci.h, fw_secureboot.h, fw_tpm.h | fw_shell.c  --  WuBuFW interactive EFI-style shell. A real firmware ships an interactive shell: it i |
| `firmware/` | `fw_table` | 189 | fw.h | fw_table.c  --  Assemble the EFI system / boot / runtime service tables. |
| `firmware/` | `fw_time` | 84 | fw.h | fw_time.c  --  WuBuFW timing + CMOS RTC. TSC is calibrated against the PIT (channel 2 gate, no inter |
| `firmware/` | `fw_tpm` | 388 | fw.h, fw_acpi.h, fw_tpm.h, string.h | fw_tpm.c  --  WuBuFW TPM 2.0 driver (TIS/FIFO + CRB) and measured boot. Kernel-level anti-cheat (EAC |
| `firmware/` | `fw_tpmlog` | 119 | fw.h, fw_tpm.h | fw_tpmlog.c  --  TCG 2.0 crypto-agile event log. The event log is what makes PCR values *meaningful* |
| `firmware/` | `fw_xhci` | 128 | fw.h, fw_pci.h | fw_xhci.c  --  WuBuFW USB 3 (xHCI) host controller driver. Firmware-scope USB: bring the controller  |
| `firmware/` | `loader` | 355 | efi.h, fw_agi_attest.h, sha256.h, stddef.h, stdint.h | loader.c -- WuBuOS WuBuFW chainloader (the REAL kernel as measured payload). This is the bridge betw |
| `firmware/` | `sha256` | 94 | sha256.h, string.h | sha256.c -- self-contained SHA-256 for the WuBuOS EFI chainloader. FIPS 180-4. Freestanding C11 (onl |
| `firmware/` | `hello` | 243 | efi.h | hello.c  --  WuBuOS test EFI application. Exercises the firmware for real: ConOut, memory map, Alloc |
| `firmware/` | `mkesp` | 340 | stdint.h, stdio.h, stdlib.h, string.h | mkesp.c  --  WuBuOS FAT32 ESP image builder (C11, self-contained). Creates a GPT-partitioned disk im |
| `firmware/` | `mkpe` | 191 | stdint.h, stdio.h, stdlib.h, string.h | mkpe.c  --  WuBuOS PE32+ EFI image builder (C11, no toolchain deps). GCC on Linux cannot emit PE32+, |
| `firmware/` | `mksb` | 336 | stdint.h, stdio.h, stdlib.h, string.h | mksb.c -- WuBuOS Secure Boot key/enrollment tool (sbctl equivalent). The firmware's Secure Boot veri |
| `firmware/` | `test_fw_rsa` | 83 | fw_rsa.h, stdio.h, string.h | test_fw_rsa.c -- host test for fw_rsa2048_verify_pkcs1. Vector generated with: openssl genrsa 2048;  |
| `firmware/` | `wubu_gop_modes` | 102 | efi.h, string.h, wubu_gop_modes.h | wubu_gop_modes.c -- see wubu_gop_modes.h. Layout of EFI_GRAPHICS_OUTPUT_MODE_INFORMATION (UEFI spec  |
| `firmware/` | `wubu_gop_modes_test` | 127 | stdio.h, string.h, wubu_gop_modes.h | wubu_gop_modes_test.c -- gate for firmware GOP mode decoding. The RESOLUTIONS are this machine's rea |
| `framework/` | `wubufx` | 220 | stdio.h, stdlib.h, string.h, wubu_edr.h, wubu_holyd.h, wubufx.h | wubufx.c -- WuBuFX: WuBuOS Application Framework (implementation) Self-contained. Composition root w |
| `framework/` | `wubufx_apps` | 182 | apps.h, bonzi.h, comfy.h, stdio.h, stdlib.h, string.h, tandem.h, wm.h, wubu_edr. | wubufx_apps.c -- WuBuFX application registry (real engines, no placeholders) Phase C binding: the de |
| `framework/` | `wubufx_apps_test` | 60 | stdio.h, string.h, wm.h, wubufx.h, wubufx_apps.h | wubufx_apps_test.c -- WuBuFX real-app binding regression tests Proves Phase C: every registered app  |
| `framework/` | `wubufx_test` | 99 | stdint.h, stdio.h, string.h, wubu_edr.h, wubufx.h | wubufx_test.c -- WuBuFX framework regression tests Verifies the namespace-first contract end to end: |
| `gui/` | `bpm` | 129 | era_apps.h, stdio.h, stdlib.h, string.h | bpm.c -- BIG PICTURE MODE (the SteamOS Big Picture shell, in our code). SteamOS's Big Picture Mode i |
| `gui/` | `bpm_games` | 150 | bpm.h, dirent.h, egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, vbe.h, wm. | bpm_games.c -- Big Picture Mode's GAMES grid (the real games, not just the era apps). The BPM grid i |
| `gui/` | `gui_dbuf` | 292 | gui_dbuf.h, stdlib.h, string.h | gui_dbuf.c  --  WuBuOS Double-Buffered GUI Renderer Implementation Cell 101: Flicker-free rendering  |
| `gui/` | `gui_dbuf_test` | 343 | gui_dbuf.h, stdio.h, string.h | gui_dbuf_test.c  --  Test Suite for Double-Buffered GUI Renderer Cell 101: Tests double buffering, d |
| `gui/` | `wubu_compositor` | 495 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, styx.h, vulkan.h, wayland-s | wubu_compositor.c  --  WuBuOS Wayland Compositor Implementation Minimal Wayland compositor using lib |
| `gui/` | `wubu_compositor_standalone` | 412 | egl.h, egojar.h, egojar_net.h, gl2.h, stdio.h, stdlib.h, string.h, wayland-clien | wubu_compositor_standalone.c  --  Standalone Compositor Implementation Implements the standalone com |
| `gui/` | `wubu_compositor_test` | 135 | stdio.h, stdlib.h, string.h, styx.h, wubu_compositor.h | wubu_compositor_test.c -- Test for WuBuOS Compositor |
| `gui/` | `controlpanel` | 198 | controlpanel.h, stdio.h, string.h | controlpanel.c -- the Control Panel MANAGER (Win98/XP vibes, the applet registry). The header declar |
| `gui/` | `cp_display` | 112 | controlpanel.h, stdio.h, stdlib.h, string.h, wubu_world.h | cp_display.c -- the Control Panel Display applet (the SteamOS System Settings > Display, in our code |
| `gui/` | `cp_hardware` | 149 | controlpanel.h, stdio.h, stdlib.h, string.h | cp_hardware.c -- the Control Panel Hardware applet (the desktop shows the OS's hardware — the driver |
| `gui/` | `cp_network` | 95 | controlpanel.h, stdio.h, stdlib.h, string.h, wubu_world.h | cp_network.c -- the Control Panel Network applet (the SteamOS System Settings > Network, in our code |
| `gui/` | `cp_sound` | 147 | controlpanel.h, stdlib.h, string.h, wubu_audio.h, wubu_sound.h | cp_sound.c -- the Control Panel Sound applet (Win98/XP vibes). C11. Implements the header-declared c |
| `gui/` | `cp_theme` | 96 | controlpanel.h, stdio.h, stdlib.h, string.h, wubu_theme.h | cp_theme.c -- the Control Panel Theme applet (the SteamOS Personalization, in our code — the Win98/X |
| `gui/` | `daemon_panel` | 402 | daemon_panel.h, desktop.h, stdio.h, stdlib.h, string.h, vbe.h, wm.h, wubu_archd. | daemon_panel.c  --  WuBuOS Desktop Daemon Integration Panel Cell 400-402: Bridges wubu_archd and wub |
| `gui/` | `daemon_panel_test` | 124 | daemon_panel.h, stdint.h, stdio.h, stdlib.h, string.h | daemon_panel_test.c  --  Daemon Panel Unit Test Suite Tests daemon panel logic with stubbed GUI depe |
| `gui/` | `dos_window` | 159 | stdint.h, stdio.h, stdlib.h, string.h, vbe.h, wm.h, wubu_dos_proc.h | dos_window.c -- Render a WuBuOS 16-bit DOS process in a desktop window. The window blits the guest's |
| `gui/` | `dos_window_test` | 106 | dos_window.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_dos_ | dos_window_test.c -- Real end-to-end test for the DOS Box window. Exercises the genuine engine path: |
| `gui/` | `dos_window_test_stub` | 24 | stdlib.h, string.h, wm.h | dos_window_test_stub.c -- Minimal WM window primitive stubs for the DOS Box window test. We are test |
| `gui/` | `era_apps` | 289 | era_apps.h, guest_file.h, startmenu.h, startmenu_internal.h, stdio.h, stdlib.h,  | era_apps.c -- One representative application per computing era, each tagged with the VSL syscall per |
| `gui/` | `era_apps_test` | 110 | egojar.h, era_apps.h, startmenu.h, stdio.h, stdlib.h, string.h | era_apps_test.c -- regression test for the "one app per era" registry + launcher. Asserts that each  |
| `gui/` | `era_apps_test_stub` | 75 | era_apps.h, string.h | era_apps_test_stub.c -- the era-apps STUB for the BPM test. The real era_apps.c pulls the whole VSL  |
| `gui/` | `explorer` | 537 | ctype.h, dirent.h, egojar.h, errno.h, explorer.h, explorer_internal.h, grp.h, gu | explorer.c  --  WuBuOS File Manager (Win98/XP Explorer Shell) Phase 5: Full-featured file manager wi |
| `gui/` | `explorer_drives` | 63 | explorer_internal.h, stdio.h, string.h | explorer_drives.c -- Drive / volume enumeration subsystem. Self-contained: enumerates mounted volume |
| `gui/` | `explorer_format` | 65 | explorer.h, stdio.h, string.h, time.h | explorer_format.c -- Shared type/format helpers for the explorer. These are PUBLIC API (declared in  |
| `gui/` | `explorer_fs` | 65 | dirent.h, egojar.h, stdint.h, string.h | explorer_fs.c -- 9P/Styx filesystem backend for the WuBuOS Explorer. Self-contained module extracted |
| `gui/` | `explorer_fsops` | 278 | dirent.h, egojar.h, errno.h, explorer_internal.h, guest_file.h, stdio.h, stdlib. | explorer_fsops.c -- Explorer file-operation workers. Two backends, one decision per path: guest stor |
| `gui/` | `explorer_info` | 126 | egojar.h, explorer.h, explorer_internal.h, guest_file.h, stdio.h, string.h | explorer_info.c -- WuBuOS explorer: file metadata extraction. Extracted from explorer.c (separable l |
| `gui/` | `explorer_input` | 457 | ctype.h, explorer.h, explorer_internal.h, stdio.h, string.h, vbe.h, wubu_mime.h | explorer_input.c -- Explorer input handling (keyboard + mouse) Extracted from explorer.c (the monoli |
| `gui/` | `explorer_ops` | 245 | egojar.h, errno.h, explorer.h, explorer_internal.h, stdint.h, stdio.h, stdlib.h, | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `gui/` | `explorer_preview` | 66 | egojar.h, explorer_internal.h, string.h | explorer_preview.c -- File preview subsystem. Self-contained: builds the preview metadata/text/image |
| `gui/` | `explorer_render` | 515 | explorer_internal.h, stdint.h, stdio.h, string.h, vbe.h, wm.h | explorer_render.c  --  Win98 Explorer render layer Extracted from explorer.c — all drawing/rendering |
| `gui/` | `explorer_test` | 703 | assert.h, explorer.h, explorer_internal.h, stdint.h, stdio.h, stdlib.h, string.h | explorer_test.c  --  Test suite for explorer |
| `gui/` | `explorer_test_stub` | 105 | dirent.h, egojar.h, explorer.h, stdlib.h, string.h, time.h, wm.h | explorer_test_stub.c  --  Stub functions for explorer tests Provides minimal implementations of exte |
| `gui/` | `explorer_tree` | 218 | explorer_internal.h, stdio.h, stdlib.h, string.h | explorer_tree.c -- Tree view subsystem + shared entry helpers. Owns directory-tree population/find/f |
| `gui/` | `explorer_zip` | 332 | ctype.h, egojar.h, explorer_internal.h, stdio.h, stdlib.h, string.h, time.h, vbe | explorer_zip.c  --  WuBuOS Explorer Zip Archive Module Extracted from explorer.c lines 667-979 Self- |
| `gui/` | `wubu_lua_gui` | 314 | stdio.h, stdlib.h, string.h, vbe.h, wb_lv.h, wubu_lua_gui.h | wubu_lua_gui.c -- the C11 host for Lua-driven GUI. See wubu_lua_gui.h.  The interpreter is slerm-Lua |
| `gui/` | `wubu_lua_gui_test` | 216 | stdarg.h, stdio.h, stdlib.h, string.h, vbe.h, wubu_lua_gui.h | wubu_lua_gui_test.c -- verifies the C11 Lua GUI host. A Lua GUI is a file like any other, so these t |
| `gui/` | `mime_contract_test` | 178 | stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_mime.h, wubu_mime_internal.h | mime_contract_test.c -- hostile-input contract sweep for the .desktop parser. Shortcut icons resolve |
| `gui/` | `mime_fuzz` | 176 | fuzz_rng.h, stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_mim | mime_fuzz.c -- deterministic mutation fuzzer for the .desktop / MIME entry parser. WHY THIS PARSER S |
| `gui/` | `service_mgr` | 173 | ctype.h, service_mgr.h, stdio.h, stdlib.h, string.h, wubu_notify.h | service_mgr.c -- Desktop-side service/autostart manager (E3). Real wiring of wubu_archd (16/16) as t |
| `gui/` | `service_mgr_test` | 76 | assert.h, service_mgr.h, stdio.h, string.h | service_mgr_test.c -- Regression test for E3 integration: wubu_archd wired as the Desktop's autostar |
| `gui/` | `service_mgr_test_stub` | 17 | egojar.h, wubu_notify.h | service_mgr_test_stub.c -- link-time stub for service_mgr_test. service_mgr.c calls wubu_notify_send |
| `gui/` | `desktop` | 340 | apps.h, daemon_panel.h, desktop.h, egojar.h, egojar_net.h, era_apps.h, hosted.h, | desktop.c  --  WuBuOS DosGui Desktop Implementation Cell 401: THEMED Win98/XP desktop with launchabl |
| `gui/` | `desktop_stubs` | 33 | stdbool.h, stddef.h | desktop_stubs.c -- weak glue so WM-bearing tests can link without the shell. wm_input.c dispatches i |
| `gui/` | `power_menu` | 132 | power_menu.h, string.h, vbe.h, wubu_theme.h | power_menu.c -- the Win+X power-user menu (Windows 11 parity). Self-contained: state, geometry, key/ |
| `gui/` | `quick_settings` | 140 | stdio.h, vbe.h, wm.h, wm_internal.h, wubu_backlight.h, wubu_bt.h, wubu_drv_net.h | quick_settings.c -- Win+A quick settings panel Windows 11's Win+A panel is the model: one overlay li |
| `gui/` | `run_dialog` | 154 | apps.h, guest_file.h, notepad.h, run_dialog.h, stdio.h, stdlib.h, string.h, vbe. | THIS RUNS, AND WHAT IT DOES NOT ISOLATE |
| `gui/` | `startmenu` | 1112 | apps.h, ctype.h, desktop.h, dirent.h, egojar.h, egojar_net.h, era_apps.h, socket | startmenu.c  --  WuBuOS DosGui Start Menu Implementation Cell 402: THEMED Cascading Start Menu. Win9 |
| `gui/` | `startmenu_db` | 257 | apps.h, ctype.h, desktop.h, dirent.h, egojar.h, startmenu.h, startmenu_internal. | startmenu_db.c  --  WuBuOS Start Menu model constructor Extracted from startmenu.c (Cell 402).  Owns |
| `gui/` | `startmenu_power` | 45 | startmenu_internal.h, stdio.h | startmenu_power.c -- Power options subsystem for the Start menu. Self-contained: power action handli |
| `gui/` | `startmenu_search` | 112 | ctype.h, startmenu_internal.h, stdlib.h, string.h, wubu_theme.h | startmenu_search.c -- Start menu search + recent apps. Self-contained subsystem extracted from start |
| `gui/` | `startmenu_tree` | 105 | startmenu_internal.h, stdlib.h, string.h, wubu_theme.h | startmenu_tree.c -- Start menu program-tree (All Programs). Self-contained subsystem extracted from  |
| `gui/` | `taskbar_tasks` | 211 | egojar.h, stdio.h, stdlib.h, string.h, taskbar_tasks.h | taskbar_tasks.c -- the taskbar model.  See the header. Deliberately free of GUI concerns: no VBE, no |
| `gui/` | `taskbar_tasks_test` | 141 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, taskbar_tasks.h | taskbar_tasks_test.c -- the taskbar model, headless. A user's taskbar is the one piece of desktop st |
| `gui/` | `win11_shell` | 48 | input.h, win11_shell.h, win11_snap.h, win11_widgets.h | win11_shell.c -- strong definitions of the Win11 shell glue. One concern: route WM key/press/render  |
| `gui/` | `win11_snap` | 186 | string.h, vbe.h, win11_snap.h, wm.h, wm_internal.h, wubu_theme.h | CHANGED FROM THE OLD PICKER |
| `gui/` | `win11_widgets` | 121 | stdio.h, string.h, time.h, vbe.h, win11_widgets.h, wm.h, wm_internal.h, wubu_drv | win11_widgets.c -- Win+W widgets board (Windows 11 parity). One concern: the widgets board.  Cards a |
| `gui/` | `window_chrome` | 476 | string.h, vbe.h, window_chrome.h, wm.h, wm_internal.h, wubu_theme.h, wubu_tokens | window_chrome.c  --  Standardized Window Chrome for WuBuOS Centralized drawing of ALL window decorat |
| `gui/` | `wm_clock` | 131 | stdio.h, string.h, time.h, wm_internal.h | wm_clock.c -- WuBuOS DosGui WM: taskbar clock + clock menu Self-contained concern split out of wm_ct |
| `gui/` | `wm_ctxmenu` | 480 | stdio.h, stdlib.h, string.h, wm_internal.h, wubu_notify.h, wubu_trash.h | wm_ctxmenu.c  --  WuBuOS DosGui WM: context-menu action wiring Facade that binds the generic context |
| `gui/` | `wm_ctxmenu_engine` | 196 | wm_internal.h | wm_ctxmenu_engine.c -- WuBuOS DosGui WM: context-menu engine Self-contained concern split out of wm_ |
| `gui/` | `wm_desktop` | 381 | dirent.h, egojar.h, errno.h, fcntl.h, stdlib.h, strings.h, wm_internal.h, wubu_g | desktop_icons.c -- Desktop icons subsystem for the WuBuOS WM. Self-contained module extracted from wm.c |
| `gui/` | `wm_holyc_term` | 187 | holyd_codegen.h, stdio.h, stdlib.h, string.h, holyc_repl.h, wm_internal.h | holyc_repl.c  --  HolyC Terminal subsystem for wm Provides a REPL terminal window for HolyC scrip |
| `gui/` | `wm_icon_glyphs` | 135 | stdint.h, string.h, wm.h, wm_internal.h | wm_icon_glyphs.c -- Desktop icon glyph rendering (Chicago -> XP). The old desktop drew every icon as |
| `gui/` | `wm_icons` | 96 | wm_internal.h | wm_icons.c -- Desktop icon subsystem for the window manager. Self-contained: desktop icon grid manag |
| `gui/` | `wm_overview` | 101 | stdio.h, vbe.h, wm.h, wm_internal.h, wubu_theme.h | wm_overview.c -- Win+Tab desktop overview grid Virtual desktops existed with no way to see them: you |
| `gui/` | `wm_rim` | 205 | string.h, taskbar_tasks.h, wm_internal.h, wm_rim.h, wubu_rim.h, wubu_spring.h | wm_rim.c -- see wm_rim.h. The rim wraps the FOCUSED window: every tick the rim rect is re-bound to t |
| `gui/` | `wm_slime` | 189 | string.h, taskbar_tasks.h, wm_internal.h, wm_slime.h, wubu_slime.h, wubu_spring. | wm_slime.c -- see wm_slime.h. The seam wraps the SCREEN (unlike wm_rim, which wraps the focused wind |
| `gui/` | `wm_systray` | 229 | wm_internal.h, wubu_notify.h | wm_systray.c  --  System Tray + Notification Center Extracted from wm.c for modularity. |
| `gui/` | `wm_taskbar` | 508 | startmenu.h, taskbar_tasks.h, time.h, wm_internal.h | wm_taskbar.c -- Taskbar render + geometry. Self-contained: renders the taskbar (Start button, window |
| `gui/` | `wm_tray_world` | 91 | stdio.h, string.h, wm.h, wubu_world.h | wm_tray_world.c -- the WORLD-STATE tray indicators (the SteamOS desktop's tray: battery, network, he |
| `gui/` | `wubu_lock_screen` | 65 | input.h, stdio.h, time.h, vbe.h, wm.h, wm_internal.h, wubu_lock_screen.h, wubu_t | wubu_lock_screen.c -- Win+L lock screen. See wubu_lock_screen.h.  The overlay is opaque and drawn ab |
| `gui/` | `wubu_notify` | 422 | egojar.h, stdio.h, stdlib.h, string.h, time.h, vbe.h, wm.h, wubu_notify.h, wubu_ | wubu_notify.c  --  WuBuOS Notification Daemon Implementation Phase 2: libnotify-compatible notificat |
| `gui/` | `wubu_osd` | 110 | stdio.h, string.h, vbe.h, wm.h, wm_internal.h, wubu_osd.h, wubu_theme.h | wubu_osd.c -- system volume / brightness state and its overlay. See wubu_osd.h.  The overlay is a si |
| `gui/` | `wubu_screensaver` | 70 | stdio.h, time.h, vbe.h, wm.h, wm_internal.h, wubu_screensaver.h, wubu_theme.h | wubu_screensaver.c -- idle screen saver. See wubu_screensaver.h.  Engages after a timeout with no in |
| `gui/` | `wubu_shell_greeter` | 593 | apps.h, dirent.h, stdio.h, string.h, unistd.h, vbe.h, wm.h, wubu_account.h, wubu | wubu_shell_greeter.c -- GAP F: the visible first-boot greeter. A real modal dialog on the * stack: t |
| `gui/` | `wubu_shell_greeter_test` | 202 | egojar.h, stdio.h, stdlib.h, string.h, vbe.h, wm.h, wubu_onboarding.h, wubu_sett | wubu_shell_greeter_test.c -- GAP F behaviour gate. Proves the first-boot greeter is a REAL screen on |
| `gui/` | `wubu_wallpaper` | 255 | egojar.h, stdio.h, stdlib.h, string.h, wubu_image_ffmpeg.h, wubu_wallpaper.h | wubu_wallpaper.c -- WuBuOS Wallpaper Decoder + Placement Real implementation: decodes a BMP (24/32 b |
| `gui/` | `standalone_hosted_shim` | 34 | hosted.h, wm.h | standalone_hosted_shim.c -- standalone app shim for hosted-only symbols. The WuBuOS GUI references t |
| `gui/` | `startmenu_files_test` | 120 | guest_file.h, startmenu.h, startmenu_internal.h, stdio.h, stdlib.h, string.h, un | startmenu_files_test.c -- Start search must find FILES, not just apps. A user who just saved a docum |
| `gui/` | `startmenu_test` | 161 | apps.h, egojar.h, startmenu.h, startmenu_internal.h, stdio.h, stdlib.h, string.h | startmenu_test.c  --  WuBuOS DosGui Start Menu Test Suite (Cell 402) |
| `gui/` | `startmenu_test_stub` | 130 | startmenu.h, stdbool.h, wm.h, wubu_bonzi.h, wubu_dos_proc.h, wubu_exec.h, wubu_h | startmenu_test_stub.c  --  Stubs for startmenu_test without desktop integration |
| `gui/` | `term` | 530 | ctype.h, egojar.h, errno.h, pty.h, stdio.h, stdlib.h, string.h, term.h, term_int | term.c  --  WuBuOS Terminal (PTY + HolyC REPL + Tabbed) Phase 6: Full-featured terminal with: - PTY  |
| `gui/` | `term_ansi` | 447 | egojar.h, egojar_net.h, errno.h, string.h, term_internal.h, vbe.h | term_ansi.c  --  WuBuOS Terminal shared VT100/ANSI parser Extracted from term.c (2026-07-05): two ne |
| `gui/` | `term_pty` | 582 | ctype.h, egojar.h, errno.h, pty.h, stdbool.h, stdio.h, stdlib.h, string.h, term_ | term_pty.c  --  WuBuOS Terminal PTY Backend Implementation Extracted from term.c (2026-07-05): PTY s |
| `gui/` | `term_pty_exec_test` | 63 | stdio.h, string.h, term.h, term_pty.h, unistd.h | term_pty_exec_test.c -- prove an ARBITRARY PROGRAM can be started on a PTY. Every pre-existing entry |
| `gui/` | `term_render` | 193 | string.h, term_internal.h, vbe.h | term_render.c -- WuBuOS terminal rendering layer. Self-contained module extracted from term.c. Draws |
| `gui/` | `term_tabs` | 173 | stdio.h, stdlib.h, string.h, term_internal.h | term_tabs.c -- Terminal tab-management subsystem. Self-contained: tab creation/close/switch/move + a |
| `gui/` | `term_test` | 274 | ctype.h, egojar.h, pty.h, stdio.h, stdlib.h, string.h, term.h, term_internal.h,  | term_test.c  --  WuBuOS Terminal Test Suite Tests PTY backend, ANSI parser, scrollback, copy/paste,  |
| `gui/` | `term_test_stub` | 80 | stdio.h, stdlib.h, string.h, term.h, wm.h | term_test_stub.c  --  Stubs for term_test without full dependencies Provides minimal implementations |
| `gui/` | `test_cp_sound` | 8 | stdio.h | test_cp_sound.c — Control Panel sound applet (stub) |
| `gui/` | `test_synth` | 9 | stdio.h | test_synth.c — audio synth self-test (stub) |
| `gui/` | `test_wubu_gui_metrics` | 79 | stdio.h, stdlib.h, wubu_gui_metrics.h | test_wubu_gui_metrics.c -- the metric invariants, proven. If a surface ever starts inventing its own |
| `gui/` | `test_wubu_sound` | 9 | stdio.h | test_wubu_sound.c — sound system self-test (stub) |
| `gui/` | `wb_lv_arena` | 40 | stdlib.h, string.h, wb_lv_internal.h | wb_lv_arena.c -- the interpreter's arena, a bump-allocating slab pool. Every interpreter allocation  |
| `gui/` | `wb_lv_coro` | 315 | stdio.h, stdlib.h, string.h, wb_lv_internal.h, windows.h | wb_lv_coro.c -- coroutines, on Windows fibers. A tree-walker lives on the C stack, so a coroutine ne |
| `gui/` | `wb_lv_eval` | 1289 | errno.h, math.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_eval.c -- the tree-walking evaluator: scopes, calls, operators with metamethod dispatch, multi |
| `gui/` | `wb_lv_host` | 643 | math.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_host.c -- the interpreter's public driver: state lifecycle, the base library, run/call entry p |
| `gui/` | `wb_lv_lex` | 417 | math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_lex.c -- the token stream: whitespace/comments, numbers (decimal + hex), names/keywords, strin |
| `gui/` | `wb_lv_parse` | 717 | stdlib.h, string.h, wb_lv_internal.h | wb_lv_parse.c -- the grammar: expressions, statements and blocks, plus the AST pool. Lua 5.4 syntax  |
| `gui/` | `wb_lv_stdlib` | 1082 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_lv_internal.h | wb_lv_stdlib.c -- the table / math / os / io libraries. See wb_lv_internal.h. |
| `gui/` | `wb_lv_string` | 1084 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_string.c -- the string library, including a Lua-pattern matcher. See wb_lv_internal.h. |
| `gui/` | `wb_lv_value` | 323 | math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_value.c -- values, string interning and tables. See wb_lv_internal.h. Tables are open-addresse |
| `gui/` | `accept_wua` | 39 | stdio.h, stdlib.h, wb_lv.h, wb_lv_internal.h | accept_wua.c -- parse-only conformance probe for Wua (the analogue of `luac -p`): exit 0 if the file |
| `gui/` | `wb_lv_script_test` | 108 | stdio.h, stdlib.h, string.h, wb_lv.h | wb_lv_script_test.c -- drive engine-style bindings through wb_lv. Proves the VM meets the gamemode c |
| `gui/` | `wb_lv_test` | 246 | stdio.h, string.h, wb_lv.h | wb_lv_test.c -- verification of the slerm-Lua interpreter (slice 8). Runs real programs through wb_l |
| `gui/` | `vbe_shim` | 30 | string.h, vbe_interface.h | vbe_shim.c -- a headless stand-in for the WuBuOS VBE. The standalone repo builds and tests without a |
| `gui/` | `wet_app` | 125 | stdio.h, stdlib.h, string.h, wet_internal.h | wet_app.c -- the applet application. WetApp is the .NET Application object: it owns the widget tree, |
| `gui/` | `wet_backend_vbe` | 61 | vbe_interface.h, wet_internal.h | wet_backend_vbe.c -- the WuBuOS VBE backend for .WET. One backend, one job: turn the framework's Wet |
| `gui/` | `wet_controls` | 672 | stdlib.h, string.h, wet_internal.h | wet_controls.c -- the leaf controls. Each one is a class: init, measure, arrange, render, input, des |
| `gui/` | `wet_input` | 100 | stdlib.h, string.h, wet_internal.h | wet_input.c -- hit-testing and event routing. Input enters at the root and walks DOWN the tree to th |
| `gui/` | `wet_layout` | 428 | stdlib.h, string.h, wet_internal.h | wet_layout.c -- the panels: Stack, Dock, Canvas, Grid. Every panel runs the same two passes.  measur |
| `gui/` | `wet_lua` | 713 | stdio.h, stdlib.h, string.h, wb_lv.h, wet_internal.h, wet_lua.h | wet_lua.c -- the Lua half of the .wet framework. A .wet applet is a Lua script that calls wet.applet |
| `gui/` | `wet_lua_test` | 332 | stdio.h, stdlib.h, string.h, wb_lv.h, wet_internal.h, wet_lua.h | wet_lua_test.c -- behaviour gates for the .wet Lua binding. Builds a real applet from a real script  |
| `gui/` | `wet_render` | 56 | stdlib.h, string.h, wet_internal.h | wet_render.c -- the render primitives. Every control draws through these, and they clip against the  |
| `gui/` | `wet_selftest` | 528 | stdio.h, stdlib.h, string.h, wet_internal.h | wet_selftest.c -- behaviour gates for the .wet framework. Runs headless: the VBE entry points are st |
| `gui/` | `wet_widget` | 278 | stdio.h, stdlib.h, string.h, wet_internal.h | wet_widget.c -- the widget tree, the class registry, text measurement and the theme palette.  Owns n |
| `gui/` | `vws_cli` | 104 | stdio.h, stdlib.h, string.h, vws_templates.h | vws_cli.c -- the VWS command line. The designer's template library is reachable without opening a wi |
| `gui/` | `vws_designer_test` | 113 | stdio.h, stdlib.h, string.h, vws_internal.h, vws_lua.h, wet.h, wet_lua.h | vws_designer_test.c -- the designer applet, run for real. The designer is a .wet applet, so the test |
| `gui/` | `vws_emit` | 125 | stdarg.h, stdio.h, stdlib.h, string.h, vws_internal.h | vws_emit.c -- turn a VwsForm into a runnable .wet Lua script. The output is a plain .WET applet, so  |
| `gui/` | `vws_form` | 324 | stdio.h, stdlib.h, string.h, vws_internal.h, vws_templates.h | vws_form.c -- the designer's document: the form, its controls, and the edits a designer surface make |
| `gui/` | `vws_load` | 150 | stdio.h, stdlib.h, string.h, vws_internal.h, wet.h, wet_lua.h | vws_load.c -- turn a .vws file back into a VwsForm. A .vws file is a Lua script, so loading is: read |
| `gui/` | `vws_lua` | 390 | stdio.h, stdlib.h, string.h, vws_internal.h, vws_lua.h, wb_lv.h | vws_lua.c -- the Lua face of the VWS document model. The designer is written in Lua over .WET and dr |
| `gui/` | `vws_selftest` | 229 | stdio.h, stdlib.h, string.h, vws_internal.h, wet.h | vws_selftest.c -- behaviour gates for Visual WuBu System. The contract under test is the round trip: |
| `gui/` | `vws_templates` | 464 | stdarg.h, stdio.h, stdlib.h, string.h, vws_templates.h | vws_templates.c -- the template library's data and its emitter. Templates are DATA, not eight copy-p |
| `gui/` | `vws_templates_test` | 134 | stdio.h, stdlib.h, string.h, unistd.h, vws.h, vws_templates.h | vws_templates_test.c -- the template library's gate. A template is only useful if it is a real progr |
| `gui/` | `wb_lv_arena` | 40 | stdlib.h, string.h, wb_lv_internal.h | wb_lv_arena.c -- the interpreter's arena, a bump-allocating slab pool. Every interpreter allocation  |
| `gui/` | `wb_lv_coro` | 315 | stdio.h, stdlib.h, string.h, wb_lv_internal.h, windows.h | wb_lv_coro.c -- coroutines, on Windows fibers. A tree-walker lives on the C stack, so a coroutine ne |
| `gui/` | `wb_lv_eval` | 1297 | errno.h, math.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_eval.c -- the tree-walking evaluator: scopes, calls, operators with metamethod dispatch, multi |
| `gui/` | `wb_lv_host` | 659 | math.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_host.c -- the interpreter's public driver: state lifecycle, the base library, run/call entry p |
| `gui/` | `wb_lv_lex` | 417 | math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_lex.c -- the token stream: whitespace/comments, numbers (decimal + hex), names/keywords, strin |
| `gui/` | `wb_lv_parse` | 717 | stdlib.h, string.h, wb_lv_internal.h | wb_lv_parse.c -- the grammar: expressions, statements and blocks, plus the AST pool. Lua 5.4 syntax  |
| `gui/` | `wb_lv_stdlib` | 1082 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_lv_internal.h | wb_lv_stdlib.c -- the table / math / os / io libraries. See wb_lv_internal.h. |
| `gui/` | `wb_lv_string` | 1084 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_string.c -- the string library, including a Lua-pattern matcher. See wb_lv_internal.h. |
| `gui/` | `wb_lv_value` | 323 | math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_value.c -- values, string interning and tables. See wb_lv_internal.h. Tables are open-addresse |
| `gui/` | `accept_wua` | 39 | stdio.h, stdlib.h, wb_lv.h, wb_lv_internal.h | accept_wua.c -- parse-only conformance probe for Wua (the analogue of `luac -p`): exit 0 if the file |
| `gui/` | `wb_lv_script_test` | 108 | stdio.h, stdlib.h, string.h, wb_lv.h | wb_lv_script_test.c -- drive engine-style bindings through wb_lv. Proves the VM meets the gamemode c |
| `gui/` | `wb_lv_test` | 382 | stdio.h, string.h, wb_lv.h | wb_lv_test.c -- verification of the slerm-Lua interpreter (slice 8). Runs real programs through wb_l |
| `gui/` | `vbe_shim` | 30 | string.h, vbe_interface.h | vbe_shim.c -- a headless stand-in for the WuBuOS VBE. The standalone repo builds and tests without a |
| `gui/` | `wet_app` | 125 | stdio.h, stdlib.h, string.h, wet_internal.h | wet_app.c -- the applet application. WetApp is the .NET Application object: it owns the widget tree, |
| `gui/` | `wet_backend_vbe` | 61 | vbe_interface.h, wet_internal.h | wet_backend_vbe.c -- the WuBuOS VBE backend for .WET. One backend, one job: turn the framework's Wet |
| `gui/` | `wet_controls` | 672 | stdlib.h, string.h, wet_internal.h | wet_controls.c -- the leaf controls. Each one is a class: init, measure, arrange, render, input, des |
| `gui/` | `wet_input` | 100 | stdlib.h, string.h, wet_internal.h | wet_input.c -- hit-testing and event routing. Input enters at the root and walks DOWN the tree to th |
| `gui/` | `wet_layout` | 428 | stdlib.h, string.h, wet_internal.h | wet_layout.c -- the panels: Stack, Dock, Canvas, Grid. Every panel runs the same two passes.  measur |
| `gui/` | `wet_lua` | 766 | stdio.h, stdlib.h, string.h, wb_lv.h, wet_internal.h, wet_lua.h | wet_lua.c -- the Lua half of the .wet framework. A .wet applet is a Lua script that calls wet.applet |
| `gui/` | `wet_lua_test` | 365 | stdio.h, stdlib.h, string.h, wb_lv.h, wet_internal.h, wet_lua.h | wet_lua_test.c -- behaviour gates for the .wet Lua binding. Builds a real applet from a real script  |
| `gui/` | `wet_render` | 56 | stdlib.h, string.h, wet_internal.h | wet_render.c -- the render primitives. Every control draws through these, and they clip against the  |
| `gui/` | `wet_selftest` | 570 | stdio.h, stdlib.h, string.h, wet_internal.h | wet_selftest.c -- behaviour gates for the .wet framework. Runs headless: the VBE entry points are st |
| `gui/` | `wet_widget` | 380 | stdio.h, stdlib.h, string.h, wet_internal.h | wet_widget.c -- the widget tree, the class registry, text measurement and the theme palette.  Owns n |
| `gui/` | `wm` | 234 | dirent.h, egojar.h, egojar_net.h, errno.h, socket.h, wm_internal.h, wm_rim.h, wm | wm.c  --  WuBuOS DosGui Window Manager facade Cell 400: Fable Windowing Agent — THEMED EDITION. Port |
| `gui/` | `wm_input` | 727 | desktop.h, input.h, power_menu.h, run_dialog.h, startmenu.h, stdio.h, string.h,  | wm_input.c -- WuBuOS DosGui WM: input dispatch (key + mouse) Self-contained concern split out of wm. |
| `gui/` | `wm_layout` | 405 | stdint.h, string.h, wm.h, wm_internal.h, wubu_theme.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `gui/` | `wm_render` | 322 | power_menu.h, run_dialog.h, stdbool.h, stdint.h, stdlib.h, string.h, vbe.h, win1 | wm_render.c -- WuBuOS WM rendering (single module) ONE entry point: wm_render(fb, w, h). It draws th |
| `gui/` | `wm_systray_test_stub` | 51 | stdint.h, string.h, wm.h | wm_systray_test_stub.c -- the systray STUB for the world-state tray test. The real wm_systray.c pull |
| `gui/` | `wm_test` | 1709 | dirent.h, egojar.h, input.h, startmenu.h, stdio.h, stdlib.h, string.h, vbe.h, wi | wm_test.c  --  WuBuOS DosGui Window Manager Test Suite Cell 400: Fable Windowing Agent test suite. T |
| `gui/` | `wm_test_stub` | 302 | hosted.h, startmenu.h, stdbool.h, string.h, vbe.h, wm.h, wubu_bonzi.h, wubu_dos_ | wm_test_stub.c  --  Stubs for WM unit tests without full desktop integration. Provides minimal imple |
| `gui/` | `wm_window` | 304 | stdio.h, string.h, wm_internal.h, wubu_settings.h | wm_window.c -- WuBuOS DosGui WM: window lifecycle + icons Self-contained concern split out of wm.c ( |
| `gui/` | `wm_window_state` | 117 | string.h, wm_internal.h | wm_window_state.c -- WuBuOS DosGui WM: window state + modal dialogs Self-contained concern split out |
| `gui/` | `wubu_a11y` | 633 | math.h, stdlib.h, string.h, vbe.h, wm_internal.h, wubu_a11y.h, wubu_theme.h, wub | wubu_a11y.c  --  WuBuOS Accessibility Control Cluster The "GameCube face-button" psychology layout,  |
| `gui/` | `wubu_a11y_test` | 166 | assert.h, stdio.h, wm.h, wubu_a11y.h, wubu_theme.h, wubu_trash.h | wubu_a11y_test.c -- Regression tests for the accessibility cluster. Verifies the four GameCube-face  |
| `gui/` | `wubu_bonzi` | 435 | stdio.h, string.h, vbe.h, wm.h, holyc_repl.h, wubu_bonzi.h, wubu_buddy_sprite | wubu_bonzi.c -- WuBuOS WuBu Buddy desktop mascot (AGI gateway). WuBu-style: a purple companion that  |
| `gui/` | `wubu_clipboard` | 381 | egojar.h, egojar_net.h, hosted.h, primary-selection-client.header, stdio.h, stdl | wubu_clipboard.c  --  WuBuOS Clipboard Manager Implementation Phase 2: Wayland data device + primary |
| `gui/` | `wubu_clipboard_mime` | 61 | wubu_clipboard_internal.h | wubu_clipboard_mime.c -- Clipboard MIME-entry helpers (self-contained). Pure helpers for the X/prima |
| `gui/` | `wubu_clipboard_test` | 410 | assert.h, stdio.h, stdlib.h, string.h, wubu_clipboard.h, wubu_clipboard_internal | wubu_clipboard_test.c -- WuBuOS Clipboard Manager Test (logic only) Tests internal clipboard logic w |
| `gui/` | `wubu_clipboard_wl` | 388 | egojar.h, egojar_net.h, hosted.h, primary-selection-client.header, stdio.h, stdl | wubu_clipboard_wl.c -- WuBuOS Clipboard Wayland/X11 protocol transport Extracted from wubu_clipboard |
| `gui/` | `wubu_deploy` | 517 | dirent.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, time.h, wubu_deploy.h, | wubu_deploy.c - WuBuOS Multi-Target Deployment Implementation Build scripts for bare metal, WSL2, OC |
| `gui/` | `wubu_deploy_config` | 77 | string.h, wubu_deploy.h | wubu_deploy_config.c -- WuBuOS deploy: default target config getters. Extracted from wubu_deploy.c ( |
| `gui/` | `wubu_deploy_gen` | 247 | egojar.h, stdio.h, stdlib.h, string.h, wubu_deploy_internal.h | wubu_deploy_gen.c -- Deployment config-file generators. Self-contained module extracted from wubu_de |
| `gui/` | `wubu_deploy_test` | 384 | egojar.h, stdio.h, stdlib.h, string.h, wubu_deploy.h, wubu_spawn.h | wubu_deploy_test.c - Unit tests for WuBuOS Multi-Target Deployment |
| `gui/` | `wubu_deploy_util` | 86 | dirent.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_deploy_internal.h | wubu_deploy_util.c -- Deployment layer file/command utilities. Self-contained module extracted from  |
| `gui/` | `wubu_gamelib` | 337 | ctype.h, dirent.h, egojar.h, errno.h, glob.h, libgen.h, limits.h, stdio.h, stdli | Internal State |
| `gui/` | `wubu_gamelib_config` | 66 | stdio.h, string.h, wubu_gamelib.h, wubu_gamelib_internal.h | wubu_gamelib_config.c -- WuBuOS gamelib: config save/load (JSON file I/O). Extracted from wubu_gamel |
| `gui/` | `wubu_gamelib_playtime` | 21 | time.h, wubu_gamelib.h | wubu_gamelib_playtime.c -- WuBuOS gamelib: playtime tracking. Extracted from wubu_gamelib.c (separab |
| `gui/` | `wubu_gamelib_scan` | 263 | dirent.h, egojar.h, glob.h, stdio.h, stdlib.h, string.h, time.h, wubu_gamelib_in | wubu_gamelib_scan.c -- Game library source scanners. Self-contained module extracted from wubu_gamel |
| `gui/` | `wubu_gamelib_startmenu` | 51 | stdio.h, string.h, wubu_gamelib.h, wubu_gamelib_internal.h | wubu_gamelib_startmenu.c -- WuBuOS gamelib: start-menu (.desktop) integration. Extracted from wubu_g |
| `gui/` | `wubu_gamelib_test` | 101 | assert.h, stdio.h, string.h, wubu_gamelib.h | Test adding a game |
| `gui/` | `wubu_gui_metrics` | 67 | wubu_gui_metrics.h | wubu_gui_metrics.c -- the canonical GUI metric set (see wubu_gui_metrics.h). C11, minimal includes,  |
| `gui/` | `wubu_image_ffmpeg` | 225 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_image_ffmpeg.h | wubu_image_ffmpeg.c -- image codecs backed by ffmpeg. See wubu_image_ffmpeg.h.  One concern: turn a  |
| `gui/` | `wubu_image_ffmpeg_test` | 107 | stdio.h, stdlib.h, string.h, wubu_image_ffmpeg.h, wubu_wallpaper.h | wubu_image_ffmpeg_test.c -- the ffmpeg codec backbone. The gap this closes: WuBuOS could ENCODE a PN |
| `gui/` | `wubu_ladder` | 89 | math.h, string.h, wubu_ladder.h | wubu_ladder.c -- virtual-analog Moog-ladder lowpass (WT-B). C11. Four one-pole stages + resonance fe |
| `gui/` | `wubu_mascot` | 68 | stddef.h, vbe.h, wubu_buddy_sprite.h, wubu_mascot.h | wubu_mascot.c -- WuBu Buddy sprite blitter. One concern: alpha-composite the baked clipart onto the  |
| `gui/` | `wubu_mime` | 418 | ctype.h, dirent.h, egojar.h, glob.h, stdio.h, stdlib.h, string.h, wubu_mime.h, w | Internal State |
| `gui/` | `wubu_mime_desktop` | 146 | ctype.h, stdio.h, stdlib.h, string.h, wubu_mime_internal.h | wubu_mime_desktop.c -- MIME .desktop file parser. Self-contained module extracted from wubu_mime.c:  |
| `gui/` | `wubu_mime_test` | 49 | assert.h, stdio.h, string.h, wubu_mime.h | Test built-in MIME types |
| `gui/` | `wubu_pkgmgr` | 291 | wubu_pkgmgr_internal.h | wubu_pkgmgr.c  --  WuBuOS Package Manager (Facade) Submodules: wubu_pkgmgr_pkg.c     - package creat |
| `gui/` | `wubu_pkgmgr_db` | 79 | wubu_pkgmgr_internal.h | wubu_pkgmgr_db.c -- Package manager database subsystem. Self-contained: progress reporting + SQLite  |
| `gui/` | `wubu_pkgmgr_install` | 378 | egojar.h, glob.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_install.c  --  WuBuOS Package Manager: Install / Remove / Upgrade Depends on the facade  |
| `gui/` | `wubu_pkgmgr_manifest` | 57 | stdio.h, stdlib.h, string.h, wubu_pkgmgr.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_manifest.c -- WuBuOS pkgmgr: manifest -> JSON serialization. Extracted from wubu_pkgmgr. |
| `gui/` | `wubu_pkgmgr_pkg` | 512 | wubu_pkgmgr_internal.h | wubu_pkgmgr_pkg.c  --  WuBuOS Package Manager: Pkg |
| `gui/` | `wubu_pkgmgr_remote` | 250 | wubu_pkgmgr_internal.h | wubu_pkgmgr_remote.c -- Remote repository index subsystem. Self-contained module: fetches a repo's i |
| `gui/` | `wubu_pkgmgr_resolve` | 148 | stdlib.h, string.h, wubu_pkgmgr.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_resolve.c -- WuBuOS pkgmgr: dependency resolution + conflict check. Extracted from wubu_ |
| `gui/` | `wubu_pkgmgr_test` | 695 | egojar.h, egojar_net.h, errno.h, in.h, inet.h, socket.h, stdio.h, stdlib.h, stri | wubu_pkgmgr_test.c - Unit tests for WuBuOS Package Manager |
| `gui/` | `wubu_pkgmgr_txn` | 131 | egojar.h, glob.h, stdio.h, stdlib.h, unistd.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_txn.c  --  WuBuOS Package Manager: Txn |
| `gui/` | `wubu_pkgmgr_verify` | 108 | stdlib.h, string.h, wubu_pkgmgr.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_verify.c -- WuBuOS pkgmgr: installed-package integrity verification. Extracted from wubu |
| `gui/` | `wubu_proton` | 338 | dirent.h, egojar.h, errno.h, ftw.h, glob.h, libgen.h, limits.h, pwd.h, stdio.h,  | -- Recursive directory removal (replaces system("rm -rf")) -------- |
| `gui/` | `wubu_proton_config` | 80 | egojar.h, stdio.h, stdlib.h, string.h, wubu_proton_internal.h | wubu_proton_config.c -- Proton config persistence. Self-contained module extracted from wubu_proton. |
| `gui/` | `wubu_proton_dxvk` | 108 | egojar.h, stdio.h, stdlib.h, string.h, wubu_dxvk_conf.h, wubu_proton_dxvk.h, wub | wubu_proton_dxvk.c -- Proton DXVK/VKD3D config subsystem (GUI desktop-proton). Thin adapter over the |
| `gui/` | `wubu_proton_exec` | 246 | egojar.h, egojar_net.h, errno.h, stdio.h, stdlib.h, string.h, wubu_proton_intern | wubu_proton_exec.c -- Proton execution / launch subsystem. Self-contained module extracted from wubu |
| `gui/` | `wubu_proton_test` | 73 | assert.h, stdio.h, string.h, wubu_proton.h | Test Steam detection |
| `gui/` | `wubu_proton_util` | 177 | dirent.h, egojar.h, errno.h, ftw.h, glob.h, libgen.h, limits.h, pwd.h, stdio.h,  | wubu_proton_util.c -- Proton layer filesystem / parsing utilities. Self-contained module extracted f |
| `gui/` | `wubu_runline` | 70 | guest_file.h, notepad.h, stdio.h, stdlib.h, string.h, wubu_runline.h, wubu_spawn | wubu_runline.c -- see header. |
| `gui/` | `wubu_screenshot` | 453 | egojar.h, hosted.h, limits.h, math.h, stdio.h, stdlib.h, string.h, time.h, wm.h, | Internal State |
| `gui/` | `wubu_screenshot_clipboard_test` | 70 | assert.h, stdio.h, stdlib.h, string.h, wubu_screenshot.h | Regression test for wubu_screenshot_to_clipboard (was a no-op returning true with nothing stored). B |
| `gui/` | `wubu_screenshot_draw` | 127 | math.h, stdlib.h, wubu_screenshot_internal.h | wubu_screenshot_draw.c -- Screenshot annotation drawing primitives (self-contained). draw_line (Bres |
| `gui/` | `wubu_screenshot_png` | 239 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_screenshot.h | wubu_screenshot_png.c -- Screenshot PNG encoder subsystem. Self-contained module extracted from wubu |
| `gui/` | `wubu_screenshot_test` | 96 | assert.h, stdio.h, stdlib.h, string.h, wubu_screenshot.h, wubu_theme.h | Parse big-endian uint32 from a PNG buffer offset. |
| `gui/` | `wubu_session` | 310 | egojar.h, startmenu.h, stdio.h, stdlib.h, string.h, time.h, vbe.h, wm.h, wubu_se | wubu_session.c  --  WuBuOS Session Manager Implementation Phase 2: Session management, auto-start, s |
| `gui/` | `wubu_session_autostart` | 284 | egojar.h, stdio.h, stdlib.h, string.h, wubu_session_internal.h | wubu_session_autostart.c -- Autostart entries subsystem. Self-contained: owns g_autostart[]/g_autost |
| `gui/` | `wubu_settings` | 83 | stdio.h, string.h, wubu_settings_internal.h, wubu_theme.h | wubu_settings.c  --  WuBuOS Settings Daemon (facade) Owns the settings lifecycle + public accessor A |
| `gui/` | `wubu_settings_defaults` | 142 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_settings_internal.h, wubu_t | wubu_settings_defaults.c -- WuBuOS settings: state + default factories Self-contained concern split  |
| `gui/` | `wubu_settings_io` | 245 | stdio.h, stdlib.h, string.h, time.h, wubu_settings_internal.h | wubu_settings_io.c -- WuBuOS settings: JSON (de)serialization Self-contained concern split out of wu |
| `gui/` | `wubu_settings_test` | 166 | stdio.h, stdlib.h, string.h, vbe.h, wubu_notify.h, wubu_settings.h, wubu_theme.h | wubu_settings_test.c -- prove the settings daemon APPLIES its values. Before this gate, wubu_setting |
| `gui/` | `wubu_sound` | 171 | math.h, string.h, wubu_sound.h | wubu_sound.c -- WuBuOS system-sound engine (Win98/XP vibes). C11. Pure PCM synthesis -- no assets, n |
| `gui/` | `wubu_theme` | 528 | math.h, stdbool.h, string.h, wubu_palette.h, wubu_theme.h | wubu_theme.c  --  WuBuOS Theme Engine Implementation Cell 394: Runtime-switchable themes. Win98 Clas |
| `gui/` | `wubu_theme_bridge_test` | 62 | assert.h, stdint.h, stdio.h, wubu_theme.h | wubu_theme_bridge_test.c -- verifies the kernel /theme -> GUI render bridge: when the AGI/console wr |
| `gui/` | `wubu_trash` | 514 | dirent.h, egojar.h, errno.h, libgen.h, stdio.h, stdlib.h, string.h, time.h, wubu | Internal State |
| `gui/` | `wubu_trash_test` | 76 | assert.h, egojar.h, libgen.h, stdio.h, stdlib.h, string.h, wubu_trash.h | Create test file |
| `gui/` | `wubu_ui` | 114 | string.h, wm.h, wubu_edr.h, wubu_ui.h | wubu_ui.c -- AGI UI automation layer (see wubu_ui.h). Every action routes through wm_handle_mouse /  |
| `gui/` | `wubu_ui_test` | 98 | input.h, stdio.h, string.h, vbe.h, wm.h, wubu_theme.h, wubu_ui.h | wubu_ui_test.c -- AGI UI automation layer test suite. Proves the central AGI-OS claim: a synthetic d |
| `gui/` | `wubu_wallpaper_test` | 122 | stdio.h, stdlib.h, string.h, wubu_wallpaper.h | wubu_wallpaper_test.c -- Real wallpaper decode + placement verification. No GPU needed: writes a rea |
| `gui/` | `wubu_waveosc` | 100 | math.h, string.h, wubu_waveosc.h | wubu_waveosc.c -- bandlimited wavetable oscillator (WT-A). C11. The tables are precomputed by additi |
| `gui/` | `wubu_wayland_fallback` | 19 | wayland_state.h | wubu_wayland_fallback.c  --  weak Wayland-state default for standalone binaries. src/gui/wubu_screen |
| `gui/` | `xdg-shell-client-protocol` | 183 | stdint.h, stdlib.h, wayland-util.h | Generated by wayland-scanner 1.22.0 |
| `gui/` | `xdg-shell-server-protocol` | 183 | stdint.h, stdlib.h, wayland-util.h | Generated by wayland-scanner 1.22.0 |
| `hosted/` | `fs_crash_safe_test` | 116 | dirent.h, hosted_fs_persist.h, stdio.h, stdlib.h, string.h, unistd.h | IS ASSERTED |
| `hosted/` | `fs_persist_fuzz` | 199 | fuzz_rng.h, guest_file.h, hosted_fs_persist.h, stdbool.h, stdint.h, stdio.h, std | IT ASSERTS |
| `hosted/` | `hosted` | 417 | bridge.h, desktop.h, egojar.h, egojar_net.h, fcntl.h, hosted.h, hosted_fs_persis | hosted.c — WuBuOS Hosted Mode Launcher (Inferno emu-style) — facade WuBuOS as a clickable Linux bina |
| `hosted/` | `hosted_agent_channel` | 93 | hosted_agent_channel.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubu_bonzi.h, | hosted_agent_channel.c -- the agent -> desktop channel. See hosted_agent_channel.h.  One concern: re |
| `hosted/` | `hosted_display` | 134 | hosted_display.h, string.h | hosted_display.c -- see hosted_display.h for why this exists. Pure geometry over plain integers: no  |
| `hosted/` | `hosted_display_test` | 140 | hosted_display.h, stdio.h, string.h | hosted_display_test.c -- gate for the guest's display model. These are the rules every later feature |
| `hosted/` | `hosted_fs_persist` | 261 | errno.h, hosted.h, hosted_fs_persist.h, hosted_internal.h, stat.h, stdio.h, stdl | the first is mid-way through writing, so the rename publishes a mixture |
| `hosted/` | `hosted_pe` | 51 | egojar.h, hosted_internal.h, stdio.h, stdlib.h, string.h, wubu_ct_isolate.h, wub | hosted_pe.c -- WuBuOS hosted-mode Windows/PE launch executor Self-contained concern split out of hos |
| `hosted/` | `hosted_render` | 56 | desktop.h, hosted_internal.h, input.h, startmenu.h, stdio.h, vbe.h, wm.h, wubu_b | hosted_render.c -- WuBuOS hosted-mode frame composition + input routing Self-contained concern split |
| `hosted/` | `hosted_run` | 231 | desktop.h, hosted_agent_channel.h, hosted_fs_persist.h, hosted_internal.h, input | hosted_run.c -- WuBuOS hosted-mode run loop, shutdown, blit + accessors Self-contained concern split |
| `hosted/` | `hosted_styxfs` | 589 | guest_file.h, hosted.h, hosted_fs_persist.h, hosted_internal.h, input.h, memory. | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `hosted/` | `hosted_test` | 463 | egojar.h, egojar_net.h, hosted.h, startmenu.h, stdio.h, stdlib.h, string.h, styx | hosted_test.c  --  WuBuOS Hosted Mode Behavioral Test Suite Tests: hosted init/shutdown, Styx namesp |
| `hosted/` | `hosted_wayland` | 125 | bridge.h, desktop.h, egojar.h, egojar_net.h, hosted.h, hosted_internal.h, input. | Thin orchestration: owns the public hosted_wl_* entry points declared in hosted_internal.h. SHM pool |
| `hosted/` | `hosted_wayland_input` | 451 | bridge.h, desktop.h, egojar.h, egojar_net.h, hosted.h, hosted_internal.h, input. |  |
| `hosted/` | `hosted_wayland_shm` | 154 | bridge.h, desktop.h, egojar.h, hosted.h, hosted_internal.h, input.h, math.h, pri |  |
| `hosted/` | `hosted_wayland_surface` | 284 | bridge.h, desktop.h, egojar.h, egojar_net.h, hosted.h, hosted_internal.h, input. |  |
| `hosted/` | `primary-selection-private` | 115 | stdint.h, stdlib.h, wayland-util.h | Generated by wayland-scanner 1.22.0 |
| `hosted/` | `test_fs_persist` | 273 | hosted.h, hosted_fs_persist.h, hosted_internal.h, stat.h, stdio.h, stdlib.h, str | test_fs_persist.c -- durability and failure behaviour of the guest FS image. The point of this test  |
| `hosted/` | `test_notepad_save` | 188 | guest_file.h, hosted.h, hosted_fs_persist.h, hosted_internal.h, notepad.h, stdio | test_notepad_save.c -- does a document actually survive a restart? This is the acceptance test for l |
| `hosted/` | `wubu_display` | 297 | egojar.h, egojar_net.h, errno.h, input.h, stdio.h, stdlib.h, string.h, wubu_disp | wubu_display.c  --  WuBuOS Display Backend (DRM/KMS + X11 dual) Cell 380: Try DRM/KMS first, fall ba |
| `hosted/` | `wubu_display_test` | 64 | stdio.h, stdlib.h, string.h, wubu_display.h | wubu_drm_direct_test.c  --  Test for direct DRM/KMS implementation (Cells 388/389) |
| `hosted/` | `wubu_gbm` | 182 | egojar.h, errno.h, stdint.h, stdlib.h, string.h, wubu_gbm.h, xf86drm.h, xf86drmM | wubu_gbm.c  --  WuBuOS Custom GBM (Generic Buffer Management) Cell 389: Pure C GBM implementation wi |
| `hosted/` | `wubu_hosted_hw` | 519 | dirent.h, egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_drv.h, wubu_ | wubu_hosted_hw.c -- the HOSTED hardware backend (the OS's sense of the machine it is running on as a |
| `hosted/` | `wubu_hosted_hw_test` | 152 | dirent.h, egojar.h, egojar_net.h, stdio.h, string.h, wubu_drv.h, wubu_drv_batter | wubu_hosted_hw_test.c -- prove the HOSTED hardware backend reports the machine it is actually runnin |
| `hosted/` | `wubu_inject` | 160 | Xlib.h, egojar.h, keysym.h, stdio.h, stdlib.h, string.h, time.h | wubu_inject.c -- inject input into the workspace window, in C11. The hosted desktop runs inside a we |
| `hosted/` | `wubu_metal` | 481 | dirent.h, egojar.h, errno.h, input.h, interrupt.h, memory.h, stdio.h, stdlib.h,  | wubu_metal.c  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction Cell 400: Implementation of unified  |
| `hosted/` | `wubu_metal_audio` | 301 | asoundlib.h, egojar.h, egojar_net.h, error.h, format-utils.h, pipewire.h, simple | wubu_metal_audio.c -- WuBuOS Metal audio backends (split from wubu_metal.c). Self-contained: ALSA +  |
| `hosted/` | `wubu_metal_drm` | 367 | drm_fourcc.h, egojar.h, egojar_net.h, errno.h, stdio.h, stdlib.h, string.h, wubu | wubu_metal_drm.c -- WuBuOS Metal DRM/KMS display backend (split from wubu_metal.c). Self-contained:  |
| `hosted/` | `wubu_metal_evdev` | 201 | dirent.h, egojar.h, errno.h, fb.h, input.h, stdio.h, stdlib.h, string.h, wubu_au | wubu_metal_evdev.c -- WuBuOS evdev input backend (extracted from wubu_metal.c). Mirror of the origin |
| `hosted/` | `wubu_metal_test` | 108 | assert.h, stdio.h, stdlib.h, wubu_metal.h | wubu_metal_test.c  --  Tests for bare-metal + WSL2 abstraction layer |
| `hosted/` | `wubu_metal_vulkan` | 119 | stdio.h, string.h, vulkan.h, vulkan_wayland.h, vulkan_xcb.h, vulkan_xlib.h, wubu | wubu_metal_vulkan.c -- WuBuOS Vulkan surface creation (extracted from wubu_metal.c). Self-contained: |
| `hosted/` | `wubu_metal_x11` | 112 | Xlib.h, Xutil.h, dirent.h, egojar.h, errno.h, fb.h, input.h, stdio.h, stdlib.h,  | wubu_metal_x11.c -- WuBuOS X11 display backend (extracted from wubu_metal.c). Mirror of the original |
| `hosted/` | `wubu_vk_effect` | 298 | stdio.h, stdlib.h, string.h, wubu_vk_effect.h, wubu_vulkan.h | wubu_vk_effect.c -- the GPU leg of the effect engine. See wubu_vk_effect.h.  Everything Vulkan here  |
| `hosted/` | `wubu_vk_effect_test` | 111 | stdio.h, stdlib.h, string.h, wubu_vk_effect.h | wubu_vk_effect_test.c -- the GPU effect pass vs the CPU engine. The whole point of putting the effec |
| `hosted/` | `wubu_vulkan_cmd` | 124 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_cmd.c -- WuBuOS Vulkan command-pool + queue-submit helpers (extracted from the monolithi |
| `hosted/` | `wubu_vulkan_compute` | 326 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_compute.c -- WuBuOS Vulkan compute pipeline + result-string / memory-type utilities (ext |
| `hosted/` | `wubu_vulkan_loader` | 318 | egojar.h, stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_loader.c -- WuBuOS Vulkan: dynamic libvulkan loader, instance, physical-device selection |
| `hosted/` | `wubu_vulkan_swapchain` | 206 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_swapchain.c -- WuBuOS Vulkan swapchain + presentation (extracted from the monolithic wub |
| `hosted/` | `wubu_vulkan_util` | 96 | stdint.h, wubu_vulkan.h | wubu_vulkan_util.c -- WuBuOS Vulkan shared utilities Standalone helpers used across the Vulkan backe |
| `jit/` | `jit` | 825 | egojar.h, jit.h, jit_internal.h, limits.h, stdarg.h, stdio.h, stdlib.h, string.h | jit.c  --  My Seed JIT Runtime Implementation (mmap backend) The always-available, zero-dependency J |
| `jit/` | `jit_audit` | 30 | jit.h, stdio.h, stdlib.h, string.h | JIT code auditor: compiles representative patterns and dumps disasm so we can spot optimization oppo |
| `jit/` | `jit_branch_profile` | 90 | stdint.h, stdio.h, stdlib.h, string.h | jit_branch_profile.c -- Runtime branch-feedback subsystem (Subsystem C). Foundation for #14 (probabi |
| `jit/` | `jit_branch_profile_test` | 52 | jit_branch_profile.h, stdio.h | jit_branch_profile_test.c -- Verify the runtime branch-feedback subsystem (Subsystem C): counters in |
| `jit/` | `jit_branch_test` | 142 | jit.h, stdint.h, stdio.h, string.h, wubu_x86.h | jit_branch_test.c  --  Verify #4 cmp/jcc macro-fusion + #2 flag-dependency. When an `if`/`while` con |
| `jit/` | `jit_codegen_arm64` | 336 | jit_codegen.h, stdlib.h, string.h, wubu_arm64.h | jit_codegen_arm64.c — ARM64 backend for the abstract code generator. Wraps WArm64Enc + wubu_arm64.h  |
| `jit/` | `jit_codegen_rv64` | 246 | jit_codegen.h, stdlib.h, wubu_rv64.h | jit_codegen_rv64.c — RISC-V RV64 backend for abstract codegen. |
| `jit/` | `jit_codegen_wasm` | 416 | jit_codegen.h, stdio.h, stdlib.h, string.h, wubu_wasm.h | jit_codegen_wasm.c — WebAssembly backend for abstract codegen. WASM is a stack machine — our express |
| `jit/` | `jit_codegen_x86` | 415 | jit_codegen.h, stdlib.h, string.h, wubu_x86.h | jit_codegen_x86.c — x86-64 backend for the abstract code generator. Wraps Wx86Enc + wubu_x86.h funct |
| `jit/` | `jit_cycle_profiler` | 116 | jit.h, stdint.h, stdio.h, stdlib.h, string.h | jit_cycle_profiler.c -- Measure actual cycle costs of generated code. Uses rdtsc for wall-clock meas |
| `jit/` | `jit_deep_opt_test` | 165 | jit.h, stdint.h, stdio.h, string.h | jit_deep_opt_test.c -- Deep optimization verification: 1. Stack frame sizing (no longer fixed 256) 2 |
| `jit/` | `jit_encode` | 66 | jit_internal.h, string.h | jit_encode.c -- x86-64 opcode encoding helpers (self-contained). enc_* : emit raw x86-64 opcodes int |
| `jit/` | `jit_fuzzer` | 280 | jit.h, stdint.h, stdio.h, stdlib.h, string.h | jit_fuzzer.c -- Exhaustive edge-case sweep of the Mini-C JIT compiler. Tests every code path, bounda |
| `jit/` | `jit_gap_analysis` | 105 | jit.h, stdint.h, stdio.h, string.h | jit_gap_analysis.c -- Comprehensive self-assessment of compiler capabilities. Compiles every pattern |
| `jit/` | `jit_loop_consume_test` | 119 | jit.h, stdint.h, stdio.h, string.h | jit_loop_consume_test.c -- Verify Subsystem B's loop-body capture consumer: the JIT compiler capture |
| `jit/` | `jit_loop_test` | 80 | jit_minic_loop.h, stdint.h, stdio.h, string.h | jit_loop_test.c -- Verify the loop analysis subsystem (Subsystem B): induction-variable detection (# |
| `jit/` | `jit_minic` | 1020 | ctype.h, jit.h, jit_branch_profile.h, jit_minic_internal.h, jit_minic_loop.h, st | jit_minic.c  --  WuBuOS Mini C-to-x86-64 Compiler Self-hosted replacement for the MIR JIT backend. P |
| `jit/` | `jit_minic_cg` | 862 | ctype.h, jit.h, jit_codegen.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_wasm. | jit_minic_cg.c — Full multi-target Mini-C compiler using abstract codegen. Supports the full Mini-C  |
| `jit/` | `jit_minic_expr` | 672 | ctype.h, jit_minic_internal.h, stdarg.h, stdint.h, stdio.h, stdlib.h, string.h | jit_minic_expr.c — Expression compiler chain for Mini-C JIT. Handles: primary → multiplicative → add |
| `jit/` | `jit_minic_loop` | 193 | jit_minic_loop.h, stdbool.h, stdint.h, stdio.h, stdlib.h, string.h | jit_minic_loop.c -- Loop analysis subsystem for the Mini-C JIT. Subsystem B (foundation for #12 indu |
| `jit/` | `jit_minic_token` | 176 | ctype.h, jit_internal.h, stdlib.h, string.h | jit_minic_token.c -- Mini-C tokenizer (self-contained lexer). minic_lex_next/init/cur/advance/expect |
| `jit/` | `jit_minic_type` | 139 | jit_internal.h, stdlib.h | jit_minic_type.c -- Mini-C type system for the WuBuOS JIT. Subsystem A: a real type model (structs,  |
| `jit/` | `jit_pgo_byte_test` | 41 | jit.h, jit_branch_profile.h, stdint.h, stdio.h, stdlib.h | if(x<100) return 1; else return 2; — 1 conditional => 1 counter site |
| `jit/` | `jit_regalloc_test` | 91 | jit.h, stdio.h, stdlib.h | jit_regalloc_test.c — prove the JIT's register-allocation path survives clobber-prone nested express |
| `jit/` | `jit_regression_test` | 295 | jit.h, stdint.h, stdio.h, stdlib.h, string.h | jit_regression_test.c -- Bulletproof regression test suite. Covers EVERY code path, boundary conditi |
| `jit/` | `jit_remat_test` | 134 | stdint.h, stdio.h, wubu_x86.h, x86_regalloc.h | jit_remat_test.c  --  Verify constant rematerialization in the linear-scan register allocator: a spi |
| `jit/` | `jit_subsystem_integration_test` | 129 | jit.h, jit_branch_profile.h, jit_minic_loop.h, stdint.h, stdio.h, string.h | jit_subsystem_integration_test.c -- End-to-end integration of all three subsystems: type system (A)  |
| `jit/` | `jit_supremacy_test` | 201 | egojar.h, jit.h, stdint.h, stdio.h, stdlib.h, string.h | jit_supremacy_test.c -- Compare WuBu JIT output against GCC -O2. For each test case, both GCC and Wu |
| `jit/` | `jit_test` | 901 | assert.h, jit.h, stdio.h, stdlib.h, string.h, wubu_disasm.h, wubu_x86.h, x86_reg | jit_test.c  --  WuBuOS JIT Test Suite Tests: mmap backend, x86-64 encoder roundtrip, trivial disasse |
| `jit/` | `jit_torture_test` | 230 | egojar.h, jit.h, stdint.h, stdio.h, stdlib.h, string.h | jit_torture_test.c -- WuBu-style torture tests. Generates 50+ stress patterns that probe every opera |
| `jit/` | `jit_type_test` | 69 | jit_internal.h, stdio.h, string.h | jit_type_test.c -- Verify the Mini-C type system (Subsystem A): struct field reordering (#19) packs  |
| `jit/` | `test_album_benchmark` | 226 | egojar.h, jit.h, jit_codegen.h, stdint.h, stdio.h, stdlib.h, string.h | test_album_benchmark.c — Cleveland Browns album data processing benchmark. Simulates processing a mu |
| `jit/` | `test_arm64_enc` | 123 | stdint.h, stdio.h, string.h, wubu_arm64.h | test_arm64_enc.c — Verify ARM64 encoder emits correct bytes. Compares against known-good encodings f |
| `jit/` | `test_cross_exec` | 149 | egojar.h, jit.h, jit_codegen.h, stdint.h, stdio.h, stdlib.h, string.h | test_cross_exec.c — Cross-architecture execution test. Compiles the same C source on x86-64, ARM64,  |
| `jit/` | `test_minic_cg` | 168 | jit.h, jit_codegen.h, stdint.h, stdio.h, stdlib.h, string.h | test_minic_cg.c — Test multi-target minic compilation (full grammar). |
| `jit/` | `wubu_arm64` | 419 | stdio.h, stdlib.h, string.h, wubu_arm64.h | wubu_arm64.c — ARM64/A64 instruction encoder. Encodes ARM64 instructions into a byte buffer for late |
| `jit/` | `wubu_disasm` | 433 | stdio.h, string.h, wubu_disasm.h, wubu_x86.h | wubu_disasm.c  --  WuBuOS x86-64 Trivial Disassembler Decodes the x86-64 subset emitted by wubu_x86. |
| `jit/` | `wubu_rv64` | 264 | stdlib.h, string.h, wubu_rv64.h | wubu_rv64.c — RISC-V RV64I + M extension instruction encoder. RISC-V is beautifully simple: fixed 32 |
| `jit/` | `wubu_wasm` | 104 | stdlib.h, string.h, wubu_wasm.h | wubu_wasm.c — WebAssembly binary encoder. WASM is a stack machine — our expression compiler maps nat |
| `jit/` | `wubu_x86` | 746 | stdio.h, stdlib.h, string.h, wubu_x86.h | wubu_x86.c  --  WuBuOS x86-64 Machine Code Encoder Pure C, zero-dependency x86-64 instruction emitte |
| `jit/` | `x86_regalloc` | 305 | string.h, x86_regalloc.h | x86_regalloc.c  --  WuBuOS x86-64 Mini Register Allocator Linear-scan allocator for JIT-compiled fun |
| `kernel/` | `wubudriver` | 201 | wubudriver.h | WuBuDriver -- implementation. Registry is a simple singly-linked list guarded by a tiny test-and-set |
| `kernel/` | `wubukcore` | 212 | wubukcore.h | WuBuKernel core services. Freestanding: no libc.  The allocator is a bounded bump with watermark, th |
| `kernel/` | `wubukernel_test` | 132 | stdio.h, string.h, wubudriver.h, wubukcore.h | WuBuKernel hosted gate. Runs the driver model and core services on the host so binding order, refcou |
| `kernel/` | `ahci` | 343 | ahci.h, stdio.h, stdlib.h, string.h | ahci.c  --  WuBuOS AHCI (SATA) Disk Driver Implementation Cell 072: AHCI driver with hosted simulati |
| `kernel/` | `cab_extract` | 245 | egojar.h, errno.h, memory.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_cab.h | cab_extract.c -- list / get / extract-all from a CAB archive embedded in a Windows installer, using  |
| `kernel/` | `cpio_extract` | 175 | memory.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_zlib.h | cpio_extract -- decompress a gzipped cpio archive and extract one file, using ONLY the kernel's own  |
| `kernel/` | `fat32` | 36 | fat32_internal.h, stdlib.h, string.h, time.h | fat32.c  --  WuBuOS FAT32 Filesystem (facade) This file is the public entry point only. The real wor |
| `kernel/` | `fat32_cluster` | 94 | fat32.h, fat32_internal.h, stdint.h, stdlib.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `fat32_dir` | 355 | fat32_internal.h, stdlib.h, string.h, time.h, wubu_lfn.h | fat32_dir.c -- directory enumeration, lookup, create, delete (leaf module). Opaque fat32_volume via  |
| `kernel/` | `fat32_fat` | 88 | fat32_internal.h, stdlib.h, string.h | fat32_fat.c -- FAT entry read/write + cache (leaf module). Opaque fat32_volume via fat32_internal.h. |
| `kernel/` | `fat32_file` | 264 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_file.c -- open/close/read/write/seek on a FAT32 file (leaf module). Opaque fat32_volume via fa |
| `kernel/` | `fat32_format` | 262 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_format.c -- mount/unmount/format/validate (volume lifecycle, leaf module). Opaque fat32_volume |
| `kernel/` | `fat32_name` | 80 | ctype.h, fat32.h, fat32_internal.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `input` | 214 | input.h, stdbool.h, string.h | input.c  --  My Seed Input Subsystem (hosted stub) Circular buffers for keyboard/mouse events with p |
| `kernel/` | `interrupt` | 634 | egojar.h, interrupt.h, interrupt_apic.h, interrupt_io.h, interrupt_pic.h, memory | interrupt.c  --  My Seed IDT/PIC Interrupt Controller Full x86_64 IDT implementation with 256 interr |
| `kernel/` | `interrupt_apic` | 200 | egojar.h, interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `interrupt_exceptions` | 170 | interrupt.h | interrupt_exceptions.c — Exception handlers (double fault, NMI, page fault, GPF) Extracted from inte |
| `kernel/` | `interrupt_pic` | 107 | interrupt_apic.h, interrupt_io.h, interrupt_pic.h, memory.h, stdint.h | interrupt_pic.c -- 8259 PIC layer + IRQ routing for the WuBuOS kernel. Extracted from the monolithic |
| `kernel/` | `interrupt_pit` | 57 | egojar.h, interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `interrupt_syscall` | 180 | egojar.h, interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, stdint.h, str | interrupt_syscall.c — Syscall dispatcher + registry Split from interrupt.c: the syscall table (regis |
| `kernel/` | `interrupt_timer` | 45 | egojar.h, interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `klog` | 212 | klog.h, stdarg.h, stddef.h, stdint.h | klog.c -- WuBuOS bare-metal kernel log sink (serial COM1) Self-contained freestanding output. Writes |
| `kernel/` | `libc` | 668 | klog.h, stdarg.h, stddef.h, stdint.h | keeps boot.bin + kernel.elf + WuBuFW loadable. */ |
| `kernel/` | `libc_string` | 172 | libc.h, stdlib.h, string.h | libc_string.c -- the kernel's STRING module (the split of the freestanding libc: the string half, se |
| `kernel/` | `memory` | 697 | memory.h, stdio.h, stdlib.h, string.h | memory.c  --  My Seed Kernel Memory Subsystem Implementation Clean C11 reimplementation of ZealOS he |
| `kernel/` | `metal_main` | 728 | ahci.h, fat32.h, input.h, interrupt.h, interrupt_apic.h, klog.h, memory.h, ps2.h | metal_main.c  --  WuBuOS Bare-Metal Kernel Entry Point Called from crt0.S after Limine/Stivale2 boot |
| `kernel/` | `ps2` | 324 | input.h, interrupt.h, ps2.h, stdint.h | ps2.c  --  PS/2 Keyboard and Mouse Driver (Bare Metal) Ported from Mythos Fable (filipvabrousek/osde |
| `kernel/` | `tasking` | 654 | interrupt.h, libc.h, memory.h, setjmp.h, stddef.h, stdint.h, string.h, tasking.h | tasking.c  --  My Seed Kernel Task Management (hosted test impl) Uses setjmp/longjmp for context swi |
| `kernel/` | `cab_selftest` | 115 | memory.h, stdio.h, stdlib.h, string.h, wubu_cab.h, wubu_lzx.h | cab_selftest.c -- exercise the IN-KERNEL CAB reader (wubu_cab.c) against the real Halo PC CAB1.CAB e |
| `kernel/` | `input_keymap_test` | 72 | input.h, stdio.h | input_keymap_test.c -- what a scancode becomes when it is typed. Regression cover for a bug that man |
| `kernel/` | `ahci_test` | 471 | ahci.h, stdio.h, stdlib.h, string.h | ahci_test.c  --  Test Suite for WuBuOS AHCI (SATA) Disk Driver Cell 072: Tests HBA init, port enumer |
| `kernel/` | `fat32_test` | 818 | assert.h, fat32.h, fat32_internal.h, stdio.h, stdlib.h, string.h | fat32_test.c  --  My Seed FAT32 Filesystem Test Suite Uses a RAM-backed block device for fast, deter |
| `kernel/` | `input_test` | 278 | input.h, stdio.h, string.h | input_test.c  --  Kernel Input Subsystem Test Suite Cell 202: Tests for input queue (keyboard/mouse  |
| `kernel/` | `interrupt_pic_test` | 55 | interrupt_apic.h, interrupt_pic.h, stdio.h, string.h | interrupt_pic_test.c -- unit test for the extracted PIC + IRQ routing module (interrupt_pic.c). Buil |
| `kernel/` | `libc_string_test` | 124 | stdio.h, stdlib.h, string.h | libc_string_test.c -- the kernel string module test. NOTE: this test runs ON THE HOST (to prove the  |
| `kernel/` | `memory_test` | 385 | assert.h, memory.h, stdio.h, stdlib.h, string.h | memory_test.c  --  Test suite for My Seed Kernel Memory Subsystem |
| `kernel/` | `tasking_test` | 198 | assert.h, klog.h, memory.h, stdio.h, stdlib.h, string.h, tasking.h | tasking_test.c  --  Test suite for My Seed Tasking Subsystem |
| `kernel/` | `test_ahciseq` | 15 | ahci.c, ahci.h, stdio.h | host repro of the bootvol's ahci sequence |
| `kernel/` | `test_bootattach` | 60 | fat32.h, stdint.h, stdio.h, stdlib.h, string.h | quick host check of fat32_boot_attach on the fat32_test RAM disk |
| `kernel/` | `txfs_test` | 572 | stdio.h, stdlib.h, string.h, txfs.h | txfs_test.c  --  Test Suite for WuBuOS Transactional Filesystem Cell 100: Tests journal-based atomic |
| `kernel/` | `wubu_drv_machines_test` | 97 | stdio.h, string.h, wubu_drv.h, wubu_drv_arm.h, wubu_drv_intel.h, wubu_drv_virtio | wubu_drv_machines_test.c -- the OTHER machines (the AGI runs on everything). Three machines beyond t |
| `kernel/` | `wubu_drv_test` | 213 | stdio.h, string.h, wubu_drv.h, wubu_drv_ahci.h, wubu_drv_battery.h, wubu_drv_gpu | wubu_drv_test.c -- the DRIVER REGISTRY test (the Steam Deck bus). Simulates the REAL Steam Deck PCI  |
| `kernel/` | `wubu_elf_test` | 60 | stdio.h, stdlib.h, string.h, wubu_elf.h | wubu_elf_test.c -- the in-kernel ELF loader test (the Linux game). The user's second goal: the games |
| `kernel/` | `wubu_gaad_test` | 269 | assert.h, stdio.h, stdlib.h, string.h, wubu_gaad.h | wubu_gaad_test.c  --  GAAD: Golden Aspect Adaptive Decomposition Cell 393: Tests for the universal r |
| `kernel/` | `wubu_kvfs_route_test` | 89 | stdint.h, stdio.h, stdlib.h, string.h, wubu_kvfs.h | wubu_kvfs_route_test.c — proves THE DOCTRINE end-to-end: The OS state IS the KV cache, served as 9P. |
| `kernel/` | `wubu_ns_effect_test` | 96 | stdio.h, stdlib.h, string.h, wubu_ns_bridge_internal.h, wubu_ns_effect.h | wubu_ns_effect_test.c -- the /n/effect subtree test. Asserts the file<->API routing: 1. publish crea |
| `kernel/` | `wubu_ns_world_test` | 112 | stdio.h, string.h, wubu_drv.h, wubu_drv_battery.h, wubu_drv_gpu.h, wubu_drv_net. | wubu_ns_world_test.c -- the /n/world subtree test. Asserts the file<->API routing: 1. publish create |
| `kernel/` | `wubu_pe_personality_test` | 78 | stdio.h, stdlib.h, string.h, wubu_pe.h, wubu_pe_personality.h | wubu_pe_personality_test.c -- the in-kernel Win32 personality audit. Proves: for EVERY DLL the real  |
| `kernel/` | `wubu_pe_test` | 88 | stdio.h, stdlib.h, string.h, wubu_pe.h | wubu_pe_test.c -- the in-kernel PE loader test (the REAL Windows game binaries). Parses the ACTUAL g |
| `kernel/` | `wubu_self_test` | 93 | string.h, wubu_self_test.h | wubu_self_test.c  --  kernel-resident self-test gate (gap G2) The suite exercises the live kernel st |
| `kernel/` | `wubu_world_test` | 119 | stdio.h, string.h, wubu_drv.h, wubu_drv_battery.h, wubu_drv_gpu.h, wubu_drv_net. | wubu_world_test.c -- the world-state bridge test. The OS as the AGI's training space: assemble the w |
| `kernel/` | `lzx_selftest` | 127 | memory.h, stdio.h, stdlib.h, string.h, wubu_lzx.h | lzx_selftest.c -- validate the kernel wubu_lzx.c decoder against real Halo PC cabinet bytes, walking |
| `kernel/` | `test_acpi` | 96 | stdint.h, stdio.h, stdlib.h, string.h, wubu_acpi.c | test_acpi.c -- host tests for wubu_acpi (RSDP -> RSDT/XSDT -> FADT). The RSDP scan targets the EBDA/ |
| `kernel/` | `test_agi_kernel` | 195 | stdio.h, string.h, wubu_agi_kernel.h, wubu_attest.h, wubu_bonzi.h | test_agi_kernel.c -- Verify the WuBuOS bare-metal AGI kernel supervisor runs correctly (hosted harne |
| `kernel/` | `test_agi_kernel_stub` | 74 | input.h, klog.h, stdio.h, tasking.h, vbe.h, wubu_sync.h | test_agi_kernel_stub.c -- Minimal kernel-API shims so wubu_agi_kernel.c links + runs in the HOSTED u |
| `kernel/` | `test_ahcifat` | 57 | ahci.c, ahci.h, fat32.h, fat32_cluster.c, fat32_dir.c, fat32_fat.c, fat32_file.c | host repro: FAT32 create through the REAL ahci sim backend (the metal's exact path: ahci hba + sim d |
| `kernel/` | `test_as` | 79 | egojar.h, egojar_net.h, stdint.h, stdio.h, wubu_as.c, wubu_as.h | test_as.c -- host tests for the address-space module (gap B7). The PML4 clone + CR3 switch normally  |
| `kernel/` | `test_blk` | 266 | stdio.h, string.h, wubu_blk.h | test_blk.c -- host tests for the FS-A block-layer frontier (100 gaps). |
| `kernel/` | `test_bonzi_study` | 190 | stdio.h, string.h, wubu_bonzi_study.h, wubu_psych.h | test_bonzi_study.c -- host tests for the HX-D Bonzi Buddy GUI study. |
| `kernel/` | `test_fat2` | 149 | stdio.h, string.h, wubu_fat2.h | test_fat2.c -- host tests for the FS-B FAT family frontier (100 gaps). |
| `kernel/` | `test_heap_stress` | 160 | memory.h, stdio.h, stdlib.h, string.h | test_heap_stress.c -- devil's-advocate churn test for the kernel heap (memory.c: hash buckets + free |
| `kernel/` | `test_hive` | 183 | stdio.h, stdlib.h, string.h, wubu_hive.h | test_hive.c -- wubu_hive (C11 luddite hive) unit tests. Verifies the three-way tradeoff the hand-dra |
| `kernel/` | `test_hpet` | 36 | stdint.h, stdio.h, wubu_hpet.h | test_hpet.c -- host tests for the HPET helpers (gap A19). The MMIO probe is metal-only; the pure con |
| `kernel/` | `test_iommu` | 57 | stdint.h, stdio.h, string.h, wubu_iommu.c, wubu_iommu.h | test_iommu.c -- host tests for the IOMMU/VT-d discovery (gap E5). The engine-capability MMIO reads a |
| `kernel/` | `test_lfn` | 105 | stdio.h, stdlib.h, string.h, wubu_lfn.h | test_lfn.c  --  host test for the wubu_lfn VFAT LFN codec (A16) |
| `kernel/` | `test_psych` | 172 | stdio.h, string.h, wubu_psych.h | test_psych.c -- host tests for the HX psychology/timing/patience engine. |
| `kernel/` | `test_recovery` | 92 | stdio.h, string.h, wubu_recovery.h | test_recovery.c -- host tests for the 5+1 recovery substrate. Compiles the freestanding wubu_recover |
| `kernel/` | `test_rtc` | 45 | stdio.h, wubu_rtc.h | test_rtc.c -- host test for wubu_rtc's pure conversion helpers (the CMOS port reads are metal-only;  |
| `kernel/` | `test_sha256` | 81 | stdio.h, string.h, wubu_sha256.h | test_sha256.c -- host test for wubu_sha256 (FIPS 180-4 vectors). |
| `kernel/` | `test_smbios` | 74 | stdint.h, stdio.h, string.h, wubu_smbios.c | test_smbios.c -- host tests for the SMBIOS walk (gap I3). The EPS search targets the BIOS ROM area ( |
| `kernel/` | `test_swap` | 78 | stdint.h, stdio.h, string.h, wubu_swap.c, wubu_vmm.h | test_swap.c -- host tests for the swap module (gap B3). The phys window is identity-mapped metal mem |
| `kernel/` | `test_sync` | 104 | egojar.h, stdbool.h, stdint.h, stdio.h, wubu_sync.c, wubu_sync.h | test_sync.c -- host tests for wubu_sync (spinlock + ISR-safe FIFO). |
| `kernel/` | `test_theme_hid` | 92 | assert.h, stddef.h, stdint.h, stdio.h, string.h, wubu_hid.c, wubu_hid.h, wubu_th | test_theme_hid.c -- host tests for the /theme namespace + unified HID. Builds the two freestanding k |
| `kernel/` | `test_tutor` | 174 | stdio.h, string.h, wubu_tutor.h | test_tutor.c -- host tests for the HX-C learning/education frontier. |
| `kernel/` | `test_vdso` | 51 | stdint.h, stdio.h, string.h, wubu_vdso.c, wubu_vdso.h | test_vdso.c -- host tests for the vDSO page (gap H6). The VA mapping is metal-only; the header + the |
| `kernel/` | `test_verifier` | 80 | stdbool.h, stdint.h, stdio.h, wubu_verifier.c, wubu_verifier.h | test_verifier.c -- host tests for the DA-3 independent verifier. Builds wubu_verifier.c with a minim |
| `kernel/` | `test_vmm` | 110 | stdint.h, stdio.h, wubu_memmap.h, wubu_sync.h, wubu_vmm.c, wubu_vmm.h | test_vmm.c -- host tests for wubu_vmm (bitmap allocator + demand registry). The page-table map + dem |
| `kernel/` | `test_wdt` | 34 | stdint.h, stdio.h, wubu_wdt.h | test_wdt.c -- host tests for the 8254 watchdog helpers (gap E7). The port I/O is metal-only; the pur |
| `kernel/` | `test_xhci` | 72 | egojar.h, stdint.h, stdio.h, string.h, wubu_xhci.c, wubu_xhci.h | test_xhci.c -- host tests for the xHCI controller driver (gap E1). The PCI scan + the BAR0 read are  |
| `kernel/` | `tokenizer_test` | 203 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | tokenizer_test.c -- B3: a self-hosted TOKENIZER (a real compiler stage) is emitted from its own holy |
| `kernel/` | `w9pc_test` | 116 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, styx.h, styxfs.h, wubu_9pc.h, w | test_w9pc.c -- the KERNEL-CLEAN 9P client (wubu_9pc) drives a real Styx/9P server and feeds the load |
| `kernel/` | `wubu_accel_selftest` | 51 | stdio.h, string.h, wubu_accel.h, wubu_hw_detect.h | wubu_accel_selftest.c -- verifies kernel-owned NPU/accelerator routing. |
| `kernel/` | `wubu_adreno600_selftest` | 41 | stdio.h, string.h, wubu_adreno600.h, wubu_hw_detect.h | wubu_adreno600_selftest.c -- verifies Qualcomm Adreno 600 routing. |
| `kernel/` | `wubu_adreno700_selftest` | 41 | stdio.h, string.h, wubu_adreno700.h, wubu_hw_detect.h | wubu_adreno700_selftest.c -- verifies Qualcomm Adreno 700 routing. |
| `kernel/` | `wubu_aec_selftest` | 57 | stdio.h, string.h, wubu_aec.h, wubu_hw_detect.h | wubu_aec_selftest.c -- verifies kernel-owned AEC routing. |
| `kernel/` | `wubu_ampere_selftest` | 41 | stdio.h, string.h, wubu_ampere.h, wubu_hw_detect.h | wubu_ampere_selftest.c -- verifies NVIDIA Ampere routing. |
| `kernel/` | `wubu_arctic_islands_selftest` | 41 | stdio.h, string.h, wubu_arctic_islands.h, wubu_hw_detect.h | wubu_arctic_islands_selftest.c -- verifies AMD GCN4 routing. |
| `kernel/` | `wubu_audio_selftest` | 95 | assert.h, stdio.h, string.h, wubu_audio.h, wubu_hw_detect.h | wubu_audio_selftest.c -- verifies the kernel-owned audio driver routing. Tests the gaps closed: 1. A |
| `kernel/` | `wubu_audiofw_selftest` | 65 | stdio.h, string.h, wubu_audiofw.h, wubu_hw_detect.h | wubu_audiofw_selftest.c -- verifies audio firmware routing. |
| `kernel/` | `wubu_auracast_selftest` | 45 | stdio.h, string.h, wubu_auracast.h, wubu_hw_detect.h | wubu_auracast_selftest.c -- verifies Bluetooth Auracast routing. |
| `kernel/` | `wubu_backlight_selftest` | 62 | stdio.h, string.h, wubu_backlight.h, wubu_hw_detect.h | wubu_backlight_selftest.c -- verifies kernel-owned backlight/WoL routing. |
| `kernel/` | `wubu_backlightpwm_selftest` | 64 | stdio.h, string.h, wubu_backlightpwm.h, wubu_hw_detect.h | wubu_backlightpwm_selftest.c -- verifies kernel-owned backlight routing. |
| `kernel/` | `wubu_bap_selftest` | 44 | stdio.h, string.h, wubu_bap.h, wubu_hw_detect.h | wubu_bap_selftest.c -- verifies Bluetooth BAP routing. |
| `kernel/` | `wubu_bcache_selftest` | 40 | stdio.h, string.h, wubu_bcache.h, wubu_hw_detect.h | wubu_bcache_selftest.c -- verifies storage bcache routing. |
| `kernel/` | `wubu_bio_selftest` | 57 | stdio.h, string.h, wubu_bio.h, wubu_hw_detect.h | wubu_bio_selftest.c -- verifies storage bio routing. |
| `kernel/` | `wubu_blkqos_selftest` | 57 | stdio.h, string.h, wubu_blkqos.h, wubu_hw_detect.h | wubu_blkqos_selftest.c -- verifies blk-QoS throttling routing. |
| `kernel/` | `wubu_bt_selftest` | 47 | stdio.h, string.h, wubu_bt.h, wubu_hw_detect.h | wubu_bt_selftest.c -- verifies kernel-owned Bluetooth routing. |
| `kernel/` | `wubu_bta2dp_selftest` | 48 | stdio.h, string.h, wubu_bta2dp.h, wubu_hw_detect.h | wubu_bta2dp_selftest.c -- verifies Bluetooth A2DP routing. |
| `kernel/` | `wubu_btamesh_selftest` | 44 | stdio.h, string.h, wubu_btamesh.h, wubu_hw_detect.h | wubu_btamesh_selftest.c -- verifies Bluetooth mesh routing. |
| `kernel/` | `wubu_btaudio_selftest` | 45 | stdio.h, string.h, wubu_btaudio.h, wubu_hw_detect.h | wubu_btaudio_selftest.c -- verifies Bluetooth audio profile routing. |
| `kernel/` | `wubu_btbeacon_selftest` | 45 | stdio.h, string.h, wubu_btbeacon.h, wubu_hw_detect.h | wubu_btbeacon_selftest.c -- verifies Bluetooth beacon routing. |
| `kernel/` | `wubu_btclassic_selftest` | 43 | stdio.h, string.h, wubu_btclassic.h, wubu_hw_detect.h | wubu_btclassic_selftest.c -- verifies Bluetooth classic routing. |
| `kernel/` | `wubu_bthfp_selftest` | 44 | stdio.h, string.h, wubu_bthfp.h, wubu_hw_detect.h | wubu_bthfp_selftest.c -- verifies Bluetooth HSP/HFP routing. |
| `kernel/` | `wubu_bus_selftest` | 63 | stdio.h, string.h, wubu_bus.h, wubu_hw_detect.h | wubu_bus_selftest.c -- verifies kernel-owned I2C/SPI bus routing. |
| `kernel/` | `wubu_calib_selftest` | 49 | stdio.h, string.h, wubu_calib.h, wubu_hw_detect.h | wubu_calib_selftest.c -- verifies kernel-owned calibration routing. |
| `kernel/` | `wubu_camera_selftest` | 50 | stdio.h, string.h, wubu_camera.h, wubu_hw_detect.h | wubu_camera_selftest.c -- verifies kernel-owned V4L2 camera routing. |
| `kernel/` | `wubu_can_selftest` | 53 | stdio.h, string.h, wubu_can.h, wubu_hw_detect.h | wubu_can_selftest.c -- verifies kernel-owned CAN bus routing. |
| `kernel/` | `wubu_chanmap_selftest` | 59 | stdio.h, string.h, wubu_chanmap.h, wubu_hw_detect.h | wubu_chanmap_selftest.c -- verifies kernel-owned channel-map routing. |
| `kernel/` | `wubu_clock_selftest` | 67 | stdio.h, string.h, wubu_clock.h, wubu_hw_detect.h | wubu_clock_selftest.c -- verifies kernel-owned clock/thermal routing. |
| `kernel/` | `wubu_cmb_selftest` | 57 | stdio.h, string.h, wubu_cmb.h, wubu_hw_detect.h | wubu_cmb_selftest.c -- verifies kernel-owned CMB routing. |
| `kernel/` | `wubu_codec_selftest` | 60 | stdio.h, string.h, wubu_codec.h, wubu_hw_detect.h | wubu_codec_selftest.c -- verifies kernel-owned audio codec routing. |
| `kernel/` | `wubu_codecgraph_selftest` | 60 | stdio.h, string.h, wubu_codecgraph.h, wubu_hw_detect.h | wubu_codecgraph_selftest.c -- verifies kernel-owned codec-graph routing. |
| `kernel/` | `wubu_colormgmt_selftest` | 60 | stdio.h, string.h, wubu_colormgmt.h, wubu_hw_detect.h | wubu_colormgmt_selftest.c -- verifies kernel-owned color-mgmt routing. |
| `kernel/` | `wubu_compress_selftest` | 59 | stdio.h, string.h, wubu_compress.h, wubu_hw_detect.h | wubu_compress_selftest.c -- verifies kernel-owned compression routing. |
| `kernel/` | `wubu_compressor_selftest` | 60 | stdio.h, string.h, wubu_compressor.h, wubu_hw_detect.h | wubu_compressor_selftest.c -- verifies compressor routing. |
| `kernel/` | `wubu_compute_selftest` | 51 | stdio.h, string.h, wubu_compute.h, wubu_hw_detect.h | wubu_compute_selftest.c -- verifies kernel-owned graphics compute routing. |
| `kernel/` | `wubu_computectx_selftest` | 57 | stdio.h, string.h, wubu_computectx.h, wubu_hw_detect.h | wubu_computectx_selftest.c -- verifies compute-context routing. |
| `kernel/` | `wubu_cuda_selftest` | 41 | stdio.h, string.h, wubu_cuda.h, wubu_hw_detect.h | wubu_cuda_selftest.c -- verifies CUDA runtime routing. |
| `kernel/` | `wubu_dapm_selftest` | 42 | stdio.h, string.h, wubu_dapm.h, wubu_hw_detect.h | wubu_dapm_selftest.c -- verifies audio DAPM routing. |
| `kernel/` | `wubu_dapmwidget_selftest` | 47 | stdio.h, string.h, wubu_dapmwidget.h, wubu_hw_detect.h | wubu_dapmwidget_selftest.c -- verifies DAPM widget routing. |
| `kernel/` | `wubu_dappath_selftest` | 71 | stdio.h, string.h, wubu_dappath.h, wubu_hw_detect.h | wubu_dappath_selftest.c -- verifies DAPM path routing. |
| `kernel/` | `wubu_dax_selftest` | 57 | stdio.h, string.h, wubu_dax.h, wubu_hw_detect.h | wubu_dax_selftest.c -- verifies kernel-owned DAX routing. |
| `kernel/` | `wubu_ddcci_selftest` | 59 | stdio.h, string.h, wubu_ddcci.h, wubu_hw_detect.h | wubu_ddcci_selftest.c -- verifies kernel-owned DDC/CI routing. |
| `kernel/` | `wubu_decode_selftest` | 71 | stdio.h, string.h, wubu_decode.h, wubu_hw_detect.h | wubu_decode_selftest.c -- verifies GPU video decode routing. |
| `kernel/` | `wubu_dedup_selftest` | 59 | stdio.h, string.h, wubu_dedup.h, wubu_hw_detect.h | wubu_dedup_selftest.c -- verifies kernel-owned dedup routing. |
| `kernel/` | `wubu_density_selftest` | 65 | egojar.h, stdio.h, string.h, wubu_density_plan.h | wubu_density_selftest.c -- SELFTEST for the AGF density planner. PROVEN: theme_write(0) -> theme_wri |
| `kernel/` | `wubu_devmapper_selftest` | 66 | stdio.h, string.h, wubu_devmapper.h, wubu_hw_detect.h | wubu_devmapper_selftest.c -- verifies DM routing. |
| `kernel/` | `wubu_display_selftest` | 53 | stdio.h, string.h, wubu_display.h, wubu_hw_detect.h | wubu_display_selftest.c -- verifies kernel-owned display driver routing. |
| `kernel/` | `wubu_dmcrypt_selftest` | 55 | stdio.h, string.h, wubu_dmcrypt.h, wubu_hw_detect.h | wubu_dmcrypt_selftest.c -- verifies kernel-owned dm-crypt routing. |
| `kernel/` | `wubu_drm_selftest` | 71 | stdio.h, string.h, wubu_drm.h, wubu_hw_detect.h | wubu_drm_selftest.c -- verifies GPU DRM routing. |
| `kernel/` | `wubu_drmx_selftest` | 55 | stdio.h, string.h, wubu_drmx.h, wubu_hw_detect.h | wubu_drmx_selftest.c -- verifies kernel-owned DRM-advanced routing. |
| `kernel/` | `wubu_drv_install_selftest` | 182 | stdio.h, stdlib.h, string.h, wubu_drv.h, wubu_drv_install.h | wubu_drv_install_selftest.c -- verifies the DRIVER SELF-INSTALLATION arm: the AGI-OS installs its ow |
| `kernel/` | `wubu_dspgraph_selftest` | 47 | stdio.h, string.h, wubu_dspgraph.h, wubu_hw_detect.h | wubu_dspgraph_selftest.c -- verifies kernel-owned DSP-graph routing. |
| `kernel/` | `wubu_dspmode_selftest` | 47 | stdio.h, string.h, wubu_dspmode.h, wubu_hw_detect.h | wubu_dspmode_selftest.c -- verifies kernel-owned DSP-mode routing. |
| `kernel/` | `wubu_dsptrace_selftest` | 44 | stdio.h, string.h, wubu_dsptrace.h, wubu_hw_detect.h | wubu_dsptrace_selftest.c -- verifies audio DSP trace routing. |
| `kernel/` | `wubu_ducking_selftest` | 59 | stdio.h, string.h, wubu_ducking.h, wubu_hw_detect.h | wubu_ducking_selftest.c -- verifies kernel-owned ducking routing. |
| `kernel/` | `wubu_encode_selftest` | 67 | stdio.h, string.h, wubu_encode.h, wubu_hw_detect.h | wubu_encode_selftest.c -- verifies GPU video encode routing. |
| `kernel/` | `wubu_eq_selftest` | 49 | stdio.h, string.h, wubu_eq.h, wubu_hw_detect.h | wubu_eq_selftest.c -- verifies kernel-owned audio-EQ routing. |
| `kernel/` | `wubu_evdev_test` | 101 | egojar.h, input.h, stdio.h, wubu_evdev.h, wubu_steaminput.h | wubu_evdev_test.c -- the evdev pipeline behavior gate. This is not an assert-nothing test: every cas |
| `kernel/` | `wubu_fantml_selftest` | 40 | stdio.h, string.h, wubu_fantml.h, wubu_hw_detect.h | wubu_fantml_selftest.c -- verifies GPU fan + thermal routing. |
| `kernel/` | `wubu_fbcon_selftest` | 53 | stdio.h, string.h, wubu_fbcon.h, wubu_hw_detect.h | wubu_fbcon_selftest.c -- verifies framebuffer console routing. |
| `kernel/` | `wubu_fc_selftest` | 55 | stdio.h, string.h, wubu_fc.h, wubu_hw_detect.h | wubu_fc_selftest.c -- verifies kernel-owned ethernet-FC routing. |
| `kernel/` | `wubu_fence_selftest` | 53 | stdio.h, string.h, wubu_fence.h, wubu_hw_detect.h | wubu_fence_selftest.c -- verifies kernel-owned GPU fence routing. |
| `kernel/` | `wubu_fencesync_selftest` | 57 | stdio.h, string.h, wubu_fencesync.h, wubu_hw_detect.h | wubu_fencesync_selftest.c -- verifies GPU fence sync routing. |
| `kernel/` | `wubu_filter_selftest` | 72 | math.h, stdio.h, string.h, wubu_filter.h, wubu_hw_detect.h | wubu_filter_selftest.c -- verifies kernel-owned DSP filter routing. |
| `kernel/` | `wubu_fingerprint_selftest` | 50 | stdio.h, string.h, wubu_fingerprint.h, wubu_hw_detect.h | wubu_fingerprint_selftest.c -- verifies kernel-owned biometric routing. |
| `kernel/` | `wubu_flush2_selftest` | 59 | stdio.h, string.h, wubu_flush2.h, wubu_hw_detect.h | wubu_flush2_selftest.c -- verifies storage flush/barrier routing. |
| `kernel/` | `wubu_flush_selftest` | 60 | stdio.h, string.h, wubu_flush.h, wubu_hw_detect.h | wubu_flush_selftest.c -- verifies kernel-owned flush routing. |
| `kernel/` | `wubu_fpga_selftest` | 47 | stdio.h, string.h, wubu_fpga.h, wubu_hw_detect.h | wubu_fpga_selftest.c -- verifies kernel-owned FPGA routing. |
| `kernel/` | `wubu_fusefs_selftest` | 66 | stdio.h, string.h, wubu_fusefs.h, wubu_hw_detect.h | wubu_fusefs_selftest.c -- verifies storage FUSE routing. |
| `kernel/` | `wubu_fw_selftest` | 61 | stdio.h, string.h, wubu_fw.h, wubu_hw_detect.h | wubu_fw_selftest.c -- verifies kernel-owned firmware routing. |
| `kernel/` | `wubu_gadget_selftest` | 61 | stdio.h, string.h, wubu_gadget.h, wubu_hw_detect.h | wubu_gadget_selftest.c -- verifies kernel-owned gadget/endurance routing. |
| `kernel/` | `wubu_gamepad_selftest` | 61 | stdio.h, string.h, wubu_gamepad.h, wubu_hw_detect.h | wubu_gamepad_selftest.c -- verifies kernel-owned gamepad/DSC routing. |
| `kernel/` | `wubu_gamepadbm_selftest` | 44 | stdio.h, string.h, wubu_gamepadbm.h, wubu_hw_detect.h | wubu_gamepadbm_selftest.c -- verifies gamepad button map routing. |
| `kernel/` | `wubu_gamepaddz_selftest` | 44 | stdio.h, string.h, wubu_gamepaddz.h, wubu_hw_detect.h | wubu_gamepaddz_selftest.c -- verifies gamepad deadzone routing. |
| `kernel/` | `wubu_gesture_selftest` | 211 | stdio.h, string.h, wubu_gesture.h | wubu_gesture_selftest.c -- verifies the multitouch gesture recogniser with synthetic contact sequenc |
| `kernel/` | `wubu_gpuband_selftest` | 59 | stdio.h, string.h, wubu_gpuband.h, wubu_hw_detect.h | wubu_gpuband_selftest.c -- verifies kernel-owned GPU-band routing. |
| `kernel/` | `wubu_gpucsched_selftest` | 44 | stdio.h, string.h, wubu_gpucsched.h, wubu_hw_detect.h | wubu_gpucsched_selftest.c -- verifies GPU compute scheduler routing. |
| `kernel/` | `wubu_gpudc_selftest` | 44 | stdio.h, string.h, wubu_gpudc.h, wubu_hw_detect.h | wubu_gpudc_selftest.c -- verifies GPU display controller routing. |
| `kernel/` | `wubu_gpufw_selftest` | 44 | stdio.h, string.h, wubu_gpufw.h, wubu_hw_detect.h | wubu_gpufw_selftest.c -- verifies GPU firmware routing. |
| `kernel/` | `wubu_gpufwupd_selftest` | 45 | stdio.h, string.h, wubu_gpufwupd.h, wubu_hw_detect.h | wubu_gpufwupd_selftest.c -- verifies GPU firmware update routing. |
| `kernel/` | `wubu_gpukms_selftest` | 46 | stdio.h, string.h, wubu_gpukms.h, wubu_hw_detect.h | wubu_gpukms_selftest.c -- verifies GPU KMS modeset routing. |
| `kernel/` | `wubu_gpumem_selftest` | 46 | stdio.h, string.h, wubu_gpumem.h, wubu_hw_detect.h | wubu_gpumem_selftest.c -- verifies GPU memory bandwidth routing. |
| `kernel/` | `wubu_gpurst_selftest` | 61 | stdio.h, string.h, wubu_gpurst.h, wubu_hw_detect.h | wubu_gpurst_selftest.c -- verifies kernel-owned GPU reset routing. |
| `kernel/` | `wubu_gpusched_selftest` | 58 | stdio.h, string.h, wubu_gpusched.h, wubu_hw_detect.h | wubu_gpusched_selftest.c -- verifies kernel-owned GPU-scheduler routing. |
| `kernel/` | `wubu_gpusensor_selftest` | 58 | stdio.h, string.h, wubu_gpusensor.h, wubu_hw_detect.h | wubu_gpusensor_selftest.c -- verifies kernel-owned GPU-sensor routing. |
| `kernel/` | `wubu_gpushader_selftest` | 45 | stdio.h, string.h, wubu_gpushader.h, wubu_hw_detect.h | wubu_gpushader_selftest.c -- verifies GPU shader model routing. |
| `kernel/` | `wubu_gt2xx_selftest` | 41 | stdio.h, string.h, wubu_gt2xx.h, wubu_hw_detect.h | wubu_gt2xx_selftest.c -- verifies NVIDIA GT2xx legacy routing. |
| `kernel/` | `wubu_hdr_selftest` | 59 | stdio.h, string.h, wubu_hdr.h, wubu_hw_detect.h | wubu_hdr_selftest.c -- verifies kernel-owned HDR/jack routing. |
| `kernel/` | `wubu_hid_selftest` | 51 | stdio.h, string.h, wubu_hid.h, wubu_hw_detect.h | wubu_hid_selftest.c -- verifies kernel-owned HID routing. |
| `kernel/` | `wubu_hidadv_selftest` | 51 | stdio.h, string.h, wubu_hidadv.h, wubu_hw_detect.h | wubu_hidadv_selftest.c -- verifies kernel-owned HID routing. |
| `kernel/` | `wubu_hw_detect_selftest` | 133 | egojar.h, stdio.h, stdlib.h, string.h, wubu_drv_gpu.h, wubu_hw_detect.h, wubu_kv | wubu_hw_detect_selftest.c — proves the OS detects its own runtime. The doctrine: "it should work on  |
| `kernel/` | `wubu_ieccontrol_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_ieccontrol.h | wubu_ieccontrol_selftest.c -- verifies IEC control routing. |
| `kernel/` | `wubu_ima_selftest` | 57 | stdio.h, string.h, wubu_hw_detect.h, wubu_ima.h | wubu_ima_selftest.c -- verifies kernel-owned IMA/EVM routing. |
| `kernel/` | `wubu_input_selftest` | 97 | stdio.h, string.h, wubu_hw_detect.h, wubu_input.h | wubu_input_selftest.c -- verifies kernel-owned input driver routing. Tests the gaps closed: 1. Contr |
| `kernel/` | `wubu_instinct_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_instinct.h | wubu_instinct_selftest.c -- verifies AMD Instinct MI routing. |
| `kernel/` | `wubu_intel_gma_selftest` | 42 | stdio.h, string.h, wubu_hw_detect.h, wubu_intel_gma.h | wubu_intel_gma_selftest.c -- verifies Intel GMA legacy GPU routing. |
| `kernel/` | `wubu_intel_icelake_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_intel_icelake.h | wubu_intel_icelake_selftest.c -- verifies Intel Gen11 Ice Lake routing. |
| `kernel/` | `wubu_intel_skylake_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_intel_skylake.h | wubu_intel_skylake_selftest.c -- verifies Intel Gen9 Skylake routing. |
| `kernel/` | `wubu_intelgpu_selftest` | 45 | stdio.h, string.h, wubu_hw_detect.h, wubu_intelgpu.h | wubu_intelgpu_selftest.c -- verifies Intel GPU routing. |
| `kernel/` | `wubu_ioprio_selftest` | 63 | stdio.h, string.h, wubu_hw_detect.h, wubu_ioprio.h | wubu_ioprio_selftest.c -- verifies I/O priority routing. |
| `kernel/` | `wubu_iosched_selftest` | 61 | stdio.h, string.h, wubu_hw_detect.h, wubu_iosched.h | wubu_iosched_selftest.c -- verifies kernel-owned I/O-scheduler routing. |
| `kernel/` | `wubu_jack_selftest` | 60 | stdio.h, string.h, wubu_hw_detect.h, wubu_jack.h | wubu_jack_selftest.c -- verifies audio jack detection routing. |
| `kernel/` | `wubu_jackdetect_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_jackdetect.h | wubu_jackdetect_selftest.c -- verifies jack-detection routing. |
| `kernel/` | `wubu_jackimpedance_selftest` | 66 | stdio.h, string.h, wubu_hw_detect.h, wubu_jackimpedance.h | wubu_jackimpedance_selftest.c -- verifies jack impedance routing. |
| `kernel/` | `wubu_jackstate_selftest` | 65 | stdio.h, string.h, wubu_hw_detect.h, wubu_jackstate.h | wubu_jackstate_selftest.c -- verifies jack-state routing. |
| `kernel/` | `wubu_kernel_bridge_test` | 88 | stdio.h, string.h, wubudriver.h, wubukcore.h | Bridge gate: WuBuOS consumes WuBuKernel's driver model and core services. This is the proof that WuB |
| `kernel/` | `wubu_kvfs_selftest` | 135 | stdint.h, stdio.h, stdlib.h, string.h, wubu_kvfs.h | wubu_kvfs_selftest.c — validates the kernel KV namespace port. Exercises the boundary contract with  |
| `kernel/` | `wubu_leaudio_selftest` | 42 | stdio.h, string.h, wubu_hw_detect.h, wubu_leaudio.h | wubu_leaudio_selftest.c -- verifies Bluetooth LE Audio routing. |
| `kernel/` | `wubu_leaudioldr_selftest` | 43 | stdio.h, string.h, wubu_hw_detect.h, wubu_leaudioldr.h | wubu_leaudioldr_selftest.c -- verifies Bluetooth LE Audio routing. |
| `kernel/` | `wubu_loudness_selftest` | 58 | stdio.h, string.h, wubu_hw_detect.h, wubu_loudness.h | wubu_loudness_selftest.c -- verifies kernel-owned loudness routing. |
| `kernel/` | `wubu_lvm_selftest` | 39 | stdio.h, string.h, wubu_hw_detect.h, wubu_lvm.h | wubu_lvm_selftest.c -- verifies storage LVM routing. |
| `kernel/` | `wubu_mali_g52_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_mali_g52.h | wubu_mali_g52_selftest.c -- verifies ARM Mali G52 routing. |
| `kernel/` | `wubu_mali_g720_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_mali_g720.h | wubu_mali_g720_selftest.c -- verifies ARM Mali G720 routing. |
| `kernel/` | `wubu_mali_g77_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_mali_g77.h | wubu_mali_g77_selftest.c -- verifies ARM Mali G77 routing. |
| `kernel/` | `wubu_mdraid_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_mdraid.h | wubu_mdraid_selftest.c -- verifies storage MD RAID routing. |
| `kernel/` | `wubu_mem_selftest` | 51 | stdio.h, string.h, wubu_hw_detect.h, wubu_mem.h | wubu_mem_selftest.c -- verifies kernel-owned memory/ECC routing. |
| `kernel/` | `wubu_memmgr_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_memmgr.h | wubu_memmgr_selftest.c -- verifies GPU memory-manager routing. |
| `kernel/` | `wubu_mixergraph_selftest` | 70 | stdio.h, string.h, wubu_hw_detect.h, wubu_mixergraph.h | wubu_mixergraph_selftest.c -- verifies mixer graph routing. |
| `kernel/` | `wubu_mixgraph_selftest` | 50 | stdio.h, string.h, wubu_hw_detect.h, wubu_mixgraph.h | wubu_mixgraph_selftest.c -- verifies kernel-owned audio-graph routing. |
| `kernel/` | `wubu_mmu_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_mmu.h | wubu_mmu_selftest.c -- verifies GPU MMU routing. |
| `kernel/` | `wubu_mst_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_mst.h | wubu_mst_selftest.c -- verifies kernel-owned MST/SRC routing. |
| `kernel/` | `wubu_multigig_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_multigig.h | wubu_multigig_selftest.c -- verifies kernel-owned multi-gig routing. |
| `kernel/` | `wubu_navi10_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_navi10.h | wubu_navi10_selftest.c -- verifies AMD Navi10 RDNA1 routing. |
| `kernel/` | `wubu_net_selftest` | 66 | stdio.h, string.h, wubu_hw_detect.h, wubu_net.h | wubu_net_selftest.c -- verifies kernel-owned network driver routing. Tests the gaps closed: 1. Wi-Fi |
| `kernel/` | `wubu_nfsclient_selftest` | 56 | stdio.h, string.h, wubu_hw_detect.h, wubu_nfsclient.h | wubu_nfsclient_selftest.c -- verifies NFS client routing. |
| `kernel/` | `wubu_nfsmount_selftest` | 64 | stdio.h, string.h, wubu_hw_detect.h, wubu_nfsmount.h | wubu_nfsmount_selftest.c -- verifies NFS mount routing. |
| `kernel/` | `wubu_nicoffload_selftest` | 58 | stdio.h, string.h, wubu_hw_detect.h, wubu_nicoffload.h | wubu_nicoffload_selftest.c -- verifies kernel-owned NIC offload routing. |
| `kernel/` | `wubu_ns_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_ns.h | wubu_ns_selftest.c -- verifies kernel-owned NVMe-namespace routing. |
| `kernel/` | `wubu_nvidia_fermi_selftest` | 40 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_fermi.h | wubu_nvidia_fermi_selftest.c -- verifies NVIDIA Fermi legacy routing. |
| `kernel/` | `wubu_nvidia_kepler_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_kepler.h | wubu_nvidia_kepler_selftest.c -- verifies NVIDIA Kepler legacy routing. |
| `kernel/` | `wubu_nvidia_maxwell_selftest` | 42 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_maxwell.h | wubu_nvidia_maxwell_selftest.c -- verifies NVIDIA Maxwell routing. |
| `kernel/` | `wubu_nvidia_pascal_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_pascal.h | wubu_nvidia_pascal_selftest.c -- verifies NVIDIA Pascal routing. |
| `kernel/` | `wubu_nvidia_turing_selftest` | 42 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_turing.h | wubu_nvidia_turing_selftest.c -- verifies NVIDIA Turing routing. |
| `kernel/` | `wubu_nvidia_volta_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_volta.h | wubu_nvidia_volta_selftest.c -- verifies NVIDIA Volta routing. |
| `kernel/` | `wubu_nvme_gen4_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvme_gen4.h | wubu_nvme_gen4_selftest.c -- verifies NVMe Gen4 routing. |
| `kernel/` | `wubu_nvme_gen5_selftest` | 43 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvme_gen5.h | wubu_nvme_gen5_selftest.c -- verifies NVMe Gen5 routing. |
| `kernel/` | `wubu_nvmehotplug_selftest` | 43 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvmehotplug.h | wubu_nvmehotplug_selftest.c -- verifies NVMe hotplug routing. |
| `kernel/` | `wubu_nvmepower_selftest` | 43 | stdio.h, string.h, wubu_hw_detect.h, wubu_nvmepower.h | wubu_nvmepower_selftest.c -- verifies NVMe power state routing. |
| `kernel/` | `wubu_opencl_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_opencl.h | wubu_opencl_selftest.c -- verifies OpenCL runtime routing. |
| `kernel/` | `wubu_overclock_selftest` | 61 | stdio.h, string.h, wubu_hw_detect.h, wubu_overclock.h | wubu_overclock_selftest.c -- verifies kernel-owned overclock routing. |
| `kernel/` | `wubu_panel_selftest` | 62 | stdio.h, string.h, wubu_hw_detect.h, wubu_panel.h | wubu_panel_selftest.c -- verifies GPU panel routing. |
| `kernel/` | `wubu_pci_selftest` | 125 | libc.h, stdint.h, wubu_pci.h | wubu_pci_selftest.c -- SELFTEST for the PCI bus enumerator. PROVEN: wubu_pci_scan() filters empty (0 |
| `kernel/` | `wubu_pcipme_selftest` | 45 | stdio.h, string.h, wubu_hw_detect.h, wubu_pcipme.h | wubu_pcipme_selftest.c -- verifies kernel-owned PCI PME routing. |
| `kernel/` | `wubu_pcmlink_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_pcmlink.h | wubu_pcmlink_selftest.c -- verifies audio PCM link routing. |
| `kernel/` | `wubu_pcmplugin_selftest` | 63 | stdio.h, string.h, wubu_hw_detect.h, wubu_pcmplugin.h | wubu_pcmplugin_selftest.c -- verifies PCM plugin routing. |
| `kernel/` | `wubu_pcmring_selftest` | 40 | stdio.h, string.h, wubu_hw_detect.h, wubu_pcmring.h | wubu_pcmring_selftest.c -- verifies audio PCM ring buffer routing. |
| `kernel/` | `wubu_pd_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_pd.h | wubu_pd_selftest.c -- verifies kernel-owned USB-PD/flow-steering routing. |
| `kernel/` | `wubu_pdpolicy_selftest` | 60 | stdio.h, string.h, wubu_hw_detect.h, wubu_pdpolicy.h | wubu_pdpolicy_selftest.c -- verifies kernel-owned PD-policy routing. |
| `kernel/` | `wubu_pe_exec_test` | 134 | egojar.h, stdint.h, stdio.h, string.h, wubu_pe.h | wubu_pe_exec_test.c — TLS callbacks + entry-point execution for the in-kernel PE loader. Builds a PE |
| `kernel/` | `wubu_pe_full_test` | 131 | egojar.h, egojar_net.h, stdint.h, stdio.h, string.h, wubu_pe.h | wubu_pe_full_test.c — the full load→entry pipeline end to end. Builds a PE whose .text entry is `cal |
| `kernel/` | `wubu_pe_globals_test` | 191 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | wubu_pe_globals_test.c -- self-hosting rung 2: a HOLYD MODULE THAT WRITES A GLOBAL survives the emit |
| `kernel/` | `wubu_pe_holyd_test` | 122 | egojar.h, holyd.h, holyd_codegen.h, stdint.h, stdio.h, string.h, wubu_pe.h | lets busily-relocatable holyd modules survive an arbitrary base. */ |
| `kernel/` | `wubu_pe_iat_test` | 141 | stdint.h, stdio.h, string.h, wubu_pe.h | wubu_pe_iat_test.c — import address table snapping for the in-kernel PE loader. Builds a PE whose .i |
| `kernel/` | `wubu_pe_import_test` | 81 | egojar.h, holyd.h, holyd_codegen.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_ | wubu_pe_import_test.c -- rung 6: holyd EMITS an import-carrying PE. Rungs 2-5 proved holyd emits pos |
| `kernel/` | `wubu_pe_modsym_test` | 130 | egojar.h, egojar_net.h, stdint.h, stdio.h, string.h, wubu_modsym.h, wubu_pe.h | wubu_pe_modsym_test.c — P2 Piece A: the loader's import resolution goes through the RUNTIME-EXTENSIB |
| `kernel/` | `wubu_pe_program_test` | 278 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | wubu_pe_program_test.c -- self-hosting rung 3: a holyd PROGRAM (module init that WRITES a global AND |
| `kernel/` | `wubu_pe_reloc_test` | 135 | stdint.h, stdio.h, string.h, wubu_pe.h | wubu_pe_reloc_test.c — base relocation (.reloc) round-trip for the in-kernel PE loader. Builds a min |
| `kernel/` | `wubu_pe_roundtrip_test` | 103 | egojar.h, egojar_net.h, holyd.h, holyd_codegen.h, stdint.h, stdio.h, stdlib.h, s | wubu_pe_roundtrip_test.c -- P2 CLOSING FORCING FUNCTION (cross-repo). The full self-hosting chain: h |
| `kernel/` | `wubu_perf_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_perf.h | wubu_perf_selftest.c -- verifies GPU performance counter routing. |
| `kernel/` | `wubu_perfmon_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_perfmon.h | wubu_perfmon_selftest.c -- verifies GPU perf-counter routing. |
| `kernel/` | `wubu_peripheral_selftest` | 43 | stdio.h, string.h, wubu_hw_detect.h, wubu_peripheral.h | wubu_peripheral_selftest.c -- verifies kernel-owned peripheral routing. |
| `kernel/` | `wubu_phy_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_phy.h | wubu_phy_selftest.c -- verifies kernel-owned Ethernet PHY routing. |
| `kernel/` | `wubu_pm_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_pm.h | wubu_pm_selftest.c -- verifies kernel-owned power-mode routing. |
| `kernel/` | `wubu_pmicaudio_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_pmicaudio.h | wubu_pmicaudio_selftest.c -- verifies kernel-owned PMIC/audio-analog routing. |
| `kernel/` | `wubu_porttiming_selftest` | 66 | stdio.h, string.h, wubu_hw_detect.h, wubu_porttiming.h | wubu_porttiming_selftest.c -- verifies kernel-owned port-timing routing. |
| `kernel/` | `wubu_power_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_power.h | wubu_power_selftest.c -- verifies kernel-owned CPU/power routing. |
| `kernel/` | `wubu_powergate_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_powergate.h | wubu_powergate_selftest.c -- verifies GPU power-gate routing. |
| `kernel/` | `wubu_powervr_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_powervr.h | wubu_powervr_selftest.c -- verifies Imagination PowerVR routing. |
| `kernel/` | `wubu_probe_selftest` | 340 | stdio.h, string.h, wubu_audio.h, wubu_hw_detect.h, wubu_input.h, wubu_net.h, wub | wubu_probe_selftest.c -- verifies the UNIFIED HARDWARE DISCOVERY. Tests: 1. wubu_probe_all() runs ev |
| `kernel/` | `wubu_psr_selftest` | 57 | stdio.h, string.h, wubu_hw_detect.h, wubu_psr.h | wubu_psr_selftest.c -- verifies kernel-owned PSR/SR-IOV routing. |
| `kernel/` | `wubu_ptp_selftest` | 61 | stdio.h, string.h, wubu_hw_detect.h, wubu_ptp.h | wubu_ptp_selftest.c -- verifies kernel-owned PTP/TSN + haptics routing. |
| `kernel/` | `wubu_ptp_sync_selftest` | 58 | stdio.h, string.h, wubu_hw_detect.h, wubu_ptp_sync.h | wubu_ptp_sync_selftest.c -- verifies kernel-owned PTP routing. |
| `kernel/` | `wubu_qos_selftest` | 51 | stdio.h, string.h, wubu_hw_detect.h, wubu_qos.h | wubu_qos_selftest.c -- verifies kernel-owned QoS/ACL routing. |
| `kernel/` | `wubu_quadro_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_quadro.h | wubu_quadro_selftest.c -- verifies NVIDIA Quadro routing. |
| `kernel/` | `wubu_radeon_5000_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_radeon_5000.h | wubu_radeon_5000_selftest.c -- verifies AMD Radeon HD 5000 routing. |
| `kernel/` | `wubu_radeon_6000_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_radeon_6000.h | wubu_radeon_6000_selftest.c -- verifies AMD Radeon HD 6000 routing. |
| `kernel/` | `wubu_radeon_legacy_selftest` | 44 | stdio.h, string.h, wubu_hw_detect.h, wubu_radeon_legacy.h | wubu_radeon_legacy_selftest.c -- verifies AMD Radeon legacy routing. |
| `kernel/` | `wubu_raid5_selftest` | 67 | stdio.h, string.h, wubu_hw_detect.h, wubu_raid5.h | wubu_raid5_selftest.c -- verifies RAID5 routing. |
| `kernel/` | `wubu_raid_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_raid.h | wubu_raid_selftest.c -- verifies kernel-owned RAID/SAS routing. |
| `kernel/` | `wubu_raidcache_selftest` | 50 | stdio.h, string.h, wubu_hw_detect.h, wubu_raidcache.h | wubu_raidcache_selftest.c -- verifies kernel-owned RAID-cache routing. |
| `kernel/` | `wubu_rdma_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_rdma.h | wubu_rdma_selftest.c -- verifies kernel-owned RDMA routing. |
| `kernel/` | `wubu_rendernode_selftest` | 43 | stdio.h, string.h, wubu_hw_detect.h, wubu_rendernode.h | wubu_rendernode_selftest.c -- verifies GPU render node routing. |
| `kernel/` | `wubu_renoir_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_renoir.h | wubu_renoir_selftest.c -- verifies AMD Raven/Renoir APU routing. |
| `kernel/` | `wubu_rtc_selftest` | 67 | stdio.h, string.h, wubu_hw_detect.h, wubu_rtc.h | wubu_rtc_selftest.c -- verifies kernel-owned RTC/thermal routing. |
| `kernel/` | `wubu_samplerate_selftest` | 62 | stdio.h, string.h, wubu_hw_detect.h, wubu_samplerate.h | wubu_samplerate_selftest.c -- verifies sample-rate routing. |
| `kernel/` | `wubu_sata_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_sata.h | wubu_sata_selftest.c -- verifies kernel-owned SATA/NCQ routing. |
| `kernel/` | `wubu_securekey_selftest` | 54 | stdio.h, string.h, wubu_hw_detect.h, wubu_securekey.h | wubu_securekey_selftest.c -- verifies kernel-owned security routing. |
| `kernel/` | `wubu_sensor_selftest` | 58 | stdio.h, string.h, wubu_hw_detect.h, wubu_sensor.h | wubu_sensor_selftest.c -- verifies kernel-owned IIO sensor routing. |
| `kernel/` | `wubu_smart_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_smart.h | wubu_smart_selftest.c -- verifies kernel-owned SMART routing. |
| `kernel/` | `wubu_smc_selftest` | 63 | stdio.h, string.h, wubu_hw_detect.h, wubu_smc.h | wubu_smc_selftest.c -- verifies SMC firmware routing. |
| `kernel/` | `wubu_smr_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_smr.h | wubu_smr_selftest.c -- verifies kernel-owned SMR routing. |
| `kernel/` | `wubu_spdif_selftest` | 62 | stdio.h, string.h, wubu_hw_detect.h, wubu_spdif.h | wubu_spdif_selftest.c -- verifies kernel-owned SPDIF routing. |
| `kernel/` | `wubu_spdifrx_selftest` | 62 | stdio.h, string.h, wubu_hw_detect.h, wubu_spdifrx.h | wubu_spdifrx_selftest.c -- verifies SPDIF receiver routing. |
| `kernel/` | `wubu_spdifstatus_selftest` | 64 | stdio.h, string.h, wubu_hw_detect.h, wubu_spdifstatus.h | wubu_spdifstatus_selftest.c -- verifies SPDIF status routing. |
| `kernel/` | `wubu_spdiftx_selftest` | 57 | stdio.h, string.h, wubu_hw_detect.h, wubu_spdiftx.h | wubu_spdiftx_selftest.c -- verifies SPDIF TX control routing. |
| `kernel/` | `wubu_storage_selftest` | 92 | assert.h, stdio.h, string.h, wubu_hw_detect.h, wubu_storage.h | wubu_storage_selftest.c -- verifies kernel-owned storage driver routing. Tests the gaps closed: 1. S |
| `kernel/` | `wubu_storagesched_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_storagesched.h | wubu_storagesched_selftest.c -- verifies storage scheduler routing. |
| `kernel/` | `wubu_switchdev_selftest` | 56 | stdio.h, string.h, wubu_hw_detect.h, wubu_switchdev.h | wubu_switchdev_selftest.c -- verifies kernel-owned switch fabric routing. |
| `kernel/` | `wubu_thermal_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_thermal.h | wubu_thermal_selftest.c -- verifies kernel-owned thermal routing. |
| `kernel/` | `wubu_thermalthrottle_selftest` | 56 | stdio.h, string.h, wubu_hw_detect.h, wubu_thermalthrottle.h | wubu_thermalthrottle_selftest.c -- verifies thermal throttling routing. |
| `kernel/` | `wubu_touch_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_touch.h | wubu_touch_selftest.c -- verifies kernel-owned touch routing. |
| `kernel/` | `wubu_tpm_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_tpm.h | wubu_tpm_selftest.c -- verifies kernel-owned TPM routing. |
| `kernel/` | `wubu_trim_selftest` | 59 | stdio.h, string.h, wubu_hw_detect.h, wubu_trim.h | wubu_trim_selftest.c -- verifies kernel-owned TRIM/alt-mode routing. |
| `kernel/` | `wubu_uac_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_uac.h | wubu_uac_selftest.c -- verifies kernel-owned UAC routing. |
| `kernel/` | `wubu_uas_selftest` | 58 | stdio.h, string.h, wubu_hw_detect.h, wubu_uas.h | wubu_uas_selftest.c -- verifies storage UAS routing. |
| `kernel/` | `wubu_ucode_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_ucode.h | wubu_ucode_selftest.c -- verifies kernel-owned microcode routing. |
| `kernel/` | `wubu_usb4_selftest` | 47 | stdio.h, string.h, wubu_hw_detect.h, wubu_usb4.h | wubu_usb4_selftest.c -- verifies kernel-owned USB4/TB routing. |
| `kernel/` | `wubu_usbf_selftest` | 69 | stdio.h, string.h, wubu_hw_detect.h, wubu_usbf.h | wubu_usbf_selftest.c -- verifies kernel-owned USB driver routing. |
| `kernel/` | `wubu_vblank_selftest` | 63 | stdio.h, string.h, wubu_hw_detect.h, wubu_vblank.h | wubu_vblank_selftest.c -- verifies GPU VBLANK routing. |
| `kernel/` | `wubu_vc4_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_vc4.h | wubu_vc4_selftest.c -- verifies Broadcom VideoCore vc4 routing. |
| `kernel/` | `wubu_vc6_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_vc6.h | wubu_vc6_selftest.c -- verifies Broadcom VideoCore VI routing. |
| `kernel/` | `wubu_vega_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_vega.h | wubu_vega_selftest.c -- verifies AMD GCN5 Vega routing. |
| `kernel/` | `wubu_video_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_video.h | wubu_video_selftest.c -- verifies kernel-owned video codec routing. |
| `kernel/` | `wubu_virt_selftest` | 53 | stdio.h, string.h, wubu_hw_detect.h, wubu_virt.h | wubu_virt_selftest.c -- verifies kernel-owned virtualization routing. |
| `kernel/` | `wubu_vlanaudio_selftest` | 56 | stdio.h, string.h, wubu_hw_detect.h, wubu_vlanaudio.h | wubu_vlanaudio_selftest.c -- verifies kernel-owned VLAN/audio-DSP routing. |
| `kernel/` | `wubu_volcanic_islands_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_volcanic_islands.h | wubu_volcanic_islands_selftest.c -- verifies AMD GCN3 routing. |
| `kernel/` | `wubu_voltagectl_selftest` | 37 | stdio.h, string.h, wubu_hw_detect.h, wubu_voltagectl.h | wubu_voltagectl_selftest.c -- verifies GPU voltage control routing. |
| `kernel/` | `wubu_vpudecode_selftest` | 46 | stdio.h, string.h, wubu_hw_detect.h, wubu_vpudecode.h | wubu_vpudecode_selftest.c -- verifies GPU video decode routing. |
| `kernel/` | `wubu_vpuencode_selftest` | 42 | stdio.h, string.h, wubu_hw_detect.h, wubu_vpuencode.h | wubu_vpuencode_selftest.c -- verifies GPU video encode routing. |
| `kernel/` | `wubu_vram_selftest` | 61 | stdio.h, string.h, wubu_hw_detect.h, wubu_vram.h | wubu_vram_selftest.c -- verifies kernel-owned VRAM routing. |
| `kernel/` | `wubu_vrr_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_vrr.h | wubu_vrr_selftest.c -- verifies G-Sync/FreeSync VRR routing. |
| `kernel/` | `wubu_vulkan14_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_vulkan14.h | wubu_vulkan14_selftest.c -- verifies Vulkan 1.4 routing. |
| `kernel/` | `wubu_wifi7_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_wifi7.h | wubu_wifi7_selftest.c -- verifies kernel-owned Wi-Fi 7 routing. |
| `kernel/` | `wubu_wifi_reg_selftest` | 57 | stdio.h, string.h, wubu_hw_detect.h, wubu_wifi_reg.h | wubu_wifi_reg_selftest.c -- verifies kernel-owned WiFi-reg routing. |
| `kernel/` | `wubu_wifiutil_selftest` | 57 | stdio.h, string.h, wubu_hw_detect.h, wubu_wifiutil.h | wubu_wifiutil_selftest.c -- verifies kernel-owned WiFi-util routing. |
| `kernel/` | `wubu_writeback_selftest` | 55 | stdio.h, string.h, wubu_hw_detect.h, wubu_writeback.h | wubu_writeback_selftest.c -- verifies writeback routing. |
| `kernel/` | `wubu_xe3_selftest` | 41 | stdio.h, string.h, wubu_hw_detect.h, wubu_xe3.h | wubu_xe3_selftest.c -- verifies Intel Xe3 (Celestial) routing. |
| `kernel/` | `wubu_znszone_selftest` | 44 | stdio.h, string.h, wubu_hw_detect.h, wubu_znszone.h | wubu_znszone_selftest.c -- verifies ZNS zone routing. |
| `kernel/` | `wubu_zoneappend_selftest` | 46 | stdio.h, string.h, wubu_hw_detect.h, wubu_zoneappend.h | wubu_zoneappend_selftest.c -- verifies ZNS zone append routing. |
| `kernel/` | `wubu_zonecap_selftest` | 42 | stdio.h, string.h, wubu_hw_detect.h, wubu_zonecap.h | wubu_zonecap_selftest.c -- verifies ZNS zone capacity routing. |
| `kernel/` | `wubu_zoned_selftest` | 47 | stdio.h, string.h, wubu_hw_detect.h, wubu_zoned.h | wubu_zoned_selftest.c -- verifies kernel-owned zoned storage routing. |
| `kernel/` | `wubu_zonefmt_selftest` | 44 | stdio.h, string.h, wubu_hw_detect.h, wubu_zonefmt.h | wubu_zonefmt_selftest.c -- verifies ZNS zone reset routing. |
| `kernel/` | `wubu_zonseqwrite_selftest` | 44 | stdio.h, string.h, wubu_hw_detect.h, wubu_zonseqwrite.h | wubu_zonseqwrite_selftest.c -- verifies ZNS sequential write routing. |
| `kernel/` | `wubu_zx9p_test` | 162 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, styx.h, styxfs.h, wubu_modsym.h | wubu_zx9p_test.c -- THE loader consumes a module whose bytes arrive over REAL Styx/9P, not stdio. Th |
| `kernel/` | `wubu_zx_test` | 186 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | wubu_zx_test.c -- the module-RUNNER primitive (wubu_zx_run). Proves ONE reusable loader call runs BO |
| `kernel/` | `wubu_zxreg_test` | 190 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | wubu_zxreg_test.c -- module registry (ZXE-style) semantics. Builds on rungs 4-6 (wubu_zx_run + wubu_ |
| `kernel/` | `zip_selftest` | 80 | memory.h, stdio.h, stdlib.h, string.h, wubu_zip.h | zip_selftest.c -- verify the kernel ZIP reader against OpenArena's real archive. Oracle: the host `u |
| `kernel/` | `zlib_selftest` | 64 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_zlib.h, zlib.h | zlib_selftest.c -- verify the kernel DEFLATE inflater (wubu_inflate) byte-for-byte against the host  |
| `kernel/` | `test_font_case` | 103 | stdio.h, string.h, vbe.h | test_font_case.c -- lower-case text must render as lower case. Regression cover for laundry-list ite |
| `kernel/` | `test_thermal_read` | 181 | dirent.h, stdio.h, stdlib.h, string.h, wubu_drv_thermal.h, wubu_thermal_read.h | test_thermal_read.c -- do the reported temperatures match the hardware? The bug this covers was not  |
| `kernel/` | `test_dram_hedge` | 212 | stdio.h, string.h, wubu_dram_hedge.h | src/kernel/tests/test_dram_hedge.c Selftest for the WuBuOS DRAM-refresh hedge (wubu_dram_hedge.c): p |
| `kernel/` | `txfs` | 345 | stdio.h, stdlib.h, string.h, txfs.h | txfs.c  --  WuBuOS Transactional Filesystem Layer Implementation Cell 100: Journal-based atomic file |
| `kernel/` | `vbe` | 795 | klog.h, math.h, memory.h, stdbool.h, stdlib.h, string.h, vbe.h | vbe.c  --  WuBuOS VBE Framebuffer Implementation Two modes: - Kernel mode (default): uses mem_alloc/ |
| `kernel/` | `wubu_9pc` | 84 | string.h, wubu_9pc.h | local strlen: -nostdlib may not provide the libc symbol |
| `kernel/` | `wubu_accel` | 126 | egojar.h, stdio.h, string.h, wubu_accel.h, wubu_hw_detect.h, wubu_pci.h | wubu_accel.c -- kernel-owned NPU/accelerator driver routing. Modern "AI PCs" ship an on-die NPU (Neu |
| `kernel/` | `wubu_acpi` | 124 | wubu_acpi.h | wubu_acpi.c  --  ACPI table discovery (RSDP -> RSDT/XSDT -> FADT) Gap A18. The RSDP lives either at  |
| `kernel/` | `wubu_adreno600` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_adreno600.h, wubu_hw_detect.h | wubu_adreno600.c -- kernel-owned Qualcomm Adreno 600 GPU routing. Adreno 600 (a6xx) binds the free-s |
| `kernel/` | `wubu_adreno700` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_adreno700.h, wubu_hw_detect.h | wubu_adreno700.c -- kernel-owned Qualcomm Adreno 700 GPU routing. Adreno 700 (7xx series) binds the  |
| `kernel/` | `wubu_aec` | 100 | egojar.h, stdio.h, string.h, wubu_aec.h, wubu_hw_detect.h | wubu_aec.c -- kernel-owned audio AEC + noise suppression routing. AEC (acoustic echo cancellation) + |
| `kernel/` | `wubu_agi_kernel` | 482 | klog.h, string.h, tasking.h, vbe.h, wubu_agi_kernel.h, wubu_attest.h, wubu_bonzi | wubu_agi_kernel.c -- WuBuOS Bare-Metal AGI Kernel Supervisor (ring-0). Freestanding C11: NO malloc,  |
| `kernel/` | `wubu_ampere` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_ampere.h, wubu_hw_detect.h | wubu_ampere.c -- kernel-owned NVIDIA Ampere routing. NVIDIA Ampere (RTX 30xx) binds nvidia 535/550/5 |
| `kernel/` | `wubu_apic` | 106 | interrupt.h, interrupt_apic.h, klog.h, stdint.h, wubu_apic.h | wubu_apic.c -- local APIC + I/O APIC bring-up (q35-correct delivery). Steps (see wubu_apic.h for the |
| `kernel/` | `wubu_arctic_islands` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_arctic_islands.h, wubu_hw_detect | wubu_arctic_islands.c -- kernel-owned AMD GCN4 Arctic routing. AMD GCN4 Arctic Islands (RX 4xx/5xx)  |
| `kernel/` | `wubu_as` | 92 | wubu_as.h | wubu_as.c  --  per-address-space isolation (gap B7) A new AS = a private PML4 page whose kernel entr |
| `kernel/` | `wubu_atom` | 98 | string.h, wubu_atom.h, wubu_recovery.h | wubu_atom.c -- see wubu_atom.h. Freestanding C11, no heap. The ATOM marker is a small fixed block wh |
| `kernel/` | `wubu_atom_test` | 65 | stdio.h, string.h, wubu_atom.h | wubu_atom_test.c -- gate for the SteamOS ATOM A/B auto-rollback spine. Simulates: a successful stage |
| `kernel/` | `wubu_attest` | 161 | string.h, wubu_attest.h | wubu_attest.c -- WuBuOS kernel-side firmware attestation consumer (ring-0). Freestanding C11: no mal |
| `kernel/` | `wubu_audio` | 332 | egojar.h, stdio.h, string.h, unistd.h, wubu_audio.h, wubu_drv.h, wubu_hw_detect. | wubu_audio.c -- kernel-owned audio driver routing. Linux audio is famously fragmented: the kernel's  |
| `kernel/` | `wubu_audiofw` | 86 | egojar.h, stdio.h, string.h, wubu_audiofw.h, wubu_hw_detect.h | wubu_audiofw.c -- kernel-owned audio firmware routing. Audio firmware loads DSP programs onto audio  |
| `kernel/` | `wubu_auracast` | 58 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_auracast.h, wubu_hw_detect.h | wubu_auracast.c -- kernel-owned Bluetooth Auracast routing. Auracast (LE Audio broadcast) routes aud |
| `kernel/` | `wubu_backlight` | 117 | dirent.h, egojar.h, stdio.h, string.h, wubu_backlight.h, wubu_hw_detect.h | wubu_backlight.c -- kernel-owned display backlight + NIC WoL routing. Two capabilities: - Backlight: |
| `kernel/` | `wubu_backlightpwm` | 95 | egojar.h, stdio.h, string.h, wubu_backlightpwm.h, wubu_hw_detect.h | wubu_backlightpwm.c -- kernel-owned display backlight PWM routing. Backlight PWM controls display br |
| `kernel/` | `wubu_bap` | 58 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_bap.h, wubu_hw_detect.h | wubu_bap.c -- kernel-owned Bluetooth BAP routing. BAP (Basic Audio Profile) routes high-quality audi |
| `kernel/` | `wubu_bcache` | 66 | egojar.h, stdio.h, string.h, wubu_bcache.h, wubu_hw_detect.h | wubu_bcache.c -- kernel-owned storage bcache routing. bcache provides SSD/HDD hybrid caching via /sy |
| `kernel/` | `wubu_bio` | 82 | egojar.h, stdio.h, string.h, wubu_bio.h, wubu_hw_detect.h | wubu_bio.c -- kernel-owned storage bio (block I/O) routing. Bio (block I/O) represents a single I/O  |
| `kernel/` | `wubu_blk` | 542 | stdio.h, string.h, wubu_blk.h | wubu_blk.c -- the block layer & media frontier, COMPLETE (FS-A). C11. The agnostic implementation: d |
| `kernel/` | `wubu_blkqos` | 87 | dirent.h, egojar.h, stdio.h, string.h, wubu_blkqos.h, wubu_hw_detect.h | wubu_blkqos.c -- kernel-owned storage blk-QoS throttling routing. Block IO QoS throttle limits disk  |
| `kernel/` | `wubu_bonzi` | 379 | input.h, klog.h, stdio.h, string.h, tasking.h, vbe.h, wubu_agi_kernel.h, wubu_at | wubu_bonzi.c -- Bonzi Buddy: bare-metal AGI agent persona (ring-0 task). Freestanding C11. Runs as a |
| `kernel/` | `wubu_bonzi_study` | 577 | stdio.h, string.h, wubu_bonzi_study.h, wubu_psych.h | wubu_bonzi_study.c -- the Bonzi Buddy GUI study (HX-D). C11. |
| `kernel/` | `wubu_bt` | 101 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_bt.h, wubu_hw_detect.h | wubu_bt.c -- kernel-owned Bluetooth driver routing + LE Audio. Bluetooth connects controllers, heads |
| `kernel/` | `wubu_bta2dp` | 70 | egojar.h, stdio.h, string.h, wubu_bta2dp.h, wubu_hw_detect.h | wubu_bta2dp.c -- kernel-owned Bluetooth A2DP audio routing. A2DP (Advanced Audio Distribution Profil |
| `kernel/` | `wubu_btamesh` | 58 | egojar.h, stdio.h, string.h, wubu_btamesh.h, wubu_hw_detect.h | wubu_btamesh.c -- kernel-owned Bluetooth mesh routing. BT mesh supports many-to-many device networki |
| `kernel/` | `wubu_btaudio` | 62 | egojar.h, stdio.h, string.h, wubu_btaudio.h, wubu_hw_detect.h | wubu_btaudio.c -- kernel-owned Bluetooth audio profile routing. BT audio profiles (A2DP, HSP/HFP, SC |
| `kernel/` | `wubu_btbeacon` | 61 | egojar.h, stdio.h, string.h, wubu_btbeacon.h, wubu_hw_detect.h | wubu_btbeacon.c -- kernel-owned Bluetooth beacon routing. BT beacons (iBeacon/AltBeacon/Eddystone) b |
| `kernel/` | `wubu_btclassic` | 64 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_btclassic.h, wubu_hw_detect.h | wubu_btclassic.c -- kernel-owned Bluetooth classic routing. BT classic carries audio (SCO/eSCO) and  |
| `kernel/` | `wubu_bthfp` | 62 | egojar.h, stdio.h, string.h, wubu_bthfp.h, wubu_hw_detect.h | wubu_bthfp.c -- kernel-owned Bluetooth HSP/HFP routing. HSP/HFP profiles carry voice audio (micropho |
| `kernel/` | `wubu_bus` | 140 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_bus.h, wubu_hw_detect.h, wubu_pc | wubu_bus.c -- kernel-owned I2C/SPI bus controller driver routing. I2C and SPI are the two ubiquitous |
| `kernel/` | `wubu_cab` | 267 | memory.h, string.h, wubu_cab.h, wubu_lzx.h, wubu_zlib.h | wubu_cab.c -- the IN-KERNEL CAB (MS-CAB) reader. Walks the CFHEADER (36 bytes, flags=0 for Halo's In |
| `kernel/` | `wubu_calib` | 104 | egojar.h, stdio.h, string.h, wubu_calib.h, wubu_hw_detect.h | wubu_calib.c -- kernel-owned display brightness/gamma calibration routing. Calibration adjusts the d |
| `kernel/` | `wubu_camera` | 120 | egojar.h, stdio.h, string.h, wubu_camera.h, wubu_hw_detect.h | wubu_camera.c -- kernel-owned V4L2 camera/ISP driver routing. Cameras feed everything: video calls,  |
| `kernel/` | `wubu_can` | 108 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_can.h, wubu_hw_detect.h, wubu_pc | wubu_can.c -- kernel-owned CAN bus driver routing (SocketCAN). CAN (Controller Area Network) is the  |
| `kernel/` | `wubu_chanmap` | 87 | egojar.h, stdio.h, string.h, wubu_chanmap.h, wubu_hw_detect.h | wubu_chanmap.c -- kernel-owned audio channel map routing. Channel maps assign ALSA channel positions |
| `kernel/` | `wubu_clock` | 134 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_clock.h, wubu_hw_detect.h | wubu_clock.c -- kernel-owned clock/thermal-topology driver routing. This module owns the *routing +  |
| `kernel/` | `wubu_cmb` | 125 | dirent.h, egojar.h, stdio.h, string.h, wubu_cmb.h, wubu_hw_detect.h | wubu_cmb.c -- kernel-owned NVMe Controller Memory Buffer routing. CMB (Controller Memory Buffer) is  |
| `kernel/` | `wubu_codec` | 130 | egojar.h, stdio.h, string.h, wubu_codec.h, wubu_hw_detect.h, wubu_pci.h | wubu_codec.c -- kernel-owned audio codec/DSP driver routing. The codec is the chip that turns digita |
| `kernel/` | `wubu_codecgraph` | 99 | egojar.h, stdio.h, string.h, wubu_codecgraph.h, wubu_hw_detect.h | wubu_codecgraph.c -- kernel-owned audio codec graph routing. The audio codec graph is the ALSA widge |
| `kernel/` | `wubu_colormgmt` | 117 | dirent.h, egojar.h, stdio.h, string.h, wubu_colormgmt.h, wubu_hw_detect.h | wubu_colormgmt.c -- kernel-owned display color management routing. Color management sets CTM/GAMMA/D |
| `kernel/` | `wubu_compress` | 89 | dirent.h, egojar.h, stdio.h, string.h, wubu_compress.h, wubu_hw_detect.h | wubu_compress.c -- kernel-owned storage transparent compression routing. Transparent compression com |
| `kernel/` | `wubu_compressor` | 82 | egojar.h, stdio.h, string.h, wubu_compressor.h, wubu_hw_detect.h | wubu_compressor.c -- kernel-owned audio compressor/limiter routing. Compressor/limiter (ALSA DSP dyn |
| `kernel/` | `wubu_compute` | 102 | egojar.h, stdio.h, string.h, wubu_compute.h, wubu_hw_detect.h | wubu_compute.c -- kernel-owned graphics compute (OpenCL/Vulkan) routing. GPU compute is the AGI's wo |
| `kernel/` | `wubu_computectx` | 93 | egojar.h, stdio.h, string.h, wubu_computectx.h, wubu_hw_detect.h | wubu_computectx.c -- kernel-owned GPU compute context routing. GPU compute contexts (amdgpu KFD queu |
| `kernel/` | `wubu_console` | 267 | ahci.h, fat32.h, klog.h, libc.h, memory.h, stdint.h, string.h, tasking.h, wubu_a | wubu_console.c — Ring-0 debug console + CLI dispatch This is the core console. The two large section |
| `kernel/` | `wubu_console_cmds` | 565 | ahci.h, fat32.h, interrupt.h, klog.h, libc.h, memory.h, stdint.h, string.h, task | wubu_console_cmds.c -- the ring-0 console BUILT-IN COMMANDS. Extracted from wubu_console.c (the comm |
| `kernel/` | `wubu_console_colonel` | 99 | klog.h, stdint.h, wubu_console.h | wubu_console_colonel.c — Live Colonel expression evaluator (ring 0) Extracted from wubu_console.c —  |
| `kernel/` | `wubu_console_recovery` | 191 | ahci.h, fat32.h, klog.h, stdint.h, wubu_console.h, wubu_recovery.h, wubu_serial. | wubu_console_recovery.c — Recovery console (5+1 rollback substrate) Extracted from wubu_console.c —  |
| `kernel/` | `wubu_crash` | 97 | string.h, wubu_crash.h | wubu_crash.c  --  crash dump to the disk + boot pickup (gaps A8/F10) The dump uses the AHCI port-0 s |
| `kernel/` | `wubu_cuda` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_cuda.h, wubu_hw_detect.h | wubu_cuda.c -- kernel-owned CUDA runtime routing. CUDA bind libcuda driver + NVIDIA developer GPU co |
| `kernel/` | `wubu_dapm` | 64 | egojar.h, stdio.h, string.h, wubu_dapm.h, wubu_hw_detect.h | wubu_dapm.c -- kernel-owned audio DAPM routing. DAPM (Dynamic Audio Power Management) routes audio p |
| `kernel/` | `wubu_dapmwidget` | 83 | egojar.h, stdio.h, string.h, wubu_dapmwidget.h, wubu_hw_detect.h | wubu_dapmwidget.c -- kernel-owned audio DAPM widget routing. DAPM (Dynamic Audio Power Management) w |
| `kernel/` | `wubu_dappath` | 87 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_dappath.h, wubu_hw_detect.h | wubu_dappath.c -- kernel-owned audio DAPM path routing. DAPM (Dynamic Audio Power Management) paths  |
| `kernel/` | `wubu_dax` | 102 | dirent.h, egojar.h, stdio.h, string.h, wubu_dax.h, wubu_hw_detect.h | wubu_dax.c -- kernel-owned storage DAX (Direct Access) routing. DAX maps persistent memory directly  |
| `kernel/` | `wubu_ddcci` | 97 | dirent.h, egojar.h, stdio.h, string.h, wubu_ddcci.h, wubu_hw_detect.h | wubu_ddcci.c -- kernel-owned display panel DDC/CI control routing. DDC/CI (Display Data Channel / Co |
| `kernel/` | `wubu_decode` | 90 | egojar.h, stdio.h, string.h, wubu_decode.h, wubu_hw_detect.h | wubu_decode.c -- kernel-owned GPU video decode routing. Video decode (VCN/UVD, NVDEC, Quick Sync) ha |
| `kernel/` | `wubu_dedup` | 113 | egojar.h, stdio.h, string.h, wubu_dedup.h, wubu_hw_detect.h | wubu_dedup.c -- kernel-owned storage deduplication routing. Deduplication removes duplicate blocks/d |
| `kernel/` | `wubu_density_plan` | 250 | stdio.h, string.h, wubu_density_plan.h | wubu_density_plan.c -- WuBuOS AGF Density Planner implementation. absorb/keep/prune metabolism. dens |
| `kernel/` | `wubu_devmapper` | 97 | egojar.h, stdio.h, string.h, wubu_devmapper.h, wubu_hw_detect.h | wubu_devmapper.c -- kernel-owned storage DM (device mapper) routing. DM (device mapper) provides vir |
| `kernel/` | `wubu_display` | 173 | stdio.h, string.h, wubu_display.h, wubu_hw_detect.h, wubu_pci.h | wubu_display.c -- kernel-owned display driver routing (DRM/KMS matrix). Linux has 3 generations of G |
| `kernel/` | `wubu_dmcrypt` | 93 | dirent.h, egojar.h, stdio.h, string.h, wubu_dmcrypt.h, wubu_hw_detect.h | wubu_dmcrypt.c -- kernel-owned storage dm-crypt/LUKS encryption routing. dm-crypt encrypts block dev |
| `kernel/` | `wubu_dram_hedge` | 559 | egojar.h, sched.h, stdio.h, stdlib.h, string.h, time.h, wubu_dram_hedge.h, wubu_ | src/kernel/wubu_dram_hedge.c WuBuOS DRAM-refresh tail-latency hedge — kernel-level Tailslayer port.  |
| `kernel/` | `wubu_drm` | 103 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_drm.h, wubu_hw_detect.h | wubu_drm.c -- kernel-owned GPU DRM routing. DRM (Direct Rendering Manager) manages GPU display + buf |
| `kernel/` | `wubu_drmx` | 111 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_drmx.h, wubu_hw_detect.h | wubu_drmx.c -- kernel-owned DRM writeback/overlay + HDR/color routing. Advanced display features bey |
| `kernel/` | `wubu_drv` | 212 | stdio.h, string.h, wubu_drv.h, wubu_pci.h | wubu_drv.c -- the DRIVER REGISTRY (the device model for the Steam Deck + laptop hardware). The Linux |
| `kernel/` | `wubu_drv_ahci` | 34 | stdio.h, wubu_drv.h, wubu_drv_ahci.h | wubu_drv_ahci.c -- the AHCI (SATA) driver shim for the registry. The registry references wubu_drv_ah |
| `kernel/` | `wubu_drv_arm` | 90 | stdio.h, string.h, wubu_drv.h, wubu_drv_arm.h | wubu_drv_arm.c -- the ARM platform drivers (aarch64 machines). "We run on everything" includes the A |
| `kernel/` | `wubu_drv_battery` | 77 | stdio.h, string.h, wubu_drv.h, wubu_drv_battery.h | wubu_drv_battery.c -- the ACPI BATTERY driver (the Deck's battery + every laptop's battery). The ACP |
| `kernel/` | `wubu_drv_gpu` | 184 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_drv.h, wubu_drv_gpu.h | wubu_drv_gpu.c -- the DISPLAY/GPU driver (the Deck's Van Gogh RDNA2 iGPU + every laptop's iGPU). The |
| `kernel/` | `wubu_drv_hda` | 75 | stdio.h, string.h, wubu_drv.h, wubu_drv_hda.h | wubu_drv_hda.c -- the HD AUDIO driver (the Deck's ALC269VC codec + every laptop's codec). The HD Aud |
| `kernel/` | `wubu_drv_install` | 664 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_drv_install.h | wubu_drv_install.c -- the DRIVER SELF-INSTALLATION arm. The AGI-OS missing arm: a device with no reg |
| `kernel/` | `wubu_drv_intel` | 54 | stdio.h, wubu_drv.h, wubu_drv_intel.h | wubu_drv_intel.c -- the INTEL laptop drivers (the "other machines" that aren't the Deck: ThinkPads,  |
| `kernel/` | `wubu_drv_net` | 121 | stdio.h, string.h, wubu_drv.h, wubu_drv_net.h | wubu_drv_net.c -- the NETWORK drivers (the Deck's RZ616 Wi-Fi 6E + every laptop's NIC). Two drivers: |
| `kernel/` | `wubu_drv_nvme` | 144 | stdio.h, string.h, wubu_drv.h, wubu_drv_nvme.h | wubu_drv_nvme.c -- the NVMe driver (the Steam Deck's SSD + every modern laptop). The Deck ships a 22 |
| `kernel/` | `wubu_drv_sd` | 92 | stdio.h, string.h, wubu_drv.h, wubu_drv_sd.h | wubu_drv_sd.c -- the SD/MMC driver (the Steam Deck's SD reader + every laptop's SD slot). The Deck h |
| `kernel/` | `wubu_drv_thermal` | 98 | stdio.h, string.h, wubu_drv.h, wubu_drv_thermal.h | wubu_drv_thermal.c -- the THERMAL driver (the Deck's cooling + every laptop's cooling). The thermal  |
| `kernel/` | `wubu_drv_usb` | 80 | stdio.h, string.h, wubu_drv.h, wubu_drv_usb.h | wubu_drv_usb.c -- the USB CLASS drivers (the Deck's USB-C + the laptops' USB ports). The xHCI stack  |
| `kernel/` | `wubu_drv_virtio` | 145 | stdio.h, string.h, wubu_drv.h, wubu_drv_virtio.h | wubu_drv_virtio.c -- the VIRTIO drivers (QEMU/KVM machines). "We are an AGI that runs on everything" |
| `kernel/` | `wubu_dspgraph` | 91 | egojar.h, stdio.h, string.h, wubu_dspgraph.h, wubu_hw_detect.h | wubu_dspgraph.c -- kernel-owned audio DSP graph routing. The DSP graph is the ALSA snd_soc_dapm rout |
| `kernel/` | `wubu_dspmode` | 103 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_dspmode.h, wubu_hw_detect.h | wubu_dspmode.c -- kernel-owned audio codec DSP modes + suspend routing. Codec DSPs (SOF - Sound Open |
| `kernel/` | `wubu_dsptrace` | 63 | egojar.h, stdio.h, string.h, wubu_dsptrace.h, wubu_hw_detect.h | wubu_dsptrace.c -- kernel-owned audio DSP trace/debug routing. DSP firmware exposes trace buffers an |
| `kernel/` | `wubu_ducking` | 90 | egojar.h, stdio.h, string.h, wubu_ducking.h, wubu_hw_detect.h | wubu_ducking.c -- kernel-owned audio ducking + compressor routing. Ducking lowers volume of one stre |
| `kernel/` | `wubu_edid` | 133 | stddef.h, string.h, wubu_edid.h | wubu_edid.c -- see wubu_edid.h.  Pure base-block parsing, no I/O. Layout reference (EDID 1.x, 128 by |
| `kernel/` | `wubu_edid_dump` | 106 | stdio.h, string.h, wubu_edid_scan.h, wubu_edid_xrandr.h | wubu_edid_dump.c -- print what each connector reports about its monitor. WHY THIS EXISTS wubu_edid_s |
| `kernel/` | `wubu_edid_kms` | 132 | dirent.h, fcntl.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_edid_kms.h, xf86d | wubu_edid_kms.c -- see wubu_edid_kms.h.  Enumeration only; never DRM master. |
| `kernel/` | `wubu_edid_kms_test` | 60 | stdio.h, string.h, wubu_edid_kms.h | wubu_edid_kms_test.c -- gate for direct KMS enumeration. Runs against the REAL DRM device when one i |
| `kernel/` | `wubu_edid_scan` | 102 | dirent.h, stdio.h, stdlib.h, string.h, wubu_edid_scan.h | wubu_edid_scan.c -- see wubu_edid_scan.h.  Directory scan + per-connector parse. |
| `kernel/` | `wubu_edid_scan_test` | 156 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_edid_scan.h | wubu_edid_scan_test.c -- gate for connector EDID discovery. Uses a FIXTURE directory (WUBU_DRM_SYSFS |
| `kernel/` | `wubu_edid_test` | 96 | stdio.h, string.h, wubu_edid.h | wubu_edid_test.c -- gate for EDID parsing. The golden vectors here are a REAL 128-byte EDID captured |
| `kernel/` | `wubu_edid_xrandr` | 177 | stdlib.h, string.h, wubu_edid_xrandr.h | wubu_edid_xrandr.c -- see wubu_edid_xrandr.h. Parssing rules taken from the REAL output format rathe |
| `kernel/` | `wubu_edid_xrandr_test` | 74 | stdio.h, string.h, wubu_edid_xrandr.h | wubu_edid_xrandr_test.c -- gate for xrandr-based display discovery. The fixture is REAL `xrandr -q - |
| `kernel/` | `wubu_elf` | 104 | string.h, wubu_elf.h | wubu_elf.c -- the IN-KERNEL ELF loader (the Linux format, on OUR kernel — not a host container exec) |
| `kernel/` | `wubu_encode` | 91 | egojar.h, stdio.h, string.h, wubu_encode.h, wubu_hw_detect.h | wubu_encode.c -- kernel-owned GPU video encode routing. Video encode (VCN/UVD, H.264/H.265) handles  |
| `kernel/` | `wubu_eq` | 106 | egojar.h, stdio.h, string.h, wubu_eq.h, wubu_hw_detect.h | wubu_eq.c -- kernel-owned audio equalizer DSP coefficients routing. Audio EQ (equalization) shapes t |
| `kernel/` | `wubu_evdev` | 303 | egojar.h, input.h, stdio.h, string.h, wubu_evdev.h, wubu_steaminput.h | wubu_evdev.c -- the evdev gamepad pipeline. Steam Input (src/runtime/wubu_steaminput.c) maps a gamep |
| `kernel/` | `wubu_fantml` | 63 | egojar.h, stdio.h, string.h, wubu_fantml.h, wubu_hw_detect.h | wubu_fantml.c -- kernel-owned GPU fan + thermal routing. Fan/thermal monitors GPU temperature + fan  |
| `kernel/` | `wubu_fat2` | 500 | stdio.h, string.h, wubu_fat2.h | wubu_fat2.c -- the FAT family frontier, COMPLETE (FS-B). C11. |
| `kernel/` | `wubu_fbcon` | 86 | egojar.h, stdio.h, string.h, wubu_fbcon.h, wubu_hw_detect.h | wubu_fbcon.c -- kernel-owned GPU framebuffer console routing. fbcon (framebuffer console) provides t |
| `kernel/` | `wubu_fc` | 98 | egojar.h, stdio.h, string.h, wubu_fc.h, wubu_hw_detect.h | wubu_fc.c -- kernel-owned ethernet flow control (pause) routing. Flow control uses pause frames to s |
| `kernel/` | `wubu_fence` | 90 | egojar.h, stdio.h, string.h, wubu_fence.h, wubu_hw_detect.h | wubu_fence.c -- kernel-owned GPU fence timeout routing. GPU fences signal completion; the timeout de |
| `kernel/` | `wubu_fencesync` | 90 | egojar.h, stdio.h, string.h, wubu_fencesync.h, wubu_hw_detect.h | wubu_fencesync.c -- kernel-owned GPU sync/fence routing. Sync fences (DMA fence, timeline) coordinat |
| `kernel/` | `wubu_filter` | 170 | egojar.h, stdio.h, string.h, wubu_filter.h, wubu_hw_detect.h | wubu_filter.c -- kernel-owned audio DSP filter + EQ routing. Audio DSP filters implement biquad EQ c |
| `kernel/` | `wubu_fingerprint` | 109 | egojar.h, stdio.h, string.h, wubu_fingerprint.h, wubu_hw_detect.h | wubu_fingerprint.c -- kernel-owned fingerprint/biometric driver routing. Fingerprint readers secure  |
| `kernel/` | `wubu_flush` | 115 | dirent.h, egojar.h, stdio.h, string.h, wubu_flush.h, wubu_hw_detect.h | wubu_flush.c -- kernel-owned storage cache flush/barrier routing. Cache flush (write barriers) ensur |
| `kernel/` | `wubu_flush2` | 82 | egojar.h, stdio.h, string.h, wubu_flush2.h, wubu_hw_detect.h | wubu_flush2.c -- kernel-owned storage flush barrier routing. Storage flush (barrier, fsync, write ba |
| `kernel/` | `wubu_fpga` | 95 | egojar.h, stdio.h, string.h, wubu_fpga.h, wubu_hw_detect.h | wubu_fpga.c -- kernel-owned FPGA driver routing (FPGA manager). FPGAs (field-programmable gate array |
| `kernel/` | `wubu_fusefs` | 104 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_fusefs.h, wubu_hw_detect.h | wubu_fusefs.c -- kernel-owned storage FUSE filesystem routing. FUSE (Filesystem in Userspace) runs f |
| `kernel/` | `wubu_fw` | 113 | egojar.h, stdio.h, string.h, wubu_fw.h, wubu_hw_detect.h | wubu_fw.c -- kernel-owned storage controller firmware routing. Storage controller firmware (RAID car |
| `kernel/` | `wubu_gaad` | 595 | math.h, stdlib.h, string.h, wubu_gaad.h, wubu_math.h | wubu_gaad.c  --  WuBuOS Golden Aspect Adaptive Decomposition Cell 393: GAAD  --  the universal resol |
| `kernel/` | `wubu_gadget` | 119 | egojar.h, stdio.h, string.h, wubu_gadget.h, wubu_hw_detect.h | wubu_gadget.c -- kernel-owned USB gadget mode + NVMe endurance routing. Two capabilities: - USB gadg |
| `kernel/` | `wubu_gamepad` | 129 | egojar.h, stdio.h, string.h, wubu_gamepad.h, wubu_hw_detect.h | wubu_gamepad.c -- kernel-owned game controller + display DSC routing. Two capabilities: - Game contr |
| `kernel/` | `wubu_gamepadbm` | 55 | egojar.h, stdio.h, string.h, wubu_gamepadbm.h, wubu_hw_detect.h | wubu_gamepadbm.c -- kernel-owned gamepad button map routing. Gamepad button maps define physical-to- |
| `kernel/` | `wubu_gamepaddz` | 56 | egojar.h, stdio.h, string.h, wubu_gamepaddz.h, wubu_hw_detect.h | wubu_gamepaddz.c -- kernel-owned gamepad deadzone routing. Gamepad deadzone filters stick drift by i |
| `kernel/` | `wubu_gesture` | 444 | dirent.h, egojar.h, input.h, math.h, stdio.h, string.h, time.h, wubu_gesture.h | wubu_gesture.c -- multitouch gesture recognition. See wubu_gesture.h for the rules.  The engine is p |
| `kernel/` | `wubu_gpu_icd` | 204 | egojar.h, stdlib.h, string.h, wubu_hw_detect.h | wubu_gpu_icd.c -- Vulkan ICD selection (extracted from wubu_hw_detect.c). WSL2 reaches the real GPU  |
| `kernel/` | `wubu_gpuband` | 88 | egojar.h, stdio.h, string.h, wubu_gpuband.h, wubu_hw_detect.h | wubu_gpuband.c -- kernel-owned GPU scheduler priority bands routing. The GPU scheduler uses priority |
| `kernel/` | `wubu_gpucsched` | 57 | egojar.h, stdio.h, string.h, wubu_gpucsched.h, wubu_hw_detect.h | wubu_gpucsched.c -- kernel-owned GPU compute scheduler routing. Compute queues feed async work to GP |
| `kernel/` | `wubu_gpudc` | 60 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_gpudc.h, wubu_hw_detect.h | wubu_gpudc.c -- kernel-owned GPU display controller routing. GPU display controller (DC) binds outpu |
| `kernel/` | `wubu_gpufw` | 62 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_gpufw.h, wubu_hw_detect.h | wubu_gpufw.c -- kernel-owned GPU firmware version routing. GPU firmware (VBIOS/ microcode) is inspec |
| `kernel/` | `wubu_gpufwupd` | 63 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_gpufwupd.h, wubu_hw_detect.h | wubu_gpufwupd.c -- kernel-owned GPU firmware update routing. GPU firmware (GDDR VBIOS + microcode) m |
| `kernel/` | `wubu_gpukms` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_gpukms.h, wubu_hw_detect.h | wubu_gpukms.c -- kernel-owned GPU KMS modeset routing. KMS (Kernel Mode Setting) routes display mode |
| `kernel/` | `wubu_gpumem` | 72 | egojar.h, stdio.h, string.h, wubu_gpumem.h, wubu_hw_detect.h | wubu_gpumem.c -- kernel-owned GPU memory bandwidth routing. GPU memory bandwidth (GB/s) is measured  |
| `kernel/` | `wubu_gpurst` | 95 | egojar.h, stdio.h, string.h, wubu_gpurst.h, wubu_hw_detect.h | wubu_gpurst.c -- kernel-owned GPU reset + recovery routing. GPU reset recovers from a hung GPU by re |
| `kernel/` | `wubu_gpusched` | 100 | egojar.h, stdio.h, string.h, wubu_gpusched.h, wubu_hw_detect.h | wubu_gpusched.c -- kernel-owned GPU compute scheduler routing. The GPU scheduler manages command sub |
| `kernel/` | `wubu_gpusensor` | 103 | egojar.h, stdio.h, string.h, wubu_gpusensor.h, wubu_hw_detect.h | wubu_gpusensor.c -- kernel-owned GPU sensor + fan curve routing. GPU sensors monitor temperature + f |
| `kernel/` | `wubu_gpushader` | 63 | egojar.h, stdio.h, string.h, wubu_gpushader.h, wubu_hw_detect.h | wubu_gpushader.c -- kernel-owned GPU shader model routing. Shader model detection routes GPU shader  |
| `kernel/` | `wubu_gt2xx` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_gt2xx.h, wubu_hw_detect.h | wubu_gt2xx.c -- kernel-owned NVIDIA GT2xx legacy routing. NVIDIA GT2xx (G8x/G9x) is legacy. Debian W |
| `kernel/` | `wubu_hdr` | 118 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hdr.h, wubu_hw_detect.h | wubu_hdr.c -- kernel-owned display HDR + audio jack detection routing. Two capabilities: - HDR (high |
| `kernel/` | `wubu_hid` | 146 | wubu_hid.h | wubu_hid.c  --  WuBuOS Unified HID Layer (GameInput-style) A single ring of unified events, common t |
| `kernel/` | `wubu_hidadv` | 103 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hidadv.h, wubu_hw_detect.h | wubu_hidadv.c -- kernel-owned USB HID advanced driver routing. HID (human interface device) is how k |
| `kernel/` | `wubu_hive` | 211 | string.h, wubu_hive.h | wubu_hive.c -- C11 "luddite hive" (see wubu_hive.h for the design). Linked fixed-capacity blocks + b |
| `kernel/` | `wubu_hpet` | 46 | wubu_hpet.h | wubu_hpet.c  --  High Precision Event Timer (gap A19) The ACPI HPET table (signature "HPET") carries |
| `kernel/` | `wubu_hw_detect` | 750 | egojar.h, egojar_net.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_accel.h, wub | wubu_hw_detect.c -- the HARDWARE BUS DETECTOR (the OS's magic sense of self). "It should work on bar |
| `kernel/` | `wubu_ieccontrol` | 82 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ieccontrol.h | wubu_ieccontrol.c -- kernel-owned audio IEC control routing. IEC control (IEC 60958) manages S/PDIF  |
| `kernel/` | `wubu_ima` | 103 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ima.h | wubu_ima.c -- kernel-owned IMA/EVM measured boot routing. IMA (Integrity Measurement Architecture) + |
| `kernel/` | `wubu_inflate` | 348 | libc.h, string.h, wubu_zlib.h | wubu_inflate.c -- the IN-KERNEL DEFLATE (RFC 1951) + zlib inflater. Ported, byte-for-byte in algorit |
| `kernel/` | `wubu_input` | 167 | stdio.h, string.h, wubu_hw_detect.h, wubu_input.h | wubu_input.c -- kernel-owned input device driver routing. Linux's in-kernel xpad driver only covers  |
| `kernel/` | `wubu_instinct` | 58 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_instinct.h | wubu_instinct.c -- kernel-owned AMD Instinct MI routing. AMD Instinct MI (data center GPU) binds the |
| `kernel/` | `wubu_intel_gma` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_intel_gma.h | wubu_intel_gma.c -- kernel-owned Intel GMA legacy GPU routing. Intel GMA (Graphics Media Accelerator |
| `kernel/` | `wubu_intel_icelake` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_intel_icelake. | wubu_intel_icelake.c -- kernel-owned Intel Gen11 Ice Lake routing. Intel Gen11 Ice Lake binds i915 k |
| `kernel/` | `wubu_intel_skylake` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_intel_skylake. | wubu_intel_skylake.c -- kernel-owned Intel Gen9 Skylake routing. Intel Gen9 Skylake binds i915 kerne |
| `kernel/` | `wubu_intelgpu` | 60 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_intelgpu.h | wubu_intelgpu.c -- kernel-owned Intel GPU routing. Intel integrated graphics (i915/iris/xe) bind acr |
| `kernel/` | `wubu_iommu` | 89 | wubu_iommu.h | wubu_iommu.c  --  IOMMU/VT-d discovery (gap E5) The DMAR ACPI table: header (36 bytes), then a seque |
| `kernel/` | `wubu_ioprio` | 85 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ioprio.h | wubu_ioprio.c -- kernel-owned storage I/O priority routing. I/O priority (ioprio) assigns class + le |
| `kernel/` | `wubu_iosched` | 100 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_iosched.h | wubu_iosched.c -- kernel-owned storage I/O scheduler routing. The I/O scheduler (elevator) orders bl |
| `kernel/` | `wubu_jack` | 79 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_jack.h | wubu_jack.c -- kernel-owned audio jack detection routing. Audio jack detects plug state (headphones, |
| `kernel/` | `wubu_jackdetect` | 85 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_jackdetect.h | wubu_jackdetect.c -- kernel-owned audio jack detection routing. Jack detection senses headset/mic in |
| `kernel/` | `wubu_jackimpedance` | 85 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_jackimpedance.h | wubu_jackimpedance.c -- kernel-owned audio jack impedance routing. Jack impedance sensing detects he |
| `kernel/` | `wubu_jackstate` | 84 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_jackstate.h | wubu_jackstate.c -- kernel-owned audio jack state machine routing. Jack state machine tracks inserti |
| `kernel/` | `wubu_kvfs` | 467 | stdio.h, stdlib.h, string.h, wubu_kvfs.h | wubu_kvfs.c — KV namespace layer (G1: path-addressable KV cache). Metal port of wubuwizard/src/wubu_ |
| `kernel/` | `wubu_leaudio` | 58 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_leaudio.h | wubu_leaudio.c -- kernel-owned Bluetooth LE Audio routing. LE Audio (LC3 codec, broadcast audio) ove |
| `kernel/` | `wubu_leaudioldr` | 61 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_leaudioldr.h | wubu_leaudioldr.c -- kernel-owned Bluetooth LE Audio routing. LE Audio Low Complexity Communication  |
| `kernel/` | `wubu_lfn` | 186 | string.h, wubu_lfn.h | wubu_lfn.c  --  FAT32 VFAT Long File Name codec (self-contained) Gap A16. VFAT LFN layout per entry  |
| `kernel/` | `wubu_loudness` | 100 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_loudness.h | wubu_loudness.c -- kernel-owned audio loudness normalization routing. Loudness normalization evens o |
| `kernel/` | `wubu_lvm` | 71 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_lvm.h | wubu_lvm.c -- kernel-owned storage LVM routing. LVM (Logical Volume Manager) provides volume groups  |
| `kernel/` | `wubu_lzx` | 555 | libc.h, memory.h, string.h, wubu_lzx.h | wubu_lzx.c -- the IN-KERNEL LZX decompressor. Faithful port of the LZX algorithm from the ReactOS ca |
| `kernel/` | `wubu_macho` | 111 | string.h, wubu_macho.h | wubu_macho.c -- the IN-KERNEL Mach-O loader (the macOS format, on OUR kernel — not host Darling). Th |
| `kernel/` | `wubu_mali_g52` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mali_g52.h | wubu_mali_g52.c -- kernel-owned ARM Mali G52 routing. Mali-G52 binds the Panfrost open-source driver |
| `kernel/` | `wubu_mali_g720` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mali_g720.h | wubu_mali_g720.c -- kernel-owned ARM Mali G720 routing. Mali-G720 (5th gen / v12) binds the Panthor  |
| `kernel/` | `wubu_mali_g77` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mali_g77.h | wubu_mali_g77.c -- kernel-owned ARM Mali G77 routing. Mali-G77 (valhall) binds the Panfrost open-sou |
| `kernel/` | `wubu_math` | 581 | math.h, stddef.h, stdint.h, stdio.h | wubu_math.c  --  WuBuOS Pure C Math Library Cell 420: Pure C implementations replacing libm. IEEE 75 |
| `kernel/` | `wubu_mdraid` | 64 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mdraid.h | wubu_mdraid.c -- kernel-owned storage MD RAID routing. MD (Multiple Device) RAID provides software R |
| `kernel/` | `wubu_mem` | 94 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mem.h | wubu_mem.c -- kernel-owned memory/ECC driver routing. Memory health is critical for "runs on everyth |
| `kernel/` | `wubu_memmap` | 37 | wubu_memmap.h | wubu_memmap.c  --  WuBuOS Memory Map (E820, gap I1) Parses the E820 table collected by boot.S. Type  |
| `kernel/` | `wubu_memmgr` | 89 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_memmgr.h | wubu_memmgr.c -- kernel-owned GPU memory manager routing. The GPU memory manager (GEM/TTM) allocates |
| `kernel/` | `wubu_mixergraph` | 84 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mixergraph.h | wubu_mixergraph.c -- kernel-owned audio mixer graph routing. Mixer graph connects audio paths (contr |
| `kernel/` | `wubu_mixgraph` | 102 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mixgraph.h | wubu_mixgraph.c -- kernel-owned audio mixing graph routing. The audio mixing graph is how apps, stre |
| `kernel/` | `wubu_mmu` | 92 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mmu.h | wubu_mmu.c -- kernel-owned GPU MMU page-table routing. GPU MMU (Memory Management Unit) handles page |
| `kernel/` | `wubu_modsym` | 91 | string.h, wubu_modsym.h | wubu_modsym.c -- runtime-extensible symbol registry for the PE loader. Kernel-clean: no libc beyond  |
| `kernel/` | `wubu_mst` | 125 | dirent.h, egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_mst. | wubu_mst.c -- kernel-owned DisplayPort MST + audio SRC routing. Two capabilities: - DisplayPort MST  |
| `kernel/` | `wubu_multigig` | 103 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_multigig.h | wubu_multigig.c -- kernel-owned Ethernet multi-gig (2.5/5/10G) PHY routing. Beyond gigabit: 2.5GBase |
| `kernel/` | `wubu_n9` | 249 | interrupt_io.h, klog.h, memory.h, stddef.h, string.h, wubu_9pc.h, wubu_modsym.h, | wubu_n9.c  --  A3: booted-image 9P proof (docs/GAP_ROADMAP.md, GAP A3). Kernel side of the A3 harnes |
| `kernel/` | `wubu_navi10` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_navi10.h | wubu_navi10.c -- kernel-owned AMD Navi10 RDNA1 routing. AMD Navi10 (RX 5700/XT, RDNA1) binds the amd |
| `kernel/` | `wubu_net` | 258 | stdio.h, string.h, wubu_hw_detect.h, wubu_net.h, wubu_pci.h | wubu_net.c -- kernel-owned network driver routing + power-save tuning. Every Wi-Fi vendor (Intel iwl |
| `kernel/` | `wubu_nfsclient` | 94 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nfsclient.h | wubu_nfsclient.c -- kernel-owned storage NFS client routing. NFS client mounts remote filesystems vi |
| `kernel/` | `wubu_nfsmount` | 98 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nfsmount.h | "wubu_nfsmount.c -- kernel-owned storage NFS mount routing. NFS mount routes remote filesystems. "Ru |
| `kernel/` | `wubu_nicoffload` | 121 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nicoffload.h | wubu_nicoffload.c -- kernel-owned NIC offload + multi-queue routing. Modern NICs offload packet proc |
| `kernel/` | `wubu_ns` | 119 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ns.h | wubu_ns.c -- kernel-owned NVMe namespace/multipath routing. NVMe namespaces are logical volumes with |
| `kernel/` | `wubu_nvidia_fermi` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_fermi.h | wubu_nvidia_fermi.c -- kernel-owned NVIDIA Fermi legacy routing. NVIDIA Fermi (GTX 4xx/5xx) binds th |
| `kernel/` | `wubu_nvidia_kepler` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_kepler. | wubu_nvidia_kepler.c -- kernel-owned NVIDIA Kepler legacy routing. NVIDIA Kepler (GTX 6xx/7xx) binds |
| `kernel/` | `wubu_nvidia_maxwell` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_maxwell | wubu_nvidia_maxwell.c -- kernel-owned NVIDIA Maxwell routing. NVIDIA Maxwell (GTX 9xx) binds the nvi |
| `kernel/` | `wubu_nvidia_pascal` | 59 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_pascal. | wubu_nvidia_pascal.c -- kernel-owned NVIDIA Pascal routing. NVIDIA Pascal (GTX 10xx) binds nvidia 53 |
| `kernel/` | `wubu_nvidia_turing` | 55 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_turing. | wubu_nvidia_turing.c -- kernel-owned NVIDIA Turing routing. NVIDIA Turing (RTX 20xx) binds nvidia 53 |
| `kernel/` | `wubu_nvidia_volta` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvidia_volta.h | wubu_nvidia_volta.c -- kernel-owned NVIDIA Volta routing. NVIDIA Volta (V100) binds nvidia datacente |
| `kernel/` | `wubu_nvme_gen4` | 55 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvme_gen4.h | wubu_nvme_gen4.c -- kernel-owned NVMe Gen4 routing. NVMe Gen4 supports up to 16 GT/s per lane (64 GT |
| `kernel/` | `wubu_nvme_gen5` | 62 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvme_gen5.h | wubu_nvme_gen5.c -- kernel-owned NVMe Gen5 routing. NVMe Gen5 supports up to 14 GT/s per lane (64 GT |
| `kernel/` | `wubu_nvmehotplug` | 57 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvmehotplug.h | wubu_nvmehotplug.c -- kernel-owned NVMe hotplug routing. NVMe hotplug detects device insertion/remov |
| `kernel/` | `wubu_nvmepower` | 58 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_nvmepower.h | wubu_nvmepower.c -- kernel-owned NVMe power state routing. NVMe drives expose power states (PS0-PS4) |
| `kernel/` | `wubu_opencl` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_opencl.h | wubu_opencl.c -- kernel-owned OpenCL runtime routing. OpenCL bind clinfo runtime detection. AMD GPUs |
| `kernel/` | `wubu_overclock` | 94 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_overclock.h | wubu_overclock.c -- kernel-owned GPU overclocking + clocks routing. GPU overclocking raises core/mem |
| `kernel/` | `wubu_panel` | 85 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_panel.h | wubu_panel.c -- kernel-owned GPU panel routing. GPU panel detects display panel / connector types. " |
| `kernel/` | `wubu_pci` | 253 | dirent.h, errno.h, libc.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_pci.h | wubu_pci.c -- minimal PCI config-space access (0xCF8/0xCFC). The metal kernel previously had no PCI  |
| `kernel/` | `wubu_pcipme` | 99 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pcipme.h | wubu_pcipme.c -- kernel-owned PCI PME (Power Management Event) routing. PCI PME generates a PME inte |
| `kernel/` | `wubu_pcmlink` | 64 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pcmlink.h | wubu_pcmlink.c -- kernel-owned audio PCM link routing. PCM link routes audio streams between ALSA PC |
| `kernel/` | `wubu_pcmplugin` | 84 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pcmplugin.h | wubu_pcmplugin.c -- kernel-owned audio PCM plugin routing. PCM plugins (ALSA plugin chain) transform |
| `kernel/` | `wubu_pcmring` | 68 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pcmring.h | wubu_pcmring.c -- kernel-owned audio PCM ring buffer routing. PCM ring buffers queue audio samples b |
| `kernel/` | `wubu_pd` | 115 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pd.h | wubu_pd.c -- kernel-owned USB Power Delivery + NIC flow steering routing. Two capabilities: - USB PD |
| `kernel/` | `wubu_pdpolicy` | 105 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pdpolicy.h | wubu_pdpolicy.c -- kernel-owned USB Power Delivery policy routing. USB PD policy negotiates power co |
| `kernel/` | `wubu_pe` | 359 | string.h, wubu_pe.h | wubu_pe.c -- the IN-KERNEL PE loader (the Windows format, on OUR kernel — not an Arch/Wine connectio |
| `kernel/` | `wubu_pe_personality` | 106 | string.h, wubu_pe_personality.h | wubu_pe_personality.c -- the in-kernel Win32 personality answer tables (the DLLs the PE loader names |
| `kernel/` | `wubu_perf` | 69 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_perf.h | wubu_perf.c -- kernel-owned GPU performance counter routing. GPU perf counters measure shader/active |
| `kernel/` | `wubu_perfmon` | 90 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_perfmon.h | wubu_perfmon.c -- kernel-owned GPU performance counters routing. Perf counters (AMD GPUPerfAPI, Inte |
| `kernel/` | `wubu_peripheral` | 122 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_peripheral.h | wubu_peripheral.c -- kernel-owned peripheral driver routing. The "long tail" of hardware that makes  |
| `kernel/` | `wubu_phy` | 111 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_phy.h | wubu_phy.c -- kernel-owned Ethernet PHY/MDIO driver routing. The PHY (physical layer transceiver) co |
| `kernel/` | `wubu_pm` | 112 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pm.h | wubu_pm.c -- kernel-owned power mode (S0ix/deep sleep/runtime PM) routing. Power modes are how the m |
| `kernel/` | `wubu_pmicaudio` | 124 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pmicaudio.h | wubu_pmicaudio.c -- kernel-owned PMIC + audio amp/DAC driver routing. Three connected analog-front-e |
| `kernel/` | `wubu_porttiming` | 95 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_porttiming.h | wubu_porttiming.c -- kernel-owned display port timing routing. Port timing sets the pixel clock / HT |
| `kernel/` | `wubu_power` | 162 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_power.h | wubu_power.c -- kernel-owned CPU/power/thermal driver routing + tuning. Power is the "runs on everyt |
| `kernel/` | `wubu_powergate` | 95 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_powergate.h | wubu_powergate.c -- kernel-owned GPU power gating routing. Power gating cuts power to GPU blocks (sh |
| `kernel/` | `wubu_powervr` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_powervr.h | wubu_powervr.c -- kernel-owned Imagination PowerVR routing. PowerVR ROGUE GPUs bind the pvrsrvkm dri |
| `kernel/` | `wubu_probe` | 405 | stdio.h, string.h, wubu_accel.h, wubu_adreno600.h, wubu_adreno700.h, wubu_aec.h, | wubu_probe.c -- the UNIFIED HARDWARE DISCOVERY dispatcher. WuBuOS doctrine: "we run everything and r |
| `kernel/` | `wubu_probe_matrix` | 928 | stdio.h, string.h, wubu_probe.h, wubu_probe_matrix.h | ---- the matrix buffer (definition; decl is in wubu_probe_matrix.h) ---- |
| `kernel/` | `wubu_psr` | 98 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_psr.h | wubu_psr.c -- kernel-owned display PSR + NIC SR-IOV routing. Two power/virtualization capabilities:  |
| `kernel/` | `wubu_psych` | 374 | stdio.h, string.h, wubu_psych.h | wubu_psych.c -- the human interaction psychology + timing loops (HX). C11. |
| `kernel/` | `wubu_ptp` | 120 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ptp.h | wubu_ptp.c -- kernel-owned Ethernet PTP/TSN + input haptics routing. Two capabilities: - PTP/TSN: pr |
| `kernel/` | `wubu_ptp_sync` | 117 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ptp_sync.h | wubu_ptp_sync.c -- kernel-owned NIC PTP time sync routing. PTP (IEEE 1588) synchronizes clocks preci |
| `kernel/` | `wubu_qos` | 98 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_qos.h | wubu_qos.c -- kernel-owned Ethernet switch QoS/ACL (tc offload) routing. Modern NICs + switch ASICs  |
| `kernel/` | `wubu_quadro` | 58 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_quadro.h | wubu_quadro.c -- kernel-owned NVIDIA Quadro professional routing. NVIDIA Quadro (professional workst |
| `kernel/` | `wubu_radeon_5000` | 55 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_radeon_5000.h | wubu_radeon_5000.c -- kernel-owned AMD Radeon HD 5000 (Evergreen) routing. Radeon HD 5000 (Evergreen |
| `kernel/` | `wubu_radeon_6000` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_radeon_6000.h | wubu_radeon_6000.c -- kernel-owned AMD Radeon HD 6000 (NI) routing. Radeon HD 6000 (Northern Islands |
| `kernel/` | `wubu_radeon_legacy` | 60 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_radeon_legacy. | wubu_radeon_legacy.c -- kernel-owned AMD Radeon legacy GPU routing. AMD Radeon HD 5000/6000 (Evergre |
| `kernel/` | `wubu_raid` | 144 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_raid.h | wubu_raid.c -- kernel-owned RAID/SAS storage driver routing. Servers and high-end workstations use h |
| `kernel/` | `wubu_raid5` | 88 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_raid5.h | wubu_raid5.c -- kernel-owned storage RAID5 routing. RAID5 stripes data + parity across disks. "Runs  |
| `kernel/` | `wubu_raidcache` | 97 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_raidcache.h | wubu_raidcache.c -- kernel-owned storage RAID cache (dm-cache) routing. SSD caching of spinning disk |
| `kernel/` | `wubu_rdma` | 121 | dirent.h, egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_rdma | wubu_rdma.c -- kernel-owned RDMA/InfiniBand driver routing. RDMA (remote direct memory access) bypas |
| `kernel/` | `wubu_recovery` | 132 | string.h, wubu_recovery.h | wubu_recovery.c -- the AGI recovery/rollback substrate (freestanding C11). The 5+1 doctrine: five ro |
| `kernel/` | `wubu_rendernode` | 58 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_rendernode.h | wubu_rendernode.c -- kernel-owned GPU render node routing. GPU render node exposes the GPU for compu |
| `kernel/` | `wubu_renoir` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_renoir.h | wubu_renoir.c -- kernel-owned AMD Raven/Renoir APU routing. AMD Raven/Renoir APU (GCN 5.1) binds amd |
| `kernel/` | `wubu_rtc` | 122 | wubu_rtc.h | wubu_rtc.c -- CMOS RTC wall clock (gap A17), freestanding C11. Self-contained: the MC146818 RTC thro |
| `kernel/` | `wubu_samplerate` | 92 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_samplerate.h | wubu_samplerate.c -- kernel-owned audio sample rate + format routing. Audio sample rate + format (PC |
| `kernel/` | `wubu_sata` | 96 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_sata.h | wubu_sata.c -- kernel-owned advanced SATA/NCQ driver routing. SATA is the ubiquitous disk bus (HDD/S |
| `kernel/` | `wubu_securekey` | 97 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_securekey.h | wubu_securekey.c -- kernel-owned security key / TOTP / TPM driver routing. The security tier: FIDO2/ |
| `kernel/` | `wubu_self_test` | 93 | string.h, wubu_self_test.h | wubu_self_test.c  --  kernel-resident self-test gate (gap G2) The suite exercises the live kernel st |
| `kernel/` | `wubu_sensor` | 132 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_sensor.h | wubu_sensor.c -- kernel-owned IIO sensor driver routing. The IIO (Industrial I/O) subsystem is how E |
| `kernel/` | `wubu_serial` | 122 | wubu_serial.h, wubu_sync.h | wubu_serial.c  --  WuBuOS Serial Console (interrupt-driven RX, gap E2) COM1 RX via the UART data-rea |
| `kernel/` | `wubu_sha256` | 173 | string.h, wubu_sha256.h | wubu_sha256.c -- SHA-256 (FIPS 180-4), freestanding C11. Self-contained: no malloc, no hosted APIs,  |
| `kernel/` | `wubu_smart` | 93 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_smart.h | wubu_smart.c -- kernel-owned storage SMART (S.M.A.R.T.) routing. SMART reports disk health + predict |
| `kernel/` | `wubu_smbios` | 108 | string.h, wubu_smbios.h | wubu_smbios.c  --  SMBIOS/DMI discovery (gap I3) Entry point search: the 32-bit anchor "_SM_" has it |
| `kernel/` | `wubu_smc` | 90 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_smc.h | wubu_smc.c -- kernel-owned GPU SMC firmware routing. SMC (System Management Controller) firmware man |
| `kernel/` | `wubu_smp` | 129 | wubu_smp.h | wubu_smp.c  --  SMP: application-processor bring-up (gap I2) AP protocol (Intel SDM): write 0x000C45 |
| `kernel/` | `wubu_smr` | 114 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_smr.h | wubu_smr.c -- kernel-owned storage SMR (Shingled Magnetic Recording) routing. SMR/ZNS drives write i |
| `kernel/` | `wubu_spdif` | 107 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_spdif.h | wubu_spdif.c -- kernel-owned audio SPDIF/HDMI passthrough routing. SPDIF (Sony/Philips Digital Inter |
| `kernel/` | `wubu_spdifrx` | 82 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_spdifrx.h | wubu_spdifrx.c -- kernel-owned audio SPDIF receiver routing. SPDIF RX (receiver) captures S/PDIF dig |
| `kernel/` | `wubu_spdifstatus` | 82 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_spdifstatus.h | wubu_spdifstatus.c -- kernel-owned audio SPDIF status routing. SPDIF status senses S/PDIF receiver/c |
| `kernel/` | `wubu_spdiftx` | 88 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_spdiftx.h | wubu_spdiftx.c -- kernel-owned audio SPDIF TX control routing. SPDIF TX control manages S/PDIF optic |
| `kernel/` | `wubu_storage` | 173 | stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_storage.h | wubu_storage.c -- kernel-owned storage driver routing + tuning. Linux storage "works" but ships with |
| `kernel/` | `wubu_storagesched` | 86 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_storagesched.h | wubu_storagesched.c -- kernel-owned storage block scheduler routing. Storage scheduler (deadline, BF |
| `kernel/` | `wubu_swap` | 135 | ahci.h, wubu_swap.h, wubu_vmm.h | wubu_swap.c  --  demand-page swap (gap B3) The swap area lives on the AHCI port-0 sim disk past the  |
| `kernel/` | `wubu_switchdev` | 110 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_switchdev.h | wubu_switchdev.c -- kernel-owned network switch fabric (switchdev) routing. Managed switches (DSA +  |
| `kernel/` | `wubu_sync` | 116 | wubu_sync.h | wubu_sync.c  --  WuBuOS Synchronization Primitives (freestanding) See wubu_sync.h. The spinlock save |
| `kernel/` | `wubu_test_stubs` | 80 | stddef.h, stdint.h, string.h | ---- Selftest output ----------------------------------------------- |
| `kernel/` | `wubu_theme` | 235 | stddef.h, stdio.h, wubu_theme.h | wubu_theme.c  --  WuBuOS Metal Theme Engine + /theme Namespace The graphic set as a writable node tr |
| `kernel/` | `wubu_thermal` | 117 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_thermal.h | wubu_thermal.c -- kernel-owned fan/thermal control routing. Thermal management controls CPU/GPU fans |
| `kernel/` | `wubu_thermal_read` | 408 | ctype.h, dirent.h, dlfcn.h, fcntl.h, stdio.h, stdlib.h, string.h, time.h, unistd | wubu_thermal_read.c -- see header for the "why". One owner for sensor VALUES.  The kernel driver (wu |
| `kernel/` | `wubu_thermalthrottle` | 86 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_thermalthrottle.h | wubu_thermalthrottle.c -- kernel-owned thermal throttling routing. Thermal throttling (CPU/GPU therm |
| `kernel/` | `wubu_touch` | 106 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_touch.h | wubu_touch.c -- kernel-owned touchscreen/trackpad driver routing. Touch is how laptops/tablets/all-i |
| `kernel/` | `wubu_tpm` | 104 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_tpm.h | wubu_tpm.c -- kernel-owned TPM 2.0 full-stack driver routing. The TPM (trusted platform module) is t |
| `kernel/` | `wubu_trim` | 123 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_trim.h | wubu_trim.c -- kernel-owned storage TRIM + USB-C alt mode routing. Two capabilities: - Storage TRIM: |
| `kernel/` | `wubu_tss` | 63 | stddef.h, stdint.h, wubu_tss.h | wubu_tss.c  --  WuBuOS TSS64 + GDT (freestanding) See wubu_tss.h. The GDT is rebuilt at runtime so t |
| `kernel/` | `wubu_tutor` | 527 | stdio.h, string.h, wubu_tutor.h | wubu_tutor.c -- the learning & education frontier (HX-C). C11. |
| `kernel/` | `wubu_uac` | 107 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_uac.h | wubu_uac.c -- kernel-owned USB audio (UAC) routing. USB Audio Class (UAC1/UAC2/UAC3) routes audio ov |
| `kernel/` | `wubu_uas` | 87 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_uas.h | wubu_uas.c -- kernel-owned storage USB Attached SCSI routing. UAS (USB Attached SCSI) provides bette |
| `kernel/` | `wubu_ucode` | 107 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_ucode.h | wubu_ucode.c -- kernel-owned CPU microcode loading routing. Microcode is CPU firmware loaded at boot |
| `kernel/` | `wubu_usb4` | 114 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_usb4.h | wubu_usb4.c -- kernel-owned USB4/Thunderbolt driver routing. USB4/Thunderbolt is the high-speed dais |
| `kernel/` | `wubu_usbf` | 215 | stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_usbf.h | wubu_usb.c -- kernel-owned USB driver routing + power management. USB is how EVERYTHING plugs in: co |
| `kernel/` | `wubu_user` | 52 | wubu_user.h | wubu_user.c  --  the ring-3 boundary (gap H4) wubu_user_enter constructs a user frame on the CURRENT |
| `kernel/` | `wubu_vblank` | 95 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vblank.h | wubu_vblank.c -- kernel-owned GPU VBLANK interrupt routing. VBLANK (vertical blank) interrupt synchr |
| `kernel/` | `wubu_vc4` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vc4.h | wubu_vc4.c -- kernel-owned Broadcom VideoCore vc4 routing. Broadcom VideoCore (RPi) binds the vc4 ke |
| `kernel/` | `wubu_vc6` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vc6.h | wubu_vc6.c -- kernel-owned Broadcom VideoCore VI routing. VideoCore VI (RPi4/5) binds the vc4 kernel |
| `kernel/` | `wubu_vdso` | 41 | wubu_vdso.h | wubu_vdso.c  --  kernel vDSO/vsyscall page (gap H6) The vDSO page is a static (BSS) page mapped read |
| `kernel/` | `wubu_vega` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vega.h | wubu_vega.c -- kernel-owned AMD GCN5 Vega routing. AMD Vega (RX Vega 56/64, GCN5) binds amdgpu + RAD |
| `kernel/` | `wubu_verifier` | 148 | wubu_agi_kernel.h, wubu_attest.h, wubu_verifier.h | wubu_verifier.c  --  WuBuOS Independent Verifier (DA-3 promotion gate) Deterministic, kernel-residen |
| `kernel/` | `wubu_video` | 116 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_video.h | wubu_video.c -- kernel-owned video encode/decode (VA-API / codec) routing. Hardware video accelerati |
| `kernel/` | `wubu_virt` | 107 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_virt.h | wubu_virt.c -- kernel-owned virtualization driver routing. WuBuOS must "run on everything" — includi |
| `kernel/` | `wubu_vlanaudio` | 125 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vlanaudio.h | wubu_vlanaudio.c -- kernel-owned NIC VLAN + audio DSP/mixing routing. Two distinct capabilities: - N |
| `kernel/` | `wubu_vmm` | 336 | wubu_memmap.h, wubu_sync.h, wubu_vmm.h | wubu_vmm.c  --  WuBuOS Virtual Memory (freestanding, metal) Bitmap page allocator + CR3 page-table w |
| `kernel/` | `wubu_volcanic_islands` | 57 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_volcanic_islan | wubu_volcanic_islands.c -- kernel-owned AMD GCN3 Volcanic routing. AMD GCN3 Volcanic Islands (R9 285 |
| `kernel/` | `wubu_voltagectl` | 57 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_voltagectl.h | wubu_voltagectl.c -- kernel-owned GPU voltage control routing. Voltage control monitors GPU core/mem |
| `kernel/` | `wubu_vpudecode` | 63 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vpudecode.h | wubu_vpudecode.c -- kernel-owned GPU video decode routing. GPU video decode (VA-API/VAAPI) routes co |
| `kernel/` | `wubu_vpuencode` | 59 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vpuencode.h | wubu_vpuencode.c -- kernel-owned GPU video encode routing. GPU video encode (VA-API/ffmpeg) routes r |
| `kernel/` | `wubu_vram` | 110 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vram.h | wubu_vram.c -- kernel-owned GPU VRAM + framebuffer memory routing. VRAM is dedicated graphics memory |
| `kernel/` | `wubu_vrr` | 58 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vrr.h | wubu_vrr.c -- kernel-owned G-Sync/FreeSync variable refresh routing. NVIDIA G-Sync / AMD FreeSync va |
| `kernel/` | `wubu_vulkan14` | 58 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_vulkan14.h | wubu_vulkan14.c -- kernel-owned Vulkan 1.4 runtime routing. Vulkan 1.4 bind full profile. NVIDIA: fu |
| `kernel/` | `wubu_wdt` | 55 | wubu_wdt.h | wubu_wdt.c  --  8254 channel-2 watchdog timer (gap E7) The 8254 ch2 runs in mode 0 (one-shot): the O |
| `kernel/` | `wubu_wifi7` | 130 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_pci.h, wubu_wifi7.h | wubu_wifi7.c -- kernel-owned Wi-Fi 7 (802.11be) / 6GHz driver routing. Wi-Fi 7 (802.11be) is the cur |
| `kernel/` | `wubu_wifi_reg` | 121 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_wifi_reg.h | wubu_wifi_reg.c -- kernel-owned WiFi regulatory/DFS routing. WiFi regulatory domains define allowed  |
| `kernel/` | `wubu_wifiutil` | 89 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_wifiutil.h | wubu_wifiutil.c -- kernel-owned WiFi channel utilization routing. Channel utilization measures how b |
| `kernel/` | `wubu_world` | 119 | stdio.h, string.h, wubu_drv.h, wubu_drv_battery.h, wubu_drv_gpu.h, wubu_drv_net. | wubu_world.c -- the WORLD-STATE bridge (the OS as the AGI's training space). WuBuOS is an AI operati |
| `kernel/` | `wubu_writeback` | 85 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_writeback.h | wubu_writeback.c -- kernel-owned storage writeback routing. Writeback flushes dirty pages from memor |
| `kernel/` | `wubu_xe3` | 56 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_hw_detect.h, wubu_xe3.h | wubu_xe3.c -- kernel-owned Intel Xe3 (Celestial) routing. Intel Xe3 (Celestial) binds the xe kernel  |
| `kernel/` | `wubu_xhci` | 170 | wubu_pci.h, wubu_xhci.h | wubu_xhci.c  --  the xHCI controller driver (gap E1) xHCI MMIO: the capability registers (CA PLENGTH |
| `kernel/` | `wubu_zip` | 130 | memory.h, string.h, wubu_zip.h, wubu_zlib.h | wubu_zip.c -- the IN-KERNEL ZIP reader. Parses the End-Of-Central-Directory record + central directo |
| `kernel/` | `wubu_znszone` | 64 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_znszone.h | wubu_znszone.c -- kernel-owned ZNS (Zoned Namespaces) routing. ZNS exposes SSDs as zones (sequential |
| `kernel/` | `wubu_zoneappend` | 67 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_zoneappend.h | wubu_zoneappend.c -- kernel-owned ZNS zone append routing. ZNS (Zoned Namespaces) drives expose zone |
| `kernel/` | `wubu_zonecap` | 57 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_zonecap.h | wubu_zonecap.c -- kernel-owned ZNS zone capacity routing. ZNS zones have a write pointer that must s |
| `kernel/` | `wubu_zoned` | 105 | dirent.h, egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_zoned.h | wubu_zoned.c -- kernel-owned SMR/Zoned storage driver routing. Zoned storage (SMR HDDs, ZNS NVMe) wr |
| `kernel/` | `wubu_zonefmt` | 66 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_zonefmt.h | wubu_zonefmt.c -- kernel-owned ZNS zone format/reset routing. ZNS zones must be reset (write pointer |
| `kernel/` | `wubu_zonseqwrite` | 68 | egojar.h, stdio.h, string.h, wubu_hw_detect.h, wubu_zonseqwrite.h | wubu_zonseqwrite.c -- kernel-owned ZNS sequential write routing. ZNS zones require sequential writes |
| `kernel/` | `wubu_zx` | 73 | wubu_modsym.h, wubu_zx.h | wubu_zx.c  --  the module-runner primitive (ZXE-style loader core). One reusable call that runs a ra |
| `kernel/` | `wubu_zxreg` | 124 | stdlib.h, string.h, wubu_sha256.h, wubu_zx.h, wubu_zxreg.h | wubu_zxreg.c  --  module registry (ZXE-style loader bookkeeping). Wraps wubu_zx_run with load/dedup/ |
| `kernel/` | `zip_extract` | 252 | egojar.h, errno.h, memory.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_zip.h | zip_extract.c -- list / get / extract-all from a ZIP archive using ONLY the in-kernel ZIP reader (wu |
| `lua/` | `wb_character_demo` | 94 | math.h, stdio.h, string.h, wb_camera.h, wb_character.h, wb_input.h, wb_physics.h | wb_character_demo.c -- slice 10 headless "playable character" demo. No window: this drives the same  |
| `lua/` | `wb_geo_demo` | 100 | stdint.h, stdio.h, string.h, wb_spatial.h, wb_tiles.h | wb_geo_demo.c -- Slice 5: wire the engine together. A Lua-driven "camera" flies over the planet; eac |
| `lua/` | `wb_assets` | 115 | stdio.h, stdlib.h, string.h, wb_assets.h, wb_assets_internal.h, wb_gltf.h | wb_assets.c -- content pipeline top level: file/container handling and the module error state. Mesh  |
| `lua/` | `wb_assets_ddc` | 107 | stdio.h, stdlib.h, string.h, wb_assets_internal.h, wb_ddc.h | wb_assets_ddc.c -- content-addressed derived-data cache. See wb_ddc.h. |
| `lua/` | `wb_assets_gltf` | 88 | stdio.h, stdlib.h, string.h, wb_assets_internal.h, wb_gltf.h | wb_assets_gltf.c -- glTF mesh decoding: primitives -> engine vertex/index arrays, plus material fact |
| `lua/` | `wb_assets_manifest` | 85 | stdio.h, stdlib.h, string.h, wb_assets_internal.h, wb_json.h, wb_manifest.h | wb_assets_manifest.c -- asset manifest load/write/verify. See wb_manifest.h. |
| `lua/` | `wb_avatar` | 89 | stdio.h, stdlib.h, string.h, wb_avatar.h, wb_avatar_internal.h, wb_gltf.h | wb_avatar.c -- avatar top level: container handling, load orchestration and the module error state.  |
| `lua/` | `wb_avatar_anim` | 85 | math.h, wb_anim.h | wb_avatar_anim.c -- animation sampling: TRS -> mat4 and keyframe interpolation. See wb_anim.h. |
| `lua/` | `wb_avatar_bones` | 67 | string.h, wb_avatar.h | wb_avatar_bones.c -- the humanoid bone name table and its lookup. See wb_avatar.h. |
| `lua/` | `wb_avatar_gltf` | 141 | stdio.h, stdlib.h, string.h, wb_avatar_internal.h, wb_gltf.h | wb_avatar_gltf.c -- glTF skin and animation decoding. See wb_avatar_internal.h. |
| `lua/` | `wb_avatar_identity` | 54 | stdio.h, stdlib.h, string.h, wb_avatar.h, wb_avatar_internal.h, wb_json.h | wb_avatar_identity.c -- identity record persistence (JSON). See wb_avatar.h. |
| `lua/` | `wb_camera` | 74 | math.h, string.h, wb_camera.h | wb_camera.c -- WuBu Engine follow/orbit camera (slice 10). See wb_camera.h. |
| `lua/` | `wb_character` | 69 | math.h, wb_character.h | wb_character.c -- WuBu Engine character controller (slice 10). See wb_character.h. |
| `lua/` | `wb_dist` | 212 | stdio.h, stdlib.h, string.h, wb_dist.h | wb_dist.c -- WuBu Engine experience distribution (slice 16). See wb_dist.h. |
| `lua/` | `wb_ecs` | 198 | stdlib.h, string.h, wb_ecs.h | wb_ecs.c -- WuBu Engine entity-component-system core. See wb_ecs.h. |
| `lua/` | `wb_edit` | 292 | stdio.h, stdlib.h, string.h, wb_edit.h, wb_json.h | wb_edit.c -- WuBu Engine in-world editor (slice 17). See wb_edit.h. |
| `lua/` | `wb_exp` | 89 | lauxlib.h, lua.h, stdio.h, stdlib.h, string.h, wb_ecs.h, wb_exp.h, wb_script.h | wb_exp.c -- experience container (slice 7). One instantiation of a gamemode spawned into a tile-cell |
| `lua/` | `wb_gltf` | 126 | string.h, wb_gltf.h | wb_gltf.c -- WuBu Engine shared glTF accessor decoding. See wb_gltf.h. |
| `lua/` | `wb_gpu_buffer` | 109 | stdlib.h, string.h, wb_gpu_internal.h | wb_gpu_buffer.c -- GPU buffers: creation, upload, map/unmap, readback. See wb_gpu.h. |
| `lua/` | `wb_gpu_dev` | 222 | stdio.h, string.h, wb_gpu_internal.h | wb_gpu_dev.c -- device, command queue, descriptor heaps, root signature and the module error state.  |
| `lua/` | `wb_gpu_exec` | 256 | stdio.h, stdlib.h, string.h, wb_gpu_internal.h | wb_gpu_exec.c -- graph execution: barriers derived from declared usage, command recording, submissio |
| `lua/` | `wb_gpu_pass` | 66 | stdio.h, string.h, wb_gpu_internal.h | wb_gpu_pass.c -- render-graph pass recording: declare targets, reads and draws. See wb_gpu.h. |
| `lua/` | `wb_gpu_shader` | 85 | stdlib.h, string.h, wb_gpu_internal.h | wb_gpu_shader.c -- runtime HLSL compilation and pipeline state. See wb_gpu.h. |
| `lua/` | `wb_gpu_texture` | 134 | stdlib.h, string.h, wb_gpu_internal.h | wb_gpu_texture.c -- texture creation, descriptors and upload. See wb_gpu.h. |
| `lua/` | `wb_host` | 195 | dirent.h, lauxlib.h, lua.h, stdio.h, stdlib.h, string.h, wb_host.h, wb_script.h, | wb_host.c -- the gametype host (slice 22). See wb_host.h. |
| `lua/` | `wb_input` | 78 | string.h, wb_input.h | wb_input.c -- WuBu Engine input state (slice 10). See wb_input.h. |
| `lua/` | `wb_json` | 192 | stdlib.h, string.h, wb_json.h | wb_json.c -- minimal JSON parser. See wb_json.h. |
| `lua/` | `wb_mode` | 274 | stdio.h, stdlib.h, string.h, wb_json.h, wb_mode.h, wb_util.h | wb_mode.c -- gametype manifest + config schema. See wb_mode.h. |
| `lua/` | `wb_physics_bind` | 59 | stdlib.h, string.h, wb_physics_internal.h | wb_physics_bind.c -- the ECS <-> physics contract: bind an entity to a body, feed ECS velocities in  |
| `lua/` | `wb_physics_body` | 124 | stdlib.h, wb_physics_internal.h | wb_physics_body.c -- per-body queries and control: step, transforms, velocities, grounding, facing.  |
| `lua/` | `wb_physics_world` | 175 | stdio.h, stdlib.h, string.h, wb_physics_internal.h | wb_physics_world.c -- physics lifecycle and body construction. See wb_physics.h. |
| `lua/` | `wb_render` | 162 | math.h, stdlib.h, string.h, wb_render.h | wb_render.c -- fixed-function software rasterizer. Perspective pipeline: world -> view (LookAt) -> c |
| `lua/` | `wb_spatial` | 132 | math.h, wb_spatial.h | MinGW/hidden-M_PI portability: define it unconditionally. |
| `lua/` | `wb_spawn` | 113 | stdio.h, stdlib.h, string.h, wb_spawn.h | wb_spawn.c -- spawn points. See wb_spawn.h. |
| `lua/` | `wb_tiles` | 174 | stdlib.h, string.h, wb_tiles.h | seat = one resident tile (one (qkey,plane) cell). Payload lives inline. |
| `lua/` | `wb_util` | 90 | direct.h, stat.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_util.h | wb_util.c -- WuBu Engine shared C11 utilities. See wb_util.h. |
| `lua/` | `wb_worldspace` | 273 | stdio.h, stdlib.h, string.h, wb_util.h, wb_worldspace.h | wb_worldspace.c -- the spawn-a-worldspace facade. See wb_worldspace.h. |
| `lua/` | `wb_lua_host` | 53 | stdio.h, wb_host.h, wb_script.h | wb_lua_host.c -- WuBu Engine standalone entrypoint. wubu_engine.exe <modes-root> <gametype> [player- |
| `lua/` | `wb_mod` | 197 | lauxlib.h, lua.h, lualib.h, stdio.h, stdlib.h, string.h, wb_json.h, wb_mod.h, wb | wb_mod.c -- WuBu Engine modding sandbox (slice 15). See wb_mod.h. |
| `lua/` | `wb_script` | 382 | lauxlib.h, lua.h, lualib.h, stdio.h, stdlib.h, string.h, wb_ecs.h, wb_mode.h, wb | wb_script.c -- shared Lua<->core binding (slice 7). The functions that both the standalone engine an |
| `lua/` | `wb_space_bind` | 219 | lauxlib.h, lua.h, stdio.h, stdlib.h, string.h, wb_script.h, wb_space_bind.h, wb_ | wb_space_bind.c -- the gamemode-facing space bindings (slice 21). See wb_space_bind.h. The engine st |
| `lua/` | `wb_lv_arena` | 40 | stdlib.h, string.h, wb_lv_internal.h | wb_lv_arena.c -- the interpreter's arena, a bump-allocating slab pool. Every interpreter allocation  |
| `lua/` | `wb_lv_coro` | 315 | stdio.h, stdlib.h, string.h, wb_lv_internal.h, windows.h | wb_lv_coro.c -- coroutines, on Windows fibers. A tree-walker lives on the C stack, so a coroutine ne |
| `lua/` | `wb_lv_eval` | 1297 | errno.h, math.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_eval.c -- the tree-walking evaluator: scopes, calls, operators with metamethod dispatch, multi |
| `lua/` | `wb_lv_host` | 659 | math.h, stdarg.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_host.c -- the interpreter's public driver: state lifecycle, the base library, run/call entry p |
| `lua/` | `wb_lv_lex` | 417 | math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_lex.c -- the token stream: whitespace/comments, numbers (decimal + hex), names/keywords, strin |
| `lua/` | `wb_lv_parse` | 717 | stdlib.h, string.h, wb_lv_internal.h | wb_lv_parse.c -- the grammar: expressions, statements and blocks, plus the AST pool. Lua 5.4 syntax  |
| `lua/` | `wb_lv_stdlib` | 1082 | math.h, stdio.h, stdlib.h, string.h, time.h, wb_lv_internal.h | wb_lv_stdlib.c -- the table / math / os / io libraries. See wb_lv_internal.h. |
| `lua/` | `wb_lv_string` | 1084 | ctype.h, math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_string.c -- the string library, including a Lua-pattern matcher. See wb_lv_internal.h. |
| `lua/` | `wb_lv_value` | 323 | math.h, stdio.h, stdlib.h, string.h, wb_lv_internal.h | wb_lv_value.c -- values, string interning and tables. See wb_lv_internal.h. Tables are open-addresse |
| `lua/` | `accept_wua` | 39 | stdio.h, stdlib.h, wb_lv.h, wb_lv_internal.h | accept_wua.c -- parse-only conformance probe for Wua (the analogue of `luac -p`): exit 0 if the file |
| `lua/` | `wb_lv_script_test` | 108 | stdio.h, stdlib.h, string.h, wb_lv.h | wb_lv_script_test.c -- drive engine-style bindings through wb_lv. Proves the VM meets the gamemode c |
| `lua/` | `wb_lv_test` | 382 | stdio.h, string.h, wb_lv.h | wb_lv_test.c -- verification of the slerm-Lua interpreter (slice 8). Runs real programs through wb_l |
| `lua/` | `wb_mesh` | 249 | math.h, stdlib.h, string.h, wb_mesh.h | wb_mesh.c -- WuBu Engine server meshing (slice 18). See wb_mesh.h. |
| `lua/` | `wb_net_aoi` | 32 | wb_net.h, wb_spatial.h | wb_net_aoi.c -- interest management on the engine's quadkey spatial core. See wb_net.h. |
| `lua/` | `wb_net_client` | 91 | math.h, stdlib.h, wb_net_internal.h | wb_net_client.c -- client-side prediction and reconciliation. See wb_net_client.h. |
| `lua/` | `wb_net_loopback` | 75 | stdlib.h, wb_net_internal.h | wb_net_loopback.c -- in-process transport: inputs client->server, snapshots server->client, with a d |
| `lua/` | `wb_net_server` | 168 | math.h, stdlib.h, string.h, wb_net_internal.h | wb_net_server.c -- the authoritative server: entity simulation, AOI snapshot building and delta comp |
| `lua/` | `wb_assets_test` | 179 | math.h, stdio.h, stdlib.h, string.h, wb_assets.h, wb_ddc.h, wb_manifest.h | wb_assets_test.c -- slice 12 gate: the content pipeline. Builds a real GLB in memory (JSON chunk + B |
| `lua/` | `wb_avatar_test` | 209 | math.h, stdio.h, stdlib.h, string.h, wb_avatar.h | wb_avatar_test.c -- slice 14 gate: avatar + identity. Builds a GLB in memory with a two-joint humano |
| `lua/` | `wb_character_test` | 168 | math.h, stdio.h, string.h, wb_camera.h, wb_character.h, wb_input.h, wb_physics.h | wb_character_test.c -- slice 10 gate: input + camera + character chain. Proves the "playable" path w |
| `lua/` | `wb_dist_test` | 171 | math.h, stdio.h, string.h, wb_dist.h | wb_dist_test.c -- slice 16 gate: experience distribution. Proves the "world space and distribution s |
| `lua/` | `wb_edit_test` | 159 | math.h, stdio.h, stdlib.h, string.h, wb_edit.h | wb_edit_test.c -- slice 17 gate: the in-world editor. Proves the edit document model: edits apply to |
| `lua/` | `wb_exp_test` | 68 | stdio.h, string.h, wb_ecs.h, wb_exp.h | wb_exp_test.c -- verify experience containers (slice 7). Two instances of the SAME gamemode spawned  |
| `lua/` | `wb_gpu_test` | 176 | math.h, stdio.h, string.h, wb_gpu.h | wb_gpu_test.c -- slice 11 gate: the D3D12 GPU backend. Renders for real and reads pixels back, so a  |
| `lua/` | `wb_host_test` | 175 | stdio.h, stdlib.h, string.h, wb_host.h, wb_script.h, wb_util.h, wb_worldspace.h | wb_host_test.c -- slice 22 gate: the gametype host. The first end-to-end test of the production path |
| `lua/` | `wb_mesh_test` | 173 | stdio.h, string.h, wb_mesh.h | wb_mesh_test.c -- slice 18 gate: server meshing. Proves authority is per-entity and per-component, t |
| `lua/` | `wb_mod_test` | 154 | stdio.h, stdlib.h, string.h, wb_mod.h, wb_script.h | wb_mod_test.c -- slice 15 gate: the modding sandbox. Writes real mods to disk and verifies the Luau- |
| `lua/` | `wb_mode_test` | 223 | stdio.h, stdlib.h, string.h, wb_mode.h, wb_util.h | wb_mode_test.c -- slice 19 gate: gametype manifests + config schema. A gametype is a declared, param |
| `lua/` | `wb_net_test` | 253 | math.h, stdio.h, string.h, wb_net.h, wb_net_client.h, wb_net_loopback.h, wb_net_ | wb_net_test.c -- slice 13 gate: the networking core. Proves the multiplayer path end to end over a l |
| `lua/` | `wb_physics_test` | 150 | math.h, stdio.h, string.h, wb_ecs.h, wb_physics.h | wb_physics_test.c -- slice 9 gate: MuJoCo-backed physics. Proves the physics pillar end to end: a bo |
| `lua/` | `wb_portal_test` | 135 | stdio.h, stdlib.h, string.h, wb_dist.h, wb_util.h, wb_worldspace.h | wb_portal_test.c -- slice 20 gate: the Yu-Gi-Oh stadium mechanic. The hub world is a 1:1 mirror-slic |
| `lua/` | `wb_render_test` | 86 | stdint.h, stdio.h, string.h, wb_render.h | wb_render_test.c -- verification of the software rasterizer. Renders a small flat plate and a plate  |
| `lua/` | `wb_space_test` | 207 | stdio.h, stdlib.h, string.h, wb_mod.h, wb_script.h, wb_util.h, wb_worldspace.h | wb_space_test.c -- slice 21 gate: the gamemode-facing space. The Yu-Gi-Oh stadium mechanic, exposed  |
| `lua/` | `wb_spatial_test` | 99 | math.h, stdio.h, wb_spatial.h | wb_spatial_test.c -- verification of the quadkey spatial core. Anchors on known geographic facts so  |
| `lua/` | `wb_spawn_test` | 125 | stdio.h, string.h, wb_spawn.h | wb_spawn_test.c -- slice 19 gate: spawn points. Spawn points are data, not code: position, team, tag |
| `lua/` | `wb_test` | 133 | stdio.h, string.h, wb_ecs.h | wb_test.c -- C11 test harness for the WuBu Engine ECS core. Tests real behavior only (no mocks): han |
| `lua/` | `wb_tiles_test` | 136 | stdio.h, string.h, wb_tiles.h | wb_tiles_test.c -- verification of the quadkey tile store. Real geodata-style loads: a synthetic "he |
| `lua/` | `wb_worldspace_test` | 265 | stdio.h, stdlib.h, string.h, wb_util.h, wb_worldspace.h | wb_worldspace_test.c -- slice 19 gate: the spawn-a-worldspace facade. Proves the whole flow a host u |
| `lua/` | `lapi` | 1463 | lapi.h, ldebug.h, ldo.h, lfunc.h, lgc.h, limits.h, lmem.h, lobject.h, lprefix.h, | $Id: lapi.c $ Lua API |
| `lua/` | `lauxlib` | 1126 | errno.h, lauxlib.h, lprefix.h, lua.h, stdarg.h, stdio.h, stdlib.h, string.h, wai | $Id: lauxlib.c $ Auxiliary functions for building Lua libraries |
| `lua/` | `lbaselib` | 549 | ctype.h, lauxlib.h, lprefix.h, lua.h, lualib.h, stdio.h, stdlib.h, string.h | $Id: lbaselib.c $ Basic library |
| `lua/` | `lcode` | 1875 | float.h, lcode.h, ldebug.h, ldo.h, lgc.h, limits.h, llex.h, lmem.h, lobject.h, l | $Id: lcode.c $ Code generator for Lua |
| `lua/` | `lcorolib` | 210 | lauxlib.h, lprefix.h, lua.h, lualib.h, stdlib.h | $Id: lcorolib.c $ Coroutine Library |
| `lua/` | `lctype` | 64 | lctype.h, limits.h, lprefix.h | $Id: lctype.c $ 'ctype' functions for Lua |
| `lua/` | `ldblib` | 483 | lauxlib.h, lprefix.h, lua.h, lualib.h, stdio.h, stdlib.h, string.h | $Id: ldblib.c $ Interface from Lua to its debug API |
| `lua/` | `ldebug` | 971 | lapi.h, lcode.h, ldebug.h, ldo.h, lfunc.h, lobject.h, lopcodes.h, lprefix.h, lst | $Id: ldebug.c $ Debug Interface |
| `lua/` | `ldo` | 1035 | lapi.h, ldebug.h, ldo.h, lfunc.h, lgc.h, lmem.h, lobject.h, lopcodes.h, lparser. | $Id: ldo.c $ Stack and Call structure of Lua |
| `lua/` | `ldump` | 230 | limits.h, lobject.h, lprefix.h, lstate.h, lua.h, lundump.h, stddef.h | $Id: ldump.c $ save precompiled Lua chunks |
| `lua/` | `lfunc` | 294 | ldebug.h, ldo.h, lfunc.h, lgc.h, lmem.h, lobject.h, lprefix.h, lstate.h, lua.h,  | $Id: lfunc.c $ Auxiliary functions to manipulate prototypes and closures |
| `lua/` | `lgc` | 1743 | ldebug.h, ldo.h, lfunc.h, lgc.h, lmem.h, lobject.h, lprefix.h, lstate.h, lstring | $Id: lgc.c $ Garbage Collector |
| `lua/` | `linit` | 65 | lauxlib.h, lprefix.h, lua.h, lualib.h, stddef.h | $Id: linit.c $ Initialization of libraries for lua.c and other clients |
| `lua/` | `liolib` | 841 | ctype.h, errno.h, lauxlib.h, locale.h, lprefix.h, lua.h, lualib.h, stdio.h, stdl | $Id: liolib.c $ Standard I/O (and system) library |
| `lua/` | `llex` | 581 | lctype.h, ldebug.h, ldo.h, lgc.h, llex.h, lobject.h, locale.h, lparser.h, lprefi | $Id: llex.c $ Lexical Analyzer |
| `lua/` | `lmathlib` | 781 | float.h, lauxlib.h, limits.h, lprefix.h, lua.h, lualib.h, math.h, stdlib.h, time | $Id: lmathlib.c $ Standard mathematical library |
| `lua/` | `lmem` | 215 | ldebug.h, ldo.h, lgc.h, lmem.h, lobject.h, lprefix.h, lstate.h, lua.h, stddef.h | $Id: lmem.c $ Interface to Memory Manager |
| `lua/` | `loadlib` | 758 | dlfcn.h, lauxlib.h, lprefix.h, lua.h, lualib.h, stdio.h, stdlib.h, string.h, win | $Id: loadlib.c $ Dynamic library loader for Lua This module contains an implementation of loadlib fo |
| `lua/` | `lobject` | 602 | lctype.h, ldebug.h, ldo.h, lmem.h, lobject.h, locale.h, lprefix.h, lstate.h, lst | $Id: lobject.c $ Some generic functions over Lua objects |
| `lua/` | `lopcodes` | 104 | lopcodes.h, lprefix.h | $Id: lopcodes.c $ Opcodes for Lua virtual machine |
| `lua/` | `loslib` | 430 | errno.h, lauxlib.h, locale.h, lprefix.h, lua.h, lualib.h, stdlib.h, string.h, ti | $Id: loslib.c $ Standard Operating System library |
| `lua/` | `lparser` | 1966 | lcode.h, ldebug.h, ldo.h, lfunc.h, limits.h, llex.h, lmem.h, lobject.h, lopcodes | $Id: lparser.c $ Lua Parser |
| `lua/` | `lstate` | 448 | lapi.h, ldebug.h, ldo.h, lfunc.h, lgc.h, llex.h, lmem.h, lprefix.h, lstate.h, ls | $Id: lstate.c $ Global State |
| `lua/` | `lstring` | 274 | ldebug.h, ldo.h, lmem.h, lobject.h, lprefix.h, lstate.h, lstring.h, lua.h, strin | $Id: lstring.c $ String table (keeps all strings handled by Lua) |
| `lua/` | `lstrlib` | 1874 | ctype.h, float.h, lauxlib.h, limits.h, locale.h, lprefix.h, lua.h, lualib.h, mat | $Id: lstrlib.c $ Standard library for string operations and pattern-matching |
| `lua/` | `ltable` | 995 | ldebug.h, ldo.h, lgc.h, limits.h, lmem.h, lobject.h, lprefix.h, lstate.h, lstrin | $Id: ltable.c $ Lua tables (hash) |
| `lua/` | `ltablib` | 430 | lauxlib.h, limits.h, lprefix.h, lua.h, lualib.h, stddef.h, string.h, time.h | $Id: ltablib.c $ Library for Table Manipulation |
| `lua/` | `ltests` | 1986 | assert.h, lapi.h, lauxlib.h, lcode.h, lctype.h, ldebug.h, ldo.h, lfunc.h, limits | $Id: ltests.c $ Internal Module for Debugging of the Lua Implementation |
| `lua/` | `ltm` | 271 | ldebug.h, ldo.h, lgc.h, lobject.h, lprefix.h, lstate.h, lstring.h, ltable.h, ltm | $Id: ltm.c $ Tag methods |
| `lua/` | `lua` | 687 | history.h, io.h, lauxlib.h, lprefix.h, lua.h, lualib.h, readline.h, signal.h, st | $Id: lua.c $ Lua stand-alone interpreter |
| `lua/` | `lundump` | 335 | ldebug.h, ldo.h, lfunc.h, limits.h, lmem.h, lobject.h, lprefix.h, lstring.h, lua | $Id: lundump.c $ load precompiled Lua chunks |
| `lua/` | `lutf8lib` | 291 | assert.h, lauxlib.h, limits.h, lprefix.h, lua.h, lualib.h, stdlib.h, string.h | $Id: lutf8lib.c $ Standard library for UTF-8 manipulation |
| `lua/` | `lvm` | 1902 | float.h, ldebug.h, ldo.h, lfunc.h, lgc.h, limits.h, ljumptab.h, lobject.h, lopco | $Id: lvm.c $ Lua virtual machine |
| `lua/` | `lzio` | 68 | llimits.h, lmem.h, lprefix.h, lstate.h, lua.h, lzio.h, string.h | $Id: lzio.c $ Buffered streams |
| `lua/` | `onelua` | 121 | assert.h, ctype.h, errno.h, float.h, lapi.c, lauxlib.c, lbaselib.c, lcode.c, lco | Lua core, libraries, and interpreter in a single file. Compiling just this file generates a complete |
| `runtime/` | `app_bundle` | 184 | app_bundle.h, string.h, wubu_container.h, wubu_exec.h | app_bundle.c -- see app_bundle.h.  Parsing is fixed-offset and explicit; launching is delegated to t |
| `runtime/` | `app_bundle_test` | 171 | app_bundle.h, stdio.h, stdlib.h, string.h, wubu_exec.h | app_bundle_test.c -- the .app container contract. A bundle that lies about where its entry point liv |
| `runtime/` | `wubu_container_universal` | 367 | dirent.h, egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_container_un | wubu_container_universal.c -- the uniform container layer + two real backends (RAM and host-director |
| `runtime/` | `wubu_container_universal_test` | 137 | egojar.h, stdio.h, stdlib.h, string.h, wubu_container_universal.h | wubu_container_universal_test.c -- the universal container must be honestly universal: every kind -- |
| `runtime/` | `wubucontainer` | 694 | egojar.h, egojar_net.h, errno.h, inet.h, json.h, socket.h, stdio.h, stdlib.h, st | wubucontainer.c  --  WuBuContainer Conversion Toolkit C Implementation Implements the C-side interfa |
| `runtime/` | `wubucontainer_registry` | 51 | string.h, wubucontainer_internal.h | wubucontainer_registry.c -- In-memory handler registry for the WuBuContainer agentic layer. This is  |
| `runtime/` | `wubucontainer_test` | 52 | assert.h, stdio.h, string.h, wubucontainer.h | wubucontainer_test.c -- Regression test for wubu_container_register_handler. Verifies the handler re |
| `runtime/` | `ct_iso_cgroup` | 24 | ct_iso_cgroup.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_ct_isolate | ct_iso_cgroup.c  --  WuBuOS container cgroups v2 write helper (Cell 420 split). The cgroup create/se |
| `runtime/` | `ct_iso_ns` | 13 | ct_iso_ns.h, sched.h, wubu_gnu_compat.h | ct_iso_ns.c  --  WuBuOS container namespace unshare helper (Cell 420 split). |
| `runtime/` | `ct_iso_seccomp` | 349 | audit.h, ct_iso_ns.h, ct_iso_seccomp.h, egojar.h, errno.h, filter.h, seccomp.h,  | ct_iso_seccomp.c  --  WuBuOS container seccomp-BPF syscall filtering (Cell 420 split). Allowlist app |
| `runtime/` | `edr_core` | 573 | edr_internal.h, sched.h, stdatomic.h | edr_core.c  --  WuBuOS EDR Engine Core Lifecycle, module system, alert buffer, FNV-1a hashing, lock- |
| `runtime/` | `edr_fanotify` | 248 | edr_internal.h, egojar.h, fanotify.h | edr_fanotify.c  --  WuBuOS EDR File Telemetry Pins Two kernel telemetry sources for file and configu |
| `runtime/` | `edr_poller` | 325 | edr_internal.h | edr_poller.c  --  WuBuOS EDR Periodic Poller Periodic /proc scans for telemetry that doesn't have a  |
| `runtime/` | `edr_proc_pin` | 202 | cn_proc.h, connector.h, edr_internal.h | edr_proc_pin.c  --  WuBuOS EDR Process Pin NETLINK_CONNECTOR + cn_proc — the Linux equivalent of Win |
| `runtime/` | `json_fuzz` | 208 | fuzz_rng.h, stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_json.h | json_fuzz.c -- deterministic mutation fuzzer for the JSON reader. WHY THIS PARSER SPECIFICALLY wubu_ |
| `runtime/` | `oci_blob_store` | 79 | oci_internal.h | oci_blob_store.c  --  OCI Content-Addressable Blob Storage Extracted from wubu_oci.c (lines 983-1053 |
| `runtime/` | `oci_cleanup` | 147 | oci_internal.h | oci_cleanup.c  --  OCI Layer Cleanup and Garbage Collection Extracted from wubu_oci.c (lines 1634-17 |
| `runtime/` | `oci_convert` | 261 | oci_internal.h | oci_convert.c  --  Convert between .wubu and OCI formats Extracted from wubu_oci.c (lines 1055-1307) |
| `runtime/` | `oci_descriptor` | 19 | oci_internal.h | oci_descriptor.c  --  OCI Descriptor Operations Extracted from wubu_oci.c (lines 506-516). |
| `runtime/` | `oci_hooks` | 28 | oci_internal.h | oci_hooks.c  --  OCI Runtime Hook Operations Extracted from wubu_oci.c (lines 1614-1632). |
| `runtime/` | `oci_http_client` | 368 | oci_internal.h | oci_http_client.c  --  HTTP/TLS Client for OCI Registry Operations Extracted from wubu_oci.c (lines  |
| `runtime/` | `oci_image_config` | 256 | oci_internal.h | oci_image_config.c  --  OCI Image Config Operations Extracted from wubu_oci.c (lines 520-768). |
| `runtime/` | `oci_image_index` | 110 | oci_internal.h | oci_image_index.c  --  OCI Image Index (Multi-arch) Operations Extracted from wubu_oci.c (lines 878- |
| `runtime/` | `oci_image_manifest` | 115 | oci_internal.h | oci_image_manifest.c  --  OCI Image Manifest Operations Extracted from wubu_oci.c (lines 770-876). |
| `runtime/` | `oci_media_types` | 18 | oci_internal.h | oci_media_types.c  --  OCI Media Type String Helpers Extracted from wubu_oci.c (lines 495-505). |
| `runtime/` | `oci_registry` | 216 | oci_internal.h | oci_registry.c  --  OCI Registry Client Operations Extracted from wubu_oci.c (lines 1309-1516). |
| `runtime/` | `oci_runtime_spec` | 87 | oci_internal.h | oci_runtime_spec.c  --  OCI Runtime Spec Operations Extracted from wubu_oci.c (lines 1518-1612). |
| `runtime/` | `seccomp_registry` | 298 | audit.h, ct_iso_seccomp.h, egojar.h, filter.h, seccomp.h, stdio.h, string.h | seccomp_registry.c -- seccomp profile registry (the Revolver Doctrine). Split out of ct_iso_seccomp. |
| `runtime/` | `spellmod_fuzz` | 215 | fuzz_rng.h, stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_spellmod.h | spellmod_fuzz.c -- deterministic mutation fuzzer for the .spell module container. WHY THIS PARSER SP |
| `runtime/` | `styx_enc` | 108 | string.h, styx.h, styx_internal.h | styx_enc.c -- Styx/9P2000 client request encoders + Rerror builder. Self-contained: every function h |
| `runtime/` | `styx_fid` | 33 | string.h, styx.h | styx_fid.c -- Styx fid (file-identifier) management subsystem. Self-contained: styx_fid_alloc / styx |
| `runtime/` | `styx_names` | 62 | string.h, styx.h | styx_names.c -- Styx/9P2000 message-name table + lookup. Self-contained: only depends on styx.h for  |
| `runtime/` | `styx_parse` | 95 | string.h, styx.h | styx_parse.c -- Styx/9P2000 client-side response/send parsers. Self-contained: each function validat |
| `runtime/` | `styx_serve` | 464 | string.h, styx.h, styx_internal.h | styx_serve.c -- Styx/9P2000 server: init + inbound dispatch + R-message response builders. Self-cont |
| `runtime/` | `styx_test` | 636 | stdio.h, stdlib.h, string.h, styx.h | styx_test.c  --  Styx/9P2000 Protocol Test Suite Tests: message building, parsing, server dispatch,  |
| `runtime/` | `styxfs_callbacks` | 385 | dirent.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_inter | the /n control plane needs -- the namespace bridge writes real files |
| `runtime/` | `styxfs_host` | 167 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h, w | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `runtime/` | `styxfs_path` | 50 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `runtime/` | `styxfs_posix` | 253 | dirent.h, egojar.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h, ti | styxfs_posix.c -- StyxFS POSIX-like file API + global server instance. Extracted from the monolithic |
| `runtime/` | `styxfs_server` | 494 | dirent.h, egojar.h, errno.h, libgen.h, stdio.h, stdlib.h, string.h, styx.h, time | styxfs_server.c  --  Styx/9P File Server Implementation A concrete 9P2000 file server that exports a |
| `runtime/` | `styxfs_test` | 287 | egojar.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h | styxfs_test.c  --  WuBuOS StyxFS Test Suite (Cell 106) Tests StyxFS mount/unmount, file namespace, . |
| `runtime/` | `styxfs_util` | 104 | dirent.h, egojar.h, stdio.h, styxfs_internal.h, time.h | styxfs_util.c -- StyxFS utility subsystem (mount/file resolution, container load). Self-contained: u |
| `runtime/` | `styxfs_vfs` | 266 | dirent.h, egojar.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h, ti | styxfs_vfs.c -- StyxFS in-memory VFS tree + open-file table. Extracted from the monolithic styxfs.c. |
| `runtime/` | `wubu_applaunch_test` | 155 | egojar.h, errno.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_par | wubu_applaunch_test.c -- the session->/n app-launch bridge. Proves the P3/module seam: a session aut |
| `runtime/` | `wubu_onboarding_test` | 113 | egojar.h, stdio.h, stdlib.h, string.h, wubu_onboarding.h, wubu_session_boot.h | wubu_onboarding_test.c -- GAP F (docs/GAP_ROADMAP): a HOSTED END-TO-END first-boot onboarding stream |
| `runtime/` | `wubu_selfbuild_test` | 257 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | wubu_selfbuild_test.c -- the stage-0 SELF-BUILD harness. The recursion that makes holyd a pipeline c |
| `runtime/` | `wubu_zxload_test` | 184 | egojar.h, holyd.h, holyd_codegen.h, holyd_codegen_internal.h, holyd_parser.h, st | wubu_zxload_test.c -- rung 5: RUN A MODULE BY NAME FROM THE /n NAMESPACE. The namespace loader (wubu |
| `runtime/` | `test_archd_n1` | 79 | egojar.h, stdio.h, stdlib.h, string.h | test_archd_n1.c — BATTLESHIP N1 gate: shell injection is dead. N1 (🔴 security): wubu_archd ran user- |
| `runtime/` | `test_revolver` | 122 | guest_file.h, hosted_internal.h, stdio.h, stdlib.h, string.h, wubu_revolver.h | test_revolver.c -- the clamping rules must hold, not just parse. A revolver is a configuration primi |
| `runtime/` | `test_svc_super` | 180 | egojar.h, stdio.h, stdlib.h, string.h, wubu_archd_svc.h | test_svc_super.c — N2 gate: the in-process service supervisor is real. Proves (no systemd, no shell) |
| `runtime/` | `test_win32_core` | 64 | stdio.h, string.h, win32_core.h | win32_core_test.c -- the Win32 personality contract. Round-trips real text through both conversions, |
| `runtime/` | `test_colonel` | 70 | stdio.h, string.h, wubu_colonel.h | test_colonel.c -- the everything-through-the-Colonel dispatcher. |
| `runtime/` | `test_hwdetect` | 68 | stdio.h, string.h, wubu_hwdetect.h | test_hwdetect.c -- WuBuOS hardware-acceleration detection strategies. |
| `runtime/` | `test_kernel_xchg` | 189 | stdio.h, string.h, wubu_kernel_xchg.h | test_kernel_xchg.c -- the kernel interchange network. Three fake kernels (linux/nt/macos) with disjo |
| `runtime/` | `test_revolver` | 108 | ct_iso_seccomp.h, stdio.h, string.h, styx.h, wubu_anticheat.h, wubu_colonel.h, w | test_revolver.c -- the Revolver Doctrine gate (S7-S11). Proves the WuBuOS policy tables are RUNTIME  |
| `runtime/` | `wubu_vsl_gpu` | 475 | drm.h, drm_fourcc.h, drm_mode.h, egojar.h, egojar_net.h, errno.h, stdio.h, stdli |  |
| `runtime/` | `test_gpu` | 119 | egojar.h, stdio.h, stdlib.h, wubu_vsl_gpu.h | VSL GPU Test - Validates DRM/KMS backend on WSL |
| `runtime/` | `test_vulkan` | 73 | stdio.h, stdlib.h, wubu_vsl_vulkan.h | VSL Vulkan Test - Validates backend selection and device enumeration Run on WSL to test Venus/Linux  |
| `runtime/` | `wubu_vsl_vulkan` | 581 | dirent.h, egojar.h, stdio.h, stdlib.h, string.h, vulkan.h, wubu_vsl_vulkan.h | VSL Vulkan Implementation - WuBuOS Vulkan Abstraction Backend selection: 1. WSL/VM: VirtIO-GPU Venus |
| `runtime/` | `test_nt_abi` | 105 | stdio.h, string.h, vsl_nt_internal.h | test_nt_abi.c -- conformance gate for docs/ABI.md. The contract makes four promises a caller can act |
| `runtime/` | `vsl` | 158 | bpf.h, egojar.h, errno.h, if.h, if_tun.h, landlock.h, perf_event.h, sched.h, std | vsl.c  --  VSL Core Lifecycle & Diagnostics This file contains the main VSL state and lifecycle func |
| `runtime/` | `vsl_driver` | 273 | cuda.h, egojar.h, errno.h, if.h, if_tun.h, stdio.h, stdlib.h, string.h, vsl_driv | vsl_driver.c  --  VSL Driver Management Implementation |
| `runtime/` | `vsl_elf` | 195 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, vsl_elf.h, vsl_internal.h | vsl_elf.c  --  VSL ELF Loading Implementation |
| `runtime/` | `vsl_file` | 132 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, vsl_file.h, vsl_internal.h | vsl_file.c  --  VSL File Operations Implementation |
| `runtime/` | `vsl_gpu_vulkan` | 639 | egojar.h, stdbool.h, stdio.h, stdlib.h, string.h, vsl_driver.h, vsl_gpu_vulkan.h | vsl_gpu_vulkan.c  --  VSL Vulkan GPU Driver Implementation Implements VSL_DRV_GPU_VULKAN driver for  |
| `runtime/` | `vsl_kernel_adapt` | 176 | stdio.h, string.h, vsl_nt_bridge.h, vsl_syscall_internal.h, wubu_kernel_xchg.h,  | vsl_kernel_adapt.c -- the real VSL personalities, registered as kernels. Until this file existed the |
| `runtime/` | `vsl_kernel_adapt_test` | 88 | stdio.h, string.h, vsl_nt_bridge.h, vsl_syscall.h, vsl_syscall_numbers.h, wubu_k | vsl_kernel_adapt_test.c -- the real personalities, reachable through the interchange.  This is the p |
| `runtime/` | `vsl_macho` | 318 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, vsl_internal.h, vsl_macho.h | vsl_macho.c  --  VSL Mach-O Binary Loader Implementation Validates and loads macOS Mach-O binaries i |
| `runtime/` | `vsl_macho_test` | 231 | assert.h, stdio.h, stdlib.h, string.h, vsl_macho.h | vsl_macho_test.c  --  VSL Mach-O Loader Tests Tests for Mach-O binary validation, FAT binary extract |
| `runtime/` | `vsl_memory` | 99 | stdint.h, stdlib.h, string.h, vsl_internal.h, vsl_memory.h, vsl_process.h | vsl_memory.c  --  VSL Memory Management Implementation |
| `runtime/` | `vsl_nt_alpc` | 434 | egojar.h, egojar_net.h, errno.h, socket.h, stdint.h, stdlib.h, string.h, vsl_nt_ | vsl_nt_alpc.c -- Windows 11 ALPC (Advanced Local Procedure Call) syscalls. ALPC is the high-performa |
| `runtime/` | `vsl_nt_atoms` | 303 | futex.h, vsl_nt_internal.h | used in this TU, so declared here |
| `runtime/` | `vsl_nt_collision_test` | 55 | stdio.h, string.h, vsl_nt_bridge.h, vsl_nt_internal.h | vsl_nt_collision_test.c -- the NT dispatch registration contract. Modules write raw slots into a sha |
| `runtime/` | `vsl_nt_contract_test` | 70 | stdio.h, stdlib.h, string.h, vsl_nt_bridge.h, vsl_nt_internal.h | vsl_nt_contract_test.c -- every NT syscall must survive garbage. The unit tests exercise individual  |
| `runtime/` | `vsl_nt_enclave` | 146 | egojar.h, errno.h, memfd.h, stdint.h, stdlib.h, string.h, vsl_nt_bridge.h, vsl_n | vsl_nt_enclave.c -- Windows 11 Enclave (VBS/SGX) syscalls. Enclaves are secure memory regions for VB |
| `runtime/` | `vsl_nt_io` | 975 | pthread.h, statvfs.h, vsl_nt_internal.h | used in this TU, so declared here |
| `runtime/` | `vsl_nt_ioring` | 163 | egojar.h, egojar_net.h, errno.h, io_uring.h, stdint.h, stdlib.h, string.h, vsl_n | vsl_nt_ioring.c -- Windows 11 IoRing syscalls. IoRing is the modern async I/O ring buffer mechanism  |
| `runtime/` | `vsl_nt_job` | 230 | futex.h, vsl_nt_internal.h | used in this TU, so declared here |
| `runtime/` | `vsl_nt_ktm` | 621 | egojar.h, errno.h, stdint.h, stdlib.h, string.h, vsl_nt_bridge.h, vsl_nt_interna | vsl_nt_ktm.c -- Windows 11 KTM (Kernel Transaction Manager) syscalls. KTM provides transactional sup |
| `runtime/` | `vsl_nt_misc` | 1024 | egojar.h, egojar_net.h, futex.h, pthread.h, vsl_nt_internal.h, wubu_ntsync.h | vsl_nt_misc.c -- WuBuOS NT transliteration: miscellaneous syscalls (blitz). Implements remaining Rea |
| `runtime/` | `vsl_nt_misc_w11` | 889 | egojar.h, egojar_net.h, errno.h, sched.h, stdbool.h, stdint.h, string.h, time.h, | vsl_nt_misc_w11.c  --  Windows 11 (24H2) syscalls not in ReactOS 99 syscalls with real Linux/VSL imp |
| `runtime/` | `vsl_nt_partition` | 176 | egojar.h, errno.h, stdint.h, stdlib.h, string.h, vsl_nt_bridge.h, vsl_nt_interna | vsl_nt_partition.c -- Windows 11 Partition/CpuPartition syscalls. Partitions are lightweight isolati |
| `runtime/` | `vsl_nt_process` | 194 | vsl_nt_internal.h | vsl_nt_process.c -- WuBuOS NT transliteration: Process lifecycle. Split from vsl_nt_proc.c (which mi |
| `runtime/` | `vsl_nt_registry` | 557 | utime.h, vsl_nt_internal.h, wubu_fs_util.h | mkdir -p semantics: create every component of `path`, tolerating EEXIST. Returns 0 on success, -1 on |
| `runtime/` | `vsl_nt_section` | 122 | vsl_nt_internal.h | vsl_nt_section.c -- WuBuOS NT transliteration: Section (memory-mapped file) objects. Split from vsl_ |
| `runtime/` | `vsl_nt_sync` | 483 | pthread.h, semaphore.h, vsl_nt_internal.h | used in this TU, so declared here |
| `runtime/` | `vsl_nt_thread` | 171 | pthread.h, vsl_nt_internal.h | vsl_nt_thread.c -- WuBuOS NT transliteration: Thread lifecycle. Split from vsl_nt_proc.c (which mixe |
| `runtime/` | `vsl_nt_timer` | 81 | timerfd.h, vsl_nt_internal.h | vsl_nt_timer.c -- WuBuOS NT transliteration: Timer objects. Split from vsl_nt_proc.c (which mixed 5  |
| `runtime/` | `vsl_nt_token` | 570 | vsl_nt_internal.h | vsl_nt_token.c -- WuBuOS NT transliteration: Token / Security subsystem. Real privilege enforcement  |
| `runtime/` | `vsl_nt_vmem` | 207 | vsl_nt_internal.h | vsl_nt_vmem.c -- WuBuOS NT transliteration: Virtual Memory. Split from vsl_nt_proc.c (which mixed 5  |
| `runtime/` | `vsl_nt_wnf` | 303 | egojar.h, errno.h, stdint.h, stdlib.h, string.h, vsl_nt_bridge.h, vsl_nt_interna | vsl_nt_wnf.c -- Windows 11 WNF (Windows Notification Facility) syscalls. WNF is state-name based, sc |
| `runtime/` | `vsl_nt_worker` | 206 | egojar.h, errno.h, futex.h, pthread.h, stdatomic.h, stdint.h, stdlib.h, string.h | vsl_nt_worker.c -- Windows 11 Worker Factory syscalls. Worker factories are kernel-managed thread po |
| `runtime/` | `vsl_process` | 156 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, vsl_elf.h, vsl_internal.h, vsl_m | vsl_process.c  --  VSL Process Management Implementation |
| `runtime/` | `vsl_shared` | 31 | stdio.h, stdlib.h, string.h, vsl_internal.h, vsl_shared.h | vsl_shared.c  --  VSL Shared Memory Implementation |
| `runtime/` | `vsl_syscall` | 267 | vsl_syscall_internal.h | vsl_syscall.c  --  VSL Syscall Bridge Facade Table-driven dispatch + helper functions. Handler imple |
| `runtime/` | `vsl_syscall_cpm` | 350 | vsl_syscall_cpm_internal.h | vsl_syscall_cpm.c  --  VSL CP/M BDOS Syscall Personality (Core) Table-driven BDOS dispatch. Every ha |
| `runtime/` | `vsl_syscall_cpm_test` | 129 | egojar.h, stdio.h, stdlib.h, string.h, vsl_syscall_cpm_internal.h, vsl_syscall_i | vsl_syscall_cpm_test.c  --  CP/M BDOS personality regression test. Builds FCBs + a DMA buffer in pro |
| `runtime/` | `vsl_syscall_fileio` | 395 | vsl_syscall_internal.h | vsl_syscall_fileio.c  --  VSL File I/O Syscalls Read, write, open, close, lseek, stat, fstat, etc. |
| `runtime/` | `vsl_syscall_mac` | 53 | stdio.h, vsl_syscall_mac_internal.h | vsl_syscall_mac.c  --  VSL macOS (XNU) Syscall Dispatch (Core) Central entry point for macOS syscall |
| `runtime/` | `vsl_syscall_mac_bsd` | 869 | futex.h, vsl_syscall_mac_internal.h | used in this TU, so declared here |
| `runtime/` | `vsl_syscall_mac_mach` | 235 | semaphore.h, vsl_syscall_mac_internal.h | vsl_syscall_mac_mach.c  --  VSL macOS Mach Trap Handlers Mach trap handlers, port table, mach_msg, a |
| `runtime/` | `vsl_syscall_mac_test` | 229 | egojar.h, egojar_net.h, socket.h, stdbool.h, stdio.h, stdlib.h, string.h, vsl_ma | vsl_syscall_mac_test.c  --  macOS Syscall Dispatch Tests Tests for the VSL macOS syscall dispatch ta |
| `runtime/` | `vsl_syscall_macclassic` | 193 | vsl_syscall_macclassic_internal.h | vsl_syscall_macclassic.c  --  VSL Classic Mac OS (68K) Trap Personality (Core) Table-driven A-line t |
| `runtime/` | `vsl_syscall_macclassic_test` | 130 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, time.h, vsl_syscall_internal.h, | vsl_syscall_macclassic_test.c  --  Classic Mac OS (68K A-line) personality test. Drives trap words t |
| `runtime/` | `vsl_syscall_memory` | 113 | egojar.h, vsl_syscall_internal.h | vsl_syscall_memory.c  --  VSL Memory Management Syscalls mmap, munmap, brk, mprotect, msync, mremap, |
| `runtime/` | `vsl_syscall_net` | 583 | futex.h, timerfd.h, vsl_syscall_internal.h | vsl_syscall_net.c  --  VSL Socket, Signal, Timer, Namespace & Misc Syscalls Socket family, rt_sigact |
| `runtime/` | `vsl_syscall_nt` | 616 | egojar.h, errno.h, futex.h, limits.h, semaphore.h, signal.h, stdint.h, stdlib.h, | vsl_syscall_nt.c -- ReactOS NT syscall -> VSL transliteration layer (E1). FACADE of a decomposed dis |
| `runtime/` | `vsl_syscall_nt_ext_test` | 131 | egojar.h, errno.h, stdint.h, stdio.h, stdlib.h, string.h, vsl_nt_bridge.h, vsl_n | vsl_syscall_nt_ext_test.c -- Behavioral regression test for the NT 6.1/W11 extension personalities ( |
| `runtime/` | `vsl_syscall_nt_test` | 1668 | assert.h, egojar.h, egojar_net.h, errno.h, futex.h, stdint.h, stdio.h, string.h, | vsl_syscall_nt_test.c -- Regression test for E1: ReactOS NT syscall transliteration (first 10 syscal |
| `runtime/` | `vsl_syscall_proc` | 424 | sched.h, vsl_syscall_internal.h | vsl_syscall_proc.c  --  VSL Process Management + Identity/Credential Syscalls Fork, clone, exec, wai |
| `runtime/` | `wibu_mips_interp` | 165 | stdint.h, stdlib.h, string.h, wibu_mips_interp.h | wibu_mips_interp.c — the MIPS interpreter. Fixed: sllv/srlv use register shift amount. Added div/mod |
| `runtime/` | `win32_core` | 225 | string.h, vsl_nt_bridge.h, win32_core.h | win32_core.c -- see win32_core.h for the why and the scope. |
| `runtime/` | `wubu_ns_win32` | 65 | stdio.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h, wubu_ns_win32.h, | wubu_ns_win32.c -- WuBuOS /n control plane: win32k window-manager surface. /n/win32/registry -- full |
| `runtime/` | `wubu_ns_win32_test` | 85 | egojar.h, stdio.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h, wubu_n | wubu_ns_win32_test.c -- WuBuOS /n/win32 control-plane test. Drives the REAL win32k registry + the na |
| `runtime/` | `wubu_win32k` | 405 | egojar.h, stdio.h, string.h, wubu_win32k.h | wubu_win32k.c -- WuBuOS win32k-lite object model on the wubu_cap authority core. See wubu_win32k.h.  |
| `runtime/` | `wubwin32k_test` | 170 | egojar.h, stdio.h, string.h, wubu_cap.h, wubu_win32k.h | wubwin32k_test.c -- WuBuOS win32k-lite self-test (hosted gate). Exercises the REAL engine against th |
| `runtime/` | `wubu_6502_interp` | 303 | stdint.h, stdlib.h, string.h | wubu_6502_interp.c -- the MOS 6502 interpreter. Executes the bytes emitted by wubu_isa_6502.c (the 6 |
| `runtime/` | `wubu_account` | 84 | ctype.h, stdio.h, string.h, time.h, unistd.h, wubu_account.h, wubu_pwcrypt.h | wubu_account.c -- see wubu_account.h. |
| `runtime/` | `wubu_account_test` | 63 | stdio.h, string.h, wubu_account.h | wubu_account_test.c -- OOBE account-record gate. |
| `runtime/` | `wubu_agent` | 238 | stdio.h, stdlib.h, string.h, wubu_agent.h, wubu_ns_bridge_internal.h, wubu_sha25 | wubu_agent.c -- the OS-side agent identity (see wubu_agent.h). The canonical card line is the single |
| `runtime/` | `wubu_agent_test` | 147 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_agent.h, wubu_ns_bridg | wubu_agent_test.c -- prove the OS-side agent identity against the real federation contract (agent-pr |
| `runtime/` | `wubu_agi_play` | 234 | input.h, stdio.h, string.h, time.h, wubu_agi_play.h, wubu_game_session.h, wubu_k | wubu_agi_play.c -- the AGI PLAYS the games (the perception-action training loop). The second goal: " |
| `runtime/` | `wubu_agi_play_test` | 122 | input.h, stdio.h, string.h, wubu_agi_play.h, wubu_game_session.h, wubu_kvfs.h, w | wubu_agi_play_test.c -- the AGI-playing bridge test. Asserts: 1. the policy: hot/throttled -> strafe |
| `runtime/` | `wubu_anticheat` | 422 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_anticheat.h | we CAN do in userspace: |
| `runtime/` | `wubu_anticheat_test` | 238 | errno.h, stdio.h, stdlib.h, string.h, wubu_anticheat.h | wubu_anticheat_test.c  --  Tests for Anti-Cheat Module Cell 470: Anti-cheat research and stubs. |
| `runtime/` | `wubu_applaunch` | 41 | wubu_applaunch.h, wubu_zx.h, wubu_zxreg.h | wubu_applaunch.c  --  run a session app by loading its PE from /n. The P3<->module bridge: a wubu_sb |
| `runtime/` | `wubu_apps_test` | 258 | errno.h, stdio.h, stdlib.h, string.h, wubu_pkg.h, wubu_proton.h, wubu_vsl.h | wubu_apps_test.c  --  WuBuOS App-Level Components Test Suite Cell 107: Package manager (Flatpak-styl |
| `runtime/` | `wubu_arch` | 475 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_arch.h, wubu_spawn.h | wubu_arch.c  --  WuBuOS Arch Linux Bootstrap for Container Roots Cell 390: Arch as the NT-era kernel |
| `runtime/` | `wubu_arch_test` | 149 | assert.h, egojar.h, stdio.h, stdlib.h, string.h, wubu_arch.h, wubu_spawn.h | wubu_arch_test.c  --  Tests for Arch Bootstrap and FreeDoom Launcher Cell 390/391: Arch root managem |
| `runtime/` | `wubu_archd_daemon` | 405 | dirent.h, egojar.h, egojar_net.h, errno.h, ftw.h, libgen.h, socket.h, stdarg.h,  | wubu_archd_daemon.c -- WuBuOS archd daemon lifecycle + main entry. Extracted from the monolithic wub |
| `runtime/` | `wubu_archd_fs` | 107 | errno.h, stdio.h, stdlib.h, string.h, wubu_archd.h, wubu_archd_internal.h, wubu_ | wubu_archd_fs.c -- WuBuOS archd: recursive filesystem delete. Extracted from wubu_archd.c (separable |
| `runtime/` | `wubu_archd_loop` | 153 | egojar.h, egojar_net.h, string.h, wubu_archd_internal.h, wubu_archd_svc.h | wubu_archd_loop.c -- WuBuOS archd daemon: main event loop Self-contained request-dispatch concern sp |
| `runtime/` | `wubu_archd_svc` | 457 | dirent.h, egojar.h, errno.h, stdarg.h, stdio.h, stdlib.h, string.h, time.h, wubu | wubu_archd_svc.c -- WuBuOS archd pkg/repo/svc/aur/hook/health/gpu mgmt. Extracted from the monolithi |
| `runtime/` | `wubu_archd_svc_super` | 531 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, time.h, wubu_archd_svc.h | wubu_archd_svc_super.c -- Real in-process service supervisor. Closes N1 (popen arch-chroot systemctl |
| `runtime/` | `wubu_archd_test` | 215 | assert.h, egojar.h, egojar_net.h, stdio.h, string.h, wubu_archd.h | wubu_archd_test.c  --  Test suite for wubu_archd (Arch Linux Daemon) Tests daemon init, root lifecyc |
| `runtime/` | `wubu_archd_util` | 109 | wubu_archd_internal.h | wubu_archd_util.c -- Process / filesystem helper utilities for the Arch daemon. Self-contained: run_ |
| `runtime/` | `wubu_atspi` | 160 | string.h, wubu_atspi.h | wubu_atspi.c -- AT-SPI2: the accessibility object model. See wubu_atspi.h. |
| `runtime/` | `wubu_atspi_test` | 137 | stdio.h, string.h, wubu_atspi.h | wubu_atspi_test.c -- the AT-SPI2 accessibility object model. The load-bearing rule: role names are t |
| `runtime/` | `wubu_atspi_tree` | 126 | stdlib.h, string.h, wubu_atspi_tree.h | wubu_atspi_tree.c -- AT-SPI2: the accessibility tree walk. See wubu_atspi_tree.h. |
| `runtime/` | `wubu_atspi_tree_test` | 180 | stdio.h, string.h, wubu_atspi_tree.h | wubu_atspi_tree_test.c -- the AT-SPI2 tree walk. The load-bearing rules: a CYCLE is detected rather  |
| `runtime/` | `wubu_aur` | 192 | stdio.h, stdlib.h, string.h, wubu_aur.h, wubu_json.h | wubu_aur.c -- the AUR RPC client core. See wubu_aur.h.  The JSON is handled by the tree's existing p |
| `runtime/` | `wubu_aur_test` | 164 | stdio.h, string.h, wubu_aur.h | wubu_aur_test.c -- the AUR RPC client core. The load-bearing rules: a name with '&' or '+' must be E |
| `runtime/` | `wubu_auth` | 127 | stdlib.h, string.h, wubu_auth.h | wubu_auth.c -- polkit-equivalent authority (P3 §4.4). Subject -> action -> allow/deny/interactive ru |
| `runtime/` | `wubu_auth_test` | 101 | stdio.h, string.h, wubu_auth.h | wubu_auth_test.c -- P3 polkit-equivalent authority (Mint baseline §4.4/§154). Tests the SUBJECT -> A |
| `runtime/` | `wubu_boot_attest_edr` | 110 | stdio.h, string.h, wubu_attest.h, wubu_boot_attest_edr.h | wubu_boot_attest_edr.c -- put the measured boot in the EDR event stream. C11, minimal includes.  The |
| `runtime/` | `wubu_boot_attest_edr_test` | 135 | stdio.h, string.h, wubu_attest.h, wubu_boot_attest_edr.h | wubu_boot_attest_edr_test.c -- the boot must appear in the EDR stream, not just on the wire.  The po |
| `runtime/` | `wubu_boot_attest_relay` | 44 | string.h, wubu_boot_attest_relay.h | wubu_boot_attest_relay.c -- publish the sealed boot report over SDR. C11, minimal includes.  The rep |
| `runtime/` | `wubu_boot_attest_relay_test` | 154 | stdio.h, string.h, wubu_boot_attest.h, wubu_boot_attest_relay.h | wubu_boot_attest_relay_test.c -- a boot report must reach every peer the relay allows and none it do |
| `runtime/` | `wubu_bottle_flatpak` | 115 | ctype.h, dirent.h, egojar.h, egojar_net.h, errno.h, ftw.h, stdio.h, stdlib.h, st | wubu_bottle_flatpak.c -- WuBuOS Bottles/Lutris: Flatpak manifest + runtime detection. Split from wub |
| `runtime/` | `wubu_bottle_io` | 139 | ctype.h, dirent.h, egojar.h, errno.h, ftw.h, stdio.h, stdlib.h, string.h, time.h | wubu_bottle_io.c -- WuBuOS Bottles/Lutris: Bottles + Lutris format import/export. Split from wubu_bo |
| `runtime/` | `wubu_bottle_lifecycle` | 344 | ctype.h, dirent.h, egojar.h, errno.h, ftw.h, stdio.h, stdlib.h, string.h, time.h | wubu_bottle_lifecycle.c -- WuBuOS Bottles/Lutris: Bottle lifecycle + install/run. Split from wubu_bo |
| `runtime/` | `wubu_bottle_ops` | 98 | ctype.h, dirent.h, egojar.h, errno.h, ftw.h, stdio.h, stdlib.h, string.h, time.h | wubu_bottle_ops.c -- WuBuOS Bottles/Lutris: List + verify installed bottles. Split from wubu_bottles |
| `runtime/` | `wubu_bottle_serialize` | 159 | stdio.h, wubu_bottles_internal.h | wubu_bottle_serialize.c -- WuBuOS Bottles: JSON (de)serialization Self-contained serialization conce |
| `runtime/` | `wubu_bottles_fs` | 19 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_bottles.h, wubu_bottles_int | wubu_bottles_fs.c -- WuBuOS bottles: recursive filesystem delete. Extracted from wubu_bottles.c (sep |
| `runtime/` | `wubu_bottles_json` | 46 | wubu_bottles_internal.h | wubu_bottles_json.c -- Bottle JSON parsing helpers (self-contained). Pure JSON literal extraction: j |
| `runtime/` | `wubu_bottles_test` | 301 | egojar.h, stdio.h, stdlib.h, string.h, wubu_bottles.h | wubu_bottles_test.c  --  Tests for Bottles/Lutris Integration Cell 480: Bottles and Lutris compatibi |
| `runtime/` | `wubu_buddy_anim` | 148 | string.h, wubu_buddy_anim.h | wubu_buddy_anim.c -- WuBu Buddy emotion/pose deck engine. One concern: the COMBINATORIAL deck mathem |
| `runtime/` | `wubu_buddy_anim_test` | 59 | stdio.h, string.h, wubu_buddy_anim.h | Emotion tokens resolve to a composed render state. |
| `runtime/` | `wubu_buddy_emotion` | 237 | ctype.h, dirent.h, errno.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_ | wubu_buddy emotion core -- read machine state, expose a continuous affect. Sensor policy (all sysfs, |
| `runtime/` | `wubu_buddy_emotion_test` | 98 | stdio.h, stdlib.h, wubu_buddy_emotion.h | wubu_buddy emotion core -- behaviour gates. These are not assert-nothing checks: they prove the sens |
| `runtime/` | `wubu_buddy_orb` | 131 | math.h, stdlib.h, wubu_buddy_orb.h | wubu_buddy_orb.c -- wubu_buddy: live metaball orb (C11, opaque). The blob choreography is deliberate |
| `runtime/` | `wubu_buddy_orb_test` | 84 | math.h, stdio.h, stdlib.h, wubu_buddy_orb.h | wubu_buddy_orb_test.c -- gates for the live metaball orb. Asserts the orb is well-formed and actuall |
| `runtime/` | `wubu_buddy_rig` | 162 | string.h, wubu_buddy_rig.h, wubu_buddy_rig_data.h | wubu_buddy_rig.c -- WuBu Buddy pixel rig renderer (C11). One base sprite + anatomical part map (bake |
| `runtime/` | `wubu_buddy_rig_test` | 102 | stdint.h, stdio.h, string.h, wubu_buddy_rig.h, wubu_buddy_rig_data.h | wubu_buddy_rig_test.c -- gate for the WuBu Buddy pixel rig renderer. Asserts the baked rig actually  |
| `runtime/` | `wubu_bus` | 228 | stdlib.h, string.h, wubu_bus.h | wubu_bus.c -- message-bus implementation (P3 /n/background spine). See wubu_bus.h for the contract.  |
| `runtime/` | `wubu_bus_test` | 101 | stdio.h, string.h, wubu_bus.h | wubu_bus_test.c -- P3 spine: the D-Bus well-known-name ownership + on-demand activation contract (Mi |
| `runtime/` | `wubu_cap_handle` | 173 | egojar.h, stdint.h, stdlib.h, string.h, wubu_cap_internal.h | wubu_cap_handle.c -- WuBuOS per-process capability handle table. Userspace (or a WuBuOS agent proces |
| `runtime/` | `wubu_cap_object` | 279 | egojar.h, stdint.h, stdlib.h, string.h, wubu_cap_internal.h | wubu_cap_object.c -- WuBuOS capability core: registry + lifecycle + resolver. Ported from GrahaOS ke |
| `runtime/` | `wubu_cap_revoke` | 61 | stddef.h, stdint.h, stdlib.h, wubu_cap_internal.h | wubu_cap_revoke.c -- WuBuOS capability core revocation cascade. Ported from GrahaOS kernel/cap/revok |
| `runtime/` | `wubu_cap_system` | 65 | string.h, wubu_cap_internal.h | wubu_cap_system.c -- WuBuOS bootcap cascade root (extends wubu_cap). Adopted from GrahaOS kernel/cap |
| `runtime/` | `wubu_cap_test` | 151 | assert.h, stdio.h, string.h, wubu_cap.h, wubu_cap_internal.h | wubu_cap_test.c -- WuBuOS capability core self-test. Exercises the real engine (not stubs): create a |
| `runtime/` | `wubu_cap_token` | 47 | stddef.h, wubu_cap_internal.h | wubu_cap_token.c -- WuBuOS capability core token slow-path helpers. Ported from GrahaOS kernel/cap/t |
| `runtime/` | `wubu_colonel` | 154 | ctype.h, stdio.h, string.h, wubu_colonel.h | wubu_colonel.c -- the everything-through-the-Colonel dispatcher. C11. |
| `runtime/` | `wubu_compat_db` | 164 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_compat_db.h | wubu_compat_db.c -- WuBuOS per-app Windows-compat database (SteamOS ProtonDB + shader-cache lesson). |
| `runtime/` | `wubu_compat_db_test` | 71 | egojar.h, stdio.h, stdlib.h, string.h, wubu_compat_db.h | wubu_compat_db_test.c -- WuBuOS per-app compat DB tests (SteamOS ProtonDB + shader-cache lesson). Re |
| `runtime/` | `wubu_container` | 291 | stddef.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_crypto.h, wubu_ct_ | wubu_container.c  --  WuBuOS Universal Container Format Implementation .wubu: one extension, infinit |
| `runtime/` | `wubu_container_test` | 377 | holyd.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_exec.h, wubu_exec_i | wubu_container_test.c  --  WuBuOS .wubu Container Format Test Suite |
| `runtime/` | `wubu_ct_bwrap` | 180 | egojar.h, egojar_net.h, errno.h, limits.h, stdio.h, stdlib.h, string.h, wubu_hos | wubu_ct_bwrap.c  --  WuBuOS Bubblewrap Container Runtime (Hosted Demo) Cell 340/391: Container execu |
| `runtime/` | `wubu_ct_isolate` | 96 | audit.h, ct_iso_cgroup.h, ct_iso_ns.h, ct_iso_seccomp.h, egojar.h, errno.h, filt | wubu_ct_isolate.c  --  WuBuOS Container Isolation (cgroups v2 + seccomp) Cell 420: Security hardenin |
| `runtime/` | `wubu_ct_isolate_cgroup` | 131 | egojar.h, errno.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_ct_isolate.h | wubu_ct_isolate_cgroup.c -- Container cgroup operations subsystem. Self-contained: cgroup file read/ |
| `runtime/` | `wubu_daemon_client` | 121 | egojar.h, egojar_net.h, errno.h, socket.h, stdio.h, stdlib.h, string.h, wubu_dae | wubu_daemon_client.c  --  WuBuOS daemon wire client (shared) Extracted from daemon_panel.c so the ar |
| `runtime/` | `wubu_daemon_client_test` | 96 | egojar.h, egojar_net.h, stdio.h, string.h, wubu_daemon_client.h | wubu_daemon_client_test.c  --  behavior gate for the shared daemon wire client Exercises wubu_json_g |
| `runtime/` | `wubu_dbus` | 244 | stdlib.h, string.h, wubu_dbus.h | wubu_dbus.c -- the D-Bus wire format marshaller. See wubu_dbus.h.  All integers are appended LITTLE- |
| `runtime/` | `wubu_dbus_test` | 145 | stdio.h, string.h, wubu_dbus.h | wubu_dbus_test.c -- the D-Bus wire format marshaller. The load-bearing rules: every type has an ALIG |
| `runtime/` | `wubu_deskenv` | 112 | stdio.h, stdlib.h, string.h, wubu_deskenv.h | wubu_ns_de.c -- the /n/background/de DE-daemon framework surface (P3 §154). Tray-slot ownership (XAp |
| `runtime/` | `wubu_deskenv_test` | 99 | egojar.h, stdio.h, stdlib.h, string.h, wubu_deskenv.h | wubu_ns_de_test.c -- P3 /n/background/de DE-daemon framework surface (Mint baseline §154 / §5). Test |
| `runtime/` | `wubu_docs_test` | 153 | glob.h, stdio.h, stdlib.h, string.h | wubu_docs_test.c -- the docs must not lie. The user: "a lot of documents are very fluffy and out of  |
| `runtime/` | `wubu_doldoc` | 128 | string.h, wubu_doldoc.h | wubu_doldoc.c -- DolDoc: the TempleOS document format. See wubu_doldoc.h.  Every read is bounds-chec |
| `runtime/` | `wubu_doldoc_test` | 196 | stdio.h, string.h, wubu_doldoc.h | wubu_doldoc_test.c -- the DolDoc document parser. The load-bearing rules: a bad magic is a different |
| `runtime/` | `wubu_dos_emu` | 197 | wubu_dos_emu_internal.h | wubu_dos_emu.c -- WuBuOS 8086/DOS shim public API (create/destroy/load/run/capture). |
| `runtime/` | `wubu_dos_emu_alu` | 255 | wubu_dos_emu_internal.h | wubu_dos_emu_alu.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_decode` | 313 | wubu_dos_emu_internal.h | wubu_dos_emu_decode.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_int` | 211 | wubu_dos_emu_internal.h | wubu_dos_emu_int.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_mem` | 30 | wubu_dos_emu_internal.h | wubu_dos_emu_mem.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_regs` | 51 | wubu_dos_emu_internal.h | wubu_dos_emu_regs.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_smoke` | 49 | stdio.h, string.h, wubu_dos_emu.h | Build a tiny .COM by hand: mov ah, 0x09      ; DOS print '$'-terminated string mov dx, msg       ; o |
| `runtime/` | `wubu_dos_emu_test` | 307 | stdio.h, stdlib.h, string.h, wubu_dos_emu.h | wubu_dos_emu_test.c -- Unit tests for the in-process 8086 + DOS-INT shim. Verifies the real engine:  |
| `runtime/` | `wubu_dos_proc` | 224 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, time.h, wubu_container.h, wubu_d | wubu_dos_proc.c -- WuBuOS 16-bit DOS process (in-process 8086 compat shim). This is the "compatible  |
| `runtime/` | `wubu_dos_proc_test` | 104 | egojar.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_dos_proc.h | wubu_dos_proc_test.c -- Unit test for the in-process 16-bit DOS shim. Builds a real .COM in memory ( |
| `runtime/` | `wubu_dxvk_conf` | 164 | stdio.h, stdlib.h, string.h, wubu_dxvk_conf.h | wubu_dxvk_conf.c -- Shared DXVK config-file core. Implements the config read/write + line set/replac |
| `runtime/` | `wubu_ec_control` | 153 | stdint.h, stdlib.h, string.h | wubu_ec_control.c -- the HANDHELD EC (fan/thermal/power) controller. The Steam Deck (and the whole h |
| `runtime/` | `wubu_ec_control_test` | 87 | stdio.h, string.h, wubu_ec_control.h, wubu_test.h | wubu_ec_control_test.c -- the handheld EC controller test. A fake EC register file (a byte array + t |
| `runtime/` | `wubu_edr` | 17 | edr_internal.h | wubu_edr.c  --  WuBuOS EDR Engine Shim All implementation moved to src/runtime/edr/ subdirectory. Th |
| `runtime/` | `wubu_edr_agent_test` | 105 | assert.h, stdio.h, string.h, vbe.h, wm.h, wubu_edr.h, wubu_ui.h | wubu_edr_agent_test.c -- AGI activity -> EDR integration test. This is the proof of the transparency |
| `runtime/` | `wubu_edr_test` | 112 | assert.h, egojar.h, stdio.h, stdlib.h, string.h, time.h, wubu_edr.h | wubu_edr_test.c  --  WuBuOS EDR Engine Test Suite |
| `runtime/` | `wubu_effect` | 193 | stdlib.h, string.h, wubu_effect.h | gui_effect.c -- the effect stack and its passes. See gui_effect.h for the model.  Everything here is |
| `runtime/` | `wubu_effect_test` | 252 | gui_dbuf.h, stdio.h, stdlib.h, string.h, wubu_effect.h | wubu_effect_test.c -- the effect engine's gate. The passes are the part that must be exactly right:  |
| `runtime/` | `wubu_exec` | 466 | egojar.h, holyd.h, stdio.h, stdlib.h, string.h, wubu_exec.h, wubu_exec_internal. | wubu_exec.c  --  WuBuOS Universal Executable Dispatcher Implementation One exec to rule them all. |
| `runtime/` | `wubu_exec_container` | 93 | egojar.h, stdio.h, stdlib.h, string.h, wubu_exec.h, wubu_vsl.h | wubu_exec_container.c -- WuBuOS exec: container-payload execution. Extracted from wubu_exec.c (separ |
| `runtime/` | `wubu_exec_dos` | 76 | stdio.h, stdlib.h, string.h, wubu_dos_proc.h, wubu_exec_internal.h | wubu_exec_dos.c -- WuBuOS exec: DOS 16-bit (.COM/.EXE) backend. Extracted from wubu_exec.c (separabl |
| `runtime/` | `wubu_exec_format` | 64 | wubu_exec_internal.h | wubu_exec_format.c -- Payload format registry + detection (self-contained). wubu_payload_name / wubu |
| `runtime/` | `wubu_exec_games_test` | 126 | egojar.h, input.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_ct_isolat | wubu_exec_games_test.c — proves the era games RUN through our exec stack. - Quake 3 ELF   → wubu_exe |
| `runtime/` | `wubu_exec_macho` | 138 | egojar.h, stdbool.h, stdio.h, stdlib.h, string.h, wubu_exec.h, wubu_host_exec.h, | wubu_exec_macho.c -- WuBuOS exec: Mach-O backend (VSL native + Darling fallback). Validates Mach-O m |
| `runtime/` | `wubu_exec_wasm` | 55 | egojar.h, stdio.h, stdlib.h, string.h, wubu_exec.h, wubu_vsl.h | wubu_exec_wasm.c -- WuBuOS exec: WASM backend (wasmtime/wasm3/wasmer). Extracted from wubu_exec.c (s |
| `runtime/` | `wubu_fs_util` | 45 | egojar.h, errno.h, ftw.h, stdio.h, stdlib.h, string.h, wubu_fs_util.h, wubu_ftw. | wubu_fs_util.c -- shared filesystem utilities (dedup home). Canonical implementation of recursive-fo |
| `runtime/` | `wubu_game_launch` | 101 | stdint.h, stdio.h, string.h, wubu_exec.h, wubu_game_launch.h | wubu_game_launch.c -- the GAME LAUNCHER (one kernel, every OS's games). The user's three goals route |
| `runtime/` | `wubu_game_launch_test` | 56 | stdio.h, string.h, wubu_game_launch.h, wubu_test.h | wubu_game_launch_test.c -- the game launcher routing test. Asserts the format classification for the |
| `runtime/` | `wubu_game_launch_test_stub` | 31 | stddef.h, stdint.h | wubu_game_launch_test_stub.c -- the exec-backend stubs for the game-launch routing test. wubu_game_l |
| `runtime/` | `wubu_game_session` | 103 | egojar.h, stdio.h, string.h, time.h, wubu_game_session.h, wubu_world.h | wubu_game_session.c -- the GAME-PLAY TRAINING LEDGER (the AGI learns via running games). The AGI doc |
| `runtime/` | `wubu_game_session_test` | 79 | egojar.h, stdio.h, stdlib.h, string.h, wubu_game_session.h, wubu_test.h, wubu_wo | wubu_game_session_test.c -- the game-play training ledger test. Asserts: 1. a play session begin ->  |
| `runtime/` | `wubu_gamemode` | 234 | egojar.h, stdio.h, stdlib.h, string.h, wubu_hwdetect.h | wubu_gamemode.c -- the GAME MODE / PERF GOVERNOR engine. Closes the SteamOS EPIC E2 gap "Game mode / |
| `runtime/` | `wubu_gamemode_test` | 120 | egojar.h, stdio.h, stdlib.h, string.h, wubu_gamemode.h, wubu_test.h | wubu_gamemode_test.c -- the game mode / perf governor test. Builds a FAKE sysfs tree under /tmp: /tm |
| `runtime/` | `wubu_gc` | 202 | stdint.h, stdio.h, stdlib.h, string.h, wubu_gc.h | wubu_gc.c  --  Simple Mark-and-Sweep GC for WuBuOS Userspace Applets Opt-in garbage collector for Ho |
| `runtime/` | `wubu_gc_test` | 201 | stdint.h, stdio.h, string.h, wubu_gc.h | wubu_gc_test.c  --  Userspace GC Test Suite Tests mark-and-sweep for HolyC REPL / container applets. |
| `runtime/` | `wubu_gdpr_age` | 206 | stat.h, stdio.h, stdlib.h, string.h, strings.h, time.h, wubu_gdpr_age.h, wubu_uu | wubu_gdpr_age.c — GDPR-compliant age assurance for WuBuOS. Implemented per wubu_gdpr_age.h. No biome |
| `runtime/` | `wubu_gpu_backend` | 136 | dirent.h, egojar.h, stdio.h, stdlib.h, string.h, wubu_drv_gpu.h, wubu_gpu_backen | wubu_gpu_backend.c -- the GPU BACKEND DISPATCHER (the magic glue). "It should work on bare metal or  |
| `runtime/` | `wubu_gpu_driver_test` | 177 | cuda.h, egojar.h, stdio.h, stdlib.h, string.h | wubu_gpu_driver_test.c -- REAL GPU behaviour gate. The old wubu_cuda_selftest asserted tautologies ( |
| `runtime/` | `wubu_guest_root` | 38 | stdio.h, stdlib.h, string.h, wubu_guest_root.h | wubu_guest_root.c -- see wubu_guest_root.h. Default is ~/.local/share/wubu/guest: inside the user's  |
| `runtime/` | `wubu_gvfs` | 161 | string.h, wubu_gvfs.h | wubu_gvfs.c -- GVfs: the async file-operation state machine. See wubu_gvfs.h. |
| `runtime/` | `wubu_gvfs_mount` | 127 | stdio.h, string.h, wubu_gvfs_mount.h, wubu_gvfs_path.h | wubu_gvfs_mount.c -- GVfs: the mount table and path-to-mount resolution. See wubu_gvfs_mount.h. |
| `runtime/` | `wubu_gvfs_mount_test` | 131 | stdio.h, string.h, wubu_gvfs_mount.h | wubu_gvfs_mount_test.c -- the mount table and path-to-mount resolution. The load-bearing rule: LONGE |
| `runtime/` | `wubu_gvfs_path` | 155 | stdio.h, string.h, wubu_gvfs_path.h | wubu_gvfs_path.c -- GVfs: path normalization and resolution. See wubu_gvfs_path.h.  Paths are split  |
| `runtime/` | `wubu_gvfs_path_test` | 141 | stdio.h, string.h, wubu_gvfs_path.h | wubu_gvfs_path_test.c -- path normalization and resolution. The load-bearing rule: ".." at the ROOT  |
| `runtime/` | `wubu_gvfs_test` | 139 | stdio.h, string.h, wubu_gvfs.h | wubu_gvfs_test.c -- the async file-operation state machine. The load-bearing rules: a terminal handl |
| `runtime/` | `wubu_hc_eval_stub` | 13 | stdint.h | wubu_hc_eval_stub.c -- Minimal definition of hd_eval for runtime test binaries that link wubu_exec.c |
| `runtime/` | `wubu_holyd` | 162 | wubu_holyd_internal.h | wubu_holyd.c  --  WuBuOS TempleOS HolyC DOS Daemon (Facade) Manages HolyC DOS sessions: REPL, compil |
| `runtime/` | `wubu_holyd_9p` | 63 | wubu_holyd_internal.h | wubu_holyd_9p.c  --  WuBuOS HolyC DOS Daemon: 9P |
| `runtime/` | `wubu_holyd_agi` | 76 | stdio.h, stdlib.h, string.h, wubu_edr.h, wubu_holyd.h, holyc_eval.h | holyc_eval.c -- Live ring-0 compiler AGI layer (TempleOS "God compiler") See holyc_eval.h. O |
| `runtime/` | `wubu_holyd_agi_test` | 76 | stdio.h, string.h, wubu_edr.h, wubu_gdpr_age.h, wubu_holyd.h, holyc_eval.h | test_holyd_agi.c -- Live ring-0 compiler AGI layer test. Proves the TempleOS "God compiler" is now C |
| `runtime/` | `wubu_holyd_event` | 24 | wubu_holyd_internal.h | wubu_holyd_event.c  --  WuBuOS HolyC DOS Daemon: Event |
| `runtime/` | `wubu_holyd_exec` | 229 | ctype.h, stdbool.h, wubu_holyd_internal.h | gives the REPL its persistent state. */ |
| `runtime/` | `wubu_holyd_input` | 102 | wubu_holyd_internal.h | wubu_holyd_input.c  --  WuBuOS HolyC DOS Daemon: Input |
| `runtime/` | `wubu_holyd_lifecycle` | 333 | wubu_holyd_internal.h | wubu_holyd_lifecycle.c  --  WuBuOS HolyC DOS Daemon: Lifecycle |
| `runtime/` | `wubu_holyd_repl` | 242 | stdlib.h, string.h, wubu_holyd_internal.h | wubu_holyd_repl.c -- WuBuOS HolyC DOS Daemon: REPL + macro storage Self-contained REPL concern split |
| `runtime/` | `wubu_holyd_save` | 120 | wubu_holyd_internal.h | wubu_holyd_save.c  --  WuBuOS HolyC DOS Daemon: Save |
| `runtime/` | `wubu_holyd_session` | 123 | wubu_holyd_internal.h | wubu_holyd_session.c  --  WuBuOS HolyC DOS Daemon: Session |
| `runtime/` | `wubu_holyd_test` | 428 | assert.h, egojar.h, egojar_net.h, jit.h, stdio.h, stdlib.h, string.h, wubu_holyd | wubu_holyd_test.c  --  Test suite for wubu_holyd (TempleOS HolyC DOS Daemon) Tests daemon init, sess |
| `runtime/` | `wubu_holyd_window` | 136 | wubu_holyd_internal.h | wubu_holyd_window.c  --  WuBuOS HolyC DOS Daemon: Window |
| `runtime/` | `wubu_hook` | 106 | string.h, wubu_hook.h | wubu_hook.c -- mkinitcpio: the hook chain. See wubu_hook.h. |
| `runtime/` | `wubu_hook_test` | 184 | stdio.h, string.h, wubu_hook.h | wubu_hook_test.c -- the mkinitcpio hook chain. The load-bearing rules: stage order is FIXED (a hook  |
| `runtime/` | `wubu_host_exec` | 507 | egojar.h, egojar_net.h, errno.h, socket.h, stdio.h, stdlib.h, string.h, styxfs.h | wubu_host_exec.c  --  WuBuOS Host Container Execution (Linux) Cell 203: Fork+exec for .wubu containe |
| `runtime/` | `wubu_host_exec_test` | 272 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_host_exec.h | wubu_host_exec_test.c  --  Cell 203 Behavioral Test Suite Tests: fork+exec container creation, start |
| `runtime/` | `wubu_hw_fallback` | 26 | stdint.h | wubu_hw_fallback.c -- Minimal hardware-detection stubs. wubu_wine_env.c calls wubu_hw_is_wsl / wubu_ |
| `runtime/` | `wubu_hw_sensors_fallback` | 29 | stdint.h | wubu_hw_sensors_fallback.c -- hosted no-hardware shim for wubu_world.c sensor refs. wubu_world.c (ke |
| `runtime/` | `wubu_hwdetect` | 192 | egojar.h, stdio.h, string.h, wubu_hwdetect.h | wubu_hwdetect.c -- WuBuOS hardware-acceleration detection. C11. CPUID via inline asm (freestanding-s |
| `runtime/` | `wubu_image` | 552 | dirent.h, egojar.h, ftw.h, time.h, wubu_container.h, wubu_image.h, wubu_image_in | wubu_image.c  --  WuBuOS Container Image Builder Phase 7: .wubu image builder implementation - WuBuF |
| `runtime/` | `wubu_image_cache` | 58 | egojar.h, stdio.h, stdlib.h, string.h, wubu_image.h | wubu_image_cache.c -- Layer cache subsystem (self-contained). wubu_layer_cache_get/put/exists: on-di |
| `runtime/` | `wubu_image_manifest` | 163 | egojar.h, wubu_image_internal.h | wubu_image_manifest.c  --  WuBuOS Image Manifest JSON Operations Extracted from wubu_image.c (2026-0 |
| `runtime/` | `wubu_image_names` | 39 | wubu_image.h | wubu_image_names.c -- arch/OS name registry (the Revolver Doctrine). Split out of wubu_image.c so th |
| `runtime/` | `wubu_image_ops` | 236 | dirent.h, egojar.h, stdio.h, stdlib.h, string.h, wubu_image_internal.h, wubu_ima | wubu_image_ops.c  --  WuBuOS Image Tag/Remove/Inspect/Push/Pull Extracted from wubu_image.c (2026-07 |
| `runtime/` | `wubu_image_parse` | 306 | wubu_image_internal.h | wubu_image_parse.c  --  WuBuOS WuBuFile Parser Extracted from wubu_image.c (2026-07-06): all WuBuFil |
| `runtime/` | `wubu_image_tar` | 162 | dirent.h, egojar.h, ftw.h, time.h, wubu_ftw.h, wubu_gnu_compat.h, wubu_image.h,  | wubu_image_tar.c -- WuBuOS image builder: TAR layer writer. Extracted from wubu_image.c (separable l |
| `runtime/` | `wubu_json` | 161 | ctype.h, stdlib.h, string.h, wubu_json.h | wubu_json.c -- Minimal JSON parser. Self-contained module extracted from wubu_settings.c. Uses the s |
| `runtime/` | `wubu_kernel_xchg` | 227 | stdio.h, string.h, wubu_kernel_xchg.h | wubu_kernel_xchg.c -- the kernel interchange network. A registry of OS kernels with a versioned cont |
| `runtime/` | `wubu_launch_test` | 91 | hosted.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_ct_isolate.h, wubu | wubu_launch_test.c -- WuBuOS container/Proton launch + session-split tests. Validates Workstream B ( |
| `runtime/` | `wubu_logind` | 94 | stdlib.h, string.h, wubu_logind.h | wubu_logind.c -- logind-equivalent session registry + gated power ops (P3). C11, runtime host, singl |
| `runtime/` | `wubu_logind_test` | 91 | stdio.h, string.h, wubu_logind.h | wubu_logindd_test.c -- P3 logind-equivalent (Mint baseline §4.3 / §153). Register sessions (id/seat/ |
| `runtime/` | `wubu_manifest` | 156 | egojar.h, stdio.h, stdlib.h, string.h, wubu_manifest_internal.h | wubu_manifest.c -- WuBuOS unified manifest API (load/resolve/gate/emit). Adopted from GrahaOS etc/gc |
| `runtime/` | `wubu_manifest_json` | 221 | stdint.h, stdio.h, stdlib.h, string.h, wubu_manifest_internal.h | wubu_manifest_json.c -- minimal JSON parser for the WuBuOS manifest. Self-contained C11. Supports on |
| `runtime/` | `wubu_manifest_test` | 85 | stdio.h, string.h, wubu_manifest.h | wubu_manifest_test.c -- unit tests for the WuBuOS unified manifest. Proves: (1) JSON parses, (2) res |
| `runtime/` | `wubu_mips_interp` | 167 | stdint.h, stdlib.h, string.h | wibu_mips_interp.c — the MIPS interpreter. Fixed: sllv/srlv use register shift amount. Added div/mod |
| `runtime/` | `wubu_netlink` | 323 | egojar.h, egojar_net.h, errno.h, if.h, if_bridge.h, if_vlan.h, netlink.h, rtnetl | wubu_netlink.c  --  WuBuOS Netlink RTNETLINK Implementation Extracted from wubu_network.c (2026-07-0 |
| `runtime/` | `wubu_network` | 318 | egojar.h, egojar_net.h, errno.h, if.h, stdio.h, stdlib.h, string.h, time.h, wubu | wubu_network.c  --  WuBuOS Container Network Profiles (Real Implementation) Phase 7+: In-memory netw |
| `runtime/` | `wubu_network_cni` | 121 | egojar.h, stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_netw | wubu_network_cni.c -- WuBuOS network: CNI plugin load/exec backend. Extracted from wubu_network.c (s |
| `runtime/` | `wubu_network_create` | 271 | errno.h, stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_netwo | wubu_network_create.c -- WuBuOS network: network-create subsystem. Extracted from wubu_network.c (se |
| `runtime/` | `wubu_network_dns` | 46 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_dns.c -- WuBuOS network: DNS record management. Extracted from wubu_network.c (separabl |
| `runtime/` | `wubu_network_fw` | 41 | string.h, wubu_network.h, wubu_network_internal.h | wubu_network_fw.c -- Network firewall rule subsystem (self-contained). wubu_network_firewall_add_rul |
| `runtime/` | `wubu_network_qos` | 67 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_qos.c -- WuBuOS network: QoS / traffic shaping. Extracted from wubu_network.c (separabl |
| `runtime/` | `wubu_network_svc` | 46 | wubu_network_internal.h | wubu_network_svc.c -- Network port-mapping + DNS record subsystem. Self-contained: port map add/remo |
| `runtime/` | `wubu_network_test` | 532 | assert.h, stdio.h, stdlib.h, string.h, wubu_network.h | wubu_network_test.c  --  WuBuOS Container Network Profiles Test Suite Tests all network operations:  |
| `runtime/` | `wubu_network_ts` | 103 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_ts.c -- WuBuOS network: Tailscale up/down/status backend. Extracted from wubu_network.c |
| `runtime/` | `wubu_network_wg` | 79 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_wg.c -- WuBuOS network: WireGuard peer management backend. Extracted from wubu_network. |
| `runtime/` | `wubu_ns_9p_test` | 166 | egojar.h, stdio.h, stdlib.h, string.h, styx.h, styxfs.h, wubu_ns_bridge.h, wubu_ | wubu_ns_9p_test.c -- prove /n is served over REAL Styx/9P end-to-end. The bridge publishes control-p |
| `runtime/` | `wubu_ns_background` | 137 | stdio.h, wubu_ns_background.h | wubu_ns_background.c -- the /n/background control subtree (P3). Snapshots the session-boot's live st |
| `runtime/` | `wubu_ns_background_test` | 121 | egojar.h, egojar_net.h, errno.h, stdio.h, stdlib.h, string.h, wubu_ns_background | wubu_ns_background_test.c -- P3: the /n/background control subtree. Builds a session-boot, starts it |
| `runtime/` | `wubu_ns_bridge` | 141 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_archd.h, wubu_bottles_inter | wubu_ns_bridge.c -- WuBuOS Namespace Bridge: archd services + bottles exposed as a uniform 9P/Styx c |
| `runtime/` | `wubu_ns_bridge_test` | 176 | egojar.h, stdio.h, stdlib.h, string.h, wubu_bottles.h, wubu_ns_bridge.h, wubu_sp | wubu_ns_bridge_test.c -- verify the Namespace Bridge control plane. Uses dependency-injected mock se |
| `runtime/` | `wubu_ns_dram` | 106 | stdio.h, stdlib.h, string.h, wubu_dram_hedge.h, wubu_ns_bridge.h, wubu_ns_bridge | src/runtime/wubu_ns_dram.c The /n/dram control subtree — the DRAM-refresh hedge over the Styx/9P nam |
| `runtime/` | `wubu_ns_dram_test` | 88 | egojar.h, stdio.h, stdlib.h, string.h, wubu_dram_hedge.h, wubu_ns_bridge_interna | src/runtime/wubu_ns_dram_test.c The /n/dram subtree test. Asserts the file<->API routing over a REAL |
| `runtime/` | `wubu_ns_ec` | 95 | stdio.h, stdlib.h, string.h, wubu_ec_control.h, wubu_ns_bridge.h, wubu_ns_bridge | wubu_ns_ec.c -- the /n/ec control subtree (the handheld EC over the Styx/9P namespace). The OS-sourc |
| `runtime/` | `wubu_ns_ec_test` | 101 | egojar.h, stdio.h, string.h, wubu_ec_control.h, wubu_ns_bridge.h, wubu_ns_bridge | wubu_ns_ec_test.c -- the /n/ec subtree test. Asserts the file<->API routing over a REAL ns bridge ro |
| `runtime/` | `wubu_ns_effect` | 96 | stdio.h, stdlib.h, string.h, wubu_effect.h, wubu_ns_bridge_internal.h | wubu_ns_effect.c -- the /n/effect control subtree. /n/effect/list   -> the effect names a program ca |
| `runtime/` | `wubu_ns_fs` | 74 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_archd.h, wubu_ns_bridge_int | wubu_ns_fs.c -- WuBuOS Namespace Bridge: filesystem core. The shared /n tree primitives: the root ha |
| `runtime/` | `wubu_ns_kernel` | 124 | stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h | wubu_ns_kernel.c -- WuBuOS Namespace Bridge: kernel + hw control plane (rip off CachyOS kernel-manag |
| `runtime/` | `wubu_ns_kernel_test` | 87 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_spaw | wubu_ns_kernel_test.c -- verify the kernel + hw control plane (rip off CachyOS kernel-manager / chwd |
| `runtime/` | `wubu_ns_kernels` | 41 | stdio.h, wubu_kernel_xchg.h, wubu_ns_bridge_internal.h, wubu_ns_kernels.h | wubu_ns_kernels.c -- the /n/kernels control subtree. See wubu_ns_kernels.h.  The subtree is rendered |
| `runtime/` | `wubu_ns_kernels_test` | 81 | stdio.h, string.h, wubu_kernel_xchg.h, wubu_ns_bridge_internal.h, wubu_ns_kernel | wubu_ns_kernels_test.c -- the /n/kernels subtree test. Proves the namespace is a real window on the  |
| `runtime/` | `wubu_ns_kv` | 106 | stdio.h, stdlib.h, string.h, wubu_kvfs.h, wubu_ns_bridge_internal.h, wubu_ns_kv. | wubu_ns_kv.c -- the /n/kv control subtree (the AGI's KV cache namespace). /n/kv/snapshot  -> JSON vi |
| `runtime/` | `wubu_ns_kv_test` | 99 | egojar.h, stdio.h, stdlib.h, string.h, wubu_kvfs.h, wubu_ns_bridge_internal.h, w | wubu_ns_kv_test.c -- prove the /n/kv subtree is publishable and that the tick paths a 9P client woul |
| `runtime/` | `wubu_ns_pkg` | 106 | stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h, wubu_p | wubu_ns_pkg.c -- WuBuOS Namespace Bridge: packages as a 9P control plane (rip off pacman/Chaotic-AUR |
| `runtime/` | `wubu_ns_pkg_fallback` | 35 | stdint.h | wubu_ns_pkg_fallback.c -- Minimal definitions of the namespace + package runtime symbols that GUI te |
| `runtime/` | `wubu_ns_pkg_test` | 94 | egojar.h, stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_pkg.h, wubu_spawn. | wubu_ns_pkg_test.c -- verify the package control plane (rip off pacman/Chaotic-AUR through /n). Driv |
| `runtime/` | `wubu_ns_session` | 44 | stdio.h, string.h, wubu_ns_bridge_internal.h, wubu_session.h | wubu_ns_session.c -- the /n/session control subtree (the SteamOS session-select control plane). /n/s |
| `runtime/` | `wubu_ns_session_test` | 67 | stdio.h, string.h, wubu_ns_bridge_internal.h, wubu_ns_session.h, wubu_session.h, | wubu_ns_session_test.c -- the /n/session subtree test. Asserts the file<->API routing: 1. publish cr |
| `runtime/` | `wubu_ns_snap` | 119 | stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h, wubu_s | wubu_ns_snap.c -- WuBuOS Namespace Bridge: snapshots as a 9P control plane (rip off snapper/btrfs ro |
| `runtime/` | `wubu_ns_snap_test` | 115 | egojar.h, stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_snapshot.h, wubu_s | wubu_ns_snap_test.c -- verify the snapshot control plane (rip off snapper/btrfs rollback through /n) |
| `runtime/` | `wubu_ns_steaminput` | 86 | stdio.h, stdlib.h, string.h, wubu_ns_bridge_internal.h, wubu_steaminput.h | wubu_ns_steaminput.c -- the /n/steaminput control subtree. The OS-source-steal thesis applied to Ste |
| `runtime/` | `wubu_ns_steaminput_test` | 89 | input.h, stdio.h, string.h, wubu_ns_bridge_internal.h, wubu_ns_steaminput.h, wub | wubu_ns_steaminput_test.c -- the /n/steaminput subtree test. Asserts the file<->API routing: 1. publ |
| `runtime/` | `wubu_ns_steamrt` | 93 | stdio.h, stdlib.h, string.h, wubu_ns_bridge_internal.h, wubu_steamrt.h | wubu_ns_steamrt.c -- the /n/steamrt control subtree (the sniper runtime + Proton launch integration) |
| `runtime/` | `wubu_ns_steamrt_test` | 80 | stdio.h, string.h, wubu_ns_bridge_internal.h, wubu_ns_steamrt.h, wubu_steamrt.h, | wubu_ns_steamrt_test.c -- the /n/steamrt subtree test. Asserts the file<->API routing: 1. publish cr |
| `runtime/` | `wubu_ns_world` | 54 | stdio.h, wubu_ns_bridge_internal.h, wubu_world.h | wubu_ns_world.c -- the /n/world control subtree (the AGI's eyes). /n/world/state  -> the one-line wo |
| `runtime/` | `wubu_ns_worldspace` | 109 | stdio.h, stdlib.h, string.h, wubu_agent.h, wubu_ns_bridge_internal.h, wubu_ns_wo | wubu_ns_worldspace.c -- the /n/worldspace control subtree. The ~WuBu~ space engine (the WWW submodul |
| `runtime/` | `wubu_ns_worldspace_test` | 121 | egojar.h, stdio.h, stdlib.h, string.h, wubu_ns_bridge_internal.h, wubu_world.h | wubu_ns_worldspace_test.c -- prove the /n/worldspace subtree publishes the body's space-facing state |
| `runtime/` | `wubu_ntsync` | 160 | errno.h, pthread.h, stdlib.h, string.h, time.h, wubu_ntsync.h | Keyed-event object: a small fixed table of per-key slots, each the rendezvous point for exactly one  |
| `runtime/` | `wubu_ntsync_test` | 93 | pthread.h, stdio.h, stdlib.h, unistd.h, wubu_ntsync.h | wubu_ntsync_test.c -- gate for the keyed-event pairing primitive. Proves: held-release, block-then-r |
| `runtime/` | `wubu_oci_test` | 249 | egojar.h, stdio.h, stdlib.h, string.h, time.h, wubu_image.h, wubu_oci.h | wubu_oci_test.c  --  Tests for OCI runtime |
| `runtime/` | `wubu_onboarding` | 60 | stdio.h, string.h, wubu_onboarding.h | wubu_onboarding.c -- P4 first-boot onboarding decision + sentinel persistence. A first boot (no sent |
| `runtime/` | `wubu_palette` | 137 | stddef.h, wubu_palette.h | wubu_palette.c -- see wubu_palette.h. |
| `runtime/` | `wubu_palette_test` | 142 | stdio.h, string.h, wubu_palette.h | wubu_palette_test.c -- the wallpaper-seed extractor against known images. Each case is a synthetic i |
| `runtime/` | `wubu_pkg` | 332 | stdio.h, string.h, wubu_pkg.h | wubu_pkg.c  --  WuBuOS Package Manager Implementation |
| `runtime/` | `wubu_pkg_resolve_test` | 225 | stdio.h, string.h, wubu_pkg.h | wubu_pkg_resolve_test.c -- versions, constraints, dependency resolution. The load-bearing rules: com |
| `runtime/` | `wubu_pkgbuild` | 509 | ctype.h, stdio.h, stdlib.h, string.h, wubu_pkgbuild.h | wubu_pkgbuild.c -- the PKGBUILD parser. See wubu_pkgbuild.h.  The parser is a small state machine ov |
| `runtime/` | `wubu_pkgbuild_test` | 173 | stdio.h, string.h, wubu_pkgbuild.h | wubu_pkgbuild_test.c -- the PKGBUILD parser. The load-bearing rules: NOTHING IS EXECUTED (a '$(...)' |
| `runtime/` | `wubu_posix` | 360 | syscall.h, unistd.h, wubu_posix.h | wubu_posix -- our own POSIX surface, issued as raw Linux syscalls. Freestanding: no libc header is i |
| `runtime/` | `wubu_pressure_vessel` | 169 | egojar.h, egojar_net.h, stddef.h, stdio.h, stdlib.h, string.h, wubu_host_exec.h | wubu_pressure_vessel.c -- the PRESSURE-VESSEL preset. Closes the SteamOS EPIC E2 gap "Pressure Vesse |
| `runtime/` | `wubu_pressure_vessel_test` | 78 | stdio.h, string.h, wubu_host_exec.h, wubu_pressure_vessel.h, wubu_test.h | wubu_pressure_vessel_test.c -- the pressure-vessel preset test. Asserts the SteamOS runtime-containe |
| `runtime/` | `wubu_proton` | 400 | egojar.h, egojar_net.h, errno.h, stdio.h, stdlib.h, string.h, wubu_dxvk_conf.h,  | wubu_proton.c  --  WuBuOS Proton: Windows Compatibility Layer Implementation Cell 092: Proton-style  |
| `runtime/` | `wubu_proton2` | 348 | dirent.h, egojar.h, egojar_net.h, errno.h, input.h, stdio.h, stdlib.h, string.h, | wubu_proton2.c  --  WuBuOS Proton: Real Wine/Proton Container Cell 399: Proton runs as an Arch Linux |
| `runtime/` | `wubu_proton2_device` | 148 | dirent.h, egojar.h, errno.h, input.h, stdio.h, stdlib.h, string.h, uinput.h, wub | wubu_proton2_device.c -- WuBuOS proton2: HID/USB/MIDI device enumeration. Extracted from wubu_proton |
| `runtime/` | `wubu_proton2_gamescope` | 100 | stdio.h, string.h, wubu_proton2.h | wubu_proton2_gamescope.c -- WuBuOS proton2: gamescope/wine launch-command builder. Extracted from wu |
| `runtime/` | `wubu_proton2_gpu` | 61 | dirent.h, egojar.h, stdio.h, stdlib.h, string.h, wubu_proton2.h | wubu_proton2_gpu.c -- GPU detection subsystem (self-contained). wubu_gpu_detect / wubu_gpu_open: sca |
| `runtime/` | `wubu_proton2_launch` | 67 | egojar.h, stdio.h, string.h, wubu_proton2.h | wubu_proton2_launch.c -- WuBuOS proton2: app process launch/control. Extracted from wubu_proton2.c ( |
| `runtime/` | `wubu_proton2_test` | 307 | egojar.h, stdio.h, string.h, wubu_proton2.h | wubu_proton2_test.c  --  Tests for Proton container + HID/USB + GPU Cell 399: Proton runs as real Ar |
| `runtime/` | `wubu_proton_api` | 138 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_proton.h | wubu_proton_api.c -- WuBuOS Proton Win32->VSL API translation subsystem Extracted from wubu_proton.c |
| `runtime/` | `wubu_proton_dll` | 53 | egojar.h, string.h, strings.h, wubu_proton.h | wubu_proton_dll.c -- Proton DLL management subsystem (self-contained). wubu_proton_register_dll/find |
| `runtime/` | `wubu_proton_dxvk` | 172 | stdio.h, stdlib.h, string.h, wubu_dxvk_conf.h, wubu_proton_dxvk.h | wubu_proton_dxvk.c -- canonical Proton DXVK/VKD3D config core (dedup home). Writes/reads the per-pre |
| `runtime/` | `wubu_proton_pe` | 114 | string.h, wubu_proton.h | wubu_proton_pe.c -- PE (Portable Executable) validation/parsing subsystem. Self-contained: wubu_prot |
| `runtime/` | `wubu_proton_test` | 777 | stdbool.h, stdio.h, stdlib.h, string.h, wubu_proton.h | wubu_proton_test.c  --  Test Suite for WuBuOS Proton (Windows Compat Layer) Cell 092: Tests PE valid |
| `runtime/` | `wubu_pwcrypt` | 282 | stdint.h, string.h, wubu_pwcrypt.h | wubu_pwcrypt.c -- SHA-512 crypt ($6$), the WuBuOS password-hash primitive. omarchy provisions its us |
| `runtime/` | `wubu_pwcrypt_test` | 52 | stdio.h, string.h, wubu_pwcrypt.h | wubu_pwcrypt_test.c -- SHA-512 crypt ($6$) gate vs the canonical vectors. The vectors here are the r |
| `runtime/` | `wubu_ramdisk` | 343 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_arch.h, wubu_ramdisk.h, wub | wubu_ramdisk.c  --  WuBuOS Root Mount for Arch Container Roots Cell 392: Two-mode Arch root  --  RAM |
| `runtime/` | `wubu_ramdisk_format` | 42 | egojar.h, string.h, wubu_ramdisk.h, wubu_ramdisk_internal.h | wubu_ramdisk_format.c -- WuBuOS ramdisk: image format sniffing. Extracted from wubu_ramdisk.c (separ |
| `runtime/` | `wubu_ramdisk_test` | 207 | assert.h, egojar.h, stdio.h, stdlib.h, string.h, wubu_ramdisk.h | wubu_ramdisk_test.c  --  Tests for Two-Mode Root Mount Cell 392: RAM for containers, SSD for bare me |
| `runtime/` | `wubu_realm` | 115 | stdio.h, stdlib.h, string.h, wubu_archd_svc.h, wubu_realm.h | wubu_realm.c -- Mega OS realm abstraction (Phase B + D). Lifecycle via the supervisor (N1-N4/N8/N9); |
| `runtime/` | `wubu_redsea` | 162 | string.h, wubu_redsea.h | wubu_redsea.c -- RedSea: the TempleOS filesystem (ISO-9660 derived). See wubu_redsea.h. |
| `runtime/` | `wubu_redsea_test` | 237 | stdio.h, string.h, wubu_redsea.h | wubu_redsea_test.c -- the RedSea filesystem structures. The load-bearing rule: a BOTH-ENDIAN field i |
| `runtime/` | `wubu_revolver` | 92 | errno.h, limits.h, stdio.h, stdlib.h, string.h, wubu_revolver.h | wubu_revolver.c -- see wubu_revolver.h. One owner for the whole tree: every knob that must be adjust |
| `runtime/` | `wubu_rim` | 190 | math.h, stdlib.h, string.h, wubu_rim.h | wubu_rim.c -- see wubu_rim.h. The rim is a closed loop.  Perimeter is split into four legs: [0, w)   |
| `runtime/` | `wubu_rim_test` | 199 | math.h, stdio.h, string.h, wubu_rim.h | wubu_rim_test.c -- the rim loop and the islands that travel around it. |
| `runtime/` | `wubu_riscv_interp` | 268 | stdint.h, stdio.h, stdlib.h, string.h | wubu_riscv_interp.c -- the RISC-V RV64I interpreter. Executes the bytes emitted by wubu_isa_riscv.c  |
| `runtime/` | `wubu_runtime` | 329 | stdio.h, stdlib.h, string.h, time.h, wubu_runtime.h | wubu_runtime.c -- WUBURUNTIME: the compilation-space registry. Every OO runtime gets its own compila |
| `runtime/` | `wubu_runtime_personalities` | 174 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_runtime.h | wubu_runtime_personalities.c -- WUBURUNTIME Wave 3 (the gap filler). Every OO runtime speaks syscall |
| `runtime/` | `wubu_runtime_test` | 227 | egojar.h, stdio.h, stdlib.h, string.h, wubu_runtime.h | test_runtime.c -- WUBURUNTIME test (research/063, Wave 1). The DA oracles: 1. create/read round-trip |
| `runtime/` | `wubu_sdr` | 227 | string.h, wubu_sdr.h | wubu_sdr.c -- Steam Datagram Relay: routing tickets and virtual routes. See wubu_sdr.h. |
| `runtime/` | `wubu_sdr_relay` | 101 | string.h, wubu_sdr_relay.h | wubu_sdr_relay.c -- the SDR relay decision (fail closed). See wubu_sdr_relay.h. |
| `runtime/` | `wubu_sdr_relay_test` | 161 | stdio.h, string.h, wubu_sdr_relay.h | wubu_sdr_relay_test.c -- the relay decision and the rate limiter. The load-bearing rule: a relay is  |
| `runtime/` | `wubu_sdr_session` | 153 | string.h, wubu_sdr_session.h | wubu_sdr_session.c -- the SDR POP-side session layer. See wubu_sdr_session.h. |
| `runtime/` | `wubu_sdr_session_test` | 140 | stdio.h, string.h, wubu_sdr_session.h | wubu_sdr_session_test.c -- the POP-side session layer. The load-bearing rule: a virtual port is NEVE |
| `runtime/` | `wubu_sdr_test` | 186 | stdio.h, string.h, wubu_sdr.h | wubu_sdr_test.c -- SDR routing tickets and the virtual route table. The load-bearing rules: a ticket |
| `runtime/` | `wubu_secmon` | 182 | egojar.h, stdio.h, stdlib.h, string.h, wubu_kvfs.h, wubu_secmon.h | wubu_secmon.c — the kernel's syscall camera (call interception for the AGI). Implements the ptrace-s |
| `runtime/` | `wubu_secmon_selftest` | 99 | egojar.h, stdarg.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_kvfs.h, wubu_sec | wubu_secmon_selftest.c — proves the syscall camera sees game calls. 1. init KV-FS (the substrate the |
| `runtime/` | `wubu_selfimprove` | 96 | stdlib.h, string.h, wubu_selfimprove.h | wubu_selfimprove.c -- Mega OS self-improvement loop (Phase C). Independent verifier + human gate + f |
| `runtime/` | `wubu_session` | 185 | egojar.h, stdio.h, stdlib.h, string.h, wubu_session.h | wubu_session.c -- the SESSION MANAGER (the SteamOS desktop-space steal). SteamOS runs two sessions a |
| `runtime/` | `wubu_session_boot` | 118 | stdio.h, stdlib.h, string.h, wubu_session_boot.h | wubu_session_boot.c -- P3 session-boot orchestrator. Ties wubu_xdg_autostart (scan) + wubu_bus (owne |
| `runtime/` | `wubu_session_boot_test` | 96 | egojar.h, egojar_net.h, stdio.h, stdlib.h, string.h, wubu_session_boot.h | wubu_session_boot_test.c -- P3 session-boot orchestrator: the cinnamon-session analogue that owns th |
| `runtime/` | `wubu_session_daemon` | 121 | stdlib.h, wubu_ns_background.h, wubu_ns_effect.h, wubu_session_daemon.h | wubu_session_daemon.c -- P3 session-manager daemon (the role-up). Composition over the five shipped  |
| `runtime/` | `wubu_session_daemon_test` | 132 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_auth.h, wubu_logind.h, wubu | wubu_session_daemon_test.c -- P3 session-manager daemon (the role-up). Composes the five contracts:  |
| `runtime/` | `wubu_session_mgr` | 83 | stdio.h, stdlib.h, string.h, wubu_auth.h, wubu_logind.h, wubu_ns_background.h, w | wubu_session_mgr.c -- P3 session-boot glue (see wubu_session_mgr.h). Brings up the rich background:  |
| `runtime/` | `wubu_session_mgr_test` | 120 | egojar.h, stdio.h, stdlib.h, string.h, wubu_session_mgr.h | wubu_session_mgr_test.c -- P3 SESSION-BOOT GLUE integration test. Boots the whole /n/background rich |
| `runtime/` | `wubu_session_test` | 62 | stdio.h, string.h, wubu_session.h, wubu_test.h | wubu_session_test.c -- the session manager test. Asserts the SteamOS session-select contract: 1. ini |
| `runtime/` | `wubu_slime` | 167 | stdlib.h, string.h, wubu_slime.h | wubu_slime.c -- see wubu_slime.h. Each blob is tied by a spring to a PINNED ANCHOR sitting at its ho |
| `runtime/` | `wubu_slime_test` | 150 | math.h, stdio.h, string.h, wubu_slime.h | wubu_slime_test.c -- the seam's behaviour against known answers. A blob must rest on its rim home, s |
| `runtime/` | `wubu_snapshot` | 494 | dirent.h, egojar.h, errno.h, ftw.h, limits.h, stdio.h, stdlib.h, string.h, time. | wubu_snapshot.c  --  WuBuOS Container Snapshot Manager (Real Implementation) Phase 7+: In-memory sna |
| `runtime/` | `wubu_snapshot_copy` | 168 | dirent.h, egojar.h, errno.h, ftw.h, limits.h, stdio.h, stdlib.h, string.h, time. | wubu_snapshot_copy.c -- WuBuOS snapshot manager: recursive fs copy + dir size. Extracted from wubu_s |
| `runtime/` | `wubu_snapshot_diff` | 32 | wubu_snapshot.h, wubu_snapshot_internal.h | wubu_snapshot_diff.c -- Snapshot diff subsystem (self-contained). wubu_snapshot_diff: textual diff b |
| `runtime/` | `wubu_snapshot_fs` | 232 | btrfs.h, egojar.h, egojar_net.h, errno.h, fs.h, libgen.h, stdio.h, stdlib.h, str | wubu_snapshot_fs.c  --  WuBuOS Filesystem Native Snapshot Operations Extracted from wubu_snapshot.c  |
| `runtime/` | `wubu_snapshot_gc` | 126 | string.h, wubu_snapshot.h, wubu_snapshot_internal.h | wubu_snapshot_gc.c -- WuBuOS snapshot: garbage collection + retention rules. Extracted from wubu_sna |
| `runtime/` | `wubu_snapshot_tag` | 54 | wubu_snapshot_internal.h | wubu_snapshot_tag.c -- Snapshot tag operations subsystem. Self-contained: tag create/delete/list. Us |
| `runtime/` | `wubu_snapshot_test` | 465 | assert.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_snapshot.h, wubu_ | wubu_snapshot_test.c  --  WuBuOS Container Snapshot Manager Test Suite Tests all snapshot operations |
| `runtime/` | `wubu_snapshot_xport` | 98 | stdio.h, stdlib.h, string.h, wubu_snapshot.h, wubu_snapshot_internal.h | wubu_snapshot_xport.c -- WuBuOS snapshot: tarball export/import. Extracted from wubu_snapshot.c (sep |
| `runtime/` | `wubu_spawn` | 82 | egojar.h, errno.h, stdlib.h, wubu_spawn.h | wubu_spawn.c  --  Shell-free external program launcher. See wubu_spawn.h. Dependency-free: only POSI |
| `runtime/` | `wubu_spawn_test` | 99 | egojar.h, stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_spawn.h | wubu_spawn_test.c  --  Regression test for the shell-free launcher. Asserts wubu_run_program() actua |
| `runtime/` | `wubu_spell_cli` | 150 | stdio.h, stdlib.h, string.h, wubu_spell.h | wubu_spell_cli.c -- the spell-book command line. wubu_spell list   [book] [genre]   list spells in t |
| `runtime/` | `wubu_spell_resolve` | 219 | stdint.h, stdlib.h, string.h, wubu_spell_resolve.h | wubu_spell_resolve.c -- see wubu_spell_resolve.h for why this exists. Pure graph work over borrowed  |
| `runtime/` | `wubu_spell_resolve_test` | 181 | spellmod_test_img.h, stdio.h, stdlib.h, string.h, wubu_spell_resolve.h, wubu_spe | wubu_spell_resolve_test.c -- unit gate for the .spell dependency resolver. Builds real .spell images |
| `runtime/` | `wubu_spell_scan` | 106 | dirent.h, stdio.h, stdlib.h, string.h, wubu_spell_resolve.h | wubu_spell_scan.c -- directory discovery for .spell modules. WHY A SEPARATE FILE wubu_spell_resolve. |
| `runtime/` | `wubu_spell_scan_test` | 153 | spellmod_test_img.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_spell_r | wubu_spell_scan_test.c -- gate for .spell directory discovery. Discovery is the half that was missin |
| `runtime/` | `wubu_spellbook` | 472 | dirent.h, stdarg.h, stdio.h, stdlib.h, string.h, wubu_spell.h | wubu_spellbook.c -- scan, validate, resolve and cast spells. A spell book is a directory tree: <root |
| `runtime/` | `wubu_spellbook_test` | 312 | egojar.h, stdio.h, stdlib.h, string.h, wubu_spell.h | wubu_spellbook_test.c -- the spell book must be honest about its contents. Every assertion here is a |
| `runtime/` | `wubu_spellmod` | 197 | stdlib.h, string.h, wubu_spellmod.h | wubu_spellmod.c -- see wubu_spellmod.h for why this is the compiled form of a `.spell` rather than a |
| `runtime/` | `wubu_spellmod_test` | 196 | stdio.h, stdlib.h, string.h, wubu_spellmod.h | wubu_spellmod_test.c -- the .spell container contract. Builds real images in memory and checks both  |
| `runtime/` | `wubu_spring` | 194 | math.h, stdlib.h, string.h, wubu_spring.h | wubu_spring.c -- see wubu_spring.h. Verlet + Jakobsen relaxation.  No sqrt in the hot loop where it  |
| `runtime/` | `wubu_spring_test` | 245 | math.h, stdio.h, string.h, wubu_spring.h | wubu_spring_test.c -- the soft-body solver against known behaviour. Every case checks a property wit |
| `runtime/` | `wubu_steaminput` | 454 | input.h, stdio.h, stdlib.h, string.h | wubu_steaminput.c -- the STEAM INPUT layer. Closes the SteamOS EPIC E2 gap "Steam Input (controller  |
| `runtime/` | `wubu_steaminput_test` | 151 | input.h, stdio.h, stdlib.h, string.h, wubu_steaminput.h, wubu_test.h | wubu_steaminput_test.c -- the Steam Input layer test. Asserts: 1. the default config maps A->Space,  |
| `runtime/` | `wubu_steamrt` | 190 | egojar.h, stdio.h, stdlib.h, string.h, wubu_steamrt.h | wubu_steamrt.c -- the STEAM RUNTIME (sniper) integration driver. SteamOS runs every Proton game insi |
| `runtime/` | `wubu_steamrt_test` | 93 | stdio.h, string.h, wubu_steamrt.h, wubu_test.h | wubu_steamrt_test.c -- the Steam Runtime (sniper) integration test. Asserts (against the real Proton |
| `runtime/` | `wubu_svc_socket` | 108 | egojar.h, egojar_net.h, errno.h, socket.h, stdio.h, stdlib.h, string.h, wubu_svc | wubu_svc_socket.c -- socket-activation contract (P3). A named Unix-domain listening socket. On the F |
| `runtime/` | `wubu_svc_socket_test` | 109 | egojar.h, egojar_net.h, errno.h, socket.h, stdio.h, stdlib.h, string.h, wubu_svc | wubu_svc_socket_test.c -- P3 socket-activation contract (§4.2). A real Unix-domain client connects;  |
| `runtime/` | `wubu_system` | 138 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_snapshot.h, wubu_system.h | wubu_system.c -- WuBuOS System Root (immutable / atomic) layer. Implementation wraps wubu_snapshot.c |
| `runtime/` | `wubu_system_test` | 85 | egojar.h, stdio.h, stdlib.h, string.h, wubu_system.h | wubu_system_test.c -- WuBuOS System Root (immutable/atomic) tests. Verifies the SteamOS-style immuta |
| `runtime/` | `wubu_trace` | 132 | egojar.h, stdio.h, stdlib.h, string.h, time.h, wubu_trace.h | wubu_trace.c -- Mega OS trace foundation (Phase A). Immutable, versioned, user-owned trace store wit |
| `runtime/` | `wubu_track` | 140 | ctype.h, string.h, wubu_track.h | wubu_track.c -- Tracker: the full-text tokenizer and filter chain. See wubu_track.h. |
| `runtime/` | `wubu_track_test` | 144 | stdio.h, string.h, wubu_track.h | wubu_track_test.c -- the full-text tokenizer and filter chain. The load-bearing rule: STEMMING MUST  |
| `runtime/` | `wubu_txn` | 239 | egojar.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_spawn.h, wubu_txn_internal | wubu_txn.c -- WuBuOS transactional speculation engine. Ported from GrahaOS kernel/txn/transaction.c  |
| `runtime/` | `wubu_txn_test` | 98 | egojar.h, stdio.h, stdlib.h, string.h, wubu_spawn.h, wubu_txn.h | wubu_txn_test.c -- WuBuOS transactional speculation self-test. Proves the engine does real work (not |
| `runtime/` | `wubu_uuid` | 132 | egojar.h, stdio.h, stdlib.h, string.h, time.h, wubu_uuid.h | wubu_uuid.c — UUIDv7 generation (RFC 9562) for WuBuOS session tracking. Pure C11, no external libs.  |
| `runtime/` | `wubu_uuid_test` | 75 | stdio.h, string.h, time.h, wubu_uuid.h | wubu_uuid_test.c — Test UUIDv7 generation and parsing. |
| `runtime/` | `wubu_verifier_bytropix` | 125 | egojar.h, egojar_net.h, spawn.h, stdbool.h, stdio.h, stdlib.h, string.h, wubu_ve | wubu_verifier_bytropix.c -- bind bytropix (local C/CUDA inference engine) as the INDEPENDENT verifie |
| `runtime/` | `wubu_vga` | 127 | string.h, wubu_vga.h | wubu_vga.c -- raw VGA text mode (the HolyC console surface). See wubu_vga.h. |
| `runtime/` | `wubu_vga_test` | 132 | stdio.h, string.h, wubu_vga.h | wubu_vga_test.c -- the raw VGA text console. The load-bearing rules: '\n' moves to column 0 AND adva |
| `runtime/` | `wubu_vsl` | 14 | vsl_internal.h | wubu_vsl.c  --  WuBuOS VSL Legacy API Shim This file exists ONLY for backward compatibility with the |
| `runtime/` | `wubu_vsl_test` | 367 | egojar.h, errno.h, stdio.h, stdlib.h, string.h, vsl_syscall_numbers.h, wubu_vsl. | wubu_vsl_test.c  --  WuBuOS VSL (Virtualization Substrate Layer) Test Suite Tests the "Proton within |
| `runtime/` | `wubu_wine_env` | 181 | egojar.h, stdio.h, stdlib.h, string.h, wubu_exec.h, wubu_host_exec.h | wubu_wine_env.c -- Wine/Proton environment setup for Windows PE execution. Sets up Wine prefix, DXVK |
| `runtime/` | `wubu_xdg_autostart` | 181 | ctype.h, dirent.h, egojar.h, limits.h, stdio.h, stdlib.h, string.h, wubu_xdg_aut | wubu_xdg_autostart.c -- XDG Autostart .desktop parser + launcher (P3). .desktop files are INI-ish: ` |
| `runtime/` | `wubu_xdg_autostart_test` | 122 | egojar.h, stdio.h, stdlib.h, string.h, wubu_xdg_autostart.h | wubu_xdg_autostart_test.c -- P3: the XDG Autostart .desktop contract (Mint baseline §3.2). Writes re |
| `runtime/` | `wubu_zxload` | 96 | stdio.h, stdlib.h, string.h, wubu_ns_bridge_internal.h, wubu_zx.h, wubu_zxload.h | wubu_zxload.{c,h} -- the /n NAMESPACE loader. The glue between the named filesystem and the module-r |
| `runtime/` | `wubutime` | 324 | wubu_posix.h, wubutime.h | WuBuTIME -- time service. Freestanding: it includes only its own header and our wubu_posix surface,  |
| `runtime/` | `wubutime_test` | 198 | stdio.h, string.h, wubu_posix.h, wubutime.h | WuBuTIME + our POSIX surface -- behaviour gates. Proves the clock is coherent (civil <-> epoch round |
| `shell/` | `wubu_shell` | 151 | ctype.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_shell_in | wubu_shell.c -- WuBuOS shell: state, REPL driver, main entry Self-contained. Owns the ShellState lif |
| `shell/` | `wubu_shell_complete` | 66 | string.h, wubu_shell_internal.h | wubu_shell_complete.c -- WuBuOS shell tab completion (BATTLESHIP gap 229) Self-contained. Completes  |
| `shell/` | `wubu_shell_exec` | 266 | ctype.h, egojar.h, errno.h, stdio.h, stdlib.h, string.h, wubu_shell_internal.h | wubu_shell_exec.c -- WuBuOS shell pipeline + redirection engine (BATTLESHIP gaps 230 shell_pipe / 23 |
| `shell/` | `wubu_shell_history` | 58 | string.h, wubu_shell_internal.h | wubu_shell_history.c -- WuBuOS shell command history (BATTLESHIP gap 228) Self-contained ring buffer |
| `shell/` | `wubu_shell_test` | 149 | egojar.h, stdio.h, stdlib.h, string.h, wubu_shell_internal.h | wubu_shell_test.c -- WuBuOS shell test suite Exercises the four previously-form-not-function shell g |
| `spells/` | `wubu_vkd3d` | 354 | stdlib.h, string.h, wubu_vkd3d.h | wubu_vkd3d.c -- VKD3D slice 1: the D3D12 resource-state translation core. See wubu_vkd3d.h for the c |
| `spells/` | `wubu_vkd3d_cmd` | 226 | stdlib.h, string.h, wubu_vkd3d_cmd.h | wubu_vkd3d_cmd.c -- VKD3D slice 3: D3D12 command allocators and lists. See wubu_vkd3d_cmd.h.  The st |
| `spells/` | `wubu_vkd3d_cmd_test` | 139 | stdio.h, string.h, wubu_vkd3d_cmd.h | wubu_vkd3d_cmd_test.c -- the command-list state machine. Every rejected transition is a use-after-fr |
| `spells/` | `wubu_vkd3d_descset` | 103 | string.h, wubu_vkd3d_descset.h | wubu_vkd3d_descset.c -- VKD3D slice 13: root signature -> pipeline layout. See wubu_vkd3d_descset.h. |
| `spells/` | `wubu_vkd3d_descset_test` | 158 | stdio.h, string.h, wubu_vkd3d_descset.h | wubu_vkd3d_descset_test.c -- root signature -> pipeline layout. The load-bearing rule: a D3D12 regis |
| `spells/` | `wubu_vkd3d_device` | 157 | stdlib.h, string.h, wubu_vkd3d.h, wubu_vkd3d_cmd.h, wubu_vkd3d_device.h | wubu_vkd3d_device.c -- VKD3D slice 5: the feature-level device. See wubu_vkd3d_device.h.  The device |
| `spells/` | `wubu_vkd3d_device_test` | 150 | stdio.h, string.h, wubu_vkd3d.h, wubu_vkd3d_device.h | wubu_vkd3d_device_test.c -- the feature-level device. A feature level is a CONTRACT, so the load-bea |
| `spells/` | `wubu_vkd3d_fence` | 70 | string.h, wubu_vkd3d_fence.h | wubu_vkd3d_fence.c -- VKD3D slice 6: fences as Vulkan timeline semaphores. See wubu_vkd3d_fence.h.   |
| `spells/` | `wubu_vkd3d_fence_test` | 74 | stdio.h, string.h, wubu_vkd3d_fence.h | wubu_vkd3d_fence_test.c -- the timeline-semaphore fence. The load-bearing rule: timeline values are  |
| `spells/` | `wubu_vkd3d_framebuffer` | 106 | string.h, wubu_vkd3d_framebuffer.h | wubu_vkd3d_framebuffer.c -- VKD3D slice 11: image views and the framebuffer. See wubu_vkd3d_framebuf |
| `spells/` | `wubu_vkd3d_framebuffer_test` | 150 | stdio.h, string.h, wubu_vkd3d_framebuffer.h | wubu_vkd3d_framebuffer_test.c -- image views and the framebuffer. The load-bearing rule: the aspect  |
| `spells/` | `wubu_vkd3d_heap` | 200 | stdlib.h, string.h, wubu_vkd3d_heap.h | wubu_vkd3d_heap.c -- VKD3D slice 2: D3D12 descriptor heaps. See wubu_vkd3d_heap.h.  The heap is a by |
| `spells/` | `wubu_vkd3d_heap_test` | 167 | stdio.h, string.h, wubu_vkd3d_heap.h | wubu_vkd3d_heap_test.c -- the descriptor-heap slice, proven without a GPU. |
| `spells/` | `wubu_vkd3d_live_binding_test` | 661 | stdio.h, stdlib.h, string.h, wubu_vkd3d.h, wubu_vkd3d_descset.h, wubu_vkd3d_fram | wubu_vkd3d_live_binding_test.c -- VKD3D: a REAL root signature on hardware. The live_pipeline test h |
| `spells/` | `wubu_vkd3d_live_msaa_test` | 523 | stdio.h, stdlib.h, string.h, wubu_vkd3d.h, wubu_vkd3d_framebuffer.h, wubu_vkd3d_ | wubu_vkd3d_live_msaa_test.c -- VKD3D: real 4x MSAA with an explicit resolve. A 4x multisampled colou |
| `spells/` | `wubu_vkd3d_live_pipeline_test` | 576 | stdio.h, stdlib.h, string.h, wubu_vkd3d.h, wubu_vkd3d_framebuffer.h, wubu_vkd3d_ | wubu_vkd3d_live_pipeline_test.c -- VKD3D: a REAL graphics pipeline + draw. The live_render test clea |
| `spells/` | `wubu_vkd3d_live_present_test` | 516 | stdio.h, stdlib.h, string.h, wubu_vkd3d.h, wubu_vkd3d_framebuffer.h, wubu_vkd3d_ | wubu_vkd3d_live_present_test.c -- VKD3D: a REAL swapchain and present. The last VKD3D gap.  Every ea |
| `spells/` | `wubu_vkd3d_live_render_test` | 348 | stdio.h, string.h, wubu_vkd3d_framebuffer.h, wubu_vkd3d_memory.h, wubu_vkd3d_phy | wubu_vkd3d_live_render_test.c -- VKD3D: the first REAL GPU work. Everything before this opened a dev |
| `spells/` | `wubu_vkd3d_live_test` | 139 | stdio.h, string.h, wubu_vkd3d_physdev.h, wubu_vkd3d_vkdev.h | wubu_vkd3d_live_test.c -- VKD3D slice 8 (live): real device enumeration. Every other VKD3D slice ope |
| `spells/` | `wubu_vkd3d_live_texture_test` | 791 | stdio.h, stdlib.h, string.h, wubu_vkd3d.h, wubu_vkd3d_descset.h, wubu_vkd3d_fram | D3D12's descriptor tables really are: |
| `spells/` | `wubu_vkd3d_live_uav_test` | 320 | stdio.h, stdlib.h, string.h, wubu_vkd3d_descset.h, wubu_vkd3d_memory.h, wubu_vkd | wubu_vkd3d_live_uav_test.c -- VKD3D: a REAL UAV / storage buffer + compute. The last descriptor type |
| `spells/` | `wubu_vkd3d_memory` | 100 | string.h, wubu_vkd3d_memory.h | wubu_vkd3d_memory.c -- VKD3D slice 12: D3D12 heaps -> Vulkan memory types. See wubu_vkd3d_memory.h. |
| `spells/` | `wubu_vkd3d_memory_test` | 139 | stdio.h, string.h, wubu_vkd3d_memory.h | wubu_vkd3d_memory_test.c -- D3D12 heaps -> Vulkan memory types. The load-bearing rule: the chosen ty |
| `spells/` | `wubu_vkd3d_msaa` | 70 | string.h, wubu_vkd3d_msaa.h | wubu_vkd3d_msaa.c -- VKD3D slice 14: sample descriptions (MSAA). See wubu_vkd3d_msaa.h. |
| `spells/` | `wubu_vkd3d_msaa_test` | 114 | stdio.h, string.h, wubu_vkd3d_msaa.h | wubu_vkd3d_msaa_test.c -- sample descriptions (MSAA). The load-bearing rule: a pipeline's rasterizat |
| `spells/` | `wubu_vkd3d_physdev` | 105 | string.h, wubu_vkd3d_physdev.h | wubu_vkd3d_physdev.c -- VKD3D slice 8: physical device selection. See wubu_vkd3d_physdev.h.  The che |
| `spells/` | `wubu_vkd3d_physdev_test` | 115 | stdio.h, string.h, wubu_vkd3d_physdev.h | wubu_vkd3d_physdev_test.c -- physical device selection. The load-bearing assertion is that every req |
| `spells/` | `wubu_vkd3d_precache` | 284 | stdio.h, stdlib.h, string.h, wubu_vkd3d_precache.h | wubu_vkd3d_precache.c -- VKD3D slice 15: the shader pre-cache (fossilize). See wubu_vkd3d_precache.h |
| `spells/` | `wubu_vkd3d_precache_test` | 173 | stdio.h, stdlib.h, string.h, wubu_vkd3d_precache.h | wubu_vkd3d_precache_test.c -- the shader pre-cache (fossilize). The load-bearing rules: a cache is b |
| `spells/` | `wubu_vkd3d_pso` | 342 | stdlib.h, string.h, wubu_vkd3d_pso.h | wubu_vkd3d_pso.c -- VKD3D slice 4: pipeline state objects. See wubu_vkd3d_pso.h.  The cache key is a |
| `spells/` | `wubu_vkd3d_pso_test` | 274 | stdio.h, string.h, wubu_vkd3d_pso.h | wubu_vkd3d_pso_test.c -- the PSO cache and its key. The strongest assertion here is the MUTATION SWE |
| `spells/` | `wubu_vkd3d_renderpass` | 97 | string.h, wubu_vkd3d_renderpass.h | wubu_vkd3d_renderpass.c -- VKD3D slice 10: the render pass. See wubu_vkd3d_renderpass.h.  Every rule |
| `spells/` | `wubu_vkd3d_renderpass_test` | 158 | stdio.h, string.h, wubu_vkd3d_renderpass.h | wubu_vkd3d_renderpass_test.c -- the render pass. The load-bearing rule: D3D12's NO_ACCESS must becom |
| `spells/` | `wubu_vkd3d_swapchain` | 120 | string.h, wubu_vkd3d_swapchain.h | wubu_vkd3d_swapchain.c -- VKD3D slice 7: the swapchain / present path. See wubu_vkd3d_swapchain.h fo |
| `spells/` | `wubu_vkd3d_swapchain_test` | 134 | stdio.h, string.h, wubu_vkd3d_swapchain.h | wubu_vkd3d_swapchain_test.c -- the swapchain / present path. The load-bearing rules: a flip chain ne |
| `spells/` | `wubu_vkd3d_test` | 225 | stdio.h, string.h, wubu_vkd3d.h | wubu_vkd3d_test.c -- the state-translation core, proven without a GPU. The whole point of this modul |
| `spells/` | `wubu_vkd3d_vkdev` | 105 | string.h, wubu_vkd3d_vkdev.h | wubu_vkd3d_vkdev.c -- VKD3D slice 9: the logical VkDevice. See wubu_vkd3d_vkdev.h.  Every check here |
| `spells/` | `wubu_vkd3d_vkdev_test` | 162 | stdio.h, string.h, wubu_vkd3d_vkdev.h | wubu_vkd3d_vkdev_test.c -- the logical VkDevice. The load-bearing rule: a family must not be request |
| `spells/` | `wubu_boot_attest` | 301 | string.h, wubu_boot_attest.h | wubu_boot_attest.c -- boot tracking over the SDR relay (implementation). C11, opaque struct, libc on |
| `spells/` | `wubu_boot_attest_test` | 176 | stdio.h, string.h, wubu_boot_attest.h | wubu_boot_attest_test.c -- a boot report must be sealed, verifiable, and fail closed.  Every asserti |
| `spells/` | `wubu_swarm` | 154 | stdio.h, stdlib.h, string.h, wubu_swarm.h | wubu_swarm.c -- the aggregation ledger.  See wubu_swarm.h for the premise. |
| `spells/` | `wubu_swarm_test` | 139 | stdio.h, string.h, wubu_swarm.h, wubu_swarm_wire.h | wubu_swarm_test.c -- the aggregation ledger must do what the vision claims: peers self-issue identit |
| `spells/` | `wubu_swarm_wire` | 53 | ctype.h, string.h, wubu_swarm_wire.h | wubu_swarm_wire.c -- the fixed on-wire frame.  See the header for the layout. |
| `tools/` | `demo_capture` | 180 | apps.h, desktop.h, egojar.h, input.h, memory.h, screenshot.h, startmenu.h, stdio | demo_capture.c  --  Headless WuBuOS Demo Frame Capture Renders theme cycling demo to PPM frames, the |
| `tools/` | `desktop_render_test` | 67 | desktop.h, screenshot.h, stdio.h, stdlib.h, vbe.h, wm.h, wubu_theme.h | desktop_render_test.c -- Test that desktop rendering produces non-black output |
| `tools/` | `iso9660` | 436 | iso9660.h, stdio.h, stdlib.h, string.h | iso9660.c  --  WuBuOS ISO 9660 / Bootable ISO Builder Cell 060: Builds bootable ISO 9660 images with |
| `tools/` | `iso9660_test` | 441 | iso9660.h, stdio.h, stdlib.h, string.h | iso9660_test.c  --  Test Suite for WuBuOS ISO 9660 / Bootable ISO Builder Cell 060: Tests ISO builde |
| `tools/` | `screenshot` | 405 | screenshot.h, stb_image_write.h, stdbool.h, stdint.h, stdio.h, stdlib.h, string. | wubu_screenshot.c  --  WuBuOS Screenshot/Snipping Tool Implementation PPM/BMP/PNG capture, GIF anima |
| `tools/` | `screenshot_test` | 149 | assert.h, screenshot.h, stdio.h, stdlib.h, string.h | wubu_screenshot_test.c  --  Tests for screenshot/snipping tool |
| `tools/` | `weight_check` | 90 | stdio.h, stdlib.h, string.h, weight_check.h | weight_check.c  --  WuBuOS Vision Weight Verification Implementation Cell 051: Checks Moondream3 saf |
| `tools/` | `weight_check_test` | 131 | stdio.h, string.h, weight_check.h | weight_check_test.c  --  Test Suite for Vision Weight Verification Cell 051: Tests weight checking,  |
| `tools/` | `wubre` | 25 | wubre_internal.h | wubre.c - WuBu regex engine. "occupational supremacy of C11 code". --------------------------------- |
| `tools/` | `wubre_bre` | 229 | stdio.h, wubre_internal.h | wubre_bre.c - BRE backtracking engine (backreferences \1..\9). ------------------------------------- |
| `tools/` | `wubre_compile` | 784 | stdio.h, wubre_internal.h | wubre_compile.c - WuBu regex compiler: BRE/ERE parser -> Thompson NFA. ----------------------------- |
| `tools/` | `wubre_dfa` | 305 | immintrin.h, stdlib.h, string.h, wubre_internal.h | wubre_dfa.c - Subset-construction DFA for the Thompson NFA (wubre_compile). ------------------------ |
| `tools/` | `wubre_litpref` | 408 | stdio.h, stdlib.h, string.h, wubre_internal.h | wubre_litpref.c - Literal-SET prefilter extraction for the WuBu regex engine. ---------------------- |
| `tools/` | `wubre_match` | 291 | wubre_internal.h | wubre_match.c - Thompson NFA (Pike VM) simulation + prefilter gate. -------------------------------- |
| `tools/` | `wubre_simd` | 655 | immintrin.h, wubre_internal.h | wubre_simd.c - Our own SIMD literal search (no vectorscan dependency). ----------------------------- |
| `tools/` | `wubre_test` | 80 | stdio.h, string.h, wubre.h | wubre_test.c - unit tests for the WuBu regex engine (wubre.c). Build: $(CC) -O2 -I. wubre.c wubre_te |
| `tools/` | `wubugrep` | 1183 | ctype.h, dirent.h, errno.h, fcntl.h, immintrin.h, mman.h, pthread.h, stat.h, std | WuBuGrep - "occupational supremacy of C11 code" ---------------------------------------------------- |
| `worldsim/` | `entity` | 63 | string.h, worldsim.h | Find free slot |
| `worldsim/` | `physics` | 57 | worldsim.h | Apply gravity |
| `worldsim/` | `render` | 125 | string.h, worldsim.h | Biome colors (XRGB8888) |
| `worldsim/` | `sim` | 75 | string.h, worldsim.h | Generate terrain |
| `worldsim/` | `terrain` | 144 | string.h, worldsim.h, wubu_math.h | -- RNG -- |
| `worldsim/` | `test_worldsim` | 271 | stdio.h, stdlib.h, string.h, worldsim.h | test_worldsim.c -- WorldSim engine behaviour gate. The module (633 LOC: terrain / ECS / physics / re |
