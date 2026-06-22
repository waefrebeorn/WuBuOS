# WuBuOS Battleship v13 — 2185 REAL_GAP Deep Audit (Post-Form-vs-Function Hunt)

```
╔══════════════════════════════════════════════════════╗
║                                                        ║
║     🌱  W U B U O S                                 ║
║                                                        ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace    ║
║                                                        ║
║     245 .c · 111 .h · ~123K LOC                      ║
║     197+ tests green · 2185 REAL_GAPs identified     ║
║                                                        ║
║     Hosted ─── ZealOS 9P GUI Containers Audio Bear GPU Metal ║
║                                                        ║
╚═══════════════════════════════════════════════════════╝
```

## Methodology

Triple Devils Advocate audit. Every gap is REAL_GAP — no "for brevity", no "scaffolding", no "for extension", no "for later", no "stub for extension".
"Rewriting from scratch in C" is the point — anything falling under that IS the work.
Empty bodies `{}`, `(void)param;` casts as only statement, `return -1;` with no logic = FORM ≠ FUNCTION.

### Gap Count by Category (Automated Source Scan)

| Category | Files | Gaps | Severity |
|----------|-------|------|----------|
| Runtime (containers, network, OCI, snapshot, VSL, daemon) | 28 | 996 | 🔴 CRITICAL |
| Kernel (interrupt, FAT32, tasking, memory, AHCI, TXFS) | 13 | 254 | 🔴 CRITICAL |
| GUI (WM, desktop, startmenu, explorer, terminal, proton, gamelib) | 52 | 326 | 🟠 HIGH |
| Bear RL (NN, PPO, GAAD, Vulkan, CUDA, cuDNN, env) | 26 | 212 | 🟠 HIGH |
| Hosted (metal, vulkan, display, DRM, GBM, X11) | 9 | 163 | 🟠 HIGH |
| Compiler (HolyC lexer, parser, codegen, PTX) | 5 | 37 | 🟡 MEDIUM |
| Apps (editor, canvas, codec, freedoom, explorer, terminal, calc, control) | 8 | 88 | 🟡 MEDIUM |
| Audio (Furnace 12 chips, SF2, Ardour DAW, AI plugins) | 2 | 26 | 🟡 MEDIUM |
| Bridge (syscall, DOS flip) | 4 | 37 | 🟡 MEDIUM |
| Tools (ISO9660, screenshot, weight_check, demo_record) | 8 | 61 | 🔵 LOW |
| Shell (unified shell) | 1 | 21 | 🔵 LOW |
| Other (JIT encoder/disasm/minic) | 17 | 63 | 🔵 LOW |
| **TOTAL** | **2284** | **2284** | |

---

## RUNTIME (CONTAINERS, NETWORK, OCI, SNAPSHOT, VSL, DAEMON) (996 GAPS)

### src/runtime/wubu_network.c — 122 gaps

#### return_minus1 (78)

- Line 120: `if (!mgr) return -1;`
- Line 142: `if (!mgr) return -1;`
- Line 159: `if (!mgr || !profile) return -1;`
- Line 160: `if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;`
- Line 182: `if (!mgr || !network_id) return -1;`
- Line 186: `if (!force && mgr->networks[i].endpoint_count > 0) return -1;`
- Line 199: `if (!mgr || !network_id || !out_profile) return -1;`
- Line 201: `if (!p) return -1;`
- Line 214: `if (!mgr || !network_id) return -1;`
- Line 216: `if (!p) return -1;`
- ... and 68 more

#### return_0_stub (43)

- Line 138: `return 0;`
- Line 148: `return 0;`
- Line 178: `return 0;`
- Line 192: `return 0;`
- Line 203: `return 0;`
- Line 207: `if (!mgr || !out_profiles || max <= 0) return 0;`
- Line 219: `return 0;`
- Line 228: `return 0;`
- Line 266: `return 0;`
- Line 281: `return 0;`
- ... and 33 more

#### stub_keyword (1)

- Line 1013: `/* -- Helpers (already implemented in stub, kept as-is) ------------- */`

### src/runtime/wubu_oci.c — 85 gaps

#### return_minus1 (56)

- Line 114: `if (!dst || dst_size == 0) return -1;`
- Line 155: `if (!desc || !media_type || !sha256_digest) return -1;`
- Line 169: `if (!config || !wubu_manifest_ptr) return -1;`
- Line 218: `if (!config || !out_json || out_size < 1024) return -1;`
- Line 315: `if (!json || !config) return -1;`
- Line 411: `if (oci_config_to_json(config, json, sizeof(json)) < 0) return -1;`
- Line 419: `if (!manifest || !wubu_manifest_ptr) return -1;`
- Line 450: `if (!manifest || !out_json || out_size < 4096) return -1;`
- Line 482: `if (!json || !manifest) return -1;`
- Line 519: `if (oci_manifest_to_json(manifest, json, sizeof(json)) < 0) return -1;`
- ... and 46 more

#### return_0_stub (28)

- Line 130: `if (!pos) return 0;`
- Line 132: `if (!colon) return 0;`
- Line 161: `return 0;`
- Line 214: `return 0;`
- Line 311: `return 0;`
- Line 406: `return 0;`
- Line 413: `return 0;`
- Line 446: `return 0;`
- Line 478: `return 0;`
- Line 514: `return 0;`
- ... and 18 more

#### stub_keyword (1)

- Line 1161: `/* -- Cleanup Stub ------------------------------------------------- */`

### src/runtime/wubu_snapshot.c — 83 gaps

#### return_minus1 (53)

- Line 130: `if (ensure_dir(path) < 0 && errno != EEXIST) return -1;`
- Line 132: `if (ensure_dir(path) < 0 && errno != EEXIST) return -1;`
- Line 134: `if (ensure_dir(path) < 0 && errno != EEXIST) return -1;`
- Line 163: `if (!mgr) return -1;`
- Line 200: `if (!mgr) return -1;`
- Line 212: `if (!mgr) return -1;`
- Line 227: `if (!mgr || !container_id) return -1;`
- Line 228: `if (mgr->snapshot_count >= WUBU_MAX_SNAPSHOTS) return -1;`
- Line 299: `if (!mgr || !parent_id) return -1;`
- Line 301: `if (!parent) return -1;`
- ... and 43 more

#### return_0_stub (29)

- Line 135: `return 0;`
- Line 142: `if (!dir) return 0;`
- Line 196: `return 0;`
- Line 208: `return 0;`
- Line 214: `return 0;`
- Line 292: `return 0;`
- Line 326: `return 0;`
- Line 359: `return 0;`
- Line 381: `return 0;`
- Line 386: `if (!mgr || !out_snapshots || max <= 0) return 0;`
- ... and 19 more

#### stub_keyword (1)

- Line 869: `/* -- Helpers (already real in stub) ------------------------------- */`

### src/runtime/wubu_holyd.c — 76 gaps

#### return_minus1 (48)

- Line 135: `if (fd < 0) return -1;`
- Line 143: `return -1;`
- Line 147: `return -1;`
- Line 167: `if (!d || !name) return -1;`
- Line 168: `if (d->session_count >= d->config.max_sessions) return -1;`
- Line 204: `return -1;`
- Line 221: `if (!s) return -1;`
- Line 245: `if (!d || !out) return -1;`
- Line 253: `if (!s || !out) return -1;`
- Line 260: `if (!s) return -1;`
- ... and 38 more

#### stub_keyword (1)

- Line 207: `/* Initialize compiler placeholder */`

#### return_0_stub (27)

- Line 216: `return 0;`
- Line 241: `return 0;`
- Line 255: `return 0;`
- Line 264: `return 0;`
- Line 300: `return 0;`
- Line 319: `return 0;`
- Line 329: `return 0;`
- Line 362: `return 0;`
- Line 375: `return 0;`
- Line 388: `return 0;`
- ... and 17 more

### src/runtime/wubu_vsl.c — 72 gaps

#### return_minus1 (41)

- Line 76: `return -1;`
- Line 80: `if (child_host_pid <= 0) return -1;`
- Line 81: `if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;`
- Line 83: `if (vsl_pid < 0) return -1;`
- Line 150: `return -1;`
- Line 917: `return -1;`
- Line 1001: `if (!g_vsl.active) return -1;`
- Line 1002: `if (g_vsl.n_fds >= VSL_MAX_FDS) return -1;`
- Line 1192: `if (!g_vsl.active) return -1;`
- Line 1193: `if (g_vsl.n_procs >= VSL_MAX_PROCS) return -1;`
- ... and 31 more

#### return_0_stub (31)

- Line 112: `return 0;`
- Line 143: `return 0;`
- Line 318: `return 0;`
- Line 331: `return 0;`
- Line 436: `return 0;`
- Line 466: `return 0;`
- Line 479: `return 0;`
- Line 502: `return 0;`
- Line 514: `return 0;`
- Line 681: `return 0;`
- ... and 21 more

### src/runtime/wubu_image.c — 67 gaps

#### return_minus1 (51)

- Line 219: `return -1;`
- Line 252: `return -1;`
- Line 275: `if (!path || !ctx) return -1;`
- Line 280: `return -1;`
- Line 337: `return -1;`
- Line 370: `return -1;`
- Line 482: `if (!content || !ctx) return -1;`
- Line 487: `if (!f) return -1;`
- Line 500: `if (!digest || !out_data || !out_size) return -1;`
- Line 506: `if (fd < 0) return -1;`
- ... and 41 more

#### return_0_stub (16)

- Line 478: `return 0;`
- Line 706: `return 0;`
- Line 766: `return 0;`
- Line 983: `return 0;`
- Line 995: `return 0;`
- Line 1008: `return 0;`
- Line 1027: `return 0;`
- Line 1063: `return 0;`
- Line 1220: `return 0;`
- Line 1233: `return 0;`
- ... and 6 more

### src/runtime/wubu_proton.c — 49 gaps

#### return_minus1 (27)

- Line 110: `return -1;`
- Line 154: `if (!data || size < 64) return -1;`
- Line 157: `if (data[0] != 'M' || data[1] != 'Z') return -1;`
- Line 161: `if (pe_offset == 0 || pe_offset + 24 > size) return -1;`
- Line 165: `if (sig != PE_MAGIC) return -1;`
- Line 208: `if (!data || size < 64) return -1;`
- Line 212: `if (pe_offset + 24 > size) return -1;`
- Line 265: `if (!p || !map) return -1;`
- Line 266: `if (!p->api_table) return -1;`
- Line 275: `if (!p || !win32_name) return -1;`
- ... and 17 more

