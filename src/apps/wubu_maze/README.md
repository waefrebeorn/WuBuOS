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
against the wall z-buffer, so walls correctly occlude them.

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
- `--ppm <path>` writes a binary P6 frame for human inspection only.

## Provenance

The DDA grid-step and perpendicular-distance formulation follow Lode
Vandevenne's canonical raycasting tutorial (`lodev.org/cgtutor/raycasting`):
one map boundary crossed per loop, `deltaDist = |1/rayDir|`, and
`perpWallDist = sideDist - deltaDist` to avoid fisheye. This is a ground-up
C11 implementation, not a copied source file. Golden-frame hashing follows
the standard render → readback → hash regression pattern.
