# 02-architecture — The Graphic Set System (self-modifying)

*Kevin-Bacon pass 3 synthesis. The display system is a LANGUAGE the OS
(and the AGI) can speak — NeXT's Display PostScript principle, made
namespaced and live.*

## The insight chain

- NeXTSTEP's Display PostScript: the display system IS a language, so the
  system itself can drive the UI programmatically.
- Smalltalk: every object responds to messages — the UI is a dynamic object
  network, not a static render.
- Generative UI: interfaces are generated from intent/context in real time.
- WuBuOS already has the pieces: `wubu_theme` (runtime-switchable themes),
  the chrome system (theme-driven window decoration, content rects),
  WuBuFX (apps as Styx9 namespaces), the Win98/XP compositor.

## The design: graphics as a writable namespace

```
/theme/                 ← the graphic set root (Styx9 nodes)
├── /colors             ← palette nodes (the AGI can write)
├── /chrome             ← window chrome specs (buttons, title bars)
├── /fonts              ← bitmap font sets (ASCII 32..95 on metal)
├── /sprites            ← icon/asset sets (gorilla, folder, trash…)
├── /layout             ← the GAAD φ-layout policy
└── /agents             ← per-app theme overrides
```

- **The compositor reads the namespace every frame** (damage-rect rendering
  — WuBuFX Phase G) — so WRITING a node re-skins the desktop live.
- **The AGI modifies the graphic set** through the same capability-gated
  path as everything else (WRITE on /theme, EDR-disclosed) — the desktop
  literally reflects the Colonel's state: palette shifts with mood/load,
  the gorilla's expression tracks the AGI's current task, the layout
  re-flows by GAAD when the Colonel changes focus.
- **Simplification for transliteration:** the graphic set is a small,
  typed, versioned schema — ONE format the compositor, the theme engine,
  the AGI, and the foreign-OS overlay shims (GDI/DirectX translation) all
  speak. Translating a foreign theme = mapping a table, not reimplementing
  a renderer.

## Why this is fast

1. **No recompile for a re-skin** — the theme is data (Wine 11's lesson:
   eliminate boundaries; the namespace write is a memory op, not a build).
2. **Damage rects** — only dirty regions re-render (Phase G), so even a
   live re-skin is cheap.
3. **One format** — the DXVK lesson: a single translation target
   (DirectX→Vulkan) beats a zoo of native APIs; the graphic set is the
   single target every client translates TO.

## The layering (future-proof)

```
level 1 apps ── draw via ──> /theme namespace ──> compositor ──> vbe fb
level 0 AGI ─── writes ───> /theme namespace (capability-gated, EDR)
overlay games ─ GDI/DX ──> VSL graphics translation ──> /theme namespace
```

Each layer depends only on the one below (the layered-OS rule); the
namespace is the contract between them.

## Build status

- Existing: `wubu_theme.h` engine, chrome system (content rects, hit-test),
  WuBuFX namespaces, GAAD layout, bitmap fonts on metal.
- Next (roadmap): theme-as-namespace (`/theme`), damage-rect compositor
  (Phase G), the AGI-writable graphic set, the GDI translation shim.