#### return_0_stub (22)

- Line 119: `return 0;`
- Line 140: `if (data[0] != 'M' || data[1] != 'Z') return 0;`
- Line 144: `if (pe_offset == 0 || pe_offset + 4 > size) return 0;`
- Line 148: `if (sig != PE_MAGIC) return 0;`
- Line 204: `return 0;`
- Line 244: `if (!p || p->num_sections == 0) return 0;`
- Line 256: `if (!p) return 0;`
- Line 271: `return 0;`
- Line 295: `return 0;`
- Line 312: `return 0;`
- ... and 12 more

### src/runtime/styx.c — 46 gaps

#### return_0_stub (37)

- Line 99: `return 0;`
- Line 111: `return 0;`
- Line 127: `return 0;`
- Line 141: `return 0;`
- Line 154: `return 0;`
- Line 164: `return 0;`
- Line 171: `return 0;`
- Line 177: `return 0;`
- Line 204: `return 0;`
- Line 312: `return 0;`
- ... and 27 more

#### return_minus1 (9)

- Line 308: `if (buf[4] != STX_RVERSION) return -1;`
- Line 318: `if (buf[4] != STX_TATTACH) return -1;`
- Line 330: `if (!srv || !inbuf || !outbuf || inlen < 7) return -1;`
- Line 625: `if (!buf || len < 12 || buf[4] != STX_TOPEN) return -1;`
- Line 633: `if (!buf || len < 23 || buf[4] != STX_TREAD) return -1;`
- Line 643: `if (!buf || len < 24 || buf[4] != STX_TWRITE) return -1;`
- Line 653: `if (!buf || len < 11 || buf[4] != STX_TCLUNK) return -1;`
- Line 660: `if (!buf || len < 11 || buf[4] != STX_TSTAT) return -1;`
- Line 668: `if (!buf || len < 18 || buf[4] != STX_TWALK) return -1;`

### src/runtime/wubu_archd.c — 45 gaps

#### return_minus1 (34)

- Line 139: `if (!f) return -1;`
- Line 157: `if (fd < 0) return -1;`
- Line 165: `return -1;`
- Line 170: `return -1;`
- Line 223: `if (!name || !d) return -1;`
- Line 224: `if (d->root_count >= WUBU_ARCHD_MAX_ROOTS) return -1;`
- Line 228: `if (strcmp(d->roots[i].name, name) == 0) return -1;`
- Line 287: `if (!name || !d) return -1;`
- Line 305: `return -1;`
- Line 309: `if (!d || !out) return -1;`
- ... and 24 more

#### return_0_stub (11)

- Line 142: `return 0;`
- Line 183: `return 0;`
- Line 320: `return 0;`
- Line 368: `return 0;`
- Line 439: `return 0;`
- Line 525: `return 0;`
- Line 543: `return 0;`
- Line 577: `return 0;`
- Line 602: `return 0;`
- Line 780: `return 0;`
- ... and 1 more

### src/runtime/styxfs_server.c — 44 gaps

#### return_0_stub (10)

- Line 75: `return 0;`
- Line 126: `return 0;`
- Line 195: `return 0;`
- Line 248: `return 0;`
- Line 269: `return 0;`
- Line 290: `return 0;`
- Line 295: `if (!fs) return 0;`
- Line 298: `if (!f) return 0;`
- Line 311: `return 0;`
- Line 400: `return 0;`

#### return_minus1 (34)

- Line 86: `return -1;`
- Line 116: `if (!fs) return -1;`
- Line 119: `if (!f) return -1;`
- Line 133: `if (!fs) return -1;`
- Line 136: `if (!f) return -1;`
- Line 178: `if (!nf) return -1;`
- Line 200: `if (!fs) return -1;`
- Line 203: `if (!f) return -1;`
- Line 209: `if (!path) return -1;`
- Line 222: `if (fd < 0) return -1;`
- ... and 24 more

### src/runtime/styxfs.c — 41 gaps

#### return_0_stub (21)

- Line 147: `if (root) return 0;`
- Line 154: `return 0;`
- Line 183: `return 0;`
- Line 194: `return 0;`
- Line 211: `return 0;`
- Line 259: `return 0;`
- Line 284: `return 0;`
- Line 296: `return 0;`
- Line 323: `return 0;`
- Line 350: `return 0;`
- ... and 11 more

#### return_minus1 (20)

- Line 149: `if (!root) return -1;`
- Line 160: `if (!srv || !path || !source) return -1;`
- Line 165: `if (!m) return -1;`
- Line 187: `if (!srv || !path) return -1;`
- Line 198: `return -1;`
- Line 208: `if (!srv || !mount_path) return -1;`
- Line 219: `if (!srv || !inbuf || !outbuf || !outlen) return -1;`
- Line 299: `return -1;`
- Line 326: `return -1;`
- Line 402: `return -1;`
- ... and 10 more

### src/runtime/wubu_bottles.c — 39 gaps

#### stub_keyword (1)

- Line 4: `* Cell 480: Stub implementation for .wubu container format.`

#### return_minus1 (22)

- Line 98: `if (!bottle || bottle->dep_count >= WUBU_BOTTLE_MAX_DEPS) return -1;`
- Line 127: `if (!bottle) return -1;`
- Line 137: `return -1;`
- Line 172: `if (!bottle || !host || !guest || bottle->mount_count >= WUBU_BOTTLE_MAX_MOUNTS) return -1;`
- Line 182: `if (!bottle || !guest_path) return -1;`
- Line 192: `return -1;`
- Line 200: `if (!bottle || !key || bottle->env_count >= WUBU_BOTTLE_MAX_ENV) return -1;`
- Line 280: `if (!bottle || !output_path) return -1;`
- Line 340: `if (!f) return -1;`
- Line 408: `if (!bottle || !install_dir) return -1;`
- ... and 12 more

#### return_0_stub (16)

- Line 123: `return 0;`
- Line 134: `return 0;`
- Line 142: `return 0;`
- Line 178: `return 0;`
- Line 189: `return 0;`
- Line 204: `return 0;`
- Line 239: `if (!key_pos) return 0;`
- Line 241: `if (!colon) return 0;`
- Line 343: `return 0;`
- Line 430: `return 0;`
- ... and 6 more

### src/runtime/wubu_exec.c — 36 gaps

#### return_0_stub (3)

- Line 90: `if (g_vsl_initialized) return 0;`
- Line 126: `return 0;`
- Line 580: `return 0;`

#### return_minus1 (31)

- Line 139: `if (wubu_vsl_init() != 0) return -1;`
- Line 142: `if (!cmd || !*cmd) return -1;`
- Line 147: `return -1;`
- Line 162: `return -1;`
- Line 171: `return -1;`
- Line 245: `if (!elf_data || elf_size < 4) return -1;`
- Line 250: `return -1;`
- Line 256: `if (!f) return -1;`
- Line 268: `return -1;`
- Line 280: `return -1;`
- ... and 21 more

#### void_cast (1)

- Line 229: `int (*native_main)(void);`

#### stub_keyword (1)

- Line 496: `/* Fallback: run in VSL with Rosetta-2-like hints (stub) */`

### src/runtime/wubu_proton2.c — 32 gaps

#### return_minus1 (20)

- Line 49: `if (!d) return -1;`
- Line 287: `if (!mgr) return -1;`
- Line 292: `if (!mgr->ramdisk) return -1;`
- Line 298: `return -1;`
- Line 307: `return -1;`
- Line 382: `if (!mgr || !app || mgr->n_apps >= WUBU_PROTON_MAX_APPS) return -1;`
- Line 392: `if (!mgr || !mgr->container_running) return -1;`
- Line 393: `if (app_idx < 0 || app_idx >= mgr->n_apps) return -1;`
- Line 420: `if (!mgr || !name) return -1;`
- Line 425: `return -1;`
- ... and 10 more

#### return_0_stub (11)

- Line 99: `if (!d) return 0;`
- Line 140: `if (!d) return 0;`
- Line 288: `if (mgr->container_running) return 0;`
- Line 359: `return 0;`
- Line 434: `return 0;`
- Line 454: `return 0;`
- Line 499: `return 0;`
- Line 525: `return 0;`
- Line 544: `return 0;`
- Line 617: `return 0;`
- ... and 1 more

#### stub_keyword (1)

- Line 664: `/* This is a stub  --  real implementation would check the filesystem */`

### src/runtime/wubu_ramdisk.c — 32 gaps

#### return_minus1 (21)

- Line 36: `if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;`
- Line 40: `if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;`
- Line 49: `return -1;`
- Line 139: `if (!rd || rd->mode != WUBU_RD_RAM) return -1;`
- Line 143: `if (mkdir_p(rd->path, 0755) != 0) return -1;`
- Line 158: `return -1;`
- Line 168: `if (!rd || rd->state < WUBU_RD_MOUNTED) return -1;`
- Line 209: `return -1;`
- Line 219: `if (!rd || rd->mode != WUBU_RD_RAM) return -1;`
- Line 235: `if (!rd) return -1;`
- ... and 11 more

#### return_0_stub (11)

- Line 41: `return 0;`
- Line 155: `return 0;`
- Line 162: `return 0;`
- Line 213: `return 0;`
- Line 220: `if (rd->state < WUBU_RD_MOUNTED) return 0;`
- Line 229: `return 0;`
- Line 249: `return 0;`
- Line 261: `return 0;`
- Line 266: `return 0;`
- Line 284: `if (!rd) return 0;`
- ... and 1 more

### src/runtime/wubu_pkg.c — 26 gaps

#### return_minus1 (16)

- Line 19: `if (!mgr || !name || mgr->n_entries >= PKG_MAX_REGISTRY) return -1;`
- Line 32: `if (!mgr || !name) return -1;`
- Line 41: `return -1;`
- Line 57: `if (!mgr || !name || !dep) return -1;`
- Line 59: `if (!e || e->ndeps >= PKG_MAX_DEPS) return -1;`
- Line 66: `if (!mgr || !name) return -1;`
- Line 68: `if (!e) return -1;`
- Line 77: `if (!mgr || !name) return -1;`
- Line 78: `if (mgr->readonly) return -1;`
- Line 80: `if (!e) return -1;`
- ... and 6 more

#### return_0_stub (10)

- Line 28: `return 0;`
- Line 38: `return 0;`
- Line 62: `return 0;`
- Line 71: `if (!d || d->state != PKG_STATE_INSTALLED) return 0;`
- Line 83: `return 0;`
- Line 92: `return 0;`
- Line 101: `if (!mgr) return 0;`
- Line 114: `return 0;`
- Line 124: `return 0;`
- Line 131: `if (!mgr || !out) return 0;`

