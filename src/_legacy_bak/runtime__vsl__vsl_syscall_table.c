/*
 * vsl_syscall_table.c - VSL Syscall Dispatch Table
 * Auto-generated from vsl_syscall_list.h
 *
 * This file provides the syscall dispatch table and argument counts
 */

#include "vsl/vsl_internal.h"
#include "vsl/vsl_syscall.h"

/* Forward declarations for all syscall handlers */

extern int64_t vsl_sys_accept(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
/* NT transliteration handlers (E1) -- vsl_syscall_nt.c */
extern int64_t vsl_nt_add_atom(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_find_atom(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_clear_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_allocate_uuids(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_allocate_luid(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_alert_thread(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_cancel_io_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_assign_job(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_alloc_user_phys_pages(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_free_user_phys_pages(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_alert_resume_thread(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_are_mapped_files_same(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_create_job_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_open_job_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_terminate_job_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_is_process_in_job(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_delete_atom(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_query_information_atom(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_flush_write_buffer(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_nt_set_uuid_seed(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_clock_gettime(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_clock_settime(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_clone(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_close(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_connect(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_dup(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_epoll_create(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_epoll_ctl(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_epoll_wait(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_eventfd(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_eventfd_write(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_fcntl(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_fork(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_fstat(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_fsync(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_futex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_futex_wait(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_getcpu(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_getdents(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_getxattr(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_inotify_add_watch(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_ioctl(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_kill(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_listen(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_mkdir(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_mlock(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_mmap(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_mprotect(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_mremap(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_msync(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_munlock(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_munmap(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_nanosleep(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_nosys(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_open(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_openat(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_pipe(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_poll(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_proc_info(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_proc_vm_info(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_process_vm_readv(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_process_vm_writev(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_read(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_readlink(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_readv(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_rename(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_sched_yield(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_setxattr(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_socketpair(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_stat(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_statfs(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_symlink(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_sysinfo(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_tgkill(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_timer_delete(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_timerfd_create(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_timerfd_settime(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_unlink(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_write(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);
extern int64_t vsl_sys_writev(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);

/* Syscall dispatch table - indexed by syscall number */
typedef int64_t (*vsl_syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

vsl_syscall_fn_t vsl_syscall_table[] = {
    vsl_sys_accept,  // 1: NtAcceptConnectPort
    vsl_sys_nosys,  // 2: NtAccessCheck
    vsl_sys_nosys,  // 3: NtAccessCheckAndAuditAlarm
    vsl_sys_nosys,  // 4: NtAccessCheckByType
    vsl_sys_nosys,  // 5: NtAccessCheckByTypeAndAuditAlarm
    vsl_sys_nosys,  // 6: NtAccessCheckByTypeResultList
    vsl_sys_nosys,  // 7: NtAccessCheckByTypeResultListAndAuditAlarm
    vsl_sys_nosys,  // 8: NtAccessCheckByTypeResultListAndAuditAlarmByHandle
    vsl_nt_add_atom,  // 9: NtAddAtom
    vsl_sys_nosys,  // 10: NtAddBootEntry
    vsl_sys_nosys,  // 11: NtAddDriverEntry
    vsl_sys_nosys,  // 12: NtAdjustGroupsToken
    vsl_sys_nosys,  // 13: NtAdjustPrivilegesToken
    vsl_nt_alert_resume_thread,  // 14: NtAlertResumeThread
    vsl_nt_alert_thread,  // 15: NtAlertThread
    vsl_nt_allocate_luid,  // 16: NtAllocateLocallyUniqueId
    vsl_nt_alloc_user_phys_pages,  // 17: NtAllocateUserPhysicalPages
    vsl_nt_allocate_uuids,  // 18: NtAllocateUuids
    vsl_sys_mmap,  // 19: NtAllocateVirtualMemory
    vsl_sys_nosys,  // 20: NtApphelpCacheControl
    vsl_nt_are_mapped_files_same,  // 21: NtAreMappedFilesTheSame
    vsl_nt_assign_job,  // 22: NtAssignProcessToJobObject
    vsl_sys_nosys,  // 23: NtCallbackReturn
    vsl_sys_nosys,  // 24: NtCancelDeviceWakeupRequest
    vsl_nt_cancel_io_file,  // 25: NtCancelIoFile
    vsl_sys_timer_delete,  // 26: NtCancelTimer
    vsl_nt_clear_event,  // 27: NtClearEvent
    vsl_sys_close,  // 28: NtClose
    vsl_sys_nosys,  // 29: NtCloseObjectAuditAlarm
    vsl_sys_nosys,  // 30: NtCompactKeys
    vsl_sys_nosys,  // 31: NtCompareTokens
    vsl_sys_nosys,  // 32: NtCompleteConnectPort
    vsl_sys_nosys,  // 33: NtCompressKey
    vsl_sys_connect,  // 34: NtConnectPort
    vsl_sys_nosys,  // 35: NtContinue
    vsl_sys_nosys,  // 36: NtCreateDebugObject
    vsl_sys_mkdir,  // 37: NtCreateDirectoryObject
    vsl_sys_eventfd,  // 38: NtCreateEvent
    vsl_sys_nosys,  // 39: NtCreateEventPair
    vsl_sys_open,  // 40: NtCreateFile
    vsl_sys_epoll_create,  // 41: NtCreateIoCompletion
    vsl_nt_create_job_object,  // 42: NtCreateJobObject
    vsl_sys_nosys,  // 43: NtCreateJobSet
    vsl_sys_openat,  // 44: NtCreateKey
    vsl_sys_nosys,  // 45: NtCreateMailslotFile
    vsl_sys_futex,  // 46: NtCreateMutant
    vsl_sys_pipe,  // 47: NtCreateNamedPipeFile
    vsl_sys_nosys,  // 48: NtCreatePagingFile
    vsl_sys_socketpair,  // 49: NtCreatePort
    vsl_sys_fork,  // 50: NtCreateProcess
    vsl_sys_clone,  // 51: NtCreateProcessEx
    vsl_sys_nosys,  // 52: NtCreateProfile
    vsl_sys_mmap,  // 53: NtCreateSection
    vsl_sys_futex,  // 54: NtCreateSemaphore
    vsl_sys_symlink,  // 55: NtCreateSymbolicLinkObject
    vsl_sys_clone,  // 56: NtCreateThread
    vsl_sys_timerfd_create,  // 57: NtCreateTimer
    vsl_sys_nosys,  // 58: NtCreateToken
    vsl_sys_nosys,  // 59: NtCreateWaitablePort
    vsl_sys_nosys,  // 60: NtDebugActiveProcess
    vsl_sys_nosys,  // 61: NtDebugContinue
    vsl_sys_nanosleep,  // 62: NtDelayExecution
    vsl_nt_delete_atom,  // 63: NtDeleteAtom
    vsl_sys_nosys,  // 64: NtDeleteBootEntry
    vsl_sys_nosys,  // 65: NtDeleteDriverEntry
    vsl_sys_unlink,  // 66: NtDeleteFile
    vsl_sys_unlink,  // 67: NtDeleteKey
    vsl_sys_nosys,  // 68: NtDeleteObjectAuditAlarm
    vsl_sys_nosys,  // 69: NtDeleteValueKey
    vsl_sys_ioctl,  // 70: NtDeviceIoControlFile
    vsl_sys_nosys,  // 71: NtDisplayString
    vsl_sys_dup,  // 72: NtDuplicateObject
    vsl_sys_nosys,  // 73: NtDuplicateToken
    vsl_sys_nosys,  // 74: NtEnumerateBootEntries
    vsl_sys_nosys,  // 75: NtEnumerateDriverEntries
    vsl_sys_getdents,  // 76: NtEnumerateKey
    vsl_sys_nosys,  // 77: NtEnumerateSystemEnvironmentValuesEx
    vsl_sys_nosys,  // 78: NtEnumerateValueKey
    vsl_sys_mremap,  // 79: NtExtendSection
    vsl_sys_nosys,  // 80: NtFilterToken
    vsl_nt_find_atom,  // 81: NtFindAtom
    vsl_sys_fsync,  // 82: NtFlushBuffersFile
    vsl_sys_nosys,  // 83: NtFlushInstructionCache
    vsl_sys_nosys,  // 84: NtFlushKey
    vsl_sys_msync,  // 85: NtFlushVirtualMemory
    vsl_nt_flush_write_buffer,  // 86: NtFlushWriteBuffer
    vsl_nt_free_user_phys_pages,  // 87: NtFreeUserPhysicalPages
    vsl_sys_munmap,  // 88: NtFreeVirtualMemory
    vsl_sys_ioctl,  // 89: NtFsControlFile
    vsl_sys_nosys,  // 90: NtGetContextThread
    vsl_sys_nosys,  // 91: NtGetDevicePowerState
    vsl_sys_nosys,  // 92: NtGetPlugPlayEvent
    vsl_sys_nosys,  // 93: NtGetWriteWatch
    vsl_sys_nosys,  // 94: NtImpersonateAnonymousToken
    vsl_sys_nosys,  // 95: NtImpersonateClientOfPort
    vsl_sys_nosys,  // 96: NtImpersonateThread
    vsl_sys_nosys,  // 97: NtInitializeRegistry
    vsl_sys_nosys,  // 98: NtInitiatePowerAction
    vsl_nt_is_process_in_job,  // 99: NtIsProcessInJob
    vsl_sys_nosys,  // 100: NtIsSystemResumeAutomatic
    vsl_sys_listen,  // 101: NtListenPort
    vsl_sys_nosys,  // 102: NtLoadDriver
    vsl_sys_nosys,  // 103: NtLoadKey
    vsl_sys_nosys,  // 104: NtLoadKey2
    vsl_sys_nosys,  // 105: NtLoadKeyEx
    vsl_sys_fcntl,  // 106: NtLockFile
    vsl_sys_nosys,  // 107: NtLockProductActivationKeys
    vsl_sys_nosys,  // 108: NtLockRegistryKey
    vsl_sys_mlock,  // 109: NtLockVirtualMemory
    vsl_sys_nosys,  // 110: NtMakePermanentObject
    vsl_sys_nosys,  // 111: NtMakeTemporaryObject
    vsl_sys_nosys,  // 112: NtMapUserPhysicalPages
    vsl_sys_nosys,  // 113: NtMapUserPhysicalPagesScatter
    vsl_sys_mmap,  // 114: NtMapViewOfSection
    vsl_sys_nosys,  // 115: NtModifyBootEntry
    vsl_sys_nosys,  // 116: NtModifyDriverEntry
    vsl_sys_nosys,  // 117: NtNotifyChangeDirectoryFile
    vsl_sys_inotify_add_watch,  // 118: NtNotifyChangeKey
    vsl_sys_inotify_add_watch,  // 119: NtNotifyChangeMultipleKeys
    vsl_sys_openat,  // 120: NtOpenDirectoryObject
    vsl_sys_open,  // 121: NtOpenEvent
    vsl_sys_nosys,  // 122: NtOpenEventPair
    vsl_sys_open,  // 123: NtOpenFile
    vsl_sys_open,  // 124: NtOpenIoCompletion
    vsl_nt_open_job_object,  // 125: NtOpenJobObject
    vsl_sys_openat,  // 126: NtOpenKey
    vsl_sys_open,  // 127: NtOpenMutant
    vsl_sys_nosys,  // 128: NtOpenObjectAuditAlarm
    vsl_sys_open,  // 129: NtOpenProcess
    vsl_sys_nosys,  // 130: NtOpenProcessToken
    vsl_sys_nosys,  // 131: NtOpenProcessTokenEx
    vsl_sys_nosys,  // 132: NtOpenSection
    vsl_sys_open,  // 133: NtOpenSemaphore
    vsl_sys_readlink,  // 134: NtOpenSymbolicLinkObject
    vsl_sys_open,  // 135: NtOpenThread
    vsl_sys_nosys,  // 136: NtOpenThreadToken
    vsl_sys_nosys,  // 137: NtOpenThreadTokenEx
    vsl_sys_open,  // 138: NtOpenTimer
    vsl_sys_nosys,  // 139: NtPlugPlayControl
    vsl_sys_nosys,  // 140: NtPowerInformation
    vsl_sys_nosys,  // 141: NtPrivilegeCheck
    vsl_sys_nosys,  // 142: NtPrivilegeObjectAuditAlarm
    vsl_sys_nosys,  // 143: NtPrivilegedServiceAuditAlarm
    vsl_sys_mprotect,  // 144: NtProtectVirtualMemory
    vsl_sys_eventfd_write,  // 145: NtPulseEvent
    vsl_sys_stat,  // 146: NtQueryAttributesFile
    vsl_sys_nosys,  // 147: NtQueryBootEntryOrder
    vsl_sys_nosys,  // 148: NtQueryBootOptions
    vsl_sys_nosys,  // 149: NtQueryDebugFilterState
    vsl_sys_nosys,  // 150: NtQueryDefaultLocale
    vsl_sys_nosys,  // 151: NtQueryDefaultUILanguage
    vsl_sys_getdents,  // 152: NtQueryDirectoryFile
    vsl_sys_getdents,  // 153: NtQueryDirectoryObject
    vsl_sys_nosys,  // 154: NtQueryDriverEntryOrder
    vsl_sys_getxattr,  // 155: NtQueryEaFile
    vsl_sys_nosys,  // 156: NtQueryEvent
    vsl_sys_stat,  // 157: NtQueryFullAttributesFile
    vsl_nt_query_information_atom,  // 158: NtQueryInformationAtom
    vsl_sys_fstat,  // 159: NtQueryInformationFile
    vsl_sys_nosys,  // 160: NtQueryInformationJobObject
    vsl_sys_nosys,  // 161: NtQueryInformationPort
    vsl_sys_proc_info,  // 162: NtQueryInformationProcess
    vsl_sys_nosys,  // 163: NtQueryInformationThread
    vsl_sys_nosys,  // 164: NtQueryInformationToken
    vsl_sys_nosys,  // 165: NtQueryInstallUILanguage
    vsl_sys_nosys,  // 166: NtQueryIntervalProfile
    vsl_sys_nosys,  // 167: NtQueryIoCompletion
    vsl_sys_getdents,  // 168: NtQueryKey
    vsl_sys_nosys,  // 169: NtQueryMultipleValueKey
    vsl_sys_nosys,  // 170: NtQueryMutant
    vsl_sys_nosys,  // 171: NtQueryObject
    vsl_sys_nosys,  // 172: NtQueryOpenSubKeys
    vsl_sys_nosys,  // 173: NtQueryOpenSubKeysEx
    vsl_sys_clock_gettime,  // 174: NtQueryPerformanceCounter
    vsl_sys_nosys,  // 175: NtQueryQuotaInformationFile
    vsl_sys_nosys,  // 176: NtQuerySection
    vsl_sys_nosys,  // 177: NtQuerySecurityObject
    vsl_sys_nosys,  // 178: NtQuerySemaphore
    vsl_sys_readlink,  // 179: NtQuerySymbolicLinkObject
    vsl_sys_nosys,  // 180: NtQuerySystemEnvironmentValue
    vsl_sys_nosys,  // 181: NtQuerySystemEnvironmentValueEx
    vsl_sys_sysinfo,  // 182: NtQuerySystemInformation
    vsl_sys_clock_gettime,  // 183: NtQuerySystemTime
    vsl_sys_nosys,  // 184: NtQueryTimer
    vsl_sys_nosys,  // 185: NtQueryTimerResolution
    vsl_sys_nosys,  // 186: NtQueryValueKey
    vsl_sys_proc_vm_info,  // 187: NtQueryVirtualMemory
    vsl_sys_statfs,  // 188: NtQueryVolumeInformationFile
    vsl_sys_nosys,  // 189: NtQueueApcThread
    vsl_sys_nosys,  // 190: NtRaiseException
    vsl_sys_nosys,  // 191: NtRaiseHardError
    vsl_sys_read,  // 192: NtReadFile
    vsl_sys_readv,  // 193: NtReadFileScatter
    vsl_sys_nosys,  // 194: NtReadRequestData
    vsl_sys_process_vm_readv,  // 195: NtReadVirtualMemory
    vsl_sys_nosys,  // 196: NtRegisterThreadTerminatePort
    vsl_sys_futex,  // 197: NtReleaseMutant
    vsl_sys_futex,  // 198: NtReleaseSemaphore
    vsl_sys_epoll_wait,  // 199: NtRemoveIoCompletion
    vsl_sys_nosys,  // 200: NtRemoveProcessDebug
    vsl_sys_rename,  // 201: NtRenameKey
    vsl_sys_nosys,  // 202: NtReplaceKey
    vsl_sys_nosys,  // 203: NtReplyPort
    vsl_sys_nosys,  // 204: NtReplyWaitReceivePort
    vsl_sys_nosys,  // 205: NtReplyWaitReceivePortEx
    vsl_sys_nosys,  // 206: NtReplyWaitReplyPort
    vsl_sys_nosys,  // 207: NtRequestDeviceWakeup
    vsl_sys_nosys,  // 208: NtRequestPort
    vsl_sys_nosys,  // 209: NtRequestWaitReplyPort
    vsl_sys_nosys,  // 210: NtRequestWakeupLatency
    vsl_sys_eventfd,  // 211: NtResetEvent
    vsl_sys_nosys,  // 212: NtResetWriteWatch
    vsl_sys_nosys,  // 213: NtRestoreKey
    vsl_sys_kill,  // 214: NtResumeProcess
    vsl_sys_kill,  // 215: NtResumeThread
    vsl_sys_nosys,  // 216: NtSaveKey
    vsl_sys_nosys,  // 217: NtSaveKeyEx
    vsl_sys_nosys,  // 218: NtSaveMergedKeys
    vsl_sys_nosys,  // 219: NtSecureConnectPort
    vsl_sys_nosys,  // 220: NtSetBootEntryOrder
    vsl_sys_nosys,  // 221: NtSetBootOptions
    vsl_sys_nosys,  // 222: NtSetContextThread
    vsl_sys_nosys,  // 223: NtSetDebugFilterState
    vsl_sys_nosys,  // 224: NtSetDefaultHardErrorPort
    vsl_sys_nosys,  // 225: NtSetDefaultLocale
    vsl_sys_nosys,  // 226: NtSetDefaultUILanguage
    vsl_sys_nosys,  // 227: NtSetDriverEntryOrder
    vsl_sys_setxattr,  // 228: NtSetEaFile
    vsl_sys_eventfd_write,  // 229: NtSetEvent
    vsl_sys_nosys,  // 230: NtSetEventBoostPriority
    vsl_sys_nosys,  // 231: NtSetHighEventPair
    vsl_sys_nosys,  // 232: NtSetHighWaitLowEventPair
    vsl_sys_nosys,  // 233: NtSetInformationDebugObject
    vsl_sys_fcntl,  // 234: NtSetInformationFile
    vsl_sys_nosys,  // 235: NtSetInformationJobObject
    vsl_sys_nosys,  // 236: NtSetInformationKey
    vsl_sys_nosys,  // 237: NtSetInformationObject
    vsl_sys_nosys,  // 238: NtSetInformationProcess
    vsl_sys_nosys,  // 239: NtSetInformationThread
    vsl_sys_nosys,  // 240: NtSetInformationToken
    vsl_sys_nosys,  // 241: NtSetIntervalProfile
    vsl_sys_epoll_ctl,  // 242: NtSetIoCompletion
    vsl_sys_nosys,  // 243: NtSetLdtEntries
    vsl_sys_nosys,  // 244: NtSetLowEventPair
    vsl_sys_nosys,  // 245: NtSetLowWaitHighEventPair
    vsl_sys_nosys,  // 246: NtSetQuotaInformationFile
    vsl_sys_nosys,  // 247: NtSetSecurityObject
    vsl_sys_nosys,  // 248: NtSetSystemEnvironmentValue
    vsl_sys_nosys,  // 249: NtSetSystemEnvironmentValueEx
    vsl_sys_nosys,  // 250: NtSetSystemInformation
    vsl_sys_nosys,  // 251: NtSetSystemPowerState
    vsl_sys_clock_settime,  // 252: NtSetSystemTime
    vsl_sys_nosys,  // 253: NtSetThreadExecutionState
    vsl_sys_timerfd_settime,  // 254: NtSetTimer
    vsl_sys_nosys,  // 255: NtSetTimerResolution
    vsl_nt_set_uuid_seed,  // 256: NtSetUuidSeed
    vsl_sys_nosys,  // 257: NtSetValueKey
    vsl_sys_nosys,  // 258: NtSetVolumeInformationFile
    vsl_sys_nosys,  // 259: NtShutdownSystem
    vsl_sys_futex_wait,  // 260: NtSignalAndWaitForSingleObject
    vsl_sys_nosys,  // 261: NtStartProfile
    vsl_sys_nosys,  // 262: NtStopProfile
    vsl_sys_kill,  // 263: NtSuspendProcess
    vsl_sys_kill,  // 264: NtSuspendThread
    vsl_sys_nosys,  // 265: NtSystemDebugControl
    vsl_nt_terminate_job_object,  // 266: NtTerminateJobObject
    vsl_sys_kill,  // 267: NtTerminateProcess
    vsl_sys_tgkill,  // 268: NtTerminateThread
    vsl_sys_nosys,  // 269: NtTestAlert
    vsl_sys_nosys,  // 270: NtTraceEvent
    vsl_sys_nosys,  // 271: NtTranslateFilePath
    vsl_sys_nosys,  // 272: NtUnloadDriver
    vsl_sys_nosys,  // 273: NtUnloadKey
    vsl_sys_nosys,  // 274: NtUnloadKey2
    vsl_sys_nosys,  // 275: NtUnloadKeyEx
    vsl_sys_fcntl,  // 276: NtUnlockFile
    vsl_sys_munlock,  // 277: NtUnlockVirtualMemory
    vsl_sys_munmap,  // 278: NtUnmapViewOfSection
    vsl_sys_nosys,  // 279: NtVdmControl
    vsl_sys_nosys,  // 280: NtWaitForDebugEvent
    vsl_sys_poll,  // 281: NtWaitForMultipleObjects
    vsl_sys_poll,  // 282: NtWaitForSingleObject
    vsl_sys_nosys,  // 283: NtWaitHighEventPair
    vsl_sys_nosys,  // 284: NtWaitLowEventPair
    vsl_sys_write,  // 285: NtWriteFile
    vsl_sys_writev,  // 286: NtWriteFileGather
    vsl_sys_nosys,  // 287: NtWriteRequestData
    vsl_sys_process_vm_writev,  // 288: NtWriteVirtualMemory
    vsl_sys_sched_yield,  // 289: NtYieldExecution
    vsl_sys_futex,  // 290: NtCreateKeyedEvent
    vsl_sys_open,  // 291: NtOpenKeyedEvent
    vsl_sys_futex,  // 292: NtReleaseKeyedEvent
    vsl_sys_futex_wait,  // 293: NtWaitForKeyedEvent
    vsl_sys_nosys,  // 294: NtQueryPortInformationProcess
    vsl_sys_getcpu,  // 295: NtGetCurrentProcessorNumber
    vsl_sys_poll,  // 296: NtWaitForMultipleObjects32
};

/* Argument counts for each syscall (in 8-byte units) */
uint8_t vsl_syscall_nargs[] = {
    6,  // 1: NtAcceptConnectPort
    8,  // 2: NtAccessCheck
    11,  // 3: NtAccessCheckAndAuditAlarm
    11,  // 4: NtAccessCheckByType
    16,  // 5: NtAccessCheckByTypeAndAuditAlarm
    11,  // 6: NtAccessCheckByTypeResultList
    16,  // 7: NtAccessCheckByTypeResultListAndAuditAlarm
    17,  // 8: NtAccessCheckByTypeResultListAndAuditAlarmByHandle
    3,  // 9: NtAddAtom
    2,  // 10: NtAddBootEntry
    2,  // 11: NtAddDriverEntry
    6,  // 12: NtAdjustGroupsToken
    6,  // 13: NtAdjustPrivilegesToken
    2,  // 14: NtAlertResumeThread
    1,  // 15: NtAlertThread
    1,  // 16: NtAllocateLocallyUniqueId
    3,  // 17: NtAllocateUserPhysicalPages
    4,  // 18: NtAllocateUuids
    6,  // 19: NtAllocateVirtualMemory
    2,  // 20: NtApphelpCacheControl
    2,  // 21: NtAreMappedFilesTheSame
    2,  // 22: NtAssignProcessToJobObject
    3,  // 23: NtCallbackReturn
    1,  // 24: NtCancelDeviceWakeupRequest
    2,  // 25: NtCancelIoFile
    2,  // 26: NtCancelTimer
    1,  // 27: NtClearEvent
    1,  // 28: NtClose
    3,  // 29: NtCloseObjectAuditAlarm
    2,  // 30: NtCompactKeys
    3,  // 31: NtCompareTokens
    1,  // 32: NtCompleteConnectPort
    1,  // 33: NtCompressKey
    8,  // 34: NtConnectPort
    2,  // 35: NtContinue
    4,  // 36: NtCreateDebugObject
    3,  // 37: NtCreateDirectoryObject
    5,  // 38: NtCreateEvent
    3,  // 39: NtCreateEventPair
    11,  // 40: NtCreateFile
    4,  // 41: NtCreateIoCompletion
    3,  // 42: NtCreateJobObject
    3,  // 43: NtCreateJobSet
    7,  // 44: NtCreateKey
    8,  // 45: NtCreateMailslotFile
    4,  // 46: NtCreateMutant
    14,  // 47: NtCreateNamedPipeFile
    4,  // 48: NtCreatePagingFile
    5,  // 49: NtCreatePort
    8,  // 50: NtCreateProcess
    9,  // 51: NtCreateProcessEx
    9,  // 52: NtCreateProfile
    7,  // 53: NtCreateSection
    5,  // 54: NtCreateSemaphore
    4,  // 55: NtCreateSymbolicLinkObject
    8,  // 56: NtCreateThread
    4,  // 57: NtCreateTimer
    13,  // 58: NtCreateToken
    5,  // 59: NtCreateWaitablePort
    2,  // 60: NtDebugActiveProcess
    3,  // 61: NtDebugContinue
    2,  // 62: NtDelayExecution
    1,  // 63: NtDeleteAtom
    1,  // 64: NtDeleteBootEntry
    1,  // 65: NtDeleteDriverEntry
    1,  // 66: NtDeleteFile
    1,  // 67: NtDeleteKey
    3,  // 68: NtDeleteObjectAuditAlarm
    2,  // 69: NtDeleteValueKey
    10,  // 70: NtDeviceIoControlFile
    1,  // 71: NtDisplayString
    7,  // 72: NtDuplicateObject
    6,  // 73: NtDuplicateToken
    2,  // 74: NtEnumerateBootEntries
    2,  // 75: NtEnumerateDriverEntries
    6,  // 76: NtEnumerateKey
    3,  // 77: NtEnumerateSystemEnvironmentValuesEx
    6,  // 78: NtEnumerateValueKey
    2,  // 79: NtExtendSection
    6,  // 80: NtFilterToken
    3,  // 81: NtFindAtom
    2,  // 82: NtFlushBuffersFile
    3,  // 83: NtFlushInstructionCache
    1,  // 84: NtFlushKey
    4,  // 85: NtFlushVirtualMemory
    0,  // 86: NtFlushWriteBuffer
    3,  // 87: NtFreeUserPhysicalPages
    4,  // 88: NtFreeVirtualMemory
    10,  // 89: NtFsControlFile
    2,  // 90: NtGetContextThread
    2,  // 91: NtGetDevicePowerState
    4,  // 92: NtGetPlugPlayEvent
    7,  // 93: NtGetWriteWatch
    1,  // 94: NtImpersonateAnonymousToken
    2,  // 95: NtImpersonateClientOfPort
    3,  // 96: NtImpersonateThread
    1,  // 97: NtInitializeRegistry
    4,  // 98: NtInitiatePowerAction
    2,  // 99: NtIsProcessInJob
    0,  // 100: NtIsSystemResumeAutomatic
    2,  // 101: NtListenPort
    1,  // 102: NtLoadDriver
    2,  // 103: NtLoadKey
    3,  // 104: NtLoadKey2
    4,  // 105: NtLoadKeyEx
    10,  // 106: NtLockFile
    2,  // 107: NtLockProductActivationKeys
    1,  // 108: NtLockRegistryKey
    4,  // 109: NtLockVirtualMemory
    1,  // 110: NtMakePermanentObject
    1,  // 111: NtMakeTemporaryObject
    3,  // 112: NtMapUserPhysicalPages
    3,  // 113: NtMapUserPhysicalPagesScatter
    10,  // 114: NtMapViewOfSection
    1,  // 115: NtModifyBootEntry
    1,  // 116: NtModifyDriverEntry
    9,  // 117: NtNotifyChangeDirectoryFile
    10,  // 118: NtNotifyChangeKey
    12,  // 119: NtNotifyChangeMultipleKeys
    3,  // 120: NtOpenDirectoryObject
    3,  // 121: NtOpenEvent
    3,  // 122: NtOpenEventPair
    6,  // 123: NtOpenFile
    3,  // 124: NtOpenIoCompletion
    3,  // 125: NtOpenJobObject
    3,  // 126: NtOpenKey
    3,  // 127: NtOpenMutant
    12,  // 128: NtOpenObjectAuditAlarm
    4,  // 129: NtOpenProcess
    3,  // 130: NtOpenProcessToken
    4,  // 131: NtOpenProcessTokenEx
    3,  // 132: NtOpenSection
    3,  // 133: NtOpenSemaphore
    3,  // 134: NtOpenSymbolicLinkObject
    4,  // 135: NtOpenThread
    4,  // 136: NtOpenThreadToken
    5,  // 137: NtOpenThreadTokenEx
    3,  // 138: NtOpenTimer
    3,  // 139: NtPlugPlayControl
    5,  // 140: NtPowerInformation
    3,  // 141: NtPrivilegeCheck
    6,  // 142: NtPrivilegeObjectAuditAlarm
    5,  // 143: NtPrivilegedServiceAuditAlarm
    5,  // 144: NtProtectVirtualMemory
    2,  // 145: NtPulseEvent
    2,  // 146: NtQueryAttributesFile
    2,  // 147: NtQueryBootEntryOrder
    2,  // 148: NtQueryBootOptions
    2,  // 149: NtQueryDebugFilterState
    2,  // 150: NtQueryDefaultLocale
    1,  // 151: NtQueryDefaultUILanguage
    11,  // 152: NtQueryDirectoryFile
    7,  // 153: NtQueryDirectoryObject
    2,  // 154: NtQueryDriverEntryOrder
    9,  // 155: NtQueryEaFile
    5,  // 156: NtQueryEvent
    2,  // 157: NtQueryFullAttributesFile
    5,  // 158: NtQueryInformationAtom
    5,  // 159: NtQueryInformationFile
    5,  // 160: NtQueryInformationJobObject
    5,  // 161: NtQueryInformationPort
    5,  // 162: NtQueryInformationProcess
    5,  // 163: NtQueryInformationThread
    5,  // 164: NtQueryInformationToken
    1,  // 165: NtQueryInstallUILanguage
    2,  // 166: NtQueryIntervalProfile
    5,  // 167: NtQueryIoCompletion
    5,  // 168: NtQueryKey
    6,  // 169: NtQueryMultipleValueKey
    5,  // 170: NtQueryMutant
    5,  // 171: NtQueryObject
    2,  // 172: NtQueryOpenSubKeys
    4,  // 173: NtQueryOpenSubKeysEx
    2,  // 174: NtQueryPerformanceCounter
    9,  // 175: NtQueryQuotaInformationFile
    5,  // 176: NtQuerySection
    5,  // 177: NtQuerySecurityObject
    5,  // 178: NtQuerySemaphore
    3,  // 179: NtQuerySymbolicLinkObject
    4,  // 180: NtQuerySystemEnvironmentValue
    5,  // 181: NtQuerySystemEnvironmentValueEx
    4,  // 182: NtQuerySystemInformation
    1,  // 183: NtQuerySystemTime
    5,  // 184: NtQueryTimer
    3,  // 185: NtQueryTimerResolution
    6,  // 186: NtQueryValueKey
    6,  // 187: NtQueryVirtualMemory
    5,  // 188: NtQueryVolumeInformationFile
    5,  // 189: NtQueueApcThread
    3,  // 190: NtRaiseException
    6,  // 191: NtRaiseHardError
    9,  // 192: NtReadFile
    9,  // 193: NtReadFileScatter
    6,  // 194: NtReadRequestData
    5,  // 195: NtReadVirtualMemory
    1,  // 196: NtRegisterThreadTerminatePort
    2,  // 197: NtReleaseMutant
    3,  // 198: NtReleaseSemaphore
    5,  // 199: NtRemoveIoCompletion
    2,  // 200: NtRemoveProcessDebug
    2,  // 201: NtRenameKey
    3,  // 202: NtReplaceKey
    2,  // 203: NtReplyPort
    4,  // 204: NtReplyWaitReceivePort
    5,  // 205: NtReplyWaitReceivePortEx
    2,  // 206: NtReplyWaitReplyPort
    1,  // 207: NtRequestDeviceWakeup
    2,  // 208: NtRequestPort
    3,  // 209: NtRequestWaitReplyPort
    1,  // 210: NtRequestWakeupLatency
    2,  // 211: NtResetEvent
    3,  // 212: NtResetWriteWatch
    3,  // 213: NtRestoreKey
    1,  // 214: NtResumeProcess
    2,  // 215: NtResumeThread
    2,  // 216: NtSaveKey
    3,  // 217: NtSaveKeyEx
    3,  // 218: NtSaveMergedKeys
    9,  // 219: NtSecureConnectPort
    2,  // 220: NtSetBootEntryOrder
    2,  // 221: NtSetBootOptions
    2,  // 222: NtSetContextThread
    3,  // 223: NtSetDebugFilterState
    1,  // 224: NtSetDefaultHardErrorPort
    2,  // 225: NtSetDefaultLocale
    1,  // 226: NtSetDefaultUILanguage
    2,  // 227: NtSetDriverEntryOrder
    4,  // 228: NtSetEaFile
    2,  // 229: NtSetEvent
    1,  // 230: NtSetEventBoostPriority
    1,  // 231: NtSetHighEventPair
    1,  // 232: NtSetHighWaitLowEventPair
    5,  // 233: NtSetInformationDebugObject
    5,  // 234: NtSetInformationFile
    4,  // 235: NtSetInformationJobObject
    4,  // 236: NtSetInformationKey
    4,  // 237: NtSetInformationObject
    4,  // 238: NtSetInformationProcess
    4,  // 239: NtSetInformationThread
    4,  // 240: NtSetInformationToken
    2,  // 241: NtSetIntervalProfile
    5,  // 242: NtSetIoCompletion
    6,  // 243: NtSetLdtEntries
    1,  // 244: NtSetLowEventPair
    1,  // 245: NtSetLowWaitHighEventPair
    4,  // 246: NtSetQuotaInformationFile
    3,  // 247: NtSetSecurityObject
    2,  // 248: NtSetSystemEnvironmentValue
    5,  // 249: NtSetSystemEnvironmentValueEx
    3,  // 250: NtSetSystemInformation
    3,  // 251: NtSetSystemPowerState
    2,  // 252: NtSetSystemTime
    2,  // 253: NtSetThreadExecutionState
    7,  // 254: NtSetTimer
    3,  // 255: NtSetTimerResolution
    1,  // 256: NtSetUuidSeed
    6,  // 257: NtSetValueKey
    5,  // 258: NtSetVolumeInformationFile
    1,  // 259: NtShutdownSystem
    4,  // 260: NtSignalAndWaitForSingleObject
    1,  // 261: NtStartProfile
    1,  // 262: NtStopProfile
    1,  // 263: NtSuspendProcess
    2,  // 264: NtSuspendThread
    6,  // 265: NtSystemDebugControl
    2,  // 266: NtTerminateJobObject
    2,  // 267: NtTerminateProcess
    2,  // 268: NtTerminateThread
    0,  // 269: NtTestAlert
    4,  // 270: NtTraceEvent
    4,  // 271: NtTranslateFilePath
    1,  // 272: NtUnloadDriver
    1,  // 273: NtUnloadKey
    2,  // 274: NtUnloadKey2
    2,  // 275: NtUnloadKeyEx
    5,  // 276: NtUnlockFile
    4,  // 277: NtUnlockVirtualMemory
    2,  // 278: NtUnmapViewOfSection
    2,  // 279: NtVdmControl
    4,  // 280: NtWaitForDebugEvent
    5,  // 281: NtWaitForMultipleObjects
    3,  // 282: NtWaitForSingleObject
    1,  // 283: NtWaitHighEventPair
    1,  // 284: NtWaitLowEventPair
    9,  // 285: NtWriteFile
    9,  // 286: NtWriteFileGather
    6,  // 287: NtWriteRequestData
    5,  // 288: NtWriteVirtualMemory
    0,  // 289: NtYieldExecution
    4,  // 290: NtCreateKeyedEvent
    3,  // 291: NtOpenKeyedEvent
    4,  // 292: NtReleaseKeyedEvent
    4,  // 293: NtWaitForKeyedEvent
    0,  // 294: NtQueryPortInformationProcess
    0,  // 295: NtGetCurrentProcessorNumber
    5,  // 296: NtWaitForMultipleObjects32
};

#define VSL_SYSCALL_COUNT 296

/* NT syscall number to VSL syscall index mapping */
/* ReactOS uses same numbering as sysfuncs.lst */
uint32_t vsl_nt_to_vsl_index[] = {
    0,  // 1: NtAcceptConnectPort
    1,  // 2: NtAccessCheck
    2,  // 3: NtAccessCheckAndAuditAlarm
    3,  // 4: NtAccessCheckByType
    4,  // 5: NtAccessCheckByTypeAndAuditAlarm
    5,  // 6: NtAccessCheckByTypeResultList
    6,  // 7: NtAccessCheckByTypeResultListAndAuditAlarm
    7,  // 8: NtAccessCheckByTypeResultListAndAuditAlarmByHandle
    8,  // 9: NtAddAtom
    9,  // 10: NtAddBootEntry
    10,  // 11: NtAddDriverEntry
    11,  // 12: NtAdjustGroupsToken
    12,  // 13: NtAdjustPrivilegesToken
    13,  // 14: NtAlertResumeThread
    14,  // 15: NtAlertThread
    15,  // 16: NtAllocateLocallyUniqueId
    16,  // 17: NtAllocateUserPhysicalPages
    17,  // 18: NtAllocateUuids
    18,  // 19: NtAllocateVirtualMemory
    19,  // 20: NtApphelpCacheControl
    20,  // 21: NtAreMappedFilesTheSame
    21,  // 22: NtAssignProcessToJobObject
    22,  // 23: NtCallbackReturn
    23,  // 24: NtCancelDeviceWakeupRequest
    24,  // 25: NtCancelIoFile
    25,  // 26: NtCancelTimer
    26,  // 27: NtClearEvent
    27,  // 28: NtClose
    28,  // 29: NtCloseObjectAuditAlarm
    29,  // 30: NtCompactKeys
    30,  // 31: NtCompareTokens
    31,  // 32: NtCompleteConnectPort
    32,  // 33: NtCompressKey
    33,  // 34: NtConnectPort
    34,  // 35: NtContinue
    35,  // 36: NtCreateDebugObject
    36,  // 37: NtCreateDirectoryObject
    37,  // 38: NtCreateEvent
    38,  // 39: NtCreateEventPair
    39,  // 40: NtCreateFile
    40,  // 41: NtCreateIoCompletion
    41,  // 42: NtCreateJobObject
    42,  // 43: NtCreateJobSet
    43,  // 44: NtCreateKey
    44,  // 45: NtCreateMailslotFile
    45,  // 46: NtCreateMutant
    46,  // 47: NtCreateNamedPipeFile
    47,  // 48: NtCreatePagingFile
    48,  // 49: NtCreatePort
    49,  // 50: NtCreateProcess
    50,  // 51: NtCreateProcessEx
    51,  // 52: NtCreateProfile
    52,  // 53: NtCreateSection
    53,  // 54: NtCreateSemaphore
    54,  // 55: NtCreateSymbolicLinkObject
    55,  // 56: NtCreateThread
    56,  // 57: NtCreateTimer
    57,  // 58: NtCreateToken
    58,  // 59: NtCreateWaitablePort
    59,  // 60: NtDebugActiveProcess
    60,  // 61: NtDebugContinue
    61,  // 62: NtDelayExecution
    62,  // 63: NtDeleteAtom
    63,  // 64: NtDeleteBootEntry
    64,  // 65: NtDeleteDriverEntry
    65,  // 66: NtDeleteFile
    66,  // 67: NtDeleteKey
    67,  // 68: NtDeleteObjectAuditAlarm
    68,  // 69: NtDeleteValueKey
    69,  // 70: NtDeviceIoControlFile
    70,  // 71: NtDisplayString
    71,  // 72: NtDuplicateObject
    72,  // 73: NtDuplicateToken
    73,  // 74: NtEnumerateBootEntries
    74,  // 75: NtEnumerateDriverEntries
    75,  // 76: NtEnumerateKey
    76,  // 77: NtEnumerateSystemEnvironmentValuesEx
    77,  // 78: NtEnumerateValueKey
    78,  // 79: NtExtendSection
    79,  // 80: NtFilterToken
    80,  // 81: NtFindAtom
    81,  // 82: NtFlushBuffersFile
    82,  // 83: NtFlushInstructionCache
    83,  // 84: NtFlushKey
    84,  // 85: NtFlushVirtualMemory
    85,  // 86: NtFlushWriteBuffer
    86,  // 87: NtFreeUserPhysicalPages
    87,  // 88: NtFreeVirtualMemory
    88,  // 89: NtFsControlFile
    89,  // 90: NtGetContextThread
    90,  // 91: NtGetDevicePowerState
    91,  // 92: NtGetPlugPlayEvent
    92,  // 93: NtGetWriteWatch
    93,  // 94: NtImpersonateAnonymousToken
    94,  // 95: NtImpersonateClientOfPort
    95,  // 96: NtImpersonateThread
    96,  // 97: NtInitializeRegistry
    97,  // 98: NtInitiatePowerAction
    98,  // 99: NtIsProcessInJob
    99,  // 100: NtIsSystemResumeAutomatic
    100,  // 101: NtListenPort
    101,  // 102: NtLoadDriver
    102,  // 103: NtLoadKey
    103,  // 104: NtLoadKey2
    104,  // 105: NtLoadKeyEx
    105,  // 106: NtLockFile
    106,  // 107: NtLockProductActivationKeys
    107,  // 108: NtLockRegistryKey
    108,  // 109: NtLockVirtualMemory
    109,  // 110: NtMakePermanentObject
    110,  // 111: NtMakeTemporaryObject
    111,  // 112: NtMapUserPhysicalPages
    112,  // 113: NtMapUserPhysicalPagesScatter
    113,  // 114: NtMapViewOfSection
    114,  // 115: NtModifyBootEntry
    115,  // 116: NtModifyDriverEntry
    116,  // 117: NtNotifyChangeDirectoryFile
    117,  // 118: NtNotifyChangeKey
    118,  // 119: NtNotifyChangeMultipleKeys
    119,  // 120: NtOpenDirectoryObject
    120,  // 121: NtOpenEvent
    121,  // 122: NtOpenEventPair
    122,  // 123: NtOpenFile
    123,  // 124: NtOpenIoCompletion
    124,  // 125: NtOpenJobObject
    125,  // 126: NtOpenKey
    126,  // 127: NtOpenMutant
    127,  // 128: NtOpenObjectAuditAlarm
    128,  // 129: NtOpenProcess
    129,  // 130: NtOpenProcessToken
    130,  // 131: NtOpenProcessTokenEx
    131,  // 132: NtOpenSection
    132,  // 133: NtOpenSemaphore
    133,  // 134: NtOpenSymbolicLinkObject
    134,  // 135: NtOpenThread
    135,  // 136: NtOpenThreadToken
    136,  // 137: NtOpenThreadTokenEx
    137,  // 138: NtOpenTimer
    138,  // 139: NtPlugPlayControl
    139,  // 140: NtPowerInformation
    140,  // 141: NtPrivilegeCheck
    141,  // 142: NtPrivilegeObjectAuditAlarm
    142,  // 143: NtPrivilegedServiceAuditAlarm
    143,  // 144: NtProtectVirtualMemory
    144,  // 145: NtPulseEvent
    145,  // 146: NtQueryAttributesFile
    146,  // 147: NtQueryBootEntryOrder
    147,  // 148: NtQueryBootOptions
    148,  // 149: NtQueryDebugFilterState
    149,  // 150: NtQueryDefaultLocale
    150,  // 151: NtQueryDefaultUILanguage
    151,  // 152: NtQueryDirectoryFile
    152,  // 153: NtQueryDirectoryObject
    153,  // 154: NtQueryDriverEntryOrder
    154,  // 155: NtQueryEaFile
    155,  // 156: NtQueryEvent
    156,  // 157: NtQueryFullAttributesFile
    157,  // 158: NtQueryInformationAtom
    158,  // 159: NtQueryInformationFile
    159,  // 160: NtQueryInformationJobObject
    160,  // 161: NtQueryInformationPort
    161,  // 162: NtQueryInformationProcess
    162,  // 163: NtQueryInformationThread
    163,  // 164: NtQueryInformationToken
    164,  // 165: NtQueryInstallUILanguage
    165,  // 166: NtQueryIntervalProfile
    166,  // 167: NtQueryIoCompletion
    167,  // 168: NtQueryKey
    168,  // 169: NtQueryMultipleValueKey
    169,  // 170: NtQueryMutant
    170,  // 171: NtQueryObject
    171,  // 172: NtQueryOpenSubKeys
    172,  // 173: NtQueryOpenSubKeysEx
    173,  // 174: NtQueryPerformanceCounter
    174,  // 175: NtQueryQuotaInformationFile
    175,  // 176: NtQuerySection
    176,  // 177: NtQuerySecurityObject
    177,  // 178: NtQuerySemaphore
    178,  // 179: NtQuerySymbolicLinkObject
    179,  // 180: NtQuerySystemEnvironmentValue
    180,  // 181: NtQuerySystemEnvironmentValueEx
    181,  // 182: NtQuerySystemInformation
    182,  // 183: NtQuerySystemTime
    183,  // 184: NtQueryTimer
    184,  // 185: NtQueryTimerResolution
    185,  // 186: NtQueryValueKey
    186,  // 187: NtQueryVirtualMemory
    187,  // 188: NtQueryVolumeInformationFile
    188,  // 189: NtQueueApcThread
    189,  // 190: NtRaiseException
    190,  // 191: NtRaiseHardError
    191,  // 192: NtReadFile
    192,  // 193: NtReadFileScatter
    193,  // 194: NtReadRequestData
    194,  // 195: NtReadVirtualMemory
    195,  // 196: NtRegisterThreadTerminatePort
    196,  // 197: NtReleaseMutant
    197,  // 198: NtReleaseSemaphore
    198,  // 199: NtRemoveIoCompletion
    199,  // 200: NtRemoveProcessDebug
    200,  // 201: NtRenameKey
    201,  // 202: NtReplaceKey
    202,  // 203: NtReplyPort
    203,  // 204: NtReplyWaitReceivePort
    204,  // 205: NtReplyWaitReceivePortEx
    205,  // 206: NtReplyWaitReplyPort
    206,  // 207: NtRequestDeviceWakeup
    207,  // 208: NtRequestPort
    208,  // 209: NtRequestWaitReplyPort
    209,  // 210: NtRequestWakeupLatency
    210,  // 211: NtResetEvent
    211,  // 212: NtResetWriteWatch
    212,  // 213: NtRestoreKey
    213,  // 214: NtResumeProcess
    214,  // 215: NtResumeThread
    215,  // 216: NtSaveKey
    216,  // 217: NtSaveKeyEx
    217,  // 218: NtSaveMergedKeys
    218,  // 219: NtSecureConnectPort
    219,  // 220: NtSetBootEntryOrder
    220,  // 221: NtSetBootOptions
    221,  // 222: NtSetContextThread
    222,  // 223: NtSetDebugFilterState
    223,  // 224: NtSetDefaultHardErrorPort
    224,  // 225: NtSetDefaultLocale
    225,  // 226: NtSetDefaultUILanguage
    226,  // 227: NtSetDriverEntryOrder
    227,  // 228: NtSetEaFile
    228,  // 229: NtSetEvent
    229,  // 230: NtSetEventBoostPriority
    230,  // 231: NtSetHighEventPair
    231,  // 232: NtSetHighWaitLowEventPair
    232,  // 233: NtSetInformationDebugObject
    233,  // 234: NtSetInformationFile
    234,  // 235: NtSetInformationJobObject
    235,  // 236: NtSetInformationKey
    236,  // 237: NtSetInformationObject
    237,  // 238: NtSetInformationProcess
    238,  // 239: NtSetInformationThread
    239,  // 240: NtSetInformationToken
    240,  // 241: NtSetIntervalProfile
    241,  // 242: NtSetIoCompletion
    242,  // 243: NtSetLdtEntries
    243,  // 244: NtSetLowEventPair
    244,  // 245: NtSetLowWaitHighEventPair
    245,  // 246: NtSetQuotaInformationFile
    246,  // 247: NtSetSecurityObject
    247,  // 248: NtSetSystemEnvironmentValue
    248,  // 249: NtSetSystemEnvironmentValueEx
    249,  // 250: NtSetSystemInformation
    250,  // 251: NtSetSystemPowerState
    251,  // 252: NtSetSystemTime
    252,  // 253: NtSetThreadExecutionState
    253,  // 254: NtSetTimer
    254,  // 255: NtSetTimerResolution
    255,  // 256: NtSetUuidSeed
    256,  // 257: NtSetValueKey
    257,  // 258: NtSetVolumeInformationFile
    258,  // 259: NtShutdownSystem
    259,  // 260: NtSignalAndWaitForSingleObject
    260,  // 261: NtStartProfile
    261,  // 262: NtStopProfile
    262,  // 263: NtSuspendProcess
    263,  // 264: NtSuspendThread
    264,  // 265: NtSystemDebugControl
    265,  // 266: NtTerminateJobObject
    266,  // 267: NtTerminateProcess
    267,  // 268: NtTerminateThread
    268,  // 269: NtTestAlert
    269,  // 270: NtTraceEvent
    270,  // 271: NtTranslateFilePath
    271,  // 272: NtUnloadDriver
    272,  // 273: NtUnloadKey
    273,  // 274: NtUnloadKey2
    274,  // 275: NtUnloadKeyEx
    275,  // 276: NtUnlockFile
    276,  // 277: NtUnlockVirtualMemory
    277,  // 278: NtUnmapViewOfSection
    278,  // 279: NtVdmControl
    279,  // 280: NtWaitForDebugEvent
    280,  // 281: NtWaitForMultipleObjects
    281,  // 282: NtWaitForSingleObject
    282,  // 283: NtWaitHighEventPair
    283,  // 284: NtWaitLowEventPair
    284,  // 285: NtWriteFile
    285,  // 286: NtWriteFileGather
    286,  // 287: NtWriteRequestData
    287,  // 288: NtWriteVirtualMemory
    288,  // 289: NtYieldExecution
    289,  // 290: NtCreateKeyedEvent
    290,  // 291: NtOpenKeyedEvent
    291,  // 292: NtReleaseKeyedEvent
    292,  // 293: NtWaitForKeyedEvent
    293,  // 294: NtQueryPortInformationProcess
    294,  // 295: NtGetCurrentProcessorNumber
    295,  // 296: NtWaitForMultipleObjects32
};
