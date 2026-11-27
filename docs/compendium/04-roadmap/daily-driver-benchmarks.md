# Daily-driver benchmarks — 111 workflows a person actually performs

**What this is.** A checklist of the jobs a normal person does on a computer in a
week, written as workflows rather than feature names, and graded against this
tree. The method is the one the migration literature insists on: *check
workflows, not application names* — "office work" is not a requirement, "open,
edit and return a .docx with your colleague" is ([Hilk, Windows→Linux migration
checklist](https://dennishilk.com/blog/windows-to-linux-migration-checklist/)).
Accessibility rows follow Microsoft's [accessibility
checklist](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessibility-checklist)
(keyboard path, readable content, assistive-tech compatibility); the interaction
rows follow Nielsen's ten heuristics (visibility of state, user control, error
prevention, recognition over recall).

**How a row was graded.** Three states only:

- **WORKS** — a module is compiled into the product AND its behaviour was
  exercised in this session (run, screenshot, or a gate that fails when broken).
- **PARTIAL** — something real exists but a piece is missing (a dialog with a
  weak backend, a setting with no consumer, a vendored submodule standing in).
- **MISSING** — nothing in the tree implements it.

Every grade below was checked by reading the code or running it today. Rows
marked *(tree)* were graded by reading only — they have not been exercised, and
that distinction is deliberate: a name in a file is not a working workflow.

---

## 1. Getting online — 17 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 1 | Connect to a Wi-Fi network, type its password | **MISSING** | `wubu_wifi_reg.c` is regulatory/DFS routing only; no supplicant, no `iwd`, no nl80211 association anywhere in `src/` |
| 2 | See nearby networks and signal strength | **MISSING** | no scan path |
| 3 | Sign in at a hotel/airport hotspot | **MISSING** | no captive-portal detection |
| 4 | Plug in a cable and be online | PARTIAL *(tree)* | `wubu_network.c` models links for containers; no user-facing "connected" path |
| 5 | Get an address automatically | PARTIAL *(tree)* | DHCP client code exists in the network stack, not wired to the shell |
| 6 | Change DNS servers | PARTIAL *(tree)* | `wubu_network_dns.c` exists |
| 7 | Set a proxy | MISSING | no proxy setting with a consumer |
| 8 | Turn a VPN on | MISSING | no WireGuard/OpenVPN path |
| 9 | Use IPv6 | PARTIAL *(tree)* | `wubu_network.h` has v6 types |
| 10 | See why the network is down | MISSING | no status surface beyond a tray dot |
| 11 | **Browse the web in a real browser** | **MISSING** | no engine: the only webkit/chromium/servo/gecko hits are data files and vendored headers. `src/lua/www` is docs + examples |
| 12 | Open many tabs | MISSING | follows 11 |
| 13 | Bookmark a page | MISSING | follows 11 |
| 14 | See/clear browsing history | MISSING | follows 11 |
| 15 | Download a file into Downloads | MISSING | follows 11 |
| 16 | Know a site is secure (cert warning) | MISSING | TLS exists for package fetching, not for browsing |
| 17 | Block popups/trackers | MISSING | follows 11 |

**Read this first:** one missing engine removes eleven workflows at once. No
amount of shell polish substitutes for it.

## 2. Communication — 10 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 18 | Send and receive email | **MISSING** | no SMTP, no IMAP client in `src/` |
| 19 | Attach a file to mail | MISSING | follows 18 |
| 20 | Calendar with reminders | PARTIAL | `wm_clock.c` draws a month grid with today highlighted; no events, no reminders |
| 21 | Look up a contact | PARTIAL *(tree)* | `control_panel_people.c`, `wubu_people.c` hold people records; no address book in any app |
| 22 | Make a video call | MISSING | only `wubu_aec.c` (echo canceller) |
| 23 | Share your screen | MISSING | no screencast source |
| 24 | Chat in a messenger | MISSING | no protocol client |
| 25 | Get a notification and act on it | **WORKS** | `wubu_notify.c` + toasts; clicking a notification opens its app |
| 26 | Silence notifications | PARTIAL | `a11y.reduce_animations` and per-notification policy exist; no global DND switch |
| 27 | Copy in one app, paste in another | **WORKS** | `wubu_clipboard.c` + `wubu_clipboard_wl.c`; shared X11/Wayland path |

