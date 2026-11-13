# WuBuOS experience gap assessment — the devil's advocate laundry list

Date: 2026-11-13 · Scope: hosted desktop (`src/hosted` + `src/gui`) as a user
actually experiences it · Method: read the code paths, then run them. Every claim
below carries the file that proves it.

## Progress

- **#5 Control Panel blank pane -- NOT REPRODUCIBLE.** Verified live at `207fb35`:
  full window with sidebar ("See Also", three links), "Pick a category", eight
  labelled applets in two columns, and wallpaper/placement text. The original
  sighting came from an older build or a test-window geometry artefact. Marked
  done-as-not-a-bug so nobody re-"fixes" a working surface.

- **#1 persistence -- DONE** (`5183844`). New `hosted_fs_persist.{c,h}` writes an
  explicit little-endian image of the namespace to
  `$XDG_DATA_HOME/wubu/fs.img`, renamed into place; loads before built-ins so a
  persisted edit wins over a rebuilt default; fails closed on every malformed
  case. `styx_write_cb`/`clunk`/`remove` implemented (the FS was strictly
  read-only). `styx_stat_cb` never set `dir->length`, so *every* reader believed
  every file was zero bytes -- fixed. Test: `make test_fs_persist`, 61 checks,
  mostly negative. Real-binary proof: seeded the store, restarted, client logged
  `filesystem restored (1 entries)`.
- **#2 Notepad open/save -- DONE** (`5183844` follow-up). New `apps/guest_file.h`
  is the ONLY way an application touches files (two functions, no host types in
  the header, so Paint and Notepad cannot disagree about where files live).
  Implemented next to `g_fs`, so the namespace keeps exactly one owner.
  Notepad gained Ctrl+S / Ctrl+O with an inline filename prompt in the status
  line (there is no dialog framework yet, so this stays usable today instead of
  blocked). First save names the file, Windows-style. A document that cannot fit
  is REFUSED, never silently truncated. Test: `make test_notepad_save`, 27
  checks, ending with `CONTENT SURVIVED THE RESTART`.
- **#23 lower-case text renders lower-case -- DONE**. New `vbe_font_lower_8x8`
  (26 glyphs) in `kernel/vbe.c`, declared in `vbe.h`; `vbe_draw_char()` uses it
  instead of folding `a-z` upward. Verified LIVE (the Run dialog's own source
  string now shows mixed case) and by `make test_font_case` (58 checks): every
  lower-case letter draws its OWN bitmap, pixel-compared against the table, and
  differs visibly from the upper-case form. This also explains why "all the apps
  look bad" -- everything was capitals.
- **#6 Run dialog executes commands -- DONE**. Two-step dispatch: a registered app
  name launches that app, anything else runs as `/bin/sh -c "<query> 2>&1"` via
  the existing `wubu_popen_read`. Output goes to the guest file `run-output.txt`
  and is opened in Notepad, so the result persists like any document. Verified
  LIVE: Win+R, typed `echo hello-from-run-dialog`, Enter -> the Notepad window
  showed `$ echo hello-from-run-dialog` and the echoed line. Documented honestly:
  it is a shell, not a sandbox (same uid, no restriction on syntax), and it
  BLOCKS the UI until the command finishes.
- **NEW FINDING from that verification, diagnosed**: every string on screen
  renders UPPERCASE, including the Run dialog's own hardcoded lower-case
  instruction text. Typed bytes are CORRECT -- the executed command really was
  lower-case, which is why `echo hello-from-run-dialog` produced output instead
  of `ECHO: command not found`. The cause is the font: `vbe_font_8x8` covers only
  ASCII 32..95, and `vbe_draw_char()` folds `a-z` up to `A-Z`. It is a rendering
  limitation, not an input bug. See item 23.
- **#14 test_calc/test_greeter -Wformat-truncation -- RESOLVED** as of
  `test_high_gui`; those recipes build with `-Wno-format-truncation`. The gate
  is green end to end. Re-check before relying on this.

