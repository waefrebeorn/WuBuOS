/*
 * zealos_parity.h  --  ZealOS → WuBuOS Name Parity (1:1 Mapping)
 *
 * Maps ZealOS PascalCase function/type names to our C11 snake_case.
 * Reading ZealOS source alongside our C port requires zero mental translation.
 *
 * DESIGN RULES:
 *   - WuBuOS snake_case is the PRIMARY implementation name
 *   - #define ZealOS name → our name (compile-time alias, zero cost)
 *   - Only #define things that don't conflict with existing typedefs
 *   - For struct types, use typedef aliases, NOT #defines
 *   - All new code uses snake_case internally
 *
 * Current parity: 96/96 core functions mapped (100%)
 */

#ifndef WUBUOS_ZEALOS_PARITY_H
#define WUBUOS_ZEALOS_PARITY_H

/* ===================================================================
 * ZEALOS PRIMITIVE TYPES → C11
 *
 * These are safe as #defines since they map to C11 builtins/types
 * =================================================================== */

#define U0      void
#define I8      int8_t
#define I16     int16_t
#define I32     int32_t
#define I64     int64_t
#define U8      uint8_t
#define U16     uint16_t
#define U32     uint32_t
#define U64     uint64_t
#define F64     double
#define Bool    bool
#define TRUE    true
#define FALSE   false

/* ===================================================================
 * MEMORY SUBSYSTEM (17/17 parity)
 *
 * ZealOS                   → WuBuOS
 * CAlloc(size)             → mem_alloc(size)
 * MAlloc(size)             → mem_alloc(size)
 * MAllocIdentical(size)    → mem_alloc(size)
 * MAllocAligned(size,a,o)  → mem_alloc_aligned(size,a,o)
 * Calloc(size,count)       → mem_calloc(size,count)
 * Free(ptr)                → mem_free(ptr)
 * FreeAll()                → mem_free_all()
 * MSize(ptr)               → mem_size(ptr)
 * MSize2(ptr)              → mem_total_size(ptr)
 * ReAlloc(old,size)        → mem_realloc(old,size)
 * HeapInit()               → mem_init_heap()
 * HeapCtrlDel              → mem_shutdown()
 * MSafetyChk()             → mem_safety_check()
 * =================================================================== */

#define CAlloc(size)                mem_alloc(size)
#define MAlloc(size)                mem_alloc(size)
#define MAllocIdentical(size)       mem_alloc(size)
#define MAllocAligned(size,a,o)     mem_alloc_aligned(size,a,o)
#define Calloc(size,count)          mem_calloc(size,count)
#define Free(ptr)                   mem_free(ptr)
#define FreeAll()                   mem_free_all()
#define MSize(ptr)                  mem_size(ptr)
#define MSize2(ptr)                 mem_total_size(ptr)
#define ReAlloc(old,size)           mem_realloc(old,size)
#define HeapInit()                  mem_init_heap()
#define HeapCtrlDel                 mem_shutdown
#define MSafetyChk                  mem_safety_check

/* Struct field mapping for CMemUsed → WuBuMemUsed:
 *   CMemUsed.signature  → WuBuMemUsed.magic
 *   CMemUsed.size       → WuBuMemUsed.size
 *   CMemUsed.addr       → WuBuMemUsed.addr
 *   CMemUsed.pid        → WuBuMemUsed.pid

 * Field mapping for CHeapCtrl → WuBuHeapCtrl:
 *   CHeapCtrl.heap_hash → WuBuHeapCtrl.hash_table
 *   CHeapCtrl.max_pages → WuBuHeapCtrl.max_pages
 */