## 3. Documents and office — 12 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 28 | Open a .docx and read it | PARTIAL | only in the vendored `wubuoffice` submodule; this tree has no OOXML reader |
| 29 | Write a formatted document and save it | **WORKS** | `notepad.c`, `editor/editor.c`, `wubupad` submodule |
| 30 | Print a document | PARTIAL | `src/runtime/wubu_print.c` is a real backend (enumerates printers, submits via argv, refuses to claim success without a spooler job id). Not yet exercised end-to-end |
| 31 | Read a PDF | PARTIAL | `wubupdf` inside the vendored submodule |
| 32 | Fill a PDF form | PARTIAL | `wubupdfform` in the submodule |
| 33 | Spreadsheet with formulas | **MISSING** | `calc/calc.c` is a calculator, not a grid |
| 34 | Slide deck | PARTIAL | `view_slide.c` in the submodule (viewer, not authoring) |
| 35 | Spell check | MISSING | nothing in `src/` |
| 36 | Recover work after a crash | PARTIAL | `crash_notice.c` reports; no autosave/restore of unsaved work |
| 37 | Reopen a recent file | MISSING | no recent-files store |
| 38 | Export to PDF | MISSING | no print-to-PDF path |
| 39 | Annotate/sign a PDF | MISSING | — |

## 4. Media — 10 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 40 | Play music from a file | **WORKS** | `apps/music.c` + `wubu_sound.c` |
| 41 | Watch a video | PARTIAL | `wubu_image_ffmpeg.c` decodes frames; no player surface |
| 42 | Change volume / mute | **WORKS** | `wubu_sound.c`, ALSA/Pulse through `wubu_metal.c` |
| 43 | Switch audio output device | PARTIAL *(tree)* | sink enumeration exists in the metal layer; no chooser |
| 44 | Change screen brightness | PARTIAL | `control_panel_display.c` reads/writes a value; no backlight driver binding on metal |
| 45 | Take a screenshot (full and region) | **WORKS** | `wubu_screenshot.c` + PNG encoder, gated |
| 46 | Record the screen | MISSING | — |
| 47 | View a photo and zoom | PARTIAL | Explorer preview renders images; no viewer window with zoom |
| 48 | Import photos from a camera/phone | MISSING | no MTP/PTP |
| 49 | Subtitles | MISSING | — |

## 5. Files — 11 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 50 | Copy, move, rename, delete | **WORKS** | `explorer_fsops.c` (measured today in the live File Manager) |
| 51 | Restore a deleted file from Trash | **WORKS** | `wubu_trash.c` + Explorer menu |
| 52 | Find a file by name | **WORKS** | `explorer_search.c` filters the live list |
| 53 | Search inside documents | **MISSING** | `wubugrep` is a dev tool, not an Explorer feature |
| 54 | Zip a folder / extract a zip | PARTIAL | `EX_ENTRY_ZIP` exists as a type; no archive writer in-tree |
| 55 | Plug in a USB stick and see it | PARTIAL | USB drivers exist in the kernel registry; no hotplug → icon path |
| 56 | Safely eject a USB stick | PARTIAL *(tree)* | `umount` plumbing only |
| 57 | Open a Windows/SMB share | **MISSING** | no SMB client |
| 58 | See/fix file permissions | **MISSING** | no properties sheet writes modes |
| 59 | See how much disk space is left | PARTIAL | Explorer status bar counts files and bytes, not free space |
| 60 | Back up my files | **MISSING** | no backup tool; `pkgmgr_txn` is transactional install, not user backup |