### src/runtime/wubu_ct_isolate.c — 24 gaps

#### return_0_stub (11)

- Line 371: `return 0;`
- Line 408: `return 0;`
- Line 443: `return 0;`
- Line 528: `if (profile == SECCOMP_PROFILE_NONE) return 0;`
- Line 571: `return 0;`
- Line 584: `return 0;`
- Line 626: `return 0;`
- Line 686: `return 0;`
- Line 705: `if (profile == SECCOMP_PROFILE_NONE) return 0;`
- Line 739: `return 0;`
- ... and 1 more

#### return_minus1 (12)

- Line 390: `if (fd < 0) return -1;`
- Line 402: `if (fd < 0) return -1;`
- Line 406: `if (n <= 0) return -1;`
- Line 418: `return -1;`
- Line 433: `return -1;`
- Line 562: `return -1;`
- Line 567: `return -1;`
- Line 577: `if (!ct) return -1;`
- Line 695: `if (!path || !value) return -1;`
- Line 697: `if (fd < 0) return -1;`
- ... and 2 more

#### stub_keyword (1)

- Line 587: `/* Store path for later cleanup/attach */`

### src/runtime/wubu_host_exec.c — 18 gaps

#### return_minus1 (12)

- Line 106: `if (!ct || !argv) return -1;`
- Line 116: `if (!ct || !env) return -1;`
- Line 128: `if (!ct || !host || !guest) return -1;`
- Line 129: `if (ct->n_binds >= WUBU_CT_MAX_BINDS) return -1;`
- Line 168: `if (ct->styx_fd < 0) return -1;`
- Line 179: `return -1;`
- Line 249: `if (!ct || ct->state == CT_RUNNING) return -1;`
- Line 250: `if (!ct->argv[0]) return -1;`
- Line 274: `return -1;`
- Line 356: `if (!ct || ct->pid <= 0) return -1;`
- ... and 2 more

#### return_0_stub (6)

- Line 112: `return 0;`
- Line 121: `return 0;`
- Line 134: `return 0;`
- Line 182: `return 0;`
- Line 350: `return 0;`
- Line 381: `return 0;`

### src/runtime/wubu_arch.c — 16 gaps

#### return_minus1 (13)

- Line 41: `return -1;`
- Line 46: `return -1;`
- Line 58: `return -1;`
- Line 75: `if (fd < 0) return -1;`
- Line 87: `if (!root_path) return -1;`
- Line 95: `return -1;`
- Line 126: `return -1;`
- Line 139: `return -1;`
- Line 145: `return -1;`
- Line 155: `if (!root_path) return -1;`
- ... and 3 more

#### return_0_stub (3)

- Line 47: `return 0;`
- Line 149: `return 0;`
- Line 203: `return 0;`

### src/runtime/wubu_anticheat.c — 14 gaps

#### stub_keyword (5)

- Line 4: `* Cell 470: Stub implementations for anti-cheat compatibility.`
- Line 6: `* This is RESEARCH/STUB code. Real anti-cheat support requires:`
- Line 224: `fprintf(stderr, "wubu_anticheat: kernel_load stub - driver=%s device=%s\n", driver_path, device_name`
- Line 225: `return -1;  /* Not implemented in hosted mode */`
- Line 230: `fprintf(stderr, "wubu_anticheat: kernel_unload stub - device=%s\n", device_name);`

#### return_0_stub (5)

- Line 144: `if (!prefix_path || !out_types || max <= 0) return 0;`
- Line 178: `return 0;`
- Line 186: `return 0;`
- Line 209: `return 0;`
- Line 297: `return 0;`

#### return_minus1 (4)

- Line 173: `if (type >= 12) return -1;`
- Line 174: `if (!fn) return -1;`
- Line 182: `if (type >= 12) return -1;`
- Line 231: `return -1;`

---

## KERNEL (INTERRUPT, FAT32, TASKING, MEMORY, AHCI, TXFS) (254 GAPS)

### src/kernel/interrupt.c — 113 gaps

#### void_cast (64)

- Line 24: `extern void isr0(void);   extern void isr1(void);   extern void isr2(void);   extern void isr3(void)`
- Line 25: `extern void isr4(void);   extern void isr5(void);   extern void isr6(void);   extern void isr7(void)`
- Line 26: `extern void isr8(void);   extern void isr9(void);   extern void isr10(void);  extern void isr11(void`
- Line 27: `extern void isr12(void);  extern void isr13(void);  extern void isr14(void);  extern void isr15(void`
- Line 28: `extern void isr16(void);  extern void isr17(void);  extern void isr18(void);  extern void isr19(void`
- Line 29: `extern void isr20(void);  extern void isr21(void);  extern void isr22(void);  extern void isr23(void`
- Line 30: `extern void isr24(void);  extern void isr25(void);  extern void isr26(void);  extern void isr27(void`
- Line 31: `extern void isr28(void);  extern void isr29(void);  extern void isr30(void);  extern void isr31(void`
- Line 32: `extern void isr32(void);  extern void isr33(void);  extern void isr34(void);  extern void isr35(void`
- Line 33: `extern void isr36(void);  extern void isr37(void);  extern void isr38(void);  extern void isr39(void`
- ... and 54 more

#### return_minus1 (27)

- Line 448: `if (hz == 0 || hz > 1193182) return -1;`
- Line 491: `return -1;`
- Line 755: `return -1;`
- Line 765: `if (irq >= g_ioapic_irq_count) return -1;`
- Line 777: `return -1;`
- Line 783: `if (irq >= g_ioapic_irq_count) return -1;`
- Line 789: `return -1;`
- Line 795: `if (irq >= g_ioapic_irq_count) return -1;`
- Line 801: `return -1;`
- Line 811: `if (!g_lapic_base) return -1;`
- ... and 17 more

#### return_0_stub (20)

- Line 488: `return 0;`
- Line 594: `return 0;`
- Line 600: `return 0;`
- Line 644: `if (!g_lapic_base) return 0;`
- Line 655: `if (!g_ioapic_base) return 0;`
- Line 749: `return 0;`
- Line 774: `return 0;`
- Line 786: `return 0;`
- Line 798: `return 0;`
- Line 824: `return 0;`
- ... and 10 more

#### stub_keyword (2)

- Line 746: `/* Load TSS (kernel will do ltr in GDT setup - this is a stub) */`
- Line 1111: `/* Syscall dispatcher - called from syscall_entry assembly stub */`

### src/kernel/fat32.c — 57 gaps

#### return_minus1 (40)

- Line 37: `if (!vol->fat_cache) return -1;`
- Line 40: `if (rc != 0) return -1;`
- Line 58: `if (!vol->fat_cache) return -1;`
- Line 63: `if (rc != 0) return -1;`
- Line 71: `if (rc != 0) return -1;`
- Line 74: `if (rc != 0) return -1;`
- Line 183: `if (rc != 0) return -1;`
- Line 186: `if (bs.signature != 0xAA55) return -1;`
- Line 187: `if (bs.bytes_per_sector != FAT32_SECTOR_SIZE) return -1;`
- Line 188: `if (bs.sectors_per_cluster == 0) return -1;`
- ... and 30 more

#### return_0_stub (17)

- Line 45: `return 0;`
- Line 76: `return 0;`
- Line 224: `return 0;`
- Line 352: `return 0;`
- Line 358: `if (cluster < 2) return 0;`
- Line 360: `if (fat_read_entry(vol, cluster, &entry) != 0) return 0;`
- Line 366: `if (cluster < 2) return 0;`
- Line 371: `if (lba < vol->data_lba) return 0;`
- Line 377: `if (count == 0) return 0;`
- Line 391: `if (fat_read_entry(vol, cluster, &entry) != 0) return 0;`
- ... and 7 more

### src/kernel/ahci.c — 23 gaps

#### return_0_stub (4)

- Line 50: `return 0;`
- Line 117: `return 0;`
- Line 165: `return 0;`
- Line 255: `return 0;`

#### return_minus1 (19)

- Line 77: `if (!hba || !hba->initialized) return -1;`
- Line 96: `if (!hba || port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;`
- Line 99: `if (p->state == AHCI_PORT_EMPTY) return -1;`
- Line 103: `if (!p->cmd_list) return -1;`
- Line 111: `return -1;`
- Line 123: `if (!hba || port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;`
- Line 126: `if (p->state != AHCI_PORT_ACTIVE) return -1;`
- Line 199: `if (!hba || !buf) return -1;`
- Line 200: `if (port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;`
- Line 203: `if (p->state != AHCI_PORT_ACTIVE) return -1;`
- ... and 9 more

### src/kernel/txfs.c — 16 gaps

#### return_minus1 (9)

- Line 61: `if (!tx->journal) return -1;`
- Line 100: `if (!tx) return -1;`
- Line 114: `if (!tx || !tx->journal || !entry) return -1;`
- Line 137: `if (!tx || !tx->txn_active) return -1;`
- Line 163: `return -1;`
- Line 194: `if (!tx || !tx->txn_active) return -1;`
- Line 212: `if (!tx || !tx->txn_active) return -1;`
- Line 213: `if (tx->active_txn.op_count >= TXFS_MAX_TXN_OPS) return -1;`
- Line 262: `if (!tx) return -1;`

#### return_0_stub (7)

- Line 79: `return 0;`
- Line 127: `return 0;`
- Line 145: `return 0;`
- Line 190: `return 0;`
- Line 200: `return 0;`
- Line 231: `return 0;`
- Line 269: `if (unapplied == 0) return 0;`

### src/kernel/memory.c — 15 gaps

#### return_minus1 (10)

- Line 53: `if (front[i] != MEM_CANARY_FRONT) return -1;`
- Line 57: `if (back[i] != MEM_CANARY_BACK) return -1;`
- Line 83: `if (!g_heap_base) return -1;`
- Line 91: `if (!g_heap) return -1;`
- Line 271: `if (!g_heap) return -1;`
- Line 272: `if (g_heap->hc_signature != HEAP_CTRL_SIGNATURE) return -1;`
- Line 375: `if (!g_heap || !g_heap_base) return -1;`
- Line 426: `if (!ptr || !g_heap) return -1;`
- Line 429: `if (mu->signature != MEM_USED_SIGNATURE) return -1;`
- Line 436: `if (!g_heap) return -1;`

#### return_0_stub (5)

