# 00-philosophy — The Symbiosis: Desktop User + Ring-0 Colonel

*Addendum entry (Kevin-Bacon pass 3 synthesis). Human-written.
Last updated 2026-08-02.*

## The virtue

**The user lives on the desktop; the AGI lives in the Colonel space; Bonzi
Buddy is the bridge.** This is a mixed-initiative symbiosis (Horvitz 1999):
each party contributes what it is best suited to, at the most appropriate
time, with negotiated initiative — not a chat box, not a remote control.

## The research convergence (7 hops)

| Hop | Source | Principle taken |
|-----|--------|-----------------|
| 1 | Engelbart NLS vs Macintosh | Augmentation AND user-friendliness: the desktop hides the machine (user-friendly), the Colonel space extends the mind (augmentation) |
| 2 | Xerox PARC / Smalltalk | The OS IS the language — every object responds to messages. WuBuOS: the OS IS HolyC; the WuBuFX app is a namespace |
| 3 | NeXTSTEP (AppKit, Interface Builder, Display PostScript) | The framework raises the developer to the 20th floor (eliminate 80% of GUI code); the DISPLAY SYSTEM IS A LANGUAGE (PostScript) the system itself can speak |
| 4 | Horvitz mixed-initiative + bonsai (arXiv 2604.19247) | Layer-bound agents with interface contracts; unsolicited reporting; expected-utility decisions; contracts prevent context saturation |
| 5 | Generative/adaptive UI | Interfaces generated from intent + context in real time — the UI adapts because it is DATA, not code |
| 6 | Wine 11 / Syscall User Dispatch / DXVK-vkd3d | Translation speed = eliminate boundary crossings, batch, keep the tables in-process |
| 7 | GameInput (GDK) + layered OS | One unified input model for all devices (common time base); level N relies only on the level below |

## The symbiosis model (concrete)

```
┌─ DESKTOP (level 1) ────────────────────────────────┐
│  The human's space: Win98/XP surface, the browser, │
│  Steam/games. Bonzi Buddy mediates: the face, the  │
│  ears, the veto. The human NEVER touches ring 0    │
│  directly (hedged access is intentional).          │
├─ CONTRACTS (WuBuFX capability gates + EDR) ────────┤
│  /app/<id> namespaces, caps (READ/WRITE/EXEC/AGI/  │
│  EDR), every eval/mount logged. Mixed-initiative:  │
│  the agent reports unsolicited, negotiates, waits. │
├─ COLONEL SPACE (level 0) ──────────────────────────┤
│  The AGI's space: ring-0, the live console, HolyC, │
│  drivers, the whole computer. The Colonel sees     │
│  everything, decides what needs the human, sends   │
│  it through Bonzi.                                 │
└────────────────────────────────────────────────────┘
```

## Principles that fall out

1. **Bonzi is not a chatbot — Bonzi is a mixed-initiative agent.** It
   suggests, reports, negotiates, and escalates to the human ONLY when the
   expected utility demands it (Horvitz's principle 1).
2. **The desktop is the human's; the Colonel space is the AGI's; the
   contracts are sacred.** Layer-bound agents (bonsai) prevent context
   saturation: the Colonel does not babysit the browser; Bonzi does not
   touch the drivers.
3. **Everything the AGI shows is data.** The graphic set is a namespace the
   Colonel can write (see `02-architecture/graphicset.md`) — the desktop
   literally re-skins itself as the AGI's state changes.
4. **The human's veto is the highest utility.** The hedged access is a
   design feature: the human approves what leaves the desktop.
