/*
 * zealos_parity.h — ZealOS Name Parity Aliases
 *
 * Maps ZealOS PascalCase function/type names to our C11 snake_case.
 * This provides 1:1 name parity for anyone reading ZealOS source
 * alongside our C port — same mental model, zero translation.
 *
 * Design:
 *   - Keep our C11 snake_case as the PRIMARY implementation name
 *   - #define ZealOS name → our name (compile-time alias, zero runtime cost)
 *   - All new code should use snake_case internally
 *   - Parity aliases are for reading ZealOS source and transitioning
 *
 * Coverage: Memory, Task, FAT32, VBE, Input, Interrupt
 */

#ifndef WUBUOS_ZEALOS_PARITY_H
#define WUBUOS_ZEALOS_PARITY_H

/* ═══ Memory ════════════════════════════════════════════════════
 * ZealOS: CAlloc, MAlloc, Free, FreeAll, HeapCtrlDel
 * Ours:   mem_alloc, mem_alloc, mem_free, —, mem_shutdown
 */
#define CAlloc(size)       mem_alloc(size)
#define MAlloc(size)       mem_alloc(size)
#define Free(ptr)          mem_free(ptr)
#define HeapCtrlDel(hc)    mem_shutdown()

/* ═══ Task ══════════════════════════════════════════════════════
 * ZealOS: TaskInit, TaskDel, BirthWait, DeathWait, Kill, Exit,
 *         Suspend, IsSuspended, TaskFocusNext, TaskValidate
 * Ours:   tasking_init, task_destroy, task_block, —, —, —,
 *         task_sleep, —, —, task_switch_to, —
 */
#define TaskInit           tasking_init
#define TaskDel(t)         task_destroy(t)
#define BirthWait(t)       task_block(t)
#define Kill(pid, sig)     task_destroy(/* lookup by pid */)

/* ═══ FAT32 ════════════════════════════════════════════════════
 * ZealOS: FAT32Init, FAT32Format, FAT32FileFind, FAT32FileWrite,
 *         FAT32MkDir, FAT32FilesDel, FAT32AllocClus, FAT32FreeClus
 * Ours:   fat32_mount, fat32_format, fat32_find, fat32_write,
 *         fat32_create, fat32_delete, —, fat32_free_chain
 */
#define FAT32Init          fat32_mount
#define FAT32Format        fat32_format
#define FAT32FileFind      fat32_find
#define FAT32FileWrite     fat32_write
#define FAT32MkDir         fat32_create
#define FAT32FilesDel      fat32_delete
#define FAT32FreeClus      fat32_free_chain

/* ═══ VBE/Display ═══════════════════════════════════════════════
 * ZealOS: LFBFlush, RawPutChar
 * Ours:   vbe_swap, —
 */
#define LFBFlush           vbe_swap

/* ═══ Types (ZealOS class → C struct) ═══════════════════════════
 * Already parity-matched: CTask, CHeapCtrl, CMemBlk, CMemUsed, CMemUnused
 */
/* WuBuOS-only types (no ZealOS equivalent): */
/* WmWindow, styx_fid_t, styx_server_t, WubuCt, etc. */

/* ═══ ZealOS Primitive Types ════════════════════════════════════
 * ZealOS uses U0/I8/I16/I32/I64/U8/U16/U32/U64/F64/Bool
 * We use C11 standard: void/int8_t/../uint64_t/double/bool
 */
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

#endif /* WUBUOS_ZEALOS_PARITY_H */