- Line 59: `return 0;`
- Line 99: `return 0;`
- Line 266: `if (!g_heap) return 0;`
- Line 273: `return 0;`
- Line 324: `if (!g_heap || !g_heap_base) return 0;`

### src/kernel/tasking.c — 8 gaps

#### return_0_stub (4)

- Line 71: `if (!g_head) return 0;`
- Line 279: `if (g_initialized) return 0;`
- Line 288: `return 0;`
- Line 444: `if (!g_head || !callback) return 0;`

#### return_minus1 (1)

- Line 283: `if (!idle) return -1;`

#### stub_keyword (3)

- Line 416: `/* Context switch save (placeholder for assembly) */`
- Line 422: `/* Context switch restore (placeholder for assembly) */`
- Line 428: `/* Update derived values (placeholder for ZealOS compatibility) */`

### src/kernel/input.c — 6 gaps

#### stub_keyword (2)

- Line 2: `* input.c  --  My Seed Input Subsystem (hosted stub)`
- Line 36: `/* Nothing to clean up for stub */`

#### return_0_stub (4)

- Line 32: `return 0;`
- Line 61: `if (g_key_count == 0) return 0;`
- Line 76: `return 0;`
- Line 105: `if (g_mouse_count == 0) return 0;`

### src/kernel/fat32_test.c — 6 gaps

#### return_minus1 (4)

- Line 23: `if (!g_ram_disk) return -1;`
- Line 24: `if (lba + n > RAM_DISK_SECTORS) return -1;`
- Line 31: `if (!g_ram_disk) return -1;`
- Line 32: `if (lba + n > RAM_DISK_SECTORS) return -1;`

#### return_0_stub (2)

- Line 26: `return 0;`
- Line 34: `return 0;`

### src/kernel/vbe.c — 4 gaps

#### return_minus1 (1)

- Line 46: `return -1;`

#### return_0_stub (3)

- Line 51: `return 0;`
- Line 74: `return 0;`
- Line 261: `if (w <= 0 || h <= 0) return 0;`

### src/kernel/wubu_gaad.c — 2 gaps

#### return_0_stub (1)

- Line 24: `if (n <= 0) return 0;`

#### return_minus1 (1)

- Line 270: `if (!decomp) return -1;`

### src/kernel/metal_main.c — 2 gaps

#### void_cast (2)

- Line 135: `extern void wubu_gaad_init(void);`
- Line 156: `extern void task_preempt_enable(void);`

### src/kernel/ps2.c — 1 gaps

#### return_0_stub (1)

- Line 155: `return 0;`

### src/kernel/wubu_math.c — 1 gaps

#### return_0_stub (1)

- Line 485: `return 0;`

---

## GUI (WM, DESKTOP, STARTMENU, EXPLORER, TERMINAL, PROTON, GAMELIB) (326 GAPS)

### src/gui/wubu_proton.c — 52 gaps

#### return_0_stub (21)

- Line 47: `if (!d) return 0;`
- Line 208: `return 0;`
- Line 223: `return 0;`
- Line 288: `if (strcmp(g_proton.prefixes[i].id, id) == 0) return 0;`
- Line 316: `return 0;`
- Line 341: `return 0;`
- Line 356: `return 0;`
- Line 383: `return 0;`
- Line 388: `return 0;`
- Line 407: `return 0;`
- ... and 11 more

#### return_minus1 (31)

- Line 120: `if (!f) return -1;`
- Line 134: `if (!data) return -1;`
- Line 225: `return -1;`
- Line 229: `if (!g_proton.steamapps_path[0]) return -1;`
- Line 240: `if (!d) return -1;`
- Line 284: `if (!id || g_proton.prefix_count >= PROTON_MAX_PREFIXES) return -1;`
- Line 344: `return -1;`
- Line 349: `if (!p) return -1;`
- Line 377: `if (!game || g_proton.game_count >= PROTON_MAX_GAMES) return -1;`
- Line 410: `return -1;`
- ... and 21 more

### src/gui/wubu_gamelib.c — 39 gaps

#### return_0_stub (18)

- Line 83: `if (!d) return 0;`
- Line 121: `return 0;`
- Line 196: `return 0;`
- Line 212: `return 0;`
- Line 225: `return 0;`
- Line 235: `return 0;`
- Line 245: `return 0;`
- Line 290: `return 0;`
- Line 300: `return 0;`
- Line 310: `return 0;`
- ... and 8 more

#### return_minus1 (18)

- Line 206: `if (!game || g_gamelib.game_count >= GAME_LIB_MAX_ENTRIES) return -1;`
- Line 238: `return -1;`
- Line 248: `return -1;`
- Line 287: `if (!cat || g_gamelib.category_count >= GAME_LIB_MAX_CATEGORIES) return -1;`
- Line 303: `return -1;`
- Line 313: `return -1;`
- Line 327: `if (!game) return -1;`
- Line 338: `if (!f) return -1;`
- Line 351: `if (!data) return -1;`
- Line 391: `if (!ps->steamapps_path[0]) return -1;`
- ... and 8 more

#### block_todo (2)

- Line 329: `GameLibraryEntry *mut = (GameLibraryEntry*)game; /* FIXME: const */`
- Line 668: `/* TODO: Add to start menu when startmenu API is available */`

#### stub_keyword (1)

- Line 677: `/* For now, placeholder */`

### src/gui/wubu_mime.c — 24 gaps

#### return_0_stub (10)

- Line 222: `return 0;`
- Line 239: `if (entry.hidden || entry.no_display) return 0;`
- Line 240: `if (strcmp(entry.type, "Application") != 0) return 0;`
- Line 243: `return 0;`
- Line 289: `return 0;`
- Line 394: `return 0;`
- Line 402: `return 0;`
- Line 413: `return 0;`
- Line 467: `return 0;`
- Line 499: `return 0;`

#### return_minus1 (13)

- Line 235: `if (g_mime.desktop_entry_count >= MAX_DESKTOP_ENTRIES) return -1;`
- Line 238: `if (!parse_desktop_file(path, &entry)) return -1;`
- Line 386: `if (!extension || !handler_id) return -1;`
- Line 387: `if (g_mime.association_count >= MAX_MIME_TYPES) return -1;`
- Line 416: `return -1;`
- Line 431: `if (!cmd_template || !file_path) return -1;`
- Line 469: `return -1;`
- Line 474: `if (!de || !de->exec[0]) return -1;`
- Line 480: `if (!de) return -1;`
- Line 485: `if (!url) return -1;`
- ... and 3 more

#### stub_keyword (1)

- Line 429: `/* Simple command execution with placeholder substitution */`

### src/gui/wubu_session.c — 21 gaps

#### return_minus1 (7)

- Line 144: `if (!ensure_session_dirs()) return -1;`
- Line 147: `if (!f) return -1;`
- Line 231: `if (g_autostart_count >= MAX_AUTOSTART_ENTRIES) return -1;`
- Line 262: `return -1;`
- Line 314: `if (!ensure_session_dirs()) return -1;`
- Line 317: `if (!f) return -1;`
- Line 336: `if (!f) return -1;`

#### return_0_stub (11)

- Line 159: `return 0;`
- Line 189: `return 0;`
- Line 225: `return 0;`
- Line 237: `return 0;`
- Line 250: `return 0;`
- Line 259: `return 0;`
- Line 330: `return 0;`
- Line 369: `return 0;`
- Line 386: `return 0;`
- Line 449: `if (!g_shutdown_dialog_open) return 0;`
- ... and 1 more

#### void_cast (3)

- Line 408: `extern void dosgui_platform_shutdown(void);`
- Line 413: `extern void dosgui_platform_shutdown(void);`
- Line 418: `extern void dosgui_platform_shutdown(void);`

### src/gui/wubu_trash.c — 21 gaps

#### return_0_stub (8)

- Line 26: `return 0;`
- Line 32: `if (!d) return 0;`
- Line 231: `return 0;`
- Line 324: `return 0;`
- Line 373: `return 0;`
- Line 418: `return 0;`
- Line 428: `return 0;`
- Line 474: `if (!g_trash.auto_expire_enabled || g_trash.auto_expire_days <= 0) return 0;`

#### return_minus1 (13)

- Line 173: `return -1;`
- Line 240: `if (!path || !path[0]) return -1;`
- Line 250: `if (!getcwd(cwd, sizeof(cwd))) return -1;`
- Line 282: `if (!unique_name) return -1;`
- Line 287: `return -1;`
- Line 296: `return -1;`
- Line 328: `if (!trash_name) return -1;`
- Line 340: `if (idx < 0) return -1;`
- Line 353: `return -1;`
- Line 389: `if (!trash_name) return -1;`
- ... and 3 more

### src/gui/dosgui_wm.c — 19 gaps

#### void_cast (8)

- Line 132: `static const WubuThemeColors *tc(void);`
- Line 133: `static const WubuTheme *theme(void);`
- Line 134: `static int title_bar_height(void);`
- Line 135: `static int taskbar_height_dynamic(void);`
- Line 136: `static int border_width(void);`
- Line 137: `static int theme_radius(void);`
- Line 138: `static void load_default_wallpaper(void);`
- Line 147: `char *dosgui_taskbar_get_clock_str(void);`

#### return_minus1 (7)

- Line 337: `if (!win) return -1;`
- Line 372: `return -1;`
- Line 949: `if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;`
- Line 965: `if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;`
- Line 1000: `return -1;`
- Line 1036: `return -1;`
- Line 1237: `if (g_dwm.systray_count >= DOSGUI_MAX_SYSTRAY_ICONS) return -1;`

#### return_0_stub (1)

- Line 577: `return 0;`

#### stub_keyword (3)

- Line 1612: `(void)wubu_notify_simple("Desktop", "Rename", "F2 to rename (stub)", NULL, 1, 3000);`
- Line 1636: `(void)wubu_notify_simple("Desktop", "Create Shortcut", "Right-click empty space -> New -> Shortcut (`
- Line 1640: `(void)wubu_notify_simple("Desktop", "View", "Desktop view options (stub)", NULL, 1, 3000);`

### src/gui/wubu_pkgmgr.c — 16 gaps

#### return_0_stub (13)

- Line 69: `return 0;`
- Line 91: `return 0;`
- Line 170: `return 0;`
- Line 181: `return 0;`
- Line 635: `return 0;`
- Line 653: `return 0;`
- Line 659: `return 0;`
- Line 803: `if (!g_pkgmgr.initialized) return 0;`
- Line 1100: `if (!g_pkgmgr.initialized) return 0;`
- Line 1111: `if (!buffer) return 0;`
- ... and 3 more

#### return_minus1 (2)

