#!/usr/bin/env bash
# App smoke audit through the path a user actually takes: Super, type the name, Enter.
# For each app it records the pixels that changed and how many distinct colours the changed
# region has -- a window that opened but painted one flat colour is as broken as one that did not.
set -u
export DISPLAY=:0
OUT=/tmp/appaudit2
rm -rf "$OUT"; mkdir -p "$OUT"

pkill -x wubu 2>/dev/null; sleep 0.4
export HOME=/tmp/appaudit2/home
mkdir -p "$HOME/.config/wubu"
printf 'probe=1' > "$HOME/.config/wubu/onboarding"
unset XDG_RUNTIME_DIR
cd /home/wubu/wubuos
WUBU_NO_BOOT_APPS=1 ./src/hosted/wubu -w 1400 880 > "$OUT/run.log" 2>&1 &
sleep 5
W=$(xdotool search --name 'WuBuOS' | head -1)
xdotool windowactivate "$W"; sleep 0.6
shot() { ffmpeg -loglevel error -f x11grab -video_size 1920x1080 -i :0.0 -frames:v 1 -y "$1"; }
shot "$OUT/base.png"

printf 'app,changed_px,distinct_colors,shot\n' | tee "$OUT/report.csv"
i=0
for app in Calculator Notepad Paint Terminal "File Manager" Editor "Task Manager" Settings Music; do
  xdotool key --clearmodifiers super; sleep 0.8
  xdotool type --delay 35 "$app"; sleep 0.9
  xdotool key Return; sleep 1.6
  xdotool key Escape >/dev/null 2>&1; sleep 0.5
  shot "$OUT/$(echo "$app" | tr ' ' '_').png"
  python3 - "$OUT/base.png" "$OUT/$(echo "$app" | tr ' ' '_').png" <<'PY' | tee -a "$OUT/report.csv"
import sys
from PIL import Image, ImageChops
a,b=[Image.open(p).convert('RGB') for p in sys.argv[1:3]]
d=ImageChops.difference(a,b)
n=sum(1 for px in d.convert('L').getdata() if px>10)
regions=[(i%1920,i//1920) for i,px in enumerate(d.convert('L').getdata()) if px>10]
cols=set()
if regions:
    for (x,y) in regions[::max(1,len(regions)//4000)]:
        cols.add(b.getpixel((x,y)))
print(f"{sys.argv[2].split('/')[-1][:-4]},{n},{len(cols)},{sys.argv[2]}")
PY
  xdotool key --clearmodifiers alt+F4; sleep 1.0
  i=$((i+1))
done
pkill -x wubu 2>/dev/null
echo done