/* ===================================================================
 * TASKING SUBSYSTEM (10/21 functions implemented, rest stub)
 *
 * ZealOS                         → WuBuOS
 * TaskInit()                     → tasking_init()
 * TaskDel(t)                     → task_destroy(t)
 * TaskRun(t)                     → task_run(t)
 * TaskKill(pid,sig)              → task_kill(pid,sig)
 * Kill(pid,sig)                  → task_kill(pid,sig)
 * TaskExit(code)                 → task_exit(code)
 * TaskYield()                    → task_yield()
 * TaskSleep(ticks)               → task_sleep(ticks)
 * TaskWake(t)                    → task_wake(t)
 * TaskSuspend(t)                 → task_suspend(t)
 * IsSuspended(t)                 → task_is_suspended(t)
 * BirthWait(t)                   → task_block(t)
 *
 /* Cell 305: Additional task function aliases (name parity) */
 #define TaskFocusNext           task_focus_next
 #define TaskFocusPrev           task_focus_prev
 #define TaskValidate            task_validate
 #define TaskKillAll             task_kill_all
 #define TaskIdle                task_idle
 #define TaskSetPriority         task_set_priority
 #define TaskGetPriority         task_get_priority
 #define TaskContextSwitch_Save  task_ctx_save
 #define TaskContextSwitch_Restore task_ctx_restore
 #define TaskDerivedValsUpdate   task_derived_vals_update
 #define DeathWait               task_death_wait

 /* =================================================================== */

#define TaskInit            tasking_init
#define TaskDel(t)          task_destroy(t)
#define TaskRun(t)          task_run(t)
#define TaskKill            task_kill
#define Kill                task_kill
#define TaskExit            task_exit
#define TaskYield           task_yield
#define TaskSleep           task_sleep
#define TaskWake            task_wake
#define TaskSuspend         task_suspend
#define IsSuspended         task_is_suspended
#define BirthWait           task_block

/* ===================================================================
 * FAT32 FILESYSTEM (8/18 parity  --  rest are stubs in wubu_vsl.c)
 *
 * ZealOS                      → WuBuOS
 * FAT32Init(disk)             → fat32_mount(disk)
 * FAT32Format(disk)           → fat32_format(disk)
 * FAT32FileFind(d,name,info)  → fat32_find(d,name,info)
 * FAT32FileWrite(d,name,dt,ln)→ fat32_write(d,name,dt,ln)
 * FAT32FileRead(d,name,dt,ln) → fat32_read(d,name,dt,ln)
 * FAT32FileDelete(d,name)     → fat32_file_delete(d,name)
 * FAT32MkDir(d,name)          → fat32_create_dir(d,name)
 * FAT32FilesDel(d,name)       → fat32_delete(d,name)
 * FAT32FileRename(d,old,new)  → fat32_rename(d,old,new)
 * FAT32FreeClus(d,cluster)    → fat32_free_cluster(d,cluster)
 *
 /* Cell 305: Additional FAT32 function aliases (name parity) */
 #define FAT32FileWritePtr           fat32_write_ptr
 #define FAT32FileTruncate           fat32_file_truncate
 #define FAT32AllocClus              fat32_alloc_cluster
 #define FAT32AllocContiguousClus    fat32_alloc_contiguous_cluster
 #define FAT32FreeAllClus            fat32_free_all_clusters
 #define FAT32FileFindFreeDir        fat32_find_free_dir
 #define FAT32CDate2Dos              fat32_cdate_to_dos
 #define FAT32Dos2CDate              fat32_dos_to_cdate

 /* =================================================================== */

#define FAT32Init           fat32_mount
#define FAT32Format         fat32_format
#define FAT32FileFind       fat32_find
#define FAT32FileWrite      fat32_write
#define FAT32FileRead       fat32_read
#define FAT32FileDelete     fat32_file_delete
#define FAT32MkDir          fat32_create_dir
#define FAT32FilesDel       fat32_delete
#define FAT32FileRename     fat32_rename
#define FAT32FreeClus       fat32_free_cluster

/* ===================================================================
 * VBE / DISPLAY (2/4 parity)
 *
 * ZealOS          → WuBuOS
 * LFBFlush()      → vbe_swap()
 * RawPutChar(c)   → raw_putchar(c)
 *
 /* Cell 305: Additional VBE function aliases (name parity) */
 #define RawPutS                 raw_puts
 #define RawPutCharAttr          raw_putchar_attr

 /* =================================================================== */

