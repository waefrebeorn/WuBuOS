# Clipboard host transport: what the real protocols require

SLERM-2025 is the open goal "carry the clipboard across the host seam". Stage A
(`src/bridge/wubu_clip.c`, 26/26 checks) keeps typed items inside the process. Everything below
is what a transport has to implement to be a peer of real applications, sourced rather than
assumed.

## Sources

- ICCCM §2, *Peer-to-Peer Communication by Means of Selections* —
  <https://tronche.com/gui/x/icccm/sec-2.html>
- ICCCM PDF (X.Org mirror) — <https://xorg.freedesktop.org/archive/X11R7.0/doc/PDF/icccm.pdf>
- `ext-data-control-unstable-v1`, the production Wayland clipboard-manager protocol —
  <https://wayland.app/protocols/ext-data-control-v1>
- `wlr-data-control-unstable-v1`, **deprecated** — <https://wayland.app/protocols/wlr-data-control-unstable-v1>

## Requirement: selection ownership is a window, not a buffer

- Each selection (an arbitrary atom; `CLIPBOARD` and `PRIMARY` by convention) is owned by a
  **client** and attached to **a window**, globally to the server.
- Acquisition is `SetSelectionOwner(selection, owner_window, time)`; the time must come from the
  event that triggered it, **not** `CurrentTime` — ICCCM states a client that uses `CurrentTime`
  has no way to know when it gained ownership. A zero-length property append is the prescribed
  way to obtain a timestamp.
- A requestor supplies: selection name, property name, window, target atom, optional parameters.
  The owner converts the selection, writes it into that property on that window, and notifies.

Consequence for us: a clipboard is not a buffer we hold, it is a **service we answer requests
from**, and it dies with our window unless something else adopts the selection (a clipboard
manager). Our stage A store is the wrong shape for the seam — it needs an event source.

## Requirement: TARGETS before bytes

A well-behaved owner answers `TARGETS` with the list of types it can convert to; requestors use it
to pick a representation. Our stage A items carry a `mime[64]` per item, which maps onto TARGETS
directly (target atoms are the MIME type strings by convention), but nothing currently answers
the request.

## Requirement: large transfers cannot use one property write

Properties are not sized for arbitrary data; ICCCM's `INCR` protocol exists for transfers too
large for a single `ChangeProperty`, and a peer that does not implement `INCR` will truncate or
hang on a large copy. Any transport we build must implement it — a screenshot-sized image copy is
the normal case for a desktop OS.

## Requirement: on Wayland, wlr-data-control is deprecated

The wlr protocol's own page says: *"This protocol is deprecated and not intended for production
use. For clipboard management, use the ext-data-control-v1 protocol."* `ext-data-control-v1` is
described as in its testing phase, with backward-incompatible changes only via a new major
version.

Consequence: `wubucompositor` (our own Wayland compositor, no wlroots) should implement
`ext_data_control_manager_v1` + `ext_data_control_device_v1`, not the wlr names. Adopting the
deprecated protocol because most tutorials show it would be a parity gap, not parity.

## Design implied by the sources

| seam | transport | why |
|------|-----------|-----|
| hosted (X11/XWayland) | ICCCM selections: `CLIPBOARD` + `PRIMARY`, `TARGETS`, `TIMESTAMP`, `INCR` | it is what every X11 client speaks; there is no alternative |
| bare metal / our compositor | `ext_data_control_manager_v1` | wlr variant is deprecated by its own spec |
| inside WuBuOS | existing `wubu_clip` typed store | already implemented, 26/26 |

Oracle for the gate: a **foreign** implementation must move the bytes — `xclip`/`xsel` (X11) and
`wl-paste`/`wl-copy` (Wayland) are peers, not our own code agreeing with itself.

## Status

- Stage A store: DONE (in-process, typed, no allocation, generation counter).
- Host transport: **OPEN** (SLERM-2025). Nothing above is implemented; this note is the sourced
  requirement list, not a claim of work done.
- `TARGETS`/`TIMESTAMP`/`INCR`/ownership-loss handling are the four things a naive
  "write bytes to an X property" attempt will get wrong.
