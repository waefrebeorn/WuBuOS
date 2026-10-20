# Omarchy → WuBuOS

Reference: <https://github.com/omacom/omarchy> — "Beautiful, Modern & Opinionated
Linux" (DHH's Arch + Hyprland setup). **MIT licensed**, so unlike the GPL theme
forks, both the ideas and the code are usable.

Two ideas were taken; neither is a port of their code.

## 1. One command, no decisions (`make setup`)

Omarchy's premise is that installing a desktop should not be a series of
choices. WuBuOS had a quickstart, but it was still a list you had to read.

```sh
make setup
```

Builds, runs the onboarding gate, boots the firmware, and says so. Measured
output: `ALL DOCUMENTED STEPS PASS` including `WuBuFW reached its shell,
milestones passed`.

## 2. One file is the whole theme

Omarchy's `manual/43-making-your-own-theme.md` treats a theme as a single
source of truth every surface reads. WuBuOS had runtime theme *switching*, but
palettes lived in a C array — adding one meant rebuilding.

Now: `themes/luna.conf` (30 keys) → `wubu_theme_parse()` /
`wubu_theme_load_file()` → `wubu_theme_set_custom()`, which adopts the palette
as `THEME_WUBU_CUSTOM` and switches to it live.

- `src/gui/wubu_theme_file.{c,h}` — pure parser (no I/O in `parse`, so it is
  usable from the kernel and testable without files); `load_file` is the hosted
  convenience, capped at 64 KiB.
- Unknown keys, bad hex, missing `=`, trailing junk are **errors**, not ignored
  — a typo'd theme must not silently half-apply.
- `test_theme_file` — **20 checks, 0 failures**, including that unspecified keys
  keep their previous value and a missing file returns `-2`.
- Verified against the real file: `themes/luna.conf` applies 30 keys,
  `title=0A246A face=ECE9D8 desktop=3A6EA5`.

`wubu_theme_set_custom()` required dropping `const` from the internal theme
table, which is what makes a file-loaded palette persist across switches.

## Not taken

- The rest of Omarchy is a Hyprland/Wayland desktop with Quickshell; our
  compositor is its own thing, so the QML shell, keybinding layer and widget
  set do not transfer.
- Their installer targets Arch packages. `make setup` is the equivalent gesture,
  not a package manager.