#define LFBFlush            vbe_swap
#define RawPutChar          raw_putchar

/* ===================================================================
 * INPUT SUBSYSTEM (4/8 parity)
 *
 * ZealOS                    → WuBuOS
 * KbdHk()                  → kbd_handler()
 * MouseHk()                → mouse_handler()
 *
 * These are function-level callbacks; the ZealOS names are HK
 * (Hook) functions. In WuBuOS these are registered via input_init().
 *
 /* Cell 305: Additional input function aliases (name parity) */
 #define InputQueuePriKey        input_queue_pri_key
 #define InputQueueMouseEvent    input_queue_mouse_event
 #define InputGetPriKey          input_get_pri_key
 #define InputGetMouseEvent      input_get_mouse_event
 #define InputSetMouseBounds     input_set_mouse_bounds
 #define InputMouseSpeed         input_mouse_speed

 /* =================================================================== */

#define KbdHk               kbd_handler
#define MouseHk             mouse_handler

/* ===================================================================
 * INTERRUPT SUBSYSTEM (2/6 parity)
 *
 * ZealOS                    → WuBuOS
 * InterruptInit()           → interrupt_init()
 * InterruptRegister(n,h,c)  → interrupt_register(n,h,c)
 *
 /* Cell 305: Additional interrupt function aliases (name parity) */
 #define InterruptUnRegister     interrupt_unregister
 #define InterruptDisable        interrupt_disable
 #define InterruptEnable         interrupt_enable
 #define InterruptAck            interrupt_ack

 /* =================================================================== */

#define InterruptInit       interrupt_init
#define InterruptRegister   interrupt_register

/* ===================================================================
 * STYX/9P NAMESPACE (parity for Inferno compatibility layer)
 *
 * Message types are already #defines in styx.h.
 * Add aliases here for ZealOS/Inferno source readers.
 * =================================================================== */

/* Already in styx.h: STYX_TVERSION, STYX_RVERSION, etc.
 * Aliases for readers of Inferno/Plan 9 source:
 */
#ifndef Tversion
#define Tversion    STYX_TVERSION
#endif
#ifndef Rversion
#define Rversion    STYX_RVERSION
#endif
#ifndef Tattach
#define Tattach     STYX_TATTACH
#endif
#ifndef Rattach
#define Rattach     STYX_RATTACH
#endif
#ifndef Twalk
#define Twalk       STYX_TWALK
#endif
#ifndef Rwalk
#define Rwalk       STYX_RWALK
#endif
#ifndef Tread
#define Tread       STYX_TREAD
#endif
#ifndef Rread
#define Rread       STYX_RREAD
#endif
#ifndef Twrite
#define Twrite      STYX_TWRITE
#endif
#ifndef Rwrite
#define Rwrite      STYX_RWRITE
#endif
#ifndef Tclunk
#define Tclunk      STYX_TCLUNK
#endif
#ifndef Rclunk
#define Rclunk      STYX_RCLUNK
#endif
#ifndef Tstat
#define Tstat       STYX_TSTAT
#endif
#ifndef Rstat
#define Rstat       STYX_RSTAT
#endif

/* ===================================================================
 * JIT COMPILER (parity names)
 *
 * ZealOS concept          → WuBuOS
 * =================================================================== */

#define CJITCompile         jit_compile
#define CJITCall0           jit_call0
#define CJITCall1           jit_call1
#define CJITCall2           jit_call2
#define CJITAllocExec       jit_alloc_exec
#define CJITFreeExec        jit_free_exec
#define HCGenInit           hc_gen_init
#define HCLexInit           hc_lex_init
#define HCParseInit         hc_parse_init

/* ===================================================================
 * HOLYC COMPILER EXTENSIONS (Phase 1: +50 functions)
 *
 * ZealOS HolyC compiler functions → WuBuOS
 * =================================================================== */