- Line 168: `return -1;`
- Line 179: `return -1;`

#### stub_keyword (1)

- Line 485: `/* Simple FNV-1a hash as a placeholder signature */`

### src/gui/dosgui_term.c — 9 gaps

#### return_0_stub (2)

- Line 74: `return 0;`
- Line 374: `return 0;`

#### return_minus1 (6)

- Line 141: `if (g_term.tab_count >= TERM_MAX_TABS) return -1;`
- Line 165: `return -1;`
- Line 303: `return -1;`
- Line 310: `return -1;`
- Line 318: `return -1;`
- Line 327: `return -1;`

#### stub_keyword (1)

- Line 1011: `vbe_draw_text(x + 10, y + 10, "[Container Session - Not Implemented]", 0x808080, 1);`

### src/gui/dosgui_daemon_panel.c — 7 gaps

#### return_minus1 (5)

- Line 92: `if (!conn || !path) return -1;`
- Line 97: `return -1;`
- Line 110: `return -1;`
- Line 131: `if (conn->fd < 0) return -1;`
- Line 138: `if (conn->fd < 0) return -1;`

#### return_0_stub (2)

- Line 118: `return 0;`
- Line 425: `return 0;`

### src/gui/wm_nano/textedit.c — 7 gaps

#### comment_todo (6)

- Line 11: `// TODO: Support Macintosh(CR) line ending?`
- Line 988: `// TODO: make it so that redraw1 and redraw2 don't overlap.`
- Line 1646: `// TODO: if (TextInput_CanUndo(this))`
- Line 1783: `TextInput_RepaintLine(this, pData->m_cursorY); // TODO: More efficient way.`
- Line 1870: `// TODO text selection and stuff`
- Line 1933: `// TODO: handle these better.  Ignore 0xE0 for now.`

#### stub_keyword (1)

- Line 1601: `SLogMsg("TextInput: The command %d is not implemented", command);`

### src/gui/dosgui_wm_test_stub.c — 6 gaps

#### stub_keyword (6)

- Line 10: `/* Stub for startmenu toggle */`
- Line 15: `/* Stub for startmenu open/close */`
- Line 21: `/* Stub for launch app */`
- Line 24: `/* Stub for shutdown */`
- Line 27: `/* Stub for platform shutdown */`
- Line 30: `/* Stub for WM mouse handler (called from holyd) — weak to allow override */`

### src/gui/startmenu.c — 6 gaps

#### return_minus1 (5)

- Line 35: `if (g_count >= STARTMENU_MAX_ENTRIES) return -1;`
- Line 98: `if (index < 0 || index >= g_count) return -1;`
- Line 100: `if (!e->enabled) return -1;`
- Line 109: `if (!g_open) return -1;`
- Line 120: `return -1;`

#### return_0_stub (1)

- Line 171: `if (!g_open) return 0;`

### src/gui/wubu_notify.c — 6 gaps

#### void_cast (3)

- Line 70: `static void notify_update_animation(void);`
- Line 73: `static void notify_layout(void);`
- Line 77: `static uint64_t get_time_ms(void);`

#### return_0_stub (2)

- Line 85: `return 0;`
- Line 97: `if (g_notify.count >= MAX_NOTIFICATIONS) return 0;`

#### return_minus1 (1)

- Line 342: `return -1;`

### src/gui/wm_nano/window.c — 6 gaps

#### return_0_stub (1)

- Line 60: `return 0;`

#### return_minus1 (1)

- Line 149: `return -1;`

#### comment_todo (4)

- Line 237: `//TODO?`
- Line 241: `// TODO: fix this will leave garbage that should be occluded out. Fix this by adding a RefreshRectan`
- Line 642: `// TODO: For now.`
- Line 993: `// TODO  Free any buffers passed through such events.`

### src/gui/wubu_settings.c — 5 gaps

#### return_minus1 (3)

- Line 387: `if (!ensure_config_dir()) return -1;`
- Line 389: `if (!f) return -1;`
- Line 482: `if (!f) return -1;`

#### return_0_stub (2)

- Line 404: `return 0;`
- Line 517: `return 0;`

### src/gui/gui_dbuf.c — 5 gaps

#### return_minus1 (1)

- Line 97: `if (!db->back) return -1;`

#### return_0_stub (4)

- Line 98: `return 0;`
- Line 246: `if (!db || !db->back) return 0;`
- Line 274: `if (!db || !db->back) return 0;`
- Line 275: `if (x < 0 || x >= db->width || y < 0 || y >= db->height) return 0;`

### src/gui/dosgui_explorer.c — 5 gaps

#### return_0_stub (2)

- Line 102: `return 0;`
- Line 157: `return 0;`

#### stub_keyword (3)

- Line 552: `/* Stub: would use libzip to read zip central directory */`
- Line 557: `return false; /* Not implemented */`
- Line 724: `"Open: %s (not implemented)", entry->name);`

### src/gui/wm_nano/theme.c — 5 gaps

#### return_0_stub (1)

- Line 568: `if (type < 0 || type >= P_THEME_PARM_COUNT) return 0;`

#### comment_todo (4)

- Line 617: `// TODO: Do something other than just simply grabbing the green channel`
- Line 641: `//TODO: Ensure safety`
- Line 721: `//TODO`
- Line 727: `//TODO`

### src/gui/wm_nano/table.c — 5 gaps

#### comment_todo (3)

- Line 49: `// TODO we're doing leaps of faith right now, maybe we shouldn't!`
- Line 412: `// TODO: Choose better colors.`
- Line 431: `// TODO: Allow scrolling via the column count as well.`

#### return_minus1 (2)

- Line 336: `return -1;`
- Line 355: `return -1;`

### src/gui/dosgui_startmenu_test_stub.c — 4 gaps

#### stub_keyword (4)

- Line 9: `/* Stub for taskbar height */`
- Line 14: `/* Stub for screen h/w */`
- Line 22: `/* Stub for launch app */`
- Line 25: `/* Stub for shutdown */`

---

## BEAR RL (NN, PPO, GAAD, VULKAN, CUDA, CUDNN, ENV) (212 GAPS)

### src/bear/bear_nn.c — 47 gaps

#### return_minus1 (37)

- Line 22: `if (!net || !param_arena || obs_dim <= 0 || act_dim <= 0) return -1;`
- Line 23: `if (num_hid < 1) return -1;`
- Line 35: `if (!net->layers) return -1;`
- Line 46: `if (!l->param) return -1;`
- Line 47: `if (bear_param_create(param_arena, l->param, hid_sizes[i], prev, "policy.hid") != 0) return -1;`
- Line 58: `if (!actor->param) return -1;`
- Line 59: `if (bear_param_create(param_arena, actor->param, act_dim, prev, "policy.actor") != 0) return -1;`
- Line 69: `if (!net || !param_arena || obs_dim <= 0 || act_dim <= 0 || hid_size <= 0) return -1;`
- Line 81: `if (!net->layers) return -1;`
- Line 90: `if (!in_proj->param) return -1;`
- ... and 27 more

#### return_0_stub (9)

- Line 63: `return 0;`
- Line 108: `return 0;`
- Line 365: `return 0;`
- Line 421: `return 0;`
- Line 680: `return 0;`
- Line 832: `return 0;`
- Line 942: `return 0;`
- Line 974: `return 0;`
- Line 979: `return 0;`

#### stub_keyword (1)

- Line 970: `* Checkpointing (stub)`

### src/bear/bear_vulkan.c — 28 gaps

#### return_minus1 (18)

- Line 132: `return -1;`
- Line 240: `if (!info) return -1;`
- Line 253: `if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) return -1;`
- Line 259: `return -1;`
- Line 342: `return -1;`
- Line 646: `if (!ctx || !host || !gpu) return -1;`
- Line 661: `if (!gpu->shape) return -1;`
- Line 670: `return -1;`
- Line 685: `return -1;`
- Line 721: `if (!ctx || !gpu) return -1;`
- ... and 8 more

#### return_0_stub (7)

- Line 309: `return 0;`
- Line 714: `return 0;`
- Line 747: `return 0;`
- Line 795: `return 0;`
- Line 825: `return 0;`
- Line 891: `if (!ctx || !out || max_events <= 0) return 0;`
- Line 1386: `return 0;`

#### block_todo (3)

- Line 1077: `/* TODO: Implement full forward pass using matmul, add, relu, softmax kernels */`
- Line 1238: `/* TODO: Implement full GAE dispatch */`
- Line 1303: `/* TODO: Implement full env step dispatch */`

### src/bear/bear_cuda.c — 24 gaps

#### return_minus1 (18)

- Line 145: `if (!info) return -1;`
- Line 149: `if (device_id < 0 || device_id >= count) return -1;`
- Line 153: `if (err != cudaSuccess) return -1;`
- Line 332: `if (!ctx || !host || !gpu) return -1;`
- Line 346: `if (!gpu->shape) return -1;`
- Line 357: `if (!dev_ptr) return -1;`
- Line 362: `if (err != cudaSuccess) return -1;`
- Line 371: `if (!ctx || !gpu) return -1;`
- Line 384: `if (!gpu->shape) return -1;`
- Line 396: `return -1;`
- ... and 8 more

#### return_0_stub (6)

- Line 168: `return 0;`
- Line 364: `return 0;`
- Line 400: `return 0;`
- Line 419: `return 0;`
- Line 429: `return 0;`
- Line 456: `if (!ctx || !out || max_events <= 0) return 0;`

### src/bear/bear_ppo.c — 13 gaps

#### return_minus1 (6)

- Line 22: `if (!t || !arena) return -1;`
- Line 522: `if (!trainer || !policy || !critic || !env || !cfg) return -1;`
- Line 526: `if (bear_arena_create(&trainer->global_arena, global_arena_cap) != 0) return -1;`
- Line 529: `return -1;`
- Line 534: `return -1;`
- Line 545: `return -1;`

#### return_0_stub (5)

- Line 60: `return 0;`
- Line 208: `if (s->cursor >= s->num_minibatches) return 0;`
- Line 551: `return 0;`
- Line 770: `return 0;`
- Line 775: `return 0;`

#### stub_keyword (2)

- Line 108: `/* V-Trace requires target policy logprobs - placeholder for now */`
- Line 460: `/* Stub  --  actual gradient computation happens in bear_ppo_apply_gradients`

### src/bear/bear_vulkan_soft.c — 12 gaps

#### return_minus1 (5)