This is deliberately hostile. Where something looks done, it was checked for the
difference between *renders* and *works*. Three findings dominate everything else.

---

## The three that make the OS unusable

### 1. Nothing persists — the filesystem is a fixed array in RAM

`src/hosted/hosted_styxfs.c`:

```c
styxfs_file_t g_fs[STYXFS_MAX_FILES];
int           g_nfiles = 0;
```

That is the whole guest filesystem. It is a flat global array with a count,
living in process memory. There is no backing store, no dirty tracking, no
flush, no snapshot. When the client exits, every byte a user typed, drew or
downloaded is gone.

The Explorer agrees: it browses through `ex_9p_opendir(path)` over the 9P
namespace (`src/gui/explorer.c:414`), i.e. the virtual FS — never the host disk.
So even browsing shows you a toy world that resets on restart.

**Impact:** the OS cannot be used for anything. A user cannot create a file and
expect it tomorrow. This alone justifies "nothing to do inside the operating
system".

### 2. Notepad cannot open or save anything

`grep -icE "fopen|open_file|save|load|\.txt" src/apps/notepad.c` → **0**.

Notepad is a text widget with a caret. It has no file dialog, no path, no
open/save menu item. Combined with finding 1, even if it did save, the bytes
would evaporate.

**Impact:** the flagship editor is decoration. Same class of problem almost
certainly applies to Paint and the other editors; each must be audited
individually rather than assumed fixed.

### 3. You cannot run a command from the GUI

The Run dialog (`src/gui/shell/run_dialog.c:55-57`) does exactly one thing:

```c
if (g_len > 0 && launch_app) { launch_app(g_query); }
```

`launch_app` resolves a name against the application database. Type `ls -la`
and it looks for an app called "ls -la". There is no shell, no argument parsing,
no path resolution, no output capture.

A real exec layer **does** exist — `src/runtime/wubu_exec.c` forks and execs on
the host, with `wubu_host_exec.c` and `src/gui/term_pty.c` — so this is a
*wiring* gap, not a missing capability. The terminal has a PTY and a HolyC REPL
pane; whether a typed command actually reaches `wubu_exec` still needs a live
check (see Open questions).

**Impact:** power users, troubleshooting, and any automation are impossible
from inside the OS.

---

## The full list

Legend: **P0** blocks basic use · **P1** blocks real work · **P2** polish/consistency
· **P3** roadmap. Each item names its owner file and how to prove it fixed.

### P0 — data loss and dead apps

| # | Work item | Owner | Acceptance |
|---|---|---|---|
| 1 | ~~Give the guest a persistent backing store~~ **DONE** -- image file + dirty-flag flush + fail-closed load; write/remove/clunk implemented. Remaining: the Explorer still cannot SAVE (item 2 blocks it). | `hosted_fs_persist.c` | `make test_fs_persist` 61/61; real restart restores entries. |
| 2 | ~~Notepad open/save~~ **DONE** via `guest_file` + Ctrl+S/Ctrl+O and an inline prompt. Remaining: a real modal file dialog (blocked on a dialog framework), and non-ASCII round-trip still untested. | `apps/notepad.c` | `make test_notepad_save` 27/27. |
| 3 | Audit every editor (Paint, Calc, Control Panel panes) for file I/O. Assume zero until proven otherwise. | `apps/*` | One row per app in the audit doc: has open? has save? writes where? |
| 4 | Explorer must show the *real* tree (or clearly label the sandbox). `ex_9p_opendir` hides the host. | `gui/explorer.c` | Navigate to a directory the user created on the host; it must appear. |
| 5 | ~~Fix the Control Panel blank content pane~~ **NOT REPRODUCIBLE** (2026-11-13, `207fb35`): launched via Win+R and captured the window -- full sidebar, "Pick a category", 8 labelled applets in two columns, wallpaper/placement text. The earlier blank sighting predates the chrome/token work or was the test-window geometry. Recorded so nobody "fixes" a working surface. | `apps/control/control.c` | Live capture shows real content (see commit message). |