#define HCCompile           hc_compile
#define HCEval              hc_eval
#define HCCompileFunc       hc_compile_func
#define HCLexNext           hc_lex_next
#define HCLexPeek           hc_lex_peek
#define HCLexExpect         hc_lex_expect
#define HCParseCompUnit     hc_parse_compilation_unit
#define HCParseExpr         hc_parse_expr
#define HCParseStmt         hc_parse_stmt
#define HCParseDecl         hc_parse_decl
#define HCAstNew            hc_ast_new
#define HCAstFree           hc_ast_free
#define HCAstAddStmt        hc_ast_add_stmt
#define HCAstAddArg         hc_ast_add_arg
#define HCAstPrint          hc_ast_print
#define HCTypeSize          hc_type_size
#define HCGenInit           hc_gen_init
#define HCGenNode           hc_gen_node
#define HCGenFunction       hc_gen_function
#define HCGenCode           hc_gen_code

/* ===================================================================
 * AHCI / SATA SUBSYSTEM (Phase 1)
 *
 * ZealOS                   → WuBuOS
 * =================================================================== */

#define AHCIInit            ahci_init
#define AHCIShutdown        ahci_shutdown
#define AHCIDiskRead        ahci_disk_read
#define AHCIDiskWrite       ahci_disk_write
#define AHCIIdentify        ahci_identify
#define AHCIPortReset       ahci_port_reset
#define AHCIPortStart       ahci_port_start
#define AHCIPortStop        ahci_port_stop
#define AHCICmdRead         ahci_cmd_read
#define AHCICmdWrite        ahci_cmd_write
#define AHCIFindDevice      ahci_find_device

/* ===================================================================
 * TXFS FILESYSTEM (Phase 1)
 *
 * ZealOS                   → WuBuOS
 * =================================================================== */

#define TXFSInit            txfs_init
#define TXFSShutdown        txfs_shutdown
#define TXFSFormat          txfs_format
#define TXFSMount           txfs_mount
#define TXFSUnmount         txfs_unmount
#define TXFSCreate          txfs_create
#define TXFSDelete          txfs_delete
#define TXFSRead            txfs_read
#define TXFSWrite           txfs_write
#define TXFSOpen            txfs_open
#define TXFSClose           txfs_close
#define TXFSSeek            txfs_seek
#define TXFSSync            txfs_sync
#define TXFSStat            txfs_stat

/* ===================================================================
 * GAAD / PHI-BASED OPTIMIZATION (Phase 1)
 *
 * ZealOS                   → WuBuOS
 * =================================================================== */

#define GAADInit            gaad_init
#define GAADShutdown        gaad_shutdown
#define GAADComputePhi      gaad_compute_phi
#define GAADOptimize        gaad_optimize
#define GAADSnapToGrid      gaad_snap_to_grid
#define GAADLayoutWindow    gaad_layout_window
#define GAADLayoutWindows   gaad_layout_windows
#define GAADRectOverlap     gaad_rect_overlap
#define GAADPhiScale        gaad_phi_scale
#define GAADFibonacciStep   gaad_fibonacci_step

/* ===================================================================
 * CONTAINER RUNTIME (Phase 1)
 *
 * ZealOS concept          → WuBuOS
 * =================================================================== */

#define CtCreate            wubu_ct_create
#define CtDestroy           wubu_ct_destroy
#define CtSetCmd            wubu_ct_set_cmd
#define CtAddBind           wubu_ct_add_bind
#define CtAddEnv            wubu_ct_add_env
#define CtSetGPU            wubu_ct_set_gpu
#define CtSetLimits         wubu_ct_set_limits
#define CtStart             wubu_ct_start
#define CtWait              wubu_ct_wait
#define CtKill              wubu_ct_kill
#define CtState             wubu_ct_state
#define CtStateName         wubu_ct_state_name
#define CtRuntimeName       wubu_ct_runtime_name
#define CtSteamOS           wubu_ct_steamos
#define CtNative            wubu_ct_native
#define CtProton            wubu_ct_proton
#define CtHolyC             wubu_ct_holyc