## 6. System and stability — 12 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 61 | Suspend and resume | **MISSING** | no suspend/resume path in the OS (host guards are shell scripts, not the OS) |
| 62 | See battery percentage | PARTIAL | battery driver + tray glyph; world state is sampled, not continuously updated |
| 63 | Close the lid without losing work | MISSING | host-side guard only |
| 64 | See temperatures / stay cool | **WORKS** | `wubu_thermal*` + tray glyph, sampled |
| 65 | Install updates | **WORKS** | `wubu_pkgmgr*.c`, SQLite DB, signed packages, gate |
| 66 | Undo a bad update | PARTIAL *(tree)* | `wubu_pkgmgr_txn.c` transactions; no system rollback |
| 67 | Recover a session after a crash | **MISSING** | windows are not persisted |
| 68 | Warning before the disk fills | **MISSING** | — |
| 69 | Correct clock (NTP) | **MISSING** | clock reads the host; no sync |
| 70 | Change keyboard layout | PARTIAL | settings holds `layout`/`variant`; input path ignores them |
| 71 | Two monitors, hot-plug | **MISSING** | single output in the hosted present path |
| 72 | Change resolution | PARTIAL *(tree)* | `control_panel_display.c` sets the hosted surface size |

## 7. Security and privacy — 8 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 73 | Lock the screen, require a password | **WORKS** | `wubu_lock_screen.c` |
| 74 | Encrypt the disk | PARTIAL | `wubuencrypt` exists only inside the vendored `wubuoffice` submodule, and is not built |
| 75 | Turn on a firewall | **MISSING** | — |
| 76 | Store passwords safely | **MISSING** | no keyring/vault in the shell (API keys live in files) |
| 77 | Verify updates are signed | **WORKS** | Ed25519 package signatures, mutation-proven gate |
| 78 | Approve an app permission prompt | PARTIAL | `wubu_cap` capability gates exist in the kernel path; no user-facing prompt |
| 79 | Check privacy settings | PARTIAL | settings page stores flags; telemetry/location have no producers |
| 80 | Auto-lock after idle | PARTIAL | screensaver exists; no timeout → lock chain |

## 8. Accessibility — 8 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 81 | Screen reader | **MISSING** | no speech path in the shell |
| 82 | Magnifier | **MISSING** | a settings flag with no consumer |
| 83 | High contrast | **WORKS** | palette override, applies live |
| 84 | Scale text without breaking layout | **WORKS** | fixed today: `font_scale * dpi/96`, layout measured from the face |
| 85 | Keyboard-only navigation | PARTIAL | Explorer/taskbar/start menu honour keys; no global tab order |
| 86 | Sticky/slow keys | **MISSING** | — |
| 87 | Captions on video | **MISSING** | `caption.c` only in the vendored submodule |
| 88 | Colour-blind-safe status colours | **MISSING** | status is carried by hue alone (the tray glyphs help, the colours do not) |

## 9. Developer and pro — 10 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 89 | Use a terminal with real commands | **WORKS** | `term_pty.c` execs host programs; `term.c` renders |
| 90 | SSH to another machine | PARTIAL | through the terminal's exec path only; no ssh client in-tree |
| 91 | Use git | **MISSING** | no client in the OS |
| 92 | Run Python/Node scripts | MISSING | no interpreter in the OS (host path only) |
| 93 | Install a package from a repo | **WORKS** | `wubu_pkgmgr*` with a real DB (`list_installed`/`get_installed` gated) |
| 94 | Run a container | **WORKS** | `wubu_container.c` + `wubu_ct_isolate` |
| 95 | Compile C | PARTIAL | our HolyC compiler + WEMU; no user-facing `cc` |
| 96 | Debug a program | **MISSING** | — |
| 97 | Editor with syntax highlighting | PARTIAL | `editor/` has highlighting primitives; not language-aware |
| 98 | Automate with a script | PARTIAL | `.wet` applets + HolyC JIT |

## 10. Gaming — 7 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 99 | Run a Windows game | **WORKS** | `wubu_proton*`, `hosted_pe_executor` (SteamOS strategy: Proton in a container) |
| 100 | Use a controller | **WORKS** | `wubu_evdev` + Steam Input mapping table |
| 101 | Remap controller buttons | PARTIAL *(tree)* | mapping table exists; no UI |
| 102 | Fullscreen without tearing | PARTIAL | hosted SHM present; no vsync contract |
| 103 | FPS/performance overlay | MISSING | `WUBU_FPS_REPORT` is a developer env var |
| 104 | Games library | **WORKS** | `wubu_gamelib*` (scan, playtime, launch) |
| 105 | Voice chat while playing | MISSING | — |