- Line 54: `if (!info) return -1;`
- Line 107: `if (!host || !gpu) return -1;`
- Line 125: `if (!gpu) return -1;`
- Line 143: `if (!gpu || !host) return -1;`
- Line 151: `if (!src || !dst) return -1;`

#### return_0_stub (7)

- Line 59: `return 0;`
- Line 118: `return 0;`
- Line 138: `return 0;`
- Line 146: `return 0;`
- Line 154: `return 0;`
- Line 158: `(void)ctx; if (!gpu) return -1; gpu->mapped = 1; return 0;`
- Line 453: `return 0;`

### src/bear/bear_cartpole_video.c — 11 gaps

#### return_minus1 (8)

- Line 161: `return -1;`
- Line 261: `if (bear_arena_create(&global_arena, global_cap) != 0) return -1;`
- Line 262: `if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return -1;`
- Line 263: `if (bear_arena_create(&step_arena, step_cap) != 0) return -1;`
- Line 268: `if (!train_env) return -1;`
- Line 280: `train_env->spec.act_discrete, phid, 2) != 0) return -1;`
- Line 288: `if (bear_value_create(&critic, &global_arena, train_env->spec.obs_dim, vhid, 2) != 0) return -1;`
- Line 325: `global_cap, rollout_cap, step_cap) != 0) return -1;`

#### return_0_stub (3)

- Line 432: `return 0;`
- Line 451: `return 0;`
- Line 478: `return 0;`

### src/bear/bear_cartpole_v1_solve.c — 10 gaps

#### return_minus1 (8)

- Line 47: `return -1;`
- Line 131: `if (bear_arena_create(&global_arena, global_cap) != 0) return -1;`
- Line 132: `if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return -1;`
- Line 133: `if (bear_arena_create(&step_arena, step_cap) != 0) return -1;`
- Line 140: `return -1;`
- Line 158: `return -1;`
- Line 170: `return -1;`
- Line 210: `return -1;`

#### return_0_stub (2)

- Line 292: `return 0;`
- Line 306: `return 0;`

### src/bear/bear_cudnn.c — 10 gaps

#### stub_keyword (4)

- Line 7: `* If CUDA/cuDNN not available, provides stub implementations.`
- Line 45: `void* handle;  /* stub */`
- Line 65: `/* Stub implementation */`
- Line 176: `void* handle;  /* stub */`

#### return_0_stub (6)

- Line 471: `if (!handle) return 0;`
- Line 491: `return 0;`
- Line 503: `if (!handle) return 0;`
- Line 523: `return 0;`
- Line 535: `if (!handle) return 0;`
- Line 555: `return 0;`

### src/bear/bear_cartpole_v1_test.c — 9 gaps

#### return_minus1 (7)

- Line 113: `if (bear_arena_create(&global_arena, global_cap) != 0) return -1;`
- Line 114: `if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return -1;`
- Line 115: `if (bear_arena_create(&step_arena, step_cap) != 0) return -1;`
- Line 122: `return -1;`
- Line 138: `return -1;`
- Line 148: `return -1;`
- Line 188: `return -1;`

#### return_0_stub (2)

- Line 272: `return 0;`
- Line 286: `return 0;`

### src/bear/bear_mujoco.c — 9 gaps

#### return_0_stub (2)

- Line 131: `if (g_mjb.initialized) return 0;`
- Line 221: `return 0;`

#### return_minus1 (7)

- Line 133: `if (num_poles < 1 || num_poles > MJ_MAX_POLES) return -1;`
- Line 134: `if (num_envs < 1 || num_envs > MJ_MAX_ENVS) return -1;`
- Line 142: `if (!xml) return -1;`
- Line 158: `return -1;`
- Line 166: `return -1;`
- Line 181: `return -1;`
- Line 209: `return -1;`

### src/bear/bear_gaad_train.c — 8 gaps

#### return_0_stub (3)

- Line 18: `if (!policy || !policy->layers) return 0;`
- Line 29: `if (!critic || !critic->layers) return 0;`
- Line 165: `return 0;`

#### return_minus1 (5)

- Line 131: `if (!ext || !policy || !critic || !cfg_policy || !cfg_critic || !arena) return -1;`
- Line 137: `if (ext->n_policy_params <= 0 || ext->n_critic_params <= 0) return -1;`
- Line 141: `if (bear_arena_create(&ext->flat_arena, flat_sz) != 0) return -1;`
- Line 149: `!ext->flat_w_critic || !ext->flat_g_critic) return -1;`
- Line 159: `if (!ext->gaad_policy || !ext->gaad_critic) return -1;`

### src/bear/bear_cartpole_gaad_solve.c — 6 gaps

#### return_minus1 (2)

- Line 215: `if (rc != 0) return -1;`
- Line 614: `if (!eval_env) return -1;`

#### return_0_stub (3)

- Line 271: `return 0;`
- Line 695: `return 0;`
- Line 741: `return 0;`

#### stub_keyword (1)

- Line 575: `// Q-controller stub`

### src/bear/bear_opt.c — 4 gaps

#### return_minus1 (2)

- Line 53: `if (!opt || !param) return -1;`
- Line 54: `if (opt->num_params >= 64) return -1;`

#### return_0_stub (1)

- Line 88: `return 0;`

#### stub_keyword (1)

- Line 288: `/* Zero grads of registered params - stub */`

### src/bear/bear_arena.c — 3 gaps

#### return_minus1 (2)

- Line 19: `if (bear_arena_create(&g_bear_global_arena, global_cap) != 0) return -1;`
- Line 22: `return -1;`

#### return_0_stub (1)

- Line 24: `return 0;`

### src/bear/bear_env.c — 3 gaps

#### return_minus1 (2)

- Line 410: `if (!reset || !step) return -1;`
- Line 947: `if (!e || e->type != BEAR_ENV_N_POLE_CART) return -1;`

#### return_0_stub (1)

- Line 413: `return 0;`

### src/bear/bear_cartpole_physics.c — 2 gaps

#### return_0_stub (2)

- Line 178: `for (int i = 1; i <= s->n; ++i) if (fabs(s->q[i]) > 0.15) return 0;`
- Line 254: `return 0;`

### src/bear/bear_cartpole_train.c — 2 gaps

#### return_0_stub (2)

- Line 73: `return 0;`
- Line 295: `return 0;`

### src/bear/bear_cartpole_proper.c — 2 gaps

#### return_0_stub (2)

- Line 331: `for (int i = 1; i <= s->n; ++i) if (fabs(s->q[i]) > thresh) return 0;`
- Line 434: `return 0;`

### src/bear/bear_eval.c — 2 gaps

#### return_0_stub (2)

- Line 127: `return 0;`
- Line 208: `return 0;`

### src/bear/bear_gaad.c — 1 gaps

#### return_0_stub (1)

- Line 717: `if (!opt || !opt->config.use_resonant) return 0;`

---

## HOSTED (METAL, VULKAN, DISPLAY, DRM, GBM, X11) (163 GAPS)

### src/hosted/wubu_vulkan.c — 52 gaps

#### return_0_stub (14)

- Line 27: `if (g_vulkan_lib) return 0;`
- Line 45: `return 0;`
- Line 90: `return 0;`
- Line 195: `return 0;`
- Line 229: `return 0;`
- Line 235: `if (families[i] == fam) return 0;`
- Line 308: `return 0;`
- Line 455: `return 0;`
- Line 555: `return 0;`
- Line 789: `return 0;`
- ... and 4 more

#### return_minus1 (37)

- Line 30: `if (!g_vulkan_lib) return -1;`
- Line 36: `return -1;`
- Line 43: `return -1;`
- Line 57: `if (load_vulkan_library() != 0) return -1;`
- Line 60: `if (!vkCreateInstance) return -1;`
- Line 81: `if (r != VK_SUCCESS) return -1;`
- Line 131: `if (r != VK_SUCCESS || count == 0) return -1;`
- Line 211: `if (r != VK_SUCCESS) return -1;`
- Line 215: `if (r != VK_SUCCESS || format_count == 0) return -1;`
- Line 223: `if (r != VK_SUCCESS || mode_count == 0) return -1;`
- ... and 27 more

#### stub_keyword (1)

- Line 154: `phys->instance = inst->instance;  /* Store for later vkGetInstanceProcAddr calls */`

### src/hosted/wubu_metal.c — 35 gaps

#### return_minus1 (18)

- Line 131: `if (!res) return -1;`
- Line 161: `return -1;`
- Line 172: `return -1;`
- Line 181: `return -1;`
- Line 188: `return -1;`
- Line 199: `return -1;`
- Line 209: `return -1;`
- Line 215: `return -1;`
- Line 226: `return -1;`
- Line 245: `return -1;`
- ... and 8 more

#### return_0_stub (16)

- Line 150: `return 0;`
- Line 202: `return 0;`
- Line 278: `return 0;`
- Line 310: `if (fd < 0) return 0;`
- Line 514: `return 0;`
- Line 578: `return 0;`
- Line 634: `return 0;`
- Line 674: `return 0;`
- Line 680: `return 0;`
- Line 744: `return 0;`
- ... and 6 more

#### stub_keyword (1)

- Line 1042: `/* Simple stub - in future use GAAD translate_init + translate_pixel for nearest mode */`

### src/hosted/hosted.c — 23 gaps

#### void_cast (1)

- Line 111: `static void wayland_frame_render(void);`

#### return_minus1 (11)

- Line 502: `if (g_nfiles >= STYXFS_MAX_FILES) return -1;`
- Line 512: `if (g_nfiles >= STYXFS_MAX_FILES) return -1;`
- Line 545: `if (!f) return -1;`
- Line 559: `if (!f) return -1;`
- Line 564: `if (!nf) return -1;`
- Line 583: `if (!nf) return -1;`
- Line 596: `if (!f) return -1;`
- Line 605: `if (!f) return -1;`
- Line 616: `return -1;`
- Line 621: `if (!f) return -1;`
- ... and 1 more

#### return_0_stub (11)

- Line 508: `return 0;`
- Line 523: `return 0;`
- Line 552: `return 0;`
- Line 567: `return 0;`
- Line 590: `return 0;`
- Line 598: `return 0;`
- Line 613: `return 0;`
- Line 635: `return 0;`
- Line 818: `return 0;`
- Line 879: `return 0;`
- ... and 1 more

### src/hosted/hosted_test.c — 18 gaps

#### return_minus1 (10)

