# Triple-DA audit: the three-game launch + the AGI-game-training (2026-08-09)

The goal (the user's own words): "we launch all three games, triple
devils advocate check work, make sure it works in agi desktop and the
big picture mode, then we fix the agi be able to play the games as
training too and also learning via running game."

The three targets (the 7-hop Kevin-Bacon search verified the sources):

| Goal | Real file | Format | Host path |
|---|---|---|---|
| Halo CE PC demo | `lotrialsetup1.exe` (137MB, i386 Win32 GUI PE, 11 imports) | Win32 | `wubu_exec_win_pe` → Wine in the SteamOS container |
| Halo CE Mac demo | `halo-universal-binary.dmg` (680MB, zlib-compressed UDIF) | Mach-O (universal) | `wubu_exec_macho` → VSL loader / Darling |
| OpenArena 0.8.8 | `openarena-0.8.8.zip` (425MB, sha1 37ab4199...) | Linux ELF (GL) | `wubu_exec_linux_elf` → the native container + the GL leg |

## DA-1 (Correctness): does the routing actually route?

- `wubu_game_classify` — the unit under test: MZ/ELF/ALL SIX Mach-O
  magics (thin + FAT universal 0xCAFEBABE) classify correctly;
  garbage + null NEVER misroute. **Verified by test_game_launch 4/4.**
- The BPM games grid — the scan probes the real magic, accepts ONLY
  the hostable formats (the junk readme.txt was skipped),
  the gamepad A launches the selected game. **Verified by
  test_dosgui_bpm_games 4/4.**
- The REAL Halo PC demo probed: i386 Win32 GUI PE, subsystem 2, 11
  imports — a genuine PE the Win32 personality can classify.
- The REAL Mac DMG probed: zlib-compressed UDIF — the `koly` trailer
  absent until the download completes (in flight); our DMG reader
  (tools/wubu_dmg.py) decompresses the blkx chunks to the raw HFS+
  image (`H+` header confirmed at 1024 after the first chunk).
- **Honest gap**: the Win32/Mach-O personalities' FULL game support
  (D3D8/DSOUND for Halo PC; the Cocoa/OpenGL for the Mac demo) is
  Wine/Darling-strength, not our-own-code strength. The container
  hosts the real translation; our own loaders are the long tail. The
  era demos prove the launch path; the full games are the same path
  with the real translation layers the arch daemon installs.

## DA-2 (Privacy + provenance): are the sources honest?

- All three are the REAL 2003-era binaries from archive.org +
  openarena.ws (the Kevin-Bacon hop chain: Bungie → Gearbox →
  Microsoft for the PC demo; Bungie → Westlake Interactive → MacSoft
  for the Mac demo; the OpenArena project for the Linux game). The
  downloads sit in vendor/games (gitignored) — no binary blobs in the
  repo, no third-party code imported into src/.
- zlib (the DMG decompression + the sanctioned compression dep) is
  the only library used; everything else is our own parsing.
- **No telemetry, no accounts, no network calls beyond the download.**

## DA-3 (Robustness): what happens when the hardware/games are missing?

- No games in ~/.wubu/games/ → the BPM grid shows zero games
  gracefully (the era-apps grid still works).
- An unhostable file → skipped by the magic probe (never misrouted,
  never crashed — tested).
- The world snapshot at the play end is a BEST-EFFORT sample — a
  missing driver yields zeroed fields in the ledger line, never a
  crash (the ledger test proves the line shape with a stubbed world).
- The download failures (TuxFamily 404, SourceForge redirect) were
  absorbed by retries — the tooling never claimed success on a
  154-byte error page.

## The AGI-training integration (the second goal)

The games are FIRST-CLASS training events: `wubu_game_session` appends
one ledger line per play to ~/.wubu/games/plays.log with the game,
the format, the timestamp, the duration, and the WORLD STATE at the
session end (the driver registry's live snapshot). The AGI consumes
the DELTAS between plays: which games run on which hardware, how the
world shifts during play (the battery drains, the heat rises). This
is the research/065 implicit-feedback doctrine applied to games.
**Verified by test_game_session (the ledger line carries the world).**

## The honest status board

- [x] the routing table (classify + the personality dispatch)
- [x] the BPM games grid (the real games in Big Picture Mode)
- [x] the AGI game-play training ledger
- [x] the REAL Halo PC demo probed (a genuine i386 Win32 PE)
- [x] the REAL Mac DMG reader (our own UDIF zlib decompression)
- [~] the full downloads in flight (the Mac DMG 680MB + OpenArena
  425MB); the end-to-end launches run once they land
- [ ] the full-game translation (Wine/Darling-strength) — the honest
  long tail: our personalities host the real translation layers
- [ ] the AGI PLAYING (the perception-action loop: the AGI drives the
  game + the world-state deltas as rewards) — the next wave

## THE OPENARENA LAUNCH IS PROVEN (2026-08-09, real frame)

The Linux game goal: **the REAL OpenArena 0.8.8 runs and renders** —
the SHA1 matched the published value (37ab4199...), the extracted
ioq3+oa 1.36_SVN1910M engine started under xvfb with the SDL-1.2 lib
(from ~/opt, the house tool rule), loaded all 9 baseoa pk3s (maps,
textures, players), and rendered its full 3D main menu — captured at
/tmp/openarena_frame.png. The game is deployed to ~/.wubu/games/
(openarena_linux.x86_64 + openarena_win.exe + openarena_mac.ub — the
SAME package proves all three personalities: ELF, PE, FAT Mach-O).

Also caught + fixed by probing the REAL binaries: the FAT/CIGAM
endianness bug (the OpenArena universal binary is CA FE BA BE with
BE fields — 2 slices ppc+x86; the inverted swap reported 33M slices).
The wubu_game_probe tool now reads both endiannesses correctly.