## 11. First day — 6 workflows

| # | Workflow | State | Evidence |
|---|---|---|---|
| 106 | Complete first-run setup | **WORKS** | greeter + onboarding sentinel |
| 107 | Find and launch any app | **WORKS** | fixed today: Start lists all 32 programs, every registry app once |
| 108 | Keep settings across reboots | **WORKS** | fixed today: settings cannot be zeroed by a test; corrupt files heal |
| 109 | Know which app opens this file | **WORKS** | `file_assoc.c` + `wubu_mime.c` |
| 110 | Get help when stuck | **WORKS** | Getting Started, walkthrough, `.wet` tutorials |
| 111 | Read docs offline | PARTIAL | docs live in the repo; no in-OS browser to read them (see #11) |

---

## Ranked blockers — what actually stops a person

1. **No browser engine (#11–17).** Eleven workflows, most of the internet, and
   the only way to read our own docs offline.
2. **No Wi-Fi association (#1–2).** On a laptop the OS cannot get online at all;
   everything above is then theoretical.
3. **No email (#18–19).**
4. **No suspend/resume (#61).** A laptop that cannot sleep is a desktop.
5. **No screen reader or magnifier (#81–82).** Two settings that lie.
6. **No spreadsheet, spell check, or slide authoring (#33, #35, #34).**
7. **No SMB, USB hotplug+eject, permissions, free-space, backup (#55–60).**
8. **No NTP, multi-monitor, keyboard layouts (#69–71).** Every one is a daily
   annoyance, not an edge case.
9. **No firewall, disk encryption, password store (#74–76).**
10. **No session restore or disk-full warning (#67–68).**

## Smallest real increment per blocker

| Blocker | Increment that changes the verdict | Gate |
|---|---|---|
| Browser | a text-first HTML/DOM renderer behind one window (no JS), so a local page and a fetched page render; JS is a later leg | golden-frame render of a fixed HTML file + a real fetch of one URL |
| Wi-Fi | association via `wpa_supplicant` control socket, password entry in a dialog, status in the tray | connect to a known AP in the QEMU/VM test rig; fail on wrong password |
| Email | one IMAP fetch → list → open message; SMTP send | round-trip against a local dovecot in the test rig |
| Suspend | `/sys/power/state` write + wake resume path that repaints | suspend → 5 s → resume → window still painted |
| A11y | magnifier (scale a region) and a speech queue that reads focused control names | focus a button, assert the spoken string |
| Office | CSV/TSV grid with formulas | open a CSV, edit a formula, save, reload |
| Files | free space in status bar + eject + permissions sheet | statvfs → label; umount → row disappears |
| System | NTP client + monitor enumeration | clock offset < 1 s; second output appears |
| Security | keyring file + firewall toggle (nftables) | store/read a secret; block a port and prove it |
| Recovery | persist open windows + autosave buffers | kill -9, relaunch, windows restored |


---

# Edition 2 — corrected, broader, and ecosystem-wide

**Why a second edition.** Edition 1 graded from `src/` alone and called several
workflows MISSING that already have a real implementation in a **submodule**.
That was wrong, and this section fixes it. The correction changes the headline:
the biggest gap is not missing software, it is **software that exists and cannot
be reached**.

## The five states a capability can be in

| State | Meaning |
|---|---|
| **IN-TREE** | implemented in `src/` and compiled into the product |
| **SUBMODULE-BUILT** | implemented in a submodule, produced by `make vendored`, **not reachable from the shell** |
| **SUBMODULE-UNBUILT** | implemented in a submodule that the default build never compiles |
| **SIBLING** | implemented in another own-repo (`wubunos`, `wubulens`, `wubucompositor`, `W-Code`, `WuBuKernel`) |
| **ABSENT** | nothing anywhere in the ecosystem |

Measured today: `src/apps/wubuoffice` is **8,028 files / 535 C files / 25 apps**;
`wubupad` 1,884 / 60; `src/lua/www` 664 / 312; `wet` 74 / 23; `vws` 27 / 9;
`src/brain` 3,291 / 1,474. `make vendored` drives CMake for wubupad + wubuoffice
and its build tree has artifacts. The shell exposes **one** entry for all of it
(`WubuOffice` → `wubushow`).

## Corrections to edition 1

| # | Was | Actually | Where |
|---|---|---|---|
| 28 | docx read PARTIAL | **SUBMODULE-BUILT** | `wubuoffice/apps/wubudoc`, `wubuword`, `wuburead`, `src/wubuoxml` |
| 31 | PDF read PARTIAL | **SUBMODULE-BUILT** | `apps/wubupdf` |
| 32 | PDF form PARTIAL | **SUBMODULE-BUILT** | `src/wubupdfform` |
| 33 | spreadsheet MISSING | **SUBMODULE-BUILT** | `apps/wubucell` + `src/wubuformula`, `wubupivot`, `wubusolver`, `wubusort`, `wubusubtotal` |
| 34 | slides PARTIAL | **SUBMODULE-BUILT** | `apps/wubushow` + `src/wubumasterslide`, `wubutransition`, `wubuanimation` |
| 35 | spell check MISSING | **SUBMODULE-BUILT** | `src/wubuspell`, `wubugrammar`, `wubuthesaurus`, `wubulang` |
| 36 | crash recovery PARTIAL | **SUBMODULE-BUILT** | `src/wubuautosave`, `wubuhistory` |
| 38 | export PDF MISSING | **SUBMODULE-BUILT** | `apps/wubupdf` (writer) |
| 39 | annotate/sign MISSING | **SUBMODULE-BUILT** | `src/wuburedact`, `wubusig`, `wubuwatermark` |
| 49 | subtitles MISSING | **SUBMODULE-BUILT** | `src/wubucaption` |
| 53 | search inside docs MISSING | **SUBMODULE-BUILT** | `src/wubuindex`, `wubudiff`, `wubucite` |
| 54 | zip PARTIAL | **SUBMODULE-BUILT** | `src/wubuzip`, `apps/wubuconv` |
| 74 | disk encryption PARTIAL | **SUBMODULE-BUILT** | `src/wubuencrypt` |
| 81 | screen reader MISSING | **SUBMODULE-BUILT** | `src/wubua11y`, `wubua11yannounce`, `wubua11ytree`, plus `wubudyslexia` |
| 87 | captions MISSING | **SUBMODULE-BUILT** | `src/wubucaption` |
| 28–39, 48 | (also) OCR, EPUB, XPS, QR, redaction | **SUBMODULE-BUILT** | `apps/wubuocr`, `src/wubuepub`, `wubuxps`, `wubuqr` |

**So the real gap for sixteen workflows is one thing: a shell surface and a
build gate.** `make vendored` is not in `make test`, not in the default build, and
its only shell entry opens one app out of 25.

## The integration gap, measured

- 25 office apps exist; **1** is in the registry (`WubuOffice` → `wubushow`).
- `make vendored` exists and produces artifacts, but nothing asserts it in the
  suite (audit F6). A CMake regression would be invisible.
- No file association points at `wubucell`, `wubudoc`, `wubupdf`, `wubuocr`.
  Double-clicking a .docx still goes to the in-tree notepad.

## New families (research-grounded)

### 12. Responsiveness — Nielsen's three limits (0.1 s / 1 s / 10 s)

| # | Workflow | State | Evidence |
|---|---|---|---|
| 112 | A click responds in under 100 ms | **IN-TREE** | measured 2.2 ms/frame render (WUBU_FPS_REPORT) |
| 113 | A window opens in under 1 s | **IN-TREE** | open animation completes in WUBU_ANIM_OPEN_MS |
| 114 | An app launches in under 1 s | UNVERIFIED | no timing gate |
| 115 | Typing never lags | **IN-TREE** | input sampled each frame, 60 fps active |
| 116 | A 10,000-entry folder lists in under 1 s | ABSENT | no large-directory gate |
| 117 | Scrolling stays at 60 fps | **IN-TREE** | scroll unit reworked this week |
| 118 | Boot to desktop in under 10 s | UNVERIFIED | no boot-time budget |
| 119 | Resume shows the session immediately | ABSENT | no suspend at all (#61) |
| 120 | A screenshot lands in under 1 s | **IN-TREE** | screenshot path gated |
| 121 | Search results appear while typing | **IN-TREE** | Start search re-ranks per keystroke |
| 122 | A notification appears instantly | **IN-TREE** | toast path |
| 123 | Progress is visible for anything over 1 s | PARTIAL | pkgmgr has progress; file copies do not |

### 13. Internationalization (Microsoft i18n checklist)

| # | Workflow | State | Evidence |
|---|---|---|---|
| 124 | Use the OS in a non-English locale | ABSENT | 0 locales (reference: CachyOS 235) |
| 125 | Dates and numbers in local format | ABSENT | hard-coded formats |
| 126 | Right-to-left layout | ABSENT | no bidi |
| 127 | Type CJK via an input method | ABSENT | no IME |
| 128 | CJK glyphs render | ABSENT | face covers Latin-1 only |
| 129 | Sort names in local collation | ABSENT | strcasecmp |
| 130 | Correct case mapping for non-ASCII | PARTIAL | Latin-1 table only |
| 131 | Currency and units | ABSENT | — |
| 132 | Timezone and DST display | PARTIAL | host clock only |
| 133 | Translate the shell's own strings | ABSENT | strings are literals |

### 14. Security baseline (CIS-style defaults)

| # | Workflow | State | Evidence |
|---|---|---|---|
| 134 | Updates on by default | PARTIAL | pkgmgr exists; no schedule |
| 135 | Firewall on by default | ABSENT | no firewall |
| 136 | Disk encrypted by default | ABSENT (submodule app only) | `wubuencrypt` |
| 137 | Screen locks after 15 min | PARTIAL | screensaver exists, no policy |
| 138 | Password policy on account creation | ABSENT | no accounts |
| 139 | Audit log of privileged actions | PARTIAL | `/n` + klog |
| 140 | Secure boot chain | **IN-TREE** | WuBuFW measures + chainloads |
| 141 | Signed updates | **IN-TREE** | Ed25519, gated |
| 142 | Least privilege for apps | PARTIAL | `wubu_cap` gates kernel paths |
| 143 | No default credentials anywhere | **IN-TREE** | no accounts to have them |

### 15. Backup and data integrity (3-2-1)

| # | Workflow | State | Evidence |
|---|---|---|---|
| 144 | Back up my files to another place | ABSENT | no backup tool |
| 145 | Restore from that backup | ABSENT | — |
| 146 | Recover an older version of a file | PARTIAL | `wubuhistory` in the submodule |
| 147 | Survive power loss without corruption | PARTIAL | TXFS WAL in kernel; guest fs is host files |
| 148 | Detect silent corruption (checksums) | PARTIAL | `wubu_crc32`, `wubu_sha256` exist; no scrub |
| 149 | Trash keeps deleted files until emptied | **IN-TREE** | `wubu_trash` |
| 150 | Resolve a sync conflict | ABSENT | `wubusync` in submodule, no conflict UI |
| 151 | Know how much space a folder uses | PARTIAL | Explorer sums a selection only |

### 16. Window management

| # | Workflow | State | Evidence |
|---|---|---|---|
| 152 | Snap a window to half the screen | **IN-TREE** | `win11_snap.c` |
| 153 | Snap to a quarter | **IN-TREE** | GAAD quarter snaps |
| 154 | Virtual desktops | **IN-TREE** | `wm_overview.c`, pager |
| 155 | Move a window to another desktop | **IN-TREE** | Win+Shift+arrow, gated |
| 156 | Alt-Tab with previews | **IN-TREE** | `alt_tab_switcher.c` |
| 157 | Alt-Tab within one app | ABSENT | — |
| 158 | Always-on-top | ABSENT | — |
| 159 | Per-monitor DPI | ABSENT | single output |
| 160 | Hot-plug a monitor | ABSENT | #71 |
| 161 | Restore windows after restart | ABSENT | #67 |
| 162 | Window rules (position by app) | ABSENT | — |
| 163 | Move/resize with the keyboard | PARTIAL | snap keys only |

### 17. Accessibility extras (WCAG 2.2)

| # | Workflow | State | Evidence |
|---|---|---|---|
| 164 | Visible focus ring everywhere | PARTIAL | icons/windows; not every control |
| 165 | Logical focus order | PARTIAL | per-surface, no global order |
| 166 | Every control has a name and role | ABSENT | no accessible tree in the shell |
| 167 | Text contrast ≥ 4.5:1 | PARTIAL | dark theme passes; some muted labels do not |
| 168 | Non-text contrast ≥ 3:1 | PARTIAL | window borders are low-contrast |
| 169 | No keyboard trap | UNVERIFIED | no trap tests |
| 170 | Targets ≥ 24 px | PARTIAL | title-bar buttons are 10 px |
| 171 | Reflow at 200 % zoom | PARTIAL | text scales; layout is fixed-width in places |
| 172 | Errors say what to do | PARTIAL | dialogs exist; messages are terse |
| 173 | Consistent help / shortcuts visible | PARTIAL | walkthrough exists; no shortcut sheet |

### 18. Printing, scanning, and the physical world

| # | Workflow | State | Evidence |
|---|---|---|---|
| 174 | Print preview | ABSENT | dialog submits, no preview |
| 175 | Choose printer and options | PARTIAL | `wubu_print.c` enumerates; no options UI |
| 176 | Paper size / duplex | ABSENT | — |
| 177 | Scan a document | ABSENT | no SANE path |

### 19. Power and thermals

| # | Workflow | State | Evidence |
|---|---|---|---|
| 178 | See battery health | ABSENT | percentage only |
| 179 | Charge limit | ABSENT | — |
| 180 | Sleep on idle | ABSENT | #61 |
| 181 | See throttling | PARTIAL | thermal temps in the tray |
| 182 | Fan control | ABSENT | — |
| 183 | Power profiles (quiet/performance) | ABSENT | — |

### 20. Update lifecycle

| # | Workflow | State | Evidence |
|---|---|---|---|
| 184 | Check for updates | PARTIAL | pkgmgr, no schedule |
| 185 | Download and install | **IN-TREE** | pkgmgr + signature gate |
| 186 | Reboot into the update | ABSENT | no boot slotting |
| 187 | Roll back a bad update | PARTIAL | transactions only |
| 188 | Know support/EOL status | ABSENT | — |
| 189 | Update firmware | PARTIAL | firmware page reads DMI; never flashes |

### 21. Onboarding and help

| # | Workflow | State | Evidence |
|---|---|---|---|
| 190 | First-run setup in one screen | **IN-TREE** | greeter |
| 191 | Discover keyboard shortcuts | PARTIAL | tutorials, no reference sheet |
| 192 | Tooltips on controls | PARTIAL | hover previews exist for taskbar only |
| 193 | Undo everywhere | PARTIAL | editors only |
| 194 | Search the settings | ABSENT | — |
| 195 | Reset one category to defaults | **IN-TREE** | `wubu_settings_reset_category` |
| 196 | Offline help | PARTIAL | docs in repo, no viewer (#11) |
| 197 | Accessibility setup during onboarding | ABSENT | flag exists, not offered |
| 198 | Choose language at first run | ABSENT | #124 |
| 199 | Enter a recovery mode | PARTIAL | QEMU/WuBuFW paths |
| 200 | Report a crash with context | PARTIAL | `crash_notice` banner |

---

## What this document is not

It is not a feature promise. Edition 1 graded 111 workflows from `src/` alone; edition 2 adds 89 more and corrects sixteen verdicts that were wrong because the implementation lives in a submodule. 26 rows remain **WORKS** (compiled in and exercised); everything else is graded from code, and the section above says which tree the code lives in. A row only becomes trustworthy when
its gate exists — that is the next job, one family at a time.
