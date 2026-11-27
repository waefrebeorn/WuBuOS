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


---

# Edition 3 — requirements from support data, standards, and migration pain

Three more list sources, chosen because they are what people actually hit rather
than what vendors advertise:

- **The ticket list.** The 13 most common help-desk tickets of 2025 — password
  resets first, then software install/update errors, connectivity, email,
  hardware faults, access to shared resources, performance, bug reports, data
  loss, device onboarding, licences ([Whatfix](https://whatfix.com/blog/common-help-desk-tickets/)).
  Each one is an OS requirement; a system that cannot answer the ticket is the
  ticket.
- **The control catalogue.** NIST SP 800-53 Rev. 5's control families — AC
  access control, AU audit, CM configuration management, IA identification and
  authentication, IR incident response, RA risk assessment, SC communications
  protection, SI system integrity, PT PII processing, SR supply chain
  ([NIST](https://csrc.nist.gov/pubs/sp/800/53/r5/upd1/final)).
- **The migration pain list.** The recurring complaints about desktop Linux:
  hardware support, HiDPI, suspend, Bluetooth audio, printing, codecs, font
  quality, packaging, theme consistency, the X11/Wayland split
  ([Datamation](https://www.datamation.com/open-source/eight-complaints-about-the-linux-desktop/)).

## 22. The tickets a person will actually file (20)

| # | The ticket | State | Evidence |
|---|---|---|---|
| 201 | "I forgot my password" — and can reset it without IT | **ABSENT** | there are no accounts to reset |
| 202 | "It won't install" — with a reason the user can act on | PARTIAL | pkgmgr returns codes; no user-facing error text |
| 203 | "I can't get online" — and a way to see why | **ABSENT** | #10 |
| 204 | "My email isn't sending" | ABSENT | #18 |
| 205 | "Something is broken in the hardware" — and a report | PARTIAL | sensors exist; no diagnostics bundle |
| 206 | "I can't reach the shared drive" | ABSENT | #57 |
| 207 | "It's slow" — and something that shows what is slow | PARTIAL | frame timing only exists for developers |
| 208 | "The app crashed" — with context | PARTIAL | `crash_notice.c` banner |
| 209 | "I lost a file" | PARTIAL | Trash only; no versions, no backup |
| 210 | "Set up my new machine" | PARTIAL | greeter + onboarding sentinel |
| 211 | "Is my licence still valid?" | ABSENT | no licence concept |
| 212 | "My screen is frozen" — and a way out | PARTIAL | Alt-F4/close; no session recovery |
| 213 | "The printer won't print" | PARTIAL | backend only; no queue or status |
| 214 | "My second monitor isn't detected" | ABSENT | #71 |
| 215 | "The VPN won't connect" | ABSENT | #8 |
| 216 | "The browser is slow" | ABSENT | #11 |
| 217 | "My disk is full" — and a warning first | ABSENT | #68 |
| 218 | "No sound" | PARTIAL | mixer works; no output-device chooser |
| 219 | "My mouse/keyboard stopped" | PARTIAL | input path, no device list |
| 220 | "The clock is wrong" | ABSENT | #69 |

## 23. Security and privacy control families (NIST SP 800-53 Rev. 5, desktop subset) (15)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 221 | Account management (AC-2) | **ABSENT** | no accounts |
| 222 | Least privilege (AC-6) | PARTIAL | `wubu_cap` capability gates |
| 223 | Audit event logging (AU-2/3) | PARTIAL | kernel log + `/n` |
| 224 | Audit review (AU-6) | ABSENT | nothing reads the log back |
| 225 | Configuration baseline (CM-2/6) | PARTIAL | settings file is the baseline |
| 226 | Identification and authentication (IA-2/5), MFA | ABSENT | — |
| 227 | Incident handling (IR-4/6) | PARTIAL | crash banner |
| 228 | Vulnerability scanning / advisories (RA-5) | ABSENT | no advisory feed |
| 229 | Boundary protection (SC-7) — firewall | ABSENT | #75 |
| 230 | Cryptographic protection (SC-8/13) | PARTIAL | package signing only |
| 231 | Integrity checking of the running system (SI-7) | PARTIAL | packages verify; system files do not |
| 232 | Malicious-code protection (SI-3) | ABSENT | — |
| 233 | Spam/phishing protection (SI-8) | ABSENT | — |
| 234 | PII processing rules (PT) | ABSENT | — |
| 235 | Supply-chain provenance (SR-3/11), SBOM | PARTIAL | signed packages; no SBOM |

## 24. New-machine onboarding, the IT checklist a person gets on day one (12)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 236 | Disk encrypted before hand-over | **ABSENT** | #74 |
| 237 | Updates current at hand-over | PARTIAL | pkgmgr, no schedule |
| 238 | Device named, user account created | ABSENT | no accounts |
| 239 | VPN profile installed | ABSENT | #8 |
| 240 | Printer configured | PARTIAL | enumeration only |
| 241 | Password manager provisioned | ABSENT | #76 |
| 242 | Backup configured and tested | ABSENT | #60 |
| 243 | Second factor enrolled | ABSENT | — |
| 244 | Asset recorded in an inventory | ABSENT | — |
| 245 | User shown the shortcuts that matter | PARTIAL | tutorials exist, no shortcut sheet |
| 246 | Offboarding: wipe or hand back | ABSENT | — |
| 247 | Device transfer keeps the user's data | ABSENT | — |

## 25. Privacy-by-design requirements (10)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 248 | Data minimisation | PARTIAL | no telemetry at all; local-only stores |
| 249 | Purpose limitation and consent | ABSENT | nothing asks |
| 250 | Export my data (portability) | ABSENT | — |
| 251 | Delete my data (erasure) | PARTIAL | Trash; no "delete everything" |
| 252 | Breach notification path | ABSENT | — |
| 253 | A privacy dashboard | PARTIAL | settings page of flags |
| 254 | Telemetry off by default | **IN-TREE** | there is no telemetry |
| 255 | Location permission gate | ABSENT | no producers |
| 256 | Camera/microphone in-use indicator | ABSENT | — |
| 257 | Per-app data access list | ABSENT | — |

## 26. Energy and battery (8)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 258 | Idle power budget | PARTIAL | 30 fps idle repaint; measured, not budgeted |
| 259 | Few background wakeups | PARTIAL | AGI loop ticks twice a second; no budget |
| 260 | Display sleeps when idle | PARTIAL | screensaver exists |
| 261 | System sleeps when idle | ABSENT | #61 |
| 262 | Charge limit | ABSENT | #179 |
| 263 | Battery health reported | ABSENT | #178 |
| 264 | Power profiles | ABSENT | #183 |
| 265 | An energy report a user can read | ABSENT | — |

## 27. The migration pain list (12)

| # | Complaint about desktop Linux | State | Evidence |
|---|---|---|---|
| 266 | Hardware support out of the box | PARTIAL | driver registry; no firmware blobs |
| 267 | HiDPI scaling that works | PARTIAL | text scales; no fractional scaling |
| 268 | Suspend that reliably resumes | ABSENT | #61 |
| 269 | Bluetooth audio | ABSENT | no BlueZ path |
| 270 | Printing that just works | PARTIAL | backend, no queue |
| 271 | Codecs (mp4, mp3, h264) | PARTIAL | ffmpeg in the hosted leg |
| 272 | Font rendering quality | PARTIAL | one 11 px face + bold |
| 273 | Application packaging and install UX | PARTIAL | pkgmgr, no store |
| 274 | Theme consistency | PARTIAL | one theme family, mixed eras |
| 275 | The X11/Wayland split | **IN-TREE** | own compositor + X11 present |
| 276 | Multi-monitor hotplug | ABSENT | #71 |
| 277 | Accessibility there on day one | PARTIAL | a11y lives in the office submodule |

---


---

# Edition 4 — three audiences: the person, the developer, the administrator

A user, a developer and an admin need different things from the SAME machine, and
editions 1–3 only asked the user's question. This edition adds the other two,
grounded in their own lists:

- **Admin**: the 18 CIS Critical Security Controls ([CIS](https://www.cisecurity.org/controls/cis-controls-list))
  and the system-administrator maintenance checklist (patches, backups verified,
  logs reviewed, capacity, accounts, certificates) plus the endpoint/MDM
  checklist (inventory, policy, encryption, remote wipe, compliance reporting).
- **Developer**: the dev-workstation setup list (toolchain, debuggers, profilers,
  libraries, containers — [Red Hat](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/8/html/developing_c_and_cpp_applications_in_rhel_8/setting-up-a-development-workstation_developing-applications))
  and the onboarding-to-first-commit list.
- **Cross-audience**: shared-device and family needs that neither list covers.

## 28. Developer needs — "can I build and debug on this machine?" (28)

| # | Need | State | Evidence |
|---|---|---|---|
| 278 | A compiler and linker on the machine | PARTIAL | WuBuNOS backends in-tree; no user-facing `cc` |
| 279 | Get source onto the machine (clone) | **ABSENT** | no git |
| 280 | Build the project from inside the OS | PARTIAL | `make` exists on the host leg only |
| 281 | Run the test suite from inside the OS | PARTIAL | the suite is host-side |
| 282 | Set a breakpoint and step | **ABSENT** | no debugger |
| 283 | Profile where time goes | PARTIAL | `WUBU_FPS_REPORT`, `--bench-render` |
| 284 | A REPL to try a snippet | **IN-TREE** | HolyC REPL, `.wet`/Lua applets |
| 285 | Install a dependency/library | PARTIAL | pkgmgr has no dev packages |
| 286 | An isolated build container | **IN-TREE** | `wubu_container`, seccomp/cgroup isolation |
| 287 | Cross-compile to another ISA | **IN-TREE** | 11 backends in wubunos |
| 288 | Run a foreign binary | **IN-TREE** | WEMU |
| 289 | An editor that knows the language | PARTIAL | highlighting primitives, no language model |
| 290 | Find a symbol across the tree | **IN-TREE** | `wubugrep` (own SIMD grep) |
| 291 | Version control UI | ABSENT | — |
| 292 | Headers/SDK to write an app | PARTIAL | in-tree headers, no installed SDK |
| 293 | Sample apps and templates | PARTIAL | `.wet` tutorials |
| 294 | Crash dumps with symbols | ABSENT | — |
| 295 | Logging and tracing | PARTIAL | `klog`, no tracer |
| 296 | Reproducible builds | UNVERIFIED | nothing pins the toolchain |
| 297 | CI that runs ON the OS | ABSENT | CI is the host's |
| 298 | Terminal multiplexing | ABSENT | no panes/sessions |
| 299 | Remote development over SSH | PARTIAL | via the terminal's exec path |
| 300 | Hot reload / file watching | ABSENT | — |
| 301 | Publish a package | ABSENT | signing exists, no publish flow |
| 302 | Pin a toolchain version | ABSENT | — |
| 303 | Portable editor config | ABSENT | — |
| 304 | Documentation for the OS API | PARTIAL | `docs/` + compendium, no API reference generator |
| 305 | Deterministic test ordering/isolation | PARTIAL | tests use fixed paths in places |

## 29. Administrator needs — CIS Controls v8, mapped to one machine (30)

| # | Control | State | Evidence |
|---|---|---|---|
| 306 | 1 Inventory of hardware assets | **ABSENT** | nothing enumerates devices into a record |
| 307 | 2 Inventory of software | PARTIAL | pkgmgr DB lists installed packages |
| 308 | 3 Data protection and classification | ABSENT | — |
| 309 | 4 Secure configuration baseline | PARTIAL | settings file; no baseline compare |
| 310 | 5 Account management | **ABSENT** | no accounts |
| 311 | 6 Access control management | PARTIAL | `wubu_cap` |
| 312 | 7 Continuous vulnerability management | ABSENT | no advisories |
| 313 | 8 Audit log management | PARTIAL | kernel log; no rotation, no shipping |
| 314 | 9 Email and web protections | ABSENT | no mail, no browser |
| 315 | 10 Malware defenses | ABSENT | — |
| 316 | 11 Data recovery | ABSENT | no backup |
| 317 | 12 Network infrastructure management | ABSENT | — |
| 318 | 13 Network monitoring and defense | PARTIAL | counters only |
| 319 | 14 Security awareness training | PARTIAL | walkthrough + tutorials |
| 320 | 15 Service provider management | ABSENT | — |
| 321 | 16 Application software security | PARTIAL | package signing, seccomp profiles |
| 322 | 17 Incident response management | PARTIAL | crash banner; no runbook |
| 323 | 18 Penetration testing | ABSENT | — |
| 324 | Enrol a device in a fleet | ABSENT | no MDM/identity |
| 325 | Push policy to a device | ABSENT | — |
| 326 | Remote view/control a session | ABSENT | — |
| 327 | Compliance report per device | ABSENT | — |
| 328 | Patch rings and maintenance windows | PARTIAL | pkgmgr, no scheduling |
| 329 | Verify a backup restores | ABSENT | no backup |
| 330 | Capacity and disk forecasting | ABSENT | — |
| 331 | Certificate lifecycle | ABSENT | — |
| 332 | Staged updates with rollback | PARTIAL | txn only |
| 333 | Licence tracking | ABSENT | — |
| 334 | Export a support bundle | ABSENT | no diagnostics package |
| 335 | Wipe a device at hand-back | ABSENT | — |

## 30. Cross-audience: shared, family and guest use (10)

| # | Need | State | Evidence |
|---|---|---|---|
| 336 | Separate user accounts | **ABSENT** | single user by design (ZealOS heritage) |
| 337 | Switch users without closing apps | ABSENT | follows 336 |
| 338 | Guest session | ABSENT | — |
| 339 | Kiosk / locked-down mode | ABSENT | — |
| 340 | Parental limits | **IN-TREE** | `parental_guard.c`, family limits in the hosted loop |
| 341 | Screen time report | PARTIAL | playtime tracked for games only |
| 342 | Remote help for a family member | ABSENT | — |
| 343 | Hand a device to someone else safely | ABSENT | — |
| 344 | Factory reset | ABSENT | — |
| 345 | Upload diagnostics to get help | ABSENT | — |

---


---

# Edition 5 — the four things this OS claims to be

A consumer OS is judged by the user list. This one also claims four distinct
identities — **AGI**, **agentic OS**, **EDR**, and **live-coding TempleOS
descendant** — and each has its own published requirements list. Grading them
separately is the only way to see whether the claims are engineering or
vocabulary.

Sources: the agentic-OS requirements checklist (per-agent authority, limits
enforced at the moment of action, held work routed to a named person, every
verdict recorded, authority that expires — [Pryme](https://www.prymeintelligence.com/blog/agentic-operating-system-requirements-checklist/)),
the [OWASP Top 10 for Agentic Applications 2026](https://genai.owasp.org/resource/owasp-top-10-for-agentic-applications-for-2026/)
risk taxonomy and [NIST AI RMF](https://airc.nist.gov/airmf-resources/airmf/5-sec-core/)
(govern, map, measure, manage); for EDR, the eight evaluation criteria
([Fortinet](https://www.fortinet.com/content/dam/fortinet/assets/checklists/ch-top-eight-considerations-evaluating-edr-solutions.pdf))
— protection efficacy, ransomware defense and rollback, anti-tampering,
manageability, performance — read against MITRE ATT&CK coverage; for live
coding, TempleOS's JIT-and-REPL model ([HolyC docs](https://pacakutekagudabasa.github.io/HolyC/docs.html))
and Dart's hot-reload semantics (late binding, immutable methods, fields retain
values — [Dart hot reload](https://dart.googlesource.com/sdk/+/9b157b67e3aa4f8839df3678739c3c1e671345f0/runtime/docs/hot-reload.md)).

## 31. Agentic OS requirements (22)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 346 | A perceive → decide → act → learn loop exists | **IN-TREE** | `wubu_agi_play_tick`, `wubu_agi_play_learn` in the hosted loop |
| 347 | The agent has an identity a third party can verify | **IN-TREE** | federation agent card → `/n/agent` |
| 348 | The agent's memory is a first-class namespace | **IN-TREE** | KV-FS published as `/n/kv`, tick files per world step |
| 349 | Per-agent granted authority, not ambient power | PARTIAL | `wubu_cap` capability gates in the kernel path |
| 350 | Authority expires | **ABSENT** | nothing revokes a grant on time |
| 351 | Limits enforced AT the moment of action | PARTIAL | caps check at syscall entry; no per-action policy |
| 352 | Human approval for held work, routed to a named person | **ABSENT** | no approval queue anywhere in `src/` |
| 353 | Every verdict/action recorded, exportable | PARTIAL | `/n` + klog; no action log with verdicts |
| 354 | A dry-run / simulate mode for an action | **ABSENT** | — |
| 355 | Tool calling: the model can invoke OS verbs | PARTIAL | app launching via the agent channel; no tool registry |
| 356 | Tool surface is enumerable and documented | ABSENT | — |
| 357 | Prompt-injection resistance (goal hijacking) | ABSENT | no untrusted-content boundary |
| 358 | Memory poisoning detection | ABSENT | — |
| 359 | Cascading-failure containment (blast radius) | PARTIAL | process isolation; no agent-level budget |
| 360 | Resource limits per agent (tokens/CPU/IO) | **IN-TREE** | `control_panel_limits.c`, `wubu_parental_enforce` |
| 361 | Excessive agency is bounded by capabilities | PARTIAL | #349 |
| 362 | Output handling: agent text is never executed blindly | PARTIAL | actions go through the same launch path |
| 363 | Observability: a trace of what the agent did | PARTIAL | tick stream; no span/trace view |
| 364 | Evaluation on YOUR cases, not a demo | ABSENT | no eval harness in the OS |
| 365 | Model change under the operator's control | PARTIAL | `control_panel_ai.c` |
| 366 | Kill switch that is not the power button | ABSENT | `wubu_agi_play_set_input_injection(0)` exists; no user stop |
| 367 | AI-RMF governance artefacts (map/measure/manage) | ABSENT | — |

## 32. EDR requirements (18)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 368 | Process creation telemetry with lineage | PARTIAL | ppid recorded on exec paths |
| 369 | File write/rename/delete telemetry | ABSENT | — |
| 370 | Network connection telemetry | ABSENT | — |
| 371 | Persistence-point monitoring (autostart) | PARTIAL | `service_mgr` owns autostart; no watch |
| 372 | In-memory / fileless detection | ABSENT | — |
| 373 | Detection rules with named alerts | PARTIAL | `secmon` events, no rule language |
| 374 | IOC matching (hash, path, domain) | ABSENT | — |
| 375 | Sigma/YARA-style importable rules | ABSENT | — |
| 376 | Response: kill process | PARTIAL | host exec can terminate |
| 377 | Response: quarantine a file | **ABSENT** | — |
| 378 | Response: roll back changes (ransomware) | **ABSENT** | no versioned file store |
| 379 | Response: isolate the host | ABSENT | no firewall to isolate with (#229) |
| 380 | Anti-tamper: agent cannot be silently disabled | PARTIAL | boot attest measures the chain |
| 381 | Offline protection (no cloud dependency) | PARTIAL | local detection only, nothing to call |
| 382 | ATT&CK coverage mapped and reported | ABSENT | — |
| 383 | Alert triage UI with history | PARTIAL | "EDR Activity" app shows events |
| 384 | Exportable incident bundle | ABSENT | #334 |
| 385 | Performance budget for the agent | UNVERIFIED | no measurement |

## 33. TempleOS-style live compiler inside the OS (15)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 386 | A language that compiles while the system runs | **IN-TREE** | HolyC via `holyd` + JIT |
| 387 | A REPL where a line executes immediately | **IN-TREE** | `shell/holyc_repl.c` |
| 388 | Compile a whole program from inside the OS | PARTIAL | compiler in-tree; no in-OS `make` flow |
| 389 | Redefine a function and have callers use it | **ABSENT** | no edit-and-continue |
| 390 | Old frames keep old code (no crash mid-swap) | ABSENT | follows 389 |
| 391 | `#exe`-style in-source execution blocks | PARTIAL | REPL accepts code; no block semantics |
| 392 | The running system is the development system | PARTIAL | REPL + apps, no editor→compile→run loop |
| 393 | Source, data and OS state persist together | PARTIAL | files persist; no image |
| 394 | Program state survives a restart of the tool | ABSENT | no image persistence |
| 395 | Hypertext docs interleaved with code | ABSENT | docs are markdown, not callable |
| 396 | JIT'd code is inspectable/disassemblable | **IN-TREE** | x86 encoder + disassembler |
| 397 | Errors point at the source line in-OS | PARTIAL | compiler diagnostics exist |
| 398 | Tasks/windows are language-level objects | PARTIAL | apps are C, not language objects |
| 399 | Boot straight into the live environment | **IN-TREE** | HolyC terminal is a boot app |
| 400 | Ring-0 affordance for the live coder | **IN-TREE** | kernel runs in-process |

## 34. Live GUI coding while it runs (14)

| # | Requirement | State | Evidence |
|---|---|---|---|
| 401 | Design a window without C | **IN-TREE** | `.wet` applets, VWS designer |
| 402 | Edit the running UI and see it change | **ABSENT** | no hot reload of a running applet |
| 403 | Reload keeps widget state (fields retain values) | **ABSENT** | — |
| 404 | Reload keeps call sites future-bound (late binding) | ABSENT | — |
| 405 | Layout recompute without restarting | PARTIAL | designer preview only |
| 406 | Live style/theme change while running | **IN-TREE** | theme switch applies live |
| 407 | Live font/scale change while running | **IN-TREE** | fixed this week; layout re-measures |
| 408 | Script an event handler without rebuild | **IN-TREE** | `.wet` scripts |
| 409 | Inspect a live widget tree | PARTIAL | designer tree in the submodule |
| 410 | Design-time ⇄ run-time same code path | PARTIAL | applet runtime vs designer |
| 411 | Undo a bad live edit without restart | ABSENT | — |
| 412 | Live reload over a remote session | ABSENT | — |
| 413 | Hot-swap an app's C code | ABSENT | no in-OS linker |
| 414 | A visible "this is live" indicator | ABSENT | — |

---

## What this document is not

It is not a feature promise. Edition 1 graded 111 workflows from `src/` alone; edition 2 adds 89 and corrects sixteen verdicts that were wrong because the implementation lives in a submodule; edition 3 adds 77 from support-ticket data, NIST SP 800-53's control families, the IT onboarding checklist and the migration pain list; edition 4 adds 68 more for the developer (Red Hat workstation list, onboarding-to-first-commit) and the administrator (the 18 CIS Controls, sysadmin maintenance and MDM checklists) plus shared-device needs -- 345 workflows across three audiences; edition 5 adds 69 more for the four identities this OS claims (AGI, agentic OS, EDR, live-coding TempleOS descendant) -- 414 workflows in total. 26 rows remain **WORKS** (compiled in and exercised); everything else is graded from code, and the section above says which tree the code lives in. A row only becomes trustworthy when
its gate exists — that is the next job, one family at a time.
