# Open defects: display, measured on 2026-11-25

Two defects are recorded here because both were found by looking at a live frame rather than by a gate,
and neither is fixed.

## 1. A hosted session populates no world snapshot

The Control Panel's Display page, after being made honest, reads on a real 1400x880 hosted session:

    Display        not reported (no usable mode)
    Refresh        not reported by this machine
    Video memory   not reported
    Pixel format   32 bpp (uint32 surface)

Before the plausibility checks it printed `unknown 51325x35144` and `2202583157 MB` -- uninitialised
memory formatted as if it were a display. `wubu_world_snapshot()` is only meaningful on metal; the hosted
loop never installs a provider, so the applet's `snap` is either NULL or a struct nobody filled.

**Fix direction:** install a hosted world provider at startup that reports what the hosted session
actually knows -- the surface from `vbe_draw_surface()` (stride and height), the X11/desktop size, and
"not reported" for anything the host does not expose. The page is already able to display it; there is
simply no provider. Do not fabricate values to make the page look full.

## 2. `test_metal_display` loses its pmemsave dump under suite load

Three occurrences, all identical:

    SCANOUT: FAIL (no dump: qemu rc=0, temp file absent)

while `make test_metal_display` alone passes (verified twice in a row, 2/2). The QEMU gates are now
serialised on `/tmp/wubu-qemu-gates.lock` -- `test_wemu_kernel` and `test_deskshot` were added to it --
and the failure still occurred, so **contention between emulators was not the whole cause**. The
remaining suspect is the dump path itself: the monitor command being issued after the guest was killed, or
the wait for the dump file timing out while the guest is still booting under load.

**Fix direction:** make the gate wait for a *monitor-level acknowledgement* of the pmemsave command (the
`(qemu)` prompt after the write) instead of polling for the file, and fail with a distinct message when
the acknowledgement never arrives. A gate that cannot distinguish "the dump was late" from "the display is
wrong" will keep costing a 480-second suite run per false alarm.

## Why this file exists

Both defects were invisible to every gate in the tree: the first because the gate injects a fake snapshot
(so the page is tested against data that a real session never produces), the second because the gate passes
when run alone. A gate is only as good as the conditions it holds constant.
