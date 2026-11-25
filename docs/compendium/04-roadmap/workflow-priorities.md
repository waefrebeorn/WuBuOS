# Workflow priorities — bare metal, Linux, and the guests we host

Measured 2026-11-25, not recalled. Rank is by *what blocks a person from actually
using WuBuOS*, not by what is interesting to build.

## The measured picture

| Layer | Host-GUI dependencies | Verdict |
|---|---|---|
| `src/kernel` | **0** files reference X11/Wayland/GTK/SDL | bare metal is clean |
| `src/runtime` | 1 (`wubu_wine_env.c`, the guest's DISPLAY) | this is the leak to rip out |
| `src/gui` | 1 | the compositor's own host leg |
| `src/hosted` | 4 (`wubu_display.c`, `hosted_wayland.c`, `wubu_metal_x11.c`, `wubu_inject.c`) | CORRECT — this IS the Linux leg |
| submodules (`wubupad`, `vws`, ...) | the rest of the 147 | app-local, not on the boot path |

The kernel having zero host-GUI dependencies is the important number: the desktop
is already pure VBE + compositor, so "amazing on bare metal" is a matter of shell
and workflow, not of porting.

## P1 — bare metal must be usable, not just bootable

1. **A real boot path a person can use.** Image + ESP + the display gate exist;
   what is missing is a documented, repeatable "write this file to a USB, boot
   it" recipe with the gate output as proof. (Owner: `tools/`, `docs/`.)
2. **Session persistence.** Notes, todo, applet state must survive a reboot;
   otherwise every session starts at zero and the environment cannot feel like a
   workspace. `~/.wubu/{notes,todo.txt}` exists — prove it round-trips.
3. **Input completeness.** Win+Left/Right/Up/Down and Win+Z now exist. Missing:
   drag-to-edge snapping, Alt+Tab cycling proof, and a keyboard path to every
   panel (start menu, quick settings, notification centre).

## P2 — the Linux leg must feel identical

1. **One desktop, two hosts.** The hosted binary and the metal path must show the
   SAME shell. The contract already says C owns compositor/WM and `.WET` owns
   shell surfaces — that is what makes this true, and it is why shell surfaces
   move to applets.
2. **No host-window assumptions in the runtime.** `src/hosted` may use X11; nothing
   below it may. Enforced by a grep gate, not by discipline.

## P3 — rip the guest's GUI need out

The dependency to remove, in order of how much it costs a user:

1. **Wine/Proton inheriting the host DISPLAY** (`wubu_wine_env.c`). FIXED: it now
   prefers `WUBU_X11_SOCKET` (a display WuBuOS provides) and only falls back to
   the host display with a one-time warning on stderr. A guest's GUI must be owned
   by the WuBuOS compositor, not by the host X server.
2. **The X11 bridge itself.** What is still missing is the piece that makes
   `WUBU_X11_SOCKET` real: an X11 server whose output lands in a `DosGuiWindow`,
   the way the DOS box's framebuffer already does. That is the single highest-value
   guest feature in the tree, because it turns every X11 program into a WuBuOS
   window.
3. **Guest window chrome.** Once (2) exists, a guest window is an ordinary window:
   snapped by `wm_snap_half`, listed in the taskbar, closed by the WM. No guest
   gets its own compositor.
4. **macOS/Metal leg.** `src/hosted/wubu_metal_x11.c` exists; it must present into
   the same compositor surface as everything else, never its own window.

## X11 bridge: what exists, and the method that got it there

Exists (tested): the client setup parse (`wubu_x11_parse_setup`, 12 checks) and the
server Setup reply (`wubu_x11_build_setup_reply`, 45 checks), plus
`tools/x11_handshake_check.sh`, which runs a real `xdpyinfo` against a stub and
requires it to get past the handshake and send its first request.

Does NOT exist: the request loop, window/pixmap resources, drawing, and the
presentation into a `DosGuiWindow`. `WUBU_X11_SOCKET` therefore still points at
nothing in a real session.

**The lesson worth more than the code.** 43 self-written checks passed while a real
client refused the reply with one sentence: "unable to open display". The three
bugs behind it (empty auth is legal, `motion_buffer_size` CARD32, vendor and
max-request lengths are CARD16) were all invisible to a test written from the same
understanding as the code. What settled it was reading the reply bytes of the
host's own Xorg and matching them field by field -- the spec's prose order lists
`vendor` before `release-number`, which is semantically true and useless for
encoding.

Rule: **for a wire protocol, the oracle is another implementation's bytes, never
your own test.** Pick the reference first; write the encoder second.

## The rule this document exists to enforce

**A guest may bring its programs; it may not bring its display.** Anything that
makes a guest depend on the HOST's window system is a bug in this tree, because it
means the environment stops being WuBuOS and becomes a launcher for another OS's
desktop. `WUBU_X11_SOCKET` is the seam; the X11-to-window bridge is the work.