- Line 64: `if (g_nfiles >= STYXFS_MAX_FILES) return -1;`
- Line 74: `if (g_nfiles >= STYXFS_MAX_FILES) return -1;`
- Line 108: `if (!f) return -1;`
- Line 122: `if (!f) return -1;`
- Line 127: `if (!nf) return -1;`
- Line 146: `if (!nf) return -1;`
- Line 159: `if (!f) return -1;`
- Line 168: `if (!f) return -1;`
- Line 179: `return -1;`
- Line 184: `if (!f) return -1;`

#### return_0_stub (8)

- Line 70: `return 0;`
- Line 85: `return 0;`
- Line 115: `return 0;`
- Line 130: `return 0;`
- Line 153: `return 0;`
- Line 161: `return 0;`
- Line 176: `return 0;`
- Line 198: `return 0;`

### src/hosted/wubu_drm_direct.c — 15 gaps

#### return_minus1 (13)

- Line 375: `return -1;`
- Line 379: `return -1;`
- Line 384: `if (!conn_ids) return -1;`
- Line 389: `return -1;`
- Line 487: `if (!d) return -1;`
- Line 494: `return -1;`
- Line 503: `return -1;`
- Line 514: `return -1;`
- Line 524: `return -1;`
- Line 536: `return -1;`
- ... and 3 more

#### return_0_stub (2)

- Line 630: `return 0;`
- Line 647: `if (!d) return 0;`

### src/hosted/wubu_display.c — 10 gaps

#### return_0_stub (4)

- Line 35: `return 0;`
- Line 49: `return 0;`
- Line 143: `return 0;`
- Line 235: `return 0;`

#### return_minus1 (6)

- Line 62: `return -1;`
- Line 70: `return -1;`
- Line 78: `return -1;`
- Line 139: `return -1;`
- Line 185: `return -1;`
- Line 203: `return -1;`

### src/hosted/wubu_gbm.c — 5 gaps

#### return_0_stub (5)

- Line 44: `return 0;`
- Line 50: `return 0;`
- Line 56: `return 0;`
- Line 68: `return 0;`
- Line 75: `return 0;`

### src/hosted/wubu_metal_test.c — 3 gaps

#### stub_keyword (1)

- Line 10: `/* Stub: wubu_shell_run is not testable in isolation (requires full GUI stack) */`

#### return_0_stub (2)

- Line 13: `return 0;`
- Line 107: `return 0;`

### src/hosted/wubu_display_test.c — 2 gaps

#### return_0_stub (2)

- Line 22: `return 0;`
- Line 63: `return 0;`

---

## COMPILER (HOLYC LEXER, PARSER, CODEGEN, PTX) (37 GAPS)

### src/compiler/holyc_codegen.c — 22 gaps

#### stub_keyword (13)

- Line 217: `/* Emit Jcc (conditional jump) with placeholder rel32.`
- Line 219: `* Returns the position of the 4-byte rel32 for later patching.`
- Line 225: `emit_dword(gen, 0);  /* placeholder */`
- Line 229: `/* Emit unconditional JMP with placeholder rel32.`
- Line 235: `emit_dword(gen, 0);  /* placeholder */`
- Line 477: `*   jz false_label          (5 bytes, placeholder)`
- Line 481: `*   jmp end_label            (5 bytes, placeholder)`
- Line 505: `*   jnz true_label           (5 bytes, placeholder)`
- Line 509: `*   jmp end_label             (5 bytes, placeholder)`
- Line 882: `*   jz else_label             (5 bytes, placeholder)`
- ... and 3 more

#### return_0_stub (7)

- Line 264: `if (!node) return 0;`
- Line 849: `return 0;`
- Line 855: `if (!node) return 0;`
- Line 1188: `return 0;`
- Line 1260: `if (lex.has_error) return 0;`
- Line 1313: `return 0;`
- Line 1337: `return 0;`

#### return_minus1 (2)

- Line 1194: `if (!node) return -1;`
- Line 1201: `if (!func || func->kind != HC_AST_FUNC_DECL) return -1;`

### src/compiler/holyc_ptx.c — 8 gaps

#### return_minus1 (7)

- Line 133: `if (!ptx_code || !args || num_args <= 0) return -1;`
- Line 139: `if (cuInit(0) != CUDA_SUCCESS) return -1;`
- Line 145: `if (cuDeviceGet(&device, 0) != CUDA_SUCCESS) return -1;`
- Line 146: `if (cuCtxCreate(&context, 0, device) != CUDA_SUCCESS) return -1;`
- Line 152: `return -1;`
- Line 160: `return -1;`
- Line 184: `return -1;`

#### return_0_stub (1)

- Line 192: `return 0;`

### src/compiler/test_holyc_ptx.c — 4 gaps

#### stub_keyword (4)

- Line 90: `TEST(result == 0, "hc_builtin_gpu_matmul returns 0 (stub)");`
- Line 92: `/* Test 9: PTX Exec Stub */`
- Line 93: `printf("\n[Test 9: PTX Exec Stub]\n");`
- Line 95: `TEST(exec_result == -1, "hc_exec_ptx returns -1 (stub)");`

### src/compiler/holyc_lexer.c — 2 gaps

#### return_0_stub (1)

- Line 518: `return 0;`

#### return_minus1 (1)

- Line 521: `return -1;`

### src/compiler/holyc_parse.c — 1 gaps

#### return_0_stub (1)

- Line 155: `case HC_TYPE_VOID: return 0;`

---

## APPS (EDITOR, CANVAS, CODEC, FREEDOOM, EXPLORER, TERMINAL, CALC, CONTROL) (88 GAPS)

### src/apps/wubu_editor.c — 27 gaps

#### return_minus1 (21)

- Line 79: `if (!ed || !filename || ed->n_tabs >= WUBU_ED_MAX_TABS) return -1;`
- Line 81: `if (!f) return -1;`
- Line 129: `if (!ed || ed->active_tab < 0) return -1;`
- Line 131: `if (!tab->filename[0]) return -1;`
- Line 133: `if (!f) return -1;`
- Line 144: `if (!ed || ed->active_tab < 0 || !filename) return -1;`
- Line 176: `if (!ed || tab_idx < 0 || tab_idx >= ed->n_tabs) return -1;`
- Line 553: `if (!ed || !ed->find.find_text[0]) return -1;`
- Line 578: `return -1;`
- Line 583: `if (!ed || !ed->find.find_text[0]) return -1;`
- ... and 11 more

#### return_0_stub (6)

- Line 123: `return 0;`
- Line 140: `return 0;`
- Line 183: `return 0;`
- Line 642: `if (!ed || !ed->find.find_text[0]) return 0;`
- Line 839: `return 0;`
- Line 893: `return 0;`

### src/apps/wubu_codec.c — 24 gaps

#### return_minus1 (20)

- Line 59: `if (!path || !info) return -1;`
- Line 63: `if (!wubu_codec_available()) return -1;`
- Line 72: `if (!f) return -1;`
- Line 120: `if (!dec || !frame || dec->pipe_fd < 0) return -1;`
- Line 123: `if (frame_size == 0) return -1;`
- Line 126: `if (!frame->data) return -1;`
- Line 130: `if (n < 0) return -1;`
- Line 137: `(void)dec; (void)frame; return -1;`
- Line 141: `(void)dec; (void)timestamp; return -1;`
- Line 176: `(void)enc; (void)frame; return -1;`
- ... and 10 more

#### return_0_stub (4)

- Line 95: `return 0;`
- Line 202: `return 0;`
- Line 213: `return 0;`
- Line 333: `return 0;`

### src/apps/wubu_canvas.c — 23 gaps

#### return_minus1 (17)

- Line 88: `if (!cv || cv->n_layers >= WUBU_CV_MAX_LAYERS) return -1;`
- Line 98: `if (!l->pixels) return -1;`
- Line 110: `if (idx < 0) return -1;`
- Line 446: `if (!cv || !plugin || cv->n_plugins >= WUBU_CV_MAX_PLUGINS) return -1;`
- Line 454: `if (!cv || plugin_idx < 0 || plugin_idx >= cv->n_plugins) return -1;`
- Line 456: `if (!p->active) return -1;`
- Line 474: `if (!cv) return -1;`
- Line 476: `if (!flat) return -1;`
- Line 513: `if (!cv) return -1;`
- Line 515: `if (!flat) return -1;`
- ... and 7 more

#### return_0_stub (6)

- Line 291: `if (!cv || cv->active_layer < 0) return 0;`
- Line 293: `if (!l->pixels || x < 0 || x >= l->w || y < 0 || y >= l->h) return 0;`
- Line 459: `return 0;`
- Line 509: `return 0;`
- Line 525: `return 0;`
- Line 582: `return 0;`

### src/apps/wubu_freedoom.c — 10 gaps

#### return_minus1 (8)

- Line 74: `if (!doom) return -1;`
- Line 82: `return -1;`
- Line 118: `if (!doom) return -1;`
- Line 119: `if (doom->state == WUBU_DOOM_RUNNING) return -1;`
- Line 124: `return -1;`
- Line 133: `return -1;`
- Line 215: `return -1;`
- Line 225: `if (!doom || !doom->container) return -1;`

#### return_0_stub (2)

- Line 93: `return 0;`
- Line 219: `return 0;`

### src/apps/doom.c — 1 gaps

#### return_0_stub (1)

- Line 137: `if (x <= 0) return 0;`

### src/apps/explorer.c — 1 gaps

#### return_0_stub (1)

- Line 177: `if (is_dir) return 0;`

### src/apps/paint.c — 1 gaps

#### return_0_stub (1)

- Line 113: `if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return 0;`

### src/apps/dosgui_apps.c — 1 gaps

#### stub_keyword (1)

- Line 609: `/* HolyC Terminal draw stub - handled by dosgui_wm's holyc_term_draw */`

---

## AUDIO (FURNACE 12 CHIPS, SF2, ARDOUR DAW, AI PLUGINS) (26 GAPS)

### src/audio/wubu_audio.c — 25 gaps

#### return_0_stub (7)

- Line 964: `return 0;`
- Line 1303: `return 0;`
- Line 1621: `if (g_init) return 0;`
- Line 1651: `return 0;`
- Line 1672: `return 0;`
- Line 1828: `if (!p->active) return 0;`
- Line 1852: `return 0;`

#### return_minus1 (17)

- Line 1035: `if (pattern >= g_engine.furnace.n_patterns) return -1;`
- Line 1266: `if (size < 12) return -1;`
- Line 1267: `if (memcmp(data, "RIFF", 4) != 0) return -1;`
- Line 1268: `if (memcmp(data + 8, "sfbk", 4) != 0) return -1;`
- Line 1272: `if (!sf2->sf2_data) return -1;`
- Line 1314: `if (!f) return -1;`
- Line 1416: `return -1;`
- Line 1670: `if (!g_init) return -1;`
- Line 1707: `if (g_engine.n_midi_fds >= 4) return -1;`
- Line 1710: `if (fd < 0) return -1;`
- ... and 7 more

