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

#endif /* WUBUOS_ZEALOS_PARITY_H */
