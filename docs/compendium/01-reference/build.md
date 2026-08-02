<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Build

| Target | Recipe (first line) |
|--------|---------------------|
| `docs` |  |
| `test_uefi` |  |
| `test_agi_metal` |  |
| `shell` | $(SHELL_DIR)/wubu_shell |
| `all` | kernel jit compiler runtime tools gui bridge apps worldsim metal audio shell bear hosted_objs |
| `metal` | $(METAL_OBJS) |
| `audio` | $(AUDIO_OBJS) |
| `hosted_objs` | $(HOSTED_OBJS_LIST) |
| `bear` | $(BEAR_OBJS) |
| `bear_train` | $(BEAR_OBJS) $(BEAR)/bear_train.o |
| `kernel` | $(KERNEL_OBJS) $(KERNEL)/crt0.o $(KERNEL)/metal_main.o |
| `boot` | $(KERNEL)/boot.bin |
| `qemu` | $(KERNEL)/disk.img |
| `jit` | $(JIT_OBJS) |
| `compiler` | $(COMP_OBJS) |
| `runtime` | $(RT_OBJS) |
| `tools` | $(TOOLS_OBJS) |
| `gui` | $(GUI_OBJS) |
| `bridge` | $(BRIDGE_OBJS) |
| `apps` | $(APP_OBJS) $(APP_RT_OBJS) |
| `canvas` | apps runtime $(GUI_OBJS) $(KERNEL_OBJS) $(JIT_OBJS) $(COMP_OBJS) $(HOSTED)/archd_hosted.o $(GUI)/sta |
| `worldsim` | $(WS_OBJS) |
| `hosted` | $(HOSTED_OBJS) $(RT_OBJS) $(EDR_OBJS) $(APPS)/edr_dash.o $(RT)/wubu_holyc_agi.o $(RT)/holyd_lifecycl |
| `test_critical_runtime` | runtime test_oci test_network test_snapshot test_vsl test_holyd test_proton test_proton2 test_spawn |
| `test_critical_kernel` | test_fat32 test_txfs test_ahci test_drm_direct |
| `test_high_bridge` | runtime test_bridge test_bridge_flip test_syscall |
| `test_high_gui` | gui runtime test_dosgui_wm test_dosgui_ui test_dosgui_dos_window test_dosgui_startmenu test_dosgui_e |
| `test_high_bear` | test_jit test_memory test_tasking test_input test_holyc test_holyc_ptx |
| `test_medium_other` | runtime gui test_worldsim test_audio test_apps test_apps2 test_wubu test_host_exec test_gaad test_is |
| `test` | test_critical_runtime test_critical_kernel test_high_bridge test_high_gui test_high_bear test_medium |
| `test_jit` |  |
| `test_memory` | $(KERNEL)/memory.o |
| `test_tasking` | $(KERNEL)/memory.o |
| `test_input` |  |
| `test_math` | $(KERNEL)/wubu_math.o |
| `test_clipboard` |  |
| `test_worldsim` | $(KERNEL)/wubu_math.o |
| `test_fat32` | $(KERNEL)/fat32.o |
| `test_holyc` | $(JIT_OBJS) |
| `test_wubu` | $(JIT_OBJS) $(RT)/wubu_host_exec.o $(RT)/styxfs_path.o $(RT)/styxfs_util.o $(RT)/styx_names.o $(RT)/ |
