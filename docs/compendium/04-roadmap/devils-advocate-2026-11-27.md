# Devil's-advocate audit of my own benchmark editions, 2026-11-27

Attacking editions 1-6 of `docs/compendium/04-roadmap/daily-driver-benchmarks.md`.
Method: re-measure every claim I inherited or wrote, with detector-liveness controls,
and read the audits this tree already generates instead of re-deriving them.

## 1. Method failure (mine, named)

| Failure | What it did | Evidence |
|---|---|---|
| Inherited claims were treated as facts | "no accounts", "no package db", "no print" were carried out of a compacted summary into editions 3-5 | `wubu_account.c` 84 lines + verifier, called by the greeter at `src/gui/shell/wubu_shell_greeter.c:436`; `wubu_print.c` 202 lines; `wubu_pkgx.c` 174 lines |
| Greps searched only `src/` with narrow terms | a capability implemented in `src/bridge/` or `src/runtime/` read as ABSENT | clipboard lives in `src/bridge/bridge.c` |
| The tree's own generated audits were never opened | `tools/gap_matrix.py` already maps 44 Win11 SystemApps; `docs/owned-surface.md` is 3,809 lines of per-module claims | both existed before this session |
| No detector-liveness control | an always-missing detector would have produced the same verdicts | added below |

## 2. Detector-liveness control (now run before any ABSENT verdict)

| Control | Result |
|---|---|
| KNOWN-PRESENT: `wubu_settings_save` | FOUND — `src/gui/wubu_settings_internal.h` |
| SYNTHETIC-ABSENT: `zzz_synthetic_sentinel` | MISSING (0 hits) |
| SHIM FLIP: create a file containing the sentinel, re-run the same detector | FOUND — `src/zzz_sentinel.c`, then removed |

The detector is live. The verdicts in editions 1-6 that were produced without
this control are re-measured below, and only the contradicted ones are changed.

## 3. Contradictions found in my own rows

| Row | I wrote | Measured | Correction |
|---|---|---|---|
| 201 "forgot my password" | ABSENT "no accounts to reset" | accounts exist: `wubu_account_build/_record_str/_verify`, first-run creates one at `wubu_shell_greeter.c:436` | **PARTIAL** — an account record and a verifier exist; there is no reset flow, no recovery, no admin path |
| 221 AC-2 account management | ABSENT "no accounts" | same, plus `wubu_userinfo.c` reads the login and refuses to fake a password change ("changing a password needs a tool; pretending otherwise is the greyed-button lie", line 92) | **PARTIAL** |
| 238 "user account created" (first day) | ABSENT | the greeter creates the record during OOBE | **PARTIAL** |
| 310 CIS 5 account management | ABSENT | same | **PARTIAL** |
| 138 password policy | ABSENT | no policy, no minimum length, no lockout — the record stores whatever was typed | ABSENT stands, but the reason changes: accounts exist, policy does not |
| EDR/clipboard | "hosted only" | `src/bridge/bridge.c` owns clipboard at the host seam | **PARTIAL**, correctly stated |

Everything else I checked (print, packages, people, biometric, screenshot, lock
screen) was already graded PARTIAL with the right file named; the earlier editions
were right there and I misread my own doc.

## 4. The claim-inflation check, which I had not applied

The tree's own matrix says **44 Win11 SystemApps: 41 owned, 1 gap, 8 n/a** — and
the single GAP row is `Win32WebViewHost`. But "owned" there means *a file with the
mapped name exists*, which is name parity, not behaviour. The 2026-11-26 audit
measured the other axis and it is the one that matters to a user:

| Measure | Reference | WuBuOS |
|---|---|---|
| system packages | CachyOS 1009 | 0 |
| executables | CachyOS 3115 | 10 app sources |
| systemd services | CachyOS 386 | 22 entries |
| icon files | CachyOS 40,726 | 8 app masters + 4 action glyphs (now 30 + 10) |
| fonts | Win11 336 | 1 bitmap face + an AA path |

So the honest headline is not "41/44 parity". It is: **the shell surfaces are
mapped; the operating system behind them is ~0.3 % of a distribution by every
countable axis.** Editions 1-6 should have carried that sentence and did not.

## 5. What the generated matrix says the real gap is

One row: **`Win32WebViewHost` — "no embedded web view host (`src/lua/www` is an app,
not a host control)"**. That is exactly the leg WuBuWeb owns, and WuBuWeb is
imported at `src/web` with the message layer (URL, HTTP, HTML-to-text) and 38
tests. The host control — an embedded view a `.wet` applet can put in a window —
is still ABSENT and is now named instead of vague.

## 6. New requirements found by research this round (edition 7 sources)

- [Apple HIG — The menu bar](https://developer.apple.com/design/human-interface-guidelines/the-menu-bar):
  a global menu bar per application, with a standard order and a searchable Help menu.
- [EN 301 549 v4.1.0](https://www.etsi.org/deliver/etsi_EN/301500_301599/301549/04.01.00_20/en_301549v040100ev.pdf)
  (the EU procurement standard, WCAG 2.2 at OS level): keyboard operability, visible
  focus, contrast, reflow, text spacing, no timing traps.
- [OSWorld 2.0](https://osworld-v2.xlang.ai/) — computer-use agents are evaluated on
  long-horizon multi-app workflows, which is the strongest argument for the app
  pipeline in `gui-standard.md`: an agent needs the SAME single app registry a user does.
- [Wayland clipboard and drag & drop](https://wayland-book.com/clipboard/dnd.html) —
  data offers, MIME types and DnD as protocol objects, not as a global string.
- [Windows 11 24H2 what's new](https://learn.microsoft.com/en-us/windows/whats-new/whats-new-windows-11-version-24h2)
  — servicing cadence and feature flags as a first-class desktop concern.
- [CIS Debian Linux Benchmark](https://www.cisecurity.org/benchmark/debian_linux) —
  the hardening family: disk encryption, firewall default-deny, audit trail, file
  integrity, unattended security updates.
- [SteamOS desktop mode](https://help.steampowered.com/en/faqs/view/69E3-14AF-9764-4C28) —
  two shells over one system (game mode / desktop mode) with the same files and apps.

## 7. Findings as legs, with the smallest real increment

| # | Finding | Smallest increment | Gate |
|---|---|---|---|
| D1 | account record exists, no policy | minimum length + refusal, in `wubu_account_build` | a 3-char password is refused; a 12-char one builds |
| D2 | no password reset | reset writes a new record after re-auth | verify(old) → reset → verify(new) |
| D3 | no embedded web view | `WubuWebView` control: a `.wet` applet places a URL-bearing view; WuBuWeb renders text | an applet shows a fetched page's title and links |
| D4 | clipboard is a host string | a `WubuClip` object with MIME types, owned by the bridge | copy rich + plain, paste picks by MIME |
| D5 | no app registry read by all surfaces | `.wua` manifest promote (gui-standard §4) | one manifest appears in Start, desktop, taskbar and icon resolver |
| D6 | no audit trail of agent actions | verdict log in `/n/log/agent` | an agent action appears with its verdict |

## 8. Standing rule added

Before any ABSENT verdict: (1) run the liveness control above, (2) read
`tools/gap_matrix.py` output and `docs/owned-surface.md` for the same term,
(3) grep `src/` **and** `src/bridge`, `src/runtime`, `src/hosted`, `src/firmware`,
(4) count both sides if the claim is parity. An ABSENT verdict without those four
is a guess.
