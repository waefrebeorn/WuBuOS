# WUBU MAZE

A tiny first-person raycaster for the WuBuOS tandem tree. Pure C11, no
libm, no graphics library, deterministic enough for golden-frame gates.

## Play

```sh
make wubu_maze
./src/apps/wubu_maze/wubu_maze          # interactive: W/S move, A/D turn, Q quit
./src/apps/wubu_maze/wubu_maze --tour   # fixed scripted walk, four frames
```

Example opening frame (one char per cell):

```
=========
============
==============
```

Objective: collect three glowing gems (`*`, 100 points each) then reach the
exit tile. The exit only wins once all gems are held. Gems are billboard
sprites projected through the inverse camera matrix and clipped per column
against the wall z-buffer, so walls correctly occlude them. Add `--map` to
show a 16x16 overhead minimap (`@` player, `E` exit, `*` gems). A BFS gate
proves start → every gem → exit is reachable, so the level cannot silently
become unwinnable.

## Gate

```sh
make test_wubu_maze
```

Asserts:
- DDA hits the exact wall: centre ray from `(2.5,1.5)` east hits `x=15`,
  perpendicular distance exactly `12.5` (no skipped cells).
- Collision: 100 forward moves stop at `x=14.75` with 51 blocked attempts
  and never enter a wall cell.
- Quarter turns are exact inverses and do not count as moves.
- RGB24 frame (64x40) and ASCII frame are byte-identical across renders.
- Both frames match committed FNV-1a golden hashes; no image is stored.
- Floor and ceiling are checkerboard-cast with the scanline `rowDistance`
  formula, and the gate asserts both shades are present (not one flat fill).
- A FULL playthrough gate drives the player with real turn/forward commands
  along a BFS path through all three gems and the exit — no teleports. It
  wins in 268 steps with 0 blocked moves, proving the level is actually
  playable, not just reachable on paper.
- `--ppm <path>` writes a binary P6 frame for human inspection only.

## On the WuBuOS desktop

`WUBU MAZE` is also a desktop app in the registry. Launch it from the app
menu: `W/S` move, `A/D` turn, arrow keys work too, `R` restarts. Three gem
indicator boxes fill as gems are collected; a green bar marks a win. The app
draws the raycaster state straight into the window framebuffer (coloured
cells, no font dependency). `launch_wubu_maze()` uses the weak launch seam
already used by the petdex, so lean `apps.c` test links stay intact.

## Provenance

The DDA grid-step and perpendicular-distance formulation follow Lode
Vandevenne's canonical raycasting tutorial (`lodev.org/cgtutor/raycasting`):
one map boundary crossed per loop, `deltaDist = |1/rayDir|`, and
`perpWallDist = sideDist - deltaDist` to avoid fisheye. This is a ground-up
C11 implementation, not a copied source file. Golden-frame hashing follows
the standard render → readback → hash regression pattern.
