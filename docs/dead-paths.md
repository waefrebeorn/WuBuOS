# Dead paths: what the product silently cannot do

A capability reached through a **weak symbol** that no strong unit provides does not fail —
it resolves to NULL and the caller does nothing. That is worse than a compile error: the icon
is there, the button is there, the click lands, nothing happens, and no log says why.

## The measure

`nm src/hosted/wubu | awk '$1=="w"'` lists every weak symbol the shipping binary resolved to
nothing. It is the authoritative list; grep of the source is not.

    launch_vws_designer        <- desktop icon "VWS Designer" does nothing
    launch_wet_applet          <- desktop icon "WET Applet" does nothing
    wubu_bt_present            <- quick settings Bluetooth probe absent (guarded: HAVE() -> honest)
    wubu_theme_node_get        <- kernel-only theme lookup (not a GUI path)
    wubu_vmm_map_user_page     <- VMM, not part of the hosted product
    __gmon_start__             <- toolchain noise

So today: **two dead desktop icons**, found by opening every icon in the real product and
counting windows rather than by reading the registry.

## Why they are not linked yet

`launch_vws_designer` / `launch_wet_applet` live in `src/apps/vws_designer.c` and
`src/apps/wet_applet.c`, and both units are referenced **only by their own tests** — never by
`APP_OBJS`. Adding them needs the `vws` stack (`vws_form`, `vws_emit`, `vws_load`,
`vws_templates`, `vws_lua`) plus the `wb_lv` runtime (`wb_lv_arena`, `wb_lv_coro`,
`wb_lv_eval`, `wb_lv_host`, `wb_lv_lex`, ...).

And `wb_lv` exists **twice** in this tree:

    src/gui/wet/externals/wua/src/wb_lv.h      (the .WET toolkit's copy)
    src/lua/www/~WuBu~/src/lv/src/wb_lv.h      (the lua/www copy)

Linking the designer against one while the toolkit links the other produces duplicate
definitions. So the prerequisite is the unification that has been on the naming/duplication
list for days: pick the canonical `wb_lv`, make the other a consumer of it (or delete it), and
only then add the designers to `APP_OBJS`.

## Win-key capture in hosted mode: manual toggle, not automatic

`Ctrl+Alt+G` hands the keyboard to the guest. Two automatic alternatives were implemented, measured
and reverted, because neither can be honest with the window as it stands:

* **Grab on focus.** Delivered `Win+Shift+S`, but swallowed every host shortcut for as long as the
  guest window had focus.
* **Grab when the guest fills the screen.** Could never become true: the hosted window is pinned to
  the OS resolution (1024x768) so the WM can neither maximise nor fullscreen it, and relaxing the
  pin without resizing the framebuffer reproduces the bug fixed earlier -- a 1024x768 framebuffer
  painted into a 1920 window, with half the window dead. Real fullscreen needs a framebuffer
  resize, which is a display-stack job, not an input job.

So the copy that ships is the toggle: off by default, released by the same key and on focus loss.
A Win-key binding that only works after an explicit hand-over is worse parity than Windows, and it
is better than an OS that quietly owns the keyboard.

## The rule this establishes

**A weak symbol is a promise the linker will check at runtime, silently.** the linker will check at runtime, silently.** Any weak capability
the product depends on must either (a) have its strong unit in the product link, or (b) have a
caller that visibly degrades (`HAVE(x)` guard, greyed control, printed reason). A weak symbol
with neither is a dead path, and `nm` finds it in one command.