### P1 — no way to get things done

| # | Work item | Owner | Acceptance |
|---|---|---|---|
| 6 | ~~Run dialog must execute a command line~~ **DONE** via `wubu_popen_read`; output persisted to `run-output.txt` and shown in Notepad. Remaining: async execution so long commands do not freeze the desktop. | `shell/run_dialog.c` | Live: command ran, output visible and durable. |
| 7 | Start-menu search must find files, settings and commands, not only the built-in app DB. | `startmenu.c` | Searching a filename that exists in the FS returns it. |
| 8 | Shortcut creation exists (`test_desktop_new_shortcut` in `wm_test.c:1258`) but the *user-facing* flow is unverified end-to-end: create a shortcut, click it, confirm it launches. | `gui/wm_input.c` | Live: right-click desktop → New Shortcut → it appears → double-click launches the target. |
| 9 | `.desktop`/MIME handling exists (`wubu_mime.c`, `wubu_mime_desktop.c`, `startmenu_write_desktop`) but no MIME *association editor* in Settings. | Settings app | Associate a file type, reopen the file, correct handler starts. |
| 10 | Package manager UI: `pkgmgr_install`, `pkgmgr_engine`, `pkgmgr_add_remote` exist — but is there a window a user can drive? If not, build one over the existing engine (do NOT write a second package engine). | `apps/control` | Install a package from the UI without touching the CLI. |
| 11 | Terminal: verify whether the PTY pane executes shell commands or only evaluates HolyC. If it is HolyC-only, add a shell session mode. | `gui/term.c`, `gui/term_pty.c` | Type `echo hi`; see `hi` come back. |
| 12 | Window management is Win11-class on paper (snap layouts, task view, widgets panel) but no maximize hover flyout, no hot corners, no presentation mode, no night light. | `win11_snap.c`, Settings | Each has a live manual test. |

### P1b — input correctness

| # | Work item | Owner | Acceptance |
|---|---|---|---|
| 23 | ~~Every string renders UPPERCASE~~ **DONE**: 26-glyph lower-case table added, fold removed. Remaining: `~` `{` `}` `|` and other ASCII >95 still have no glyphs (they draw a space). | `kernel/vbe.c` | `make test_font_case` 58/58; live mixed-case render confirmed. |

### P2 — consistency and honesty

| # | Work item | Owner | Acceptance |
|---|---|---|---|
| 13 | **Theme settings disagree.** `~/.config/wubu/settings.conf` holds `"theme_id": 5` (THEME_DARK) and a plain-text `theme=Zune` (index 4). Two sources of truth; the UI can show Dark while another reader sees Zune. | `gui/wubu_settings.c`, `gui/wubu_theme.c` | One setting object owns the theme. Delete the duplicate key or make it derived. Test asserts both readers agree. |
| 14 | `make test_calc` and `test_greeter` fail on `-Wformat-truncation` in `wm_clock.c`, `wm_taskbar.c`, `wm_desktop.c`, `wm_systray.c` — pre-existing, untouched by recent work, and it means the gate is not fully green. | those four files | `make -k -j24 test_high_gui` returns zero failures. |
| 15 | The layout token scale exists (`src/gui/wubu_tokens.h`) but only `window_chrome.c` consumes it. Taskbar, Start menu, systray and context menus still hardcode their own spacing. | `gui/*` | Grep for literal geometry in those files; replace with tokens. Zero magic numbers outside the token header. |
| 16 | OOBE is one modal window with three buttons (`wubu_shell_greeter.c:160`) and a 10-byte state file (`theme=Zune`). There is no Next/Back/Finish, no progress, no locale/network/account step, and no way to know you are done. | `shell/wubu_shell_greeter.c` | A first-boot wizard with at least: language, theme, account, network, finish. State machine, not a single dialog. |
| 17 | First-run detection writes into a config the greeter never reads back consistently. Decide one flag (`first_run` boolean in the settings object) and use it everywhere. | `gui/wubu_settings.c` | Second boot skips the greeter; no orphan state files. |
| 18 | Apps that are placeholders must say so on screen. The weather card is a placeholder; Start search covers only built-ins. A user cannot tell "not implemented" from "broken". | Settings, Start | Each placeholder shows its status; the audit doc lists them. |

