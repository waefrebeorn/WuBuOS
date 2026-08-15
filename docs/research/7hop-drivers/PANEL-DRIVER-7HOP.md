# PANEL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU panel routing gaps

Panel detection identifies display panel types (LVDS, eDP, HDMI, DP, VGA)
and connector connection states for the GPU display pipeline.

### Impl routing (wubu_panel.c)

| wubu fn | source |
|---|---|
| wubu_panel_present | /sys/class/drm/card connector |
| wubu_panel_type_for | connector type string |
| wubu_panel_summary | connector + type summary |