#### stub_keyword (1)

- Line 1338: `/* Simple sine wave for SF2 sample playback (placeholder) */`

### src/audio/wubu_audio_test.c — 1 gaps

#### return_0_stub (1)

- Line 292: `return 0;`

---

## BRIDGE (SYSCALL, DOS FLIP) (37 GAPS)

### src/bridge/wubu_syscall.c — 31 gaps

#### return_0_stub (14)

- Line 33: `return 0;`
- Line 39: `return 0;`
- Line 46: `return 0;`
- Line 52: `return 0;`
- Line 58: `return 0;`
- Line 64: `return 0;`
- Line 70: `if (!s) return 0;`
- Line 77: `return 0;`
- Line 94: `return 0;`
- Line 104: `return 0;`
- ... and 4 more

#### return_minus1 (17)

- Line 96: `return -1;`
- Line 106: `return -1;`
- Line 136: `if (!f) return -1;`
- Line 144: `if (!f) return -1;`
- Line 152: `if (!f) return -1;`
- Line 161: `if (!path) return -1;`
- Line 174: `if (fd < 0) return -1;`
- Line 182: `if (fd < 0 || !buf || count <= 0) return -1;`
- Line 190: `if (n < 0) return -1;`
- Line 198: `if (fd < 0 || !buf || count <= 0) return -1;`
- ... and 7 more

### src/bridge/bridge.c — 3 gaps

#### return_0_stub (3)

- Line 19: `return 0;`
- Line 53: `if (g_msg_head == g_msg_tail) return 0;`
- Line 60: `return 0;`

### src/bridge/vbe_ws_bridge.c — 2 gaps

#### return_minus1 (1)

- Line 126: `if (!vbe || !vbe->back || !sim) return -1;`

#### return_0_stub (1)

- Line 153: `return 0;`

### src/bridge/vbe_ws_bridge_test.c — 1 gaps

#### return_0_stub (1)

- Line 46: `return 0;`

---

## TOOLS (ISO9660, SCREENSHOT, WEIGHT_CHECK, DEMO_RECORD) (61 GAPS)

### src/tools/screenshot.c — 24 gaps

#### return_minus1 (18)

- Line 23: `if (!f) return -1;`
- Line 72: `if (!f) return -1;`
- Line 135: `return -1;`
- Line 145: `if (!vbe || !vbe->fb) return -1;`
- Line 148: `if (!copy) return -1;`
- Line 164: `if (!vbe || !vbe->fb) return -1;`
- Line 171: `if (w <= 0 || h <= 0) return -1;`
- Line 174: `if (!copy) return -1;`
- Line 203: `if (!win) return -1;`
- Line 216: `if (g_gif.active) return -1;`
- ... and 8 more

#### return_0_stub (6)

- Line 35: `return 0;`
- Line 110: `return 0;`
- Line 231: `return 0;`
- Line 264: `return 0;`
- Line 272: `return 0;`
- Line 286: `return 0;`

### src/tools/iso9660.c — 19 gaps

#### return_0_stub (14)

- Line 96: `return 0;`
- Line 123: `return 0;`
- Line 143: `return 0;`
- Line 157: `return 0;`
- Line 163: `if (!b) return 0;`
- Line 213: `if (!b->image) return 0;`
- Line 415: `if (!b || !b->image || b->image_size < 17 * ISO_SECTOR_SIZE) return 0;`
- Line 419: `if (pvd->type != ISO_VD_PRIMARY) return 0;`
- Line 420: `if (memcmp(pvd->id, "CD001", 5) != 0) return 0;`
- Line 421: `if (pvd->version != 1) return 0;`
- ... and 4 more

#### return_minus1 (5)

- Line 113: `if (!b || !boot_data || boot_size == 0) return -1;`
- Line 130: `if (!b || !name || b->num_files >= ISO_MAX_FILES) return -1;`
- Line 147: `if (!b || !name || b->num_files >= ISO_MAX_FILES) return -1;`
- Line 393: `if (!b || !b->image || !path) return -1;`
- Line 396: `if (!f) return -1;`

### src/tools/weight_check.c — 7 gaps

#### return_minus1 (2)

- Line 15: `if (index < 0 || index >= WEIGHT_SHARDS || !buf || bufsz <= 0) return -1;`
- Line 39: `if (!result) return -1;`

#### return_0_stub (5)

- Line 17: `return 0;`
- Line 21: `if (!path) return 0;`
- Line 24: `if (!f) return 0;`
- Line 31: `if (size < 0) return 0;`
- Line 34: `if (fsize < min_size) return 0;`

### src/tools/wubu_demo_record.c — 5 gaps

#### void_cast (4)

- Line 43: `extern int  tasking_init(void);`
- Line 44: `extern int  interrupt_init(void);`
- Line 45: `extern void isr_install(void);`
- Line 46: `extern int  input_init(void);`

#### return_0_stub (1)

- Line 183: `return 0;`

### src/tools/screenshot_test.c — 3 gaps

#### void_cast (2)

- Line 12: `extern WubuSnipTool *wubu_snip_tool_state(void);`
- Line 13: `extern WubuGifRecorder *wubu_gif_recorder_state(void);`

#### return_0_stub (1)

- Line 148: `return 0;`

### src/tools/wubu_demo_screenshot.c — 1 gaps

#### return_0_stub (1)

- Line 137: `return 0;`

### src/tools/dosgui_screenshot.c — 1 gaps

#### return_0_stub (1)

- Line 168: `return 0;`

### src/tools/wubu_x11_recorder.c — 1 gaps

#### return_0_stub (1)

- Line 170: `return 0;`

---

## SHELL (UNIFIED SHELL) (21 GAPS)

### src/shell/wubu_shell.c — 21 gaps

#### return_minus1 (11)

- Line 56: `if (g_nfiles >= STYXFS_MAX_FILES) return -1;`
- Line 66: `if (g_nfiles >= STYXFS_MAX_FILES) return -1;`
- Line 100: `if (!f) return -1;`
- Line 114: `if (!f) return -1;`
- Line 119: `if (!nf) return -1;`
- Line 138: `if (!nf) return -1;`
- Line 151: `if (!f) return -1;`
- Line 160: `if (!f) return -1;`
- Line 171: `return -1;`
- Line 176: `if (!f) return -1;`
- ... and 1 more

#### return_0_stub (9)

- Line 62: `return 0;`
- Line 77: `return 0;`
- Line 107: `return 0;`
- Line 122: `return 0;`
- Line 145: `return 0;`
- Line 153: `return 0;`
- Line 168: `return 0;`
- Line 190: `return 0;`
- Line 376: `return 0;`

#### void_cast (1)

- Line 249: `extern int  taskbar_height(void);`

---

## OTHER (JIT ENCODER/DISASM/MINIC) (63 GAPS)

### src/jit/wubu_x86.c — 33 gaps

#### return_0_stub (30)

- Line 156: `return 0;`
- Line 166: `return 0;`
- Line 172: `return 0;`
- Line 197: `return 0;`
- Line 221: `return 0;`
- Line 228: `return 0;`
- Line 237: `return 0;`
- Line 242: `return 0;`
- Line 251: `return 0;`
- Line 261: `return 0;`
- ... and 20 more

#### stub_keyword (3)

- Line 374: `wx86_emit_dword(e, 0);  /* placeholder — patch later */`
- Line 381: `wx86_emit_dword(e, 0);  /* placeholder — patch later */`
- Line 387: `wx86_emit_dword(e, 0);  /* placeholder — patch later */`

### src/jit/jit_minic.c — 7 gaps

#### return_minus1 (3)

- Line 150: `return -1;`
- Line 558: `return -1;`
- Line 564: `return -1;`

#### stub_keyword (2)

- Line 286: `/* Placeholder call: mov rax, 0; call rax */`
- Line 604: `/* Push args to stack — saves them for later reference.`

#### return_0_stub (2)

- Line 597: `return 0;`
- Line 689: `return 0;`

### src/jit/jit.c — 4 gaps

#### stub_keyword (1)

- Line 616: `/* call label — emit rel32 placeholder, add ref */`

#### return_0_stub (3)

- Line 718: `return 0;`
- Line 725: `return 0;`
- Line 732: `return 0;`

### src/jit/wubu_disasm.c — 3 gaps

#### return_0_stub (3)

- Line 62: `if (offset >= code_len) return 0;`
- Line 79: `if (pos >= code_len) return 0;`
- Line 215: `if (remain < 2) return 0;`

### src/cartpole/npole_blog.c — 3 gaps

#### stub_keyword (2)

- Line 273: `/* Placeholder - will be replaced by validated implementation */`
- Line 290: `printf("N-pole stub - needs full Lagrangian implementation\n");`

#### return_0_stub (1)

- Line 291: `return 0;`

### src/cartpole/cartpole_blog.c — 2 gaps

#### return_0_stub (2)

- Line 141: `return 0;`
- Line 262: `return 0;`

### src/worldsim/render.c — 1 gaps

#### stub_keyword (1)

- Line 82: `/* No pixels  --  draw rect placeholder */`

### src/cartpole/victory_grid.c — 1 gaps

#### return_0_stub (1)

- Line 141: `return 0;`

### src/cartpole/test_chain_horizontal.c — 1 gaps

#### return_0_stub (1)

- Line 26: `return 0;`

### src/cartpole/npole_physics.c — 1 gaps

#### return_0_stub (1)

- Line 332: `return 0;`

### src/cartpole/test_chain_stable.c — 1 gaps

#### return_0_stub (1)

- Line 53: `return 0;`

### src/cartpole/test_npole_2to20.c — 1 gaps

#### return_0_stub (1)

- Line 135: `return 0;`

### src/cartpole/test_npole_physics.c — 1 gaps

#### return_0_stub (1)

- Line 134: `return 0;`

### src/cartpole/npole_openocl.c — 1 gaps

#### return_0_stub (1)

- Line 311: `return 0;`

### src/cartpole/test_victorious_7to20.c — 1 gaps

#### return_0_stub (1)

- Line 106: `return 0;`

### src/cartpole/test_chain_equilibria.c — 1 gaps

#### return_0_stub (1)

- Line 56: `return 0;`

### src/cartpole/test_rk4_chain.c — 1 gaps

#### return_0_stub (1)

- Line 45: `return 0;`

---