### P3 — distribution and ecosystem

| # | Work item | Owner | Acceptance |
|---|---|---|---|
| 19 | `install-wubuos.sh` is a real interactive installer (plan-based, idempotent, reversible) — but it installs to `$HOME/.local`. There is no ISO/image/EFI artifact, no bootable media, no upgrade path. | new `dist/` tooling | Produce a bootable image; boot it in QEMU; install from the image. |
| 20 | No upgrade/downgrade path, no rollback snapshot, no recovery partition. | same | Install v1, install v2, roll back to v1, boot. |
| 21 | No package repository format of our own, so the package manager installs from where? Document the remote protocol or build the registry. | pkgmgr | Publish one package; install it on a clean system. |
| 22 | WuBuWizard legs are built and tested (magnet/DHT/multiaddr/CID/bitswap/multistream/yamux) but the *inference* side has no OpenAI-compatible endpoint and no DePIN compute binding. | wubuwizard | Serve a model locally behind the standard API; route a job to a swarm peer. |

---

## What already exists — do not rebuild

The covenant says reuse, don't double-code. These are real and working; wire to
them instead of writing competitors:

- **Installer**: `install-wubuos.sh` (Omarchy style, plan JSON, idempotent).
- **Greeter surface**: `wubu_shell_greeter.c` (modal + offers row), needs a
  wizard shape, not a replacement engine.
- **Real exec**: `src/runtime/wubu_exec.c` (fork+exec host delegation),
  `wubu_host_exec.c`, `term_pty.c`.
- **Package engine**: `pkgmgr_install`, `pkgmgr_engine`, `pkgmgr_add_remote`.
- **MIME/.desktop**: `wubu_mime.c`, `wubu_mime_desktop.c`,
  `startmenu_write_desktop`.
- **Explorer**: `gui/explorer.c` — copy/cut/paste/delete/rename, F2, Ctrl+C/V/X.
- **Shortcut creation**: exercised by `test_desktop_new_shortcut`.
- **Window manager**: `win11_snap.c`, `win11_widgets.c`, task view, widgets panel.
- **Boot attestation + EDR**: `src/spells/networking/aggregation/bootattest/`,
  `EDR_EV_BOOT_ATTEST = 27`.
- **Wizard swarm legs**: magnet, SHA-1, torrent, DHT, routing table, multiaddr,
  CID, bitswap, multistream-select, yamux — all with independent tests.

---

## Open questions (verify before building)

1. Does the terminal's PTY pane execute shell commands, or only evaluate HolyC?
   `term.c` includes `wubu_exec.h` and defines a HolyC REPL session; the bridge
   between a typed command and `wubu_exec` is unconfirmed.
2. Is the Control Panel blank-pane bug a real regression or a test-window
   geometry artifact? Seen live once via the power menu, never root-caused.
3. Which surfaces still lack a test? Any item above with no "Acceptance" test
   should get one before being called done.

---

## Suggested order

1. **Persistence (1)** — nothing else matters until data survives a restart.
2. **Notepad open/save (2)** — the smallest app that proves persistence works.
3. **Run dialog executes commands (6)** — unlocks every other fix.
4. **OOBE wizard (16)** — first impression; cheap once the settings object is honest.
5. Then the P1 sweep, then P2 consistency, then P3 distribution.

Each step should land with its own gate. A gate that passes while the user
experience is still awful is a bad gate.
