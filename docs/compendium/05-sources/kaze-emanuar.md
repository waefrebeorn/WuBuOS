# Source: Kaze Emanuar -- the N64 optimization corpus

Archived 2026-08-03 (the wubuwizard 042 deep-dive is the synthesis;
the raw transcripts are the files below). All content is Kaze Emanuar's
published work (YouTube @KazeN64), credited per video.

## Catalog

| video | year | the contribution | transcript file |
|---|---|---|---|
| Finding the BEST sine function for Nintendo 64 | 2023 | the lookup table is a mis-optimization: RAM bus reads > compute; the polynomial lives in the instruction cache | (full transcript in the video; essence in 042) |
| The Folded Polynomial | 2023-09-09 | Silas Lock's algorithm: one polynomial on [0,pi/4], every symmetry, one sqrt; 3x graphics / 90x physics accuracy at equal cycles; "ask what more assumptions you can make" | (essence in 042) |
| FIXING the ENTIRE SM64 Source Code | 2023 | 100K+ lines read/edited; renders 6x faster on real hardware; the shared-RDRAM secret: the render path + memory traffic, not the logic | kaze-fixing-entire-sm64-source.md |
| Revolutionizing N64 programming! (SM64 Audio Optimization) | 2024 | the audio was the ONLY file Nintendo compiled with optimizations; still 2x faster by attacking DATA MOVEMENT (echoes/stereo/mixing = large chunk moves) | kaze-sm64-audio-optimization.md |
| How Optimizations made Mario 64 SLOWER | 2023 | cache-line/CON layout fragility; the measurement-floor problem | (essence in 042) |
| Mario 64 wastes SO MUCH MEMORY | — | the 4MB RDRAM is used exactly; the cartridge holds only 8MB | — |
| N64 Programming PRIMER series | ongoing | from-scratch N64 development tutorials | — |

## The individual's other work (credited)

- SM64: Last Impact (2016) -- full-game ROM hack (new enemies/power-ups/stages/music)
- Return to Yoshi's Island (2023+) -- full new campaign on the decomp
- n64decomp/sm64 contributor -- the Super Mario 64 decompilation team
- The audio-RSP discovery: Nintendo's only optimized file, beaten 2x by data-movement attack

## The 7-hop method applied

See wubuwizard research/042-kaze-deep-dive-and-7hop.md (the hop table:
decomp scene -> N64 hardware -> SGI heritage -> console underground ->
demoscene -> PC engine lineage -> the modern roofline; convergence:
FOLD THE DOMAIN UNTIL THE PROBLEM FITS).
