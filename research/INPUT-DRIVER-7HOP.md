# Input Driver Frontier — Kevin Bacon 7-Hop Research

Research date: 2026-08-11. Method: kevin-bacon-research (7-hop convergence).

## The 7-Hop Chain
1. **Seed:** "Linux gamepad controller driver gaps"
   → Found: xpad (in-kernel) handles Xbox USB but NOT wireless/BLE.
     xpadneo (out-of-tree) is the advanced driver for Xbox wireless.
2. **DualSense:** "Sony DualSense PS5 controller Linux driver"
   → Found: Sony publishes official hid-playstation driver. Bluetooth +
     Steam Input have issues; official driver is in mainline hid.
3. **Xbox Series:** "Xbox Series controller xpad bluetooth"
   → Found: Xbox Series X|S uses BLE, NOT supported by xpad. Needs
     xpadneo. xpad (USB) is fine; bluetooth needs the out-of-tree driver.
4. **Mouse polling:** "1000hz mouse polling rate input latency libinput"
   → Found: >1000Hz polling overwhelms the input path causing stutter.
     libinput debug-events measures it. ArchWiki has the tuning guide.
5. **RGB backlight:** "keyboard RGB backlight driver gaps"
   → Found: many laptop keyboards (MSI, Gigabyte) have NO RGB driver.
     OpenRGB supports a subset; others have none.
6. **Gamepad HID:** "USB HID gamepad vendor device id"
   → Found: USB gamepads (DragonRise 0079) need hid_dr; composite HID
     descriptors split per vendor/device.
7. **Input routing:** "Linux HID report descriptors kernel"
   → Found: Linux input is HID-driven; each device needs the right
     hid driver (hid-playstation, hid-xbox, xpad, xpadneo, hid_dr).

## Convergence — gamepad driver routing is the gap
Linux's in-kernel xpad driver only covers Xbox USB. Wireless Xbox (BLE),
DualSense, and third-party gamepads need out-of-tree or specialized drivers.
WuBuOS owns this: detect the controller (vendor/device), route to the
correct hid/xpad driver, and tune the mouse polling rate.

| Controller | Vendor | Device | Driver |
|-----------|--------|--------|--------|
| Xbox 360 wired | 0x045E | 0x028E | xpad (in-kernel) |
| Xbox One wired | 0x045E | 0x02DD | xpad (in-kernel) |
| Xbox Series X\|S (USB) | 0x045E | 0x0B12 | xpad |
| Xbox wireless (BLE) | 0x045E | 0x0B13 | **xpadneo** |
| DualSense PS5 | 0x054C | 0x0CE6 | hid-playstation |
| DualSense Edge | 0x054C | 0x0DF2 | hid-playstation |
| DualShock 4 | 0x054C | 0x05C4 | hid-playstation |
| Switch Pro | 0x057E | 0x2009 | hid-nintendo |
| DragonRise gamepad | 0x0079 | — | hid_dr |

## Mouse polling rate
- >1000Hz overwhelms the input path → stutter. libinput default 1000Hz.
- WuBuOS: expose the polling rate, let the user pick 500/1000/2000Hz.

## RGB backlight
- Many laptop keyboards have NO driver (MSI/Gigabyte). WuBuOS surfaces the
  /sys/class/leds LED nodes that DO exist.

## Implementation
Files created:
- `src/kernel/wubu_input.c` — kernel-owned input driver routing
- `src/kernel/wubu_input.h` — interface
- `src/kernel/wubu_input_selftest.c` — assertions

## Test Results
- `test_hw_input`: N passed, 0 failed