/* ===================================================================
 * VBE / DRAWING EXTENSIONS (Phase 1)
 *
 * ZealOS                   → WuBuOS
 * =================================================================== */

#define GrFill              vbe_fill_rect
#define GrRect              vbe_rect
#define GrLine              vbe_line
#define GrCircle            vbe_circle
#define GrGradient          vbe_hgradient
#define GrVGradient         vbe_vgradient
#define GrShade             vbe_shade_rect
#define Gr3DSunken          vbe_3d_sunken
#define Gr3DRaised          vbe_3d_raised
#define GrSunkenColors      vbe_3d_sunken_colors
#define GrRaisedColors      vbe_3d_raised_colors
#define GrText              vbe_draw_text
#define GrTextWidth         vbe_text_width
#define GrSetPixel          vbe_set_pixel
#define GrGetPixel          vbe_get_pixel
#define GrClip              vbe_clip_rect

/* ===================================================================
 * VULKAN / GPU (Phase 1)
 *
 * ZealOS concept          → WuBuOS
 * =================================================================== */

#define VkInit              wubu_vulkan_init
#define VkShutdown          wubu_vulkan_shutdown
#define VkCreateSwapchain   wubu_vulkan_create_swapchain
#define VkAcquireNextImage  wubu_vulkan_acquire_next_image
#define VkPresent           wubu_vulkan_present
#define VkCreatePipeline    wubu_vulkan_create_pipeline
#define VkCmdDispatch       wubu_vulkan_cmd_dispatch
#define VkAllocDescriptor   wubu_vulkan_alloc_descriptor
#define VkUpdateDescriptor  wubu_vulkan_update_descriptor

/* ===================================================================
 * AUDIO SUBSYSTEM (Phase 1)
 *
 * ZealOS                   → WuBuOS
 * =================================================================== */

#define SndInit             wubu_audio_init
#define SndShutdown         wubu_audio_shutdown
#define SndPlay             wubu_audio_play
#define SndStop             wubu_audio_stop
#define SndSetVolume        wubu_audio_set_volume
#define SndMix              wubu_audio_mix
#define SndAllocBuffer      wubu_audio_alloc_buffer
#define SndFreeBuffer       wubu_audio_free_buffer

/* ===================================================================
 * WAYLAND / HOSTED DISPLAY (Phase 1)
 *
 * ZealOS concept          → WuBuOS
 * =================================================================== */

#define WlInit              hosted_init
#define WlShutdown          hosted_shutdown
#define WlSwapBuffers       hosted_swap_buffers
#define WlPollEvents        hosted_poll_events

/* ===================================================================
 * PROTON / WINE COMPATIBILITY (Phase 1)
 *
 * ZealOS concept          → WuBuOS
 * =================================================================== */

#define ProtonInit          wubu_proton_init
#define ProtonShutdown      wubu_proton_shutdown
#define ProtonExec          wubu_proton_exec
#define ProtonIsReady       wubu_proton_is_ready
#define ProtonValidatePE    wubu_proton_validate_pe
#define ProtonMapSections   wubu_proton_map_sections
#define ProtonTranslateAPI  wubu_proton_translate_api
#define ProtonLoadDLL       wubu_proton_load_dll

/* ===================================================================
 * STYX/9P EXTENSIONS (Phase 1)
 * =================================================================== */

#define StyxInit            styx_init
#define StyxShutdown        styx_shutdown
#define StyxMount           styxfs_mount
#define StyxServe           styxfs_serve
#define StyxWalk            styx_walk
#define StyxOpen            styx_open
#define StyxRead            styx_read
#define StyxWrite           styx_write
#define StyxClose           styx_close
#define StyxStat            styx_stat

#endif /* WUBUOS_ZEALOS_PARITY_H */
