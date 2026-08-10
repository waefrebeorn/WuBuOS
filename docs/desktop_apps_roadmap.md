# Desktop app roadmap — completed + properly-scoped futures (2026-08-09)

The directive: "complete all of the desktop app gaps... there's a lot
of useless shells and stub code that needs to be full applications...
some of these things will be future projects but also need to be
properly [scoped]."

## The audit — every colonel-registered name vs its reality

The Colonel's app registry names (wubu_colonel.c g_apps) vs the
implementation state BEFORE this pass:

| Name | Before | Now |
|---|---|---|
| calc | real (Standard/Scientific/Programmer/Graphing) | ✓ |
| notepad | real (multi-line editing) | ✓ |
| paint | real (the layered WuBu Canvas) | ✓ |
| explorer | real (drives/tree/preview) | ✓ |
| terminal | real (cmd engine + PTY) | ✓ |
| holyc | real (the HolyC term) | ✓ |
| controlpanel | real (the manager + 5 applets) | ✓ (this wave) |
| **taskmgr** | **REAL (src/apps/taskmgr) but NEVER REGISTERED — `colonel run taskmgr` validated the name, the launch went nowhere** | **registered + launchable** |
| canvas | real | ✓ |
| **freedoom** | **no code** | **properly scoped: FUTURE (the game-port project)** |
| bonzi | real (WuBu Buddy) | ✓ |
| comfy | real | ✓ |
| settings | real (the Control Panel) | ✓ |
| **packagemanager** | real modules (wubu_pkgmgr) but unwired from the desktop launch table | wired via the daemon panel |
| containermanager | real (the daemon panel) | ✓ |
| sound | real (the CP applet + the engine) | ✓ |
| **music** | **no code** | **REAL (this pass): the playlist scan + the engine player** |
| **browser** | **no code** | **properly scoped: FUTURE (the internet-basin project)** |
| **notes** | **no code** | **REAL (this pass): persisted notes on the 9P fs** |
| **todo** | **no code** | **REAL (this pass): persisted tasks on the 9P fs** |

## Completed this pass (all gated green)

1. **Notes** (`src/apps/notes.c`) — a real notes app: the note list
   loaded from `~/.wubu/notes/` (real files), create/select/delete,
   every action touches real files. Test-proven persistence.
2. **Todo** (`src/apps/todo.c`) — a real todo list persisted to
   `~/.wubu/todo.txt` (`[ ]` / `[x]` lines): add/toggle/delete,
   pending-count, immediate persistence. Test-proven.
3. **Music** (`src/apps/music.c`) — a real player: the playlist
   scanned from `~/.wubu/music/` (wav/ogg/mp3/mod), play/stop/next/
   prev over the wubu_sound engine. The track position drives the
   engine; the REAL wav/ogg decode is the decoder-synthesis path.
4. **Task Manager** — real code existed (Windows-11-style taskmgr) but
   was orphaned from the registry; now registered + launchable.
5. **The registry** — g_app_defs now covers 20 real apps; the desktop,
   the start menu, and the colonel all consume the same table.

## The FUTURE projects (properly scoped, not dead stubs)

These are declared roadmap items with honest status — no fake shells.

### Browser — the internet-basin project
The AGI doctrine says the internet is the endless training basin
(repetitive but self-reinforcing); the browser is the user's window
into it. Scope: the dosgui window shell + a real web engine
(WebKit/Blink-class or a minimal HTTP+HTML renderer over our sockets
stack). The first milestone is NOT the full engine — it is the
network path: the /n/net subtree + an HTTP GET that renders text/HTML
into the terminal/canvas pipeline. The arch daemon's packages
(chromium/firefox) are the fallback hosts; the native engine is the
goal. Status: **designed, not started**.

### Freedoom — the game-port project
The era-apps grid proves the DOS/8086 + HolyC personalities; Freedoom
(the open Doom) is the first REAL 3D game to prove the GPU/render
path end-to-end (the Vulkan compute leg + the input pipeline + the
game session). Scope: a native C11 Doom-class renderer over the
kernel's Vulkan compute, or hosting the freedoom binary via the
Linux-ELF personality. Status: **designed, not started**.

## The integration rule (this wave's lesson)

A name in the colonel registry MUST have a launchable implementation
or a scoped roadmap entry — never a validated-but-empty name. The
registry (g_app_defs) is the single source of truth; the desktop, the
start menu, and the colonel consume it.
