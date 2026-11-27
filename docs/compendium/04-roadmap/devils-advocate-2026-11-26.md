# Devil's-advocate audit against tiny11 (Win11) and CachyOS, 2026-11-26

Method: measure both sides, never paraphrase. References are the extracted images under
`/home/wubu/opt/refos/{win11,cachyos}`; ours is this tree at the audit's HEAD. Every number below was
produced by a command in this session, not from memory.

## 1. Scale: we are at parity with a package LIST, not with an OS

| Measure | Reference | WuBuOS | Command |
|---|---|---|---|
| system packages | CachyOS **1009** | **0** (no package db; the OS is the tree) | `ls rootfs/var/lib/pacman/local` |
| executables in /usr/bin | CachyOS **3115** | **10** app sources | `ls rootfs/usr/bin` |
| systemd services | CachyOS **386** | **22** entries in the service manager | `ls .../systemd/system/*.service` |
| icon files | CachyOS **40,726** SVG | **8** app masters + 4 action glyphs | `find .../icons -name '*.svg'` |
| font files | Win11 **336** | **1** 8x8 bitmap (+ an AA path) | `ls win11/ext/Windows/Fonts` |
| Win11 SystemApps | **44** | 41 owned / 1 gap / 8 n/a | `tools/gap_matrix.py` |

The "42/42 owned" headline that this project has been quoting is parity with the 44-item SystemApps
directory. Against an actual distribution it is 0.3 % of the binaries, 5.7 % of the services, 0.03 % of the
icons and 0.3 % of the fonts. That is the honest framing: the *shell surfaces* are mapped; the *operating
system* is a round 1.

## 2. The ledger was counting files, not features (fixed this turn)

`tools/gap_matrix.py` decided "owned" with `find()`: the first path whose glob exists. It never asked
whether the file is compiled, called, or working. Two rows proved it:

- `Win32WebViewHost` was "owned" by `src/lua/www` -- an EXAMPLE file inside a submodule. A browser app is not
  an embedded host control. It is now declared a GAP.
- 3 rows admit their own partiality in their notes and still counted as owned: `ananicy-cpp` ("profile only,
  no live priority daemon"), `zram-generator` ("tunables only"), `fwupd` ("no flashing").

Fixed: every owning path must now appear in some `mk/*.mk` fragment, and `--check` fails on a row whose
owner is not built. Result: **41 owned, 1 missing, 8 n/a, 0 undecided, 0 not built**. The number can no
longer be inflated by a file that nothing compiles.

## 3. Findings, ranked, each with its smallest real increment

**F1. Typography is the largest visible gap (336 font files vs one bitmap).**
Our text is an 8x8 bitmap with an AA path; the reference ships proportional variable fonts. Every surface
inherits it, which is why "bootleg" keeps recurring even when layout is right.
*Increment:* add one real proportional face (baked coverage at 2 sizes) behind `vbe_draw_text`, keep the
bitmap as the fallback; gate with a golden glyph table + a legibility check at 12 px.

**F2. Icons: 8 masters vs 40,726 files.**
The taskbar and Start show placeholders/initials for most apps.
*Increment:* icon coverage per app registry entry, then file-type icons for Explorer; gate by asserting
every registered app id resolves to a master (a missing master fails the gate).

**F3. No package manager database (1009 packages vs 0).**
`wubu_pkgmgr` installs .wubu packages, but there is no installed-package DB to query, so "what is on this
machine" cannot be answered. `wubu_pkgx` registers packaged apps only.
*Increment:* a package DB file + `list/owned-by` queries sourced from it; gate on install → query → remove.

**F4. Services: 22 entries, no dependency graph, no restart policy.**
CachyOS has 386 units with ordering. Ours is a table.
*Increment:* add `after=`/`requires=` to the service table and a topological start; gate on a dependent
service refusing to start when its requirement fails.

**F5. Shell commands: 10 builtins.**
A CachyOS user has 3115 binaries; our terminal has ~10 builtins and no package-provided commands.
*Increment:* one real external-command path (exec a host binary through the existing exec seam) gated by a
test that runs a real program and asserts its stdout.

**F6. `fwupd` reports identity and never flashes.**
The firmware page reads DMI and discovers an update agent; the row says so, and it is the correct scope for
a page that cannot flash. Correction to an earlier draft of this document: I wrote that `make vendored` was
unproven for wubuoffice, then measured it -- `make -n vendored` emits 2715 lines touching wubuoffice and
`mk/vendored.mk:16` declares `vendored: wubupad wubuoffice`. The wiring exists; whether the build SUCCEEDS
is a separate question this audit did not answer, and it is not claimed here.
*Increment:* run `make vendored` in the suite (or a tiered target) and assert a produced artifact, so
"vendored" stops being a static claim.

## 4. Claims this audit could NOT falsify (verified, not asserted)

- The Control Panel renders and its pages are reachable (measured: 14 sidebar entries, live frames).
- Render cost: 2.2 ms/frame with MIT-SHM present (10.94 ms before the fixes), measured via `WUBU_FPS_REPORT`.
- A running pin keeps its taskbar slot (mutation-proven in `test_wm`).
- Lowercase text renders from its own glyph table (`test_font_case`, 58 checks).

## 5. Status

| Leg | State |
|---|---|
| Ledger honesty (built-ness gate, Win32WebViewHost) | **CLOSED**, gated by `test_gap_matrix` |
| F1 proportional font | open, no owner yet |
| F2 icon coverage | open |
| F3 package DB | open |
| F4 service graph | open |
| F5 external command path | open |
| F6 vendored build / fwupd note | open |

Nothing in section 5 is "in flight": they are named, estimated and unstarted, and this document exists so
the next session does not rediscover them as adjectives.
