# ReactOS NT Syscall Study for WuBuOS

## Overview

This document analyzes ReactOS's NT syscall implementation (297 syscalls up to Vista/Longhorn level) as a study reference for WuBuOS's VSL (Virtual Syscall Layer) and the TempleOS→Windows/Linux transliteration via SteamOS/Arch + NVK.

**ReactOS Status**: Implements ~297 NT syscalls (NT 5.2 / Vista era parity)
**WuBuOS VSL**: Implements ~50+ Linux syscall translations via host delegation
**Target**: Map NT syscalls → VSL syscalls → Styx/9P → ZealOS HolyC JIT → TempleOS

---

## ReactOS Syscall Architecture

### System Call Entry Points

**x86-64 (AMD64)**:
```asm
; ntoskrnl/ke/amd64/traphandler.c:137
KiSystemCallHandler() 
  - Reads syscall number from RAX
  - Decodes table index (WIN32K_SERVICE_INDEX vs core)
  - Validates against KeServiceDescriptorTable[TableIndex].Limit
  - Copies up to 16 args from user stack (UserParams) to kernel stack (KernelParams)
  - Returns function pointer from DescriptorTable->Base[ServiceNumber]

; syscalls.inc:77-83 (STUBCODE_U for AMD64)
STUBCODE_U:
    mov eax, SyscallId
    mov r10, rcx
    syscall
    ret
```

**i386 (32-bit)**:
```asm
; Uses INT 0x2E or SYSENTER
; KiSystemCallEntry32 via MSR_CSTAR
```

### System Service Descriptor Tables (SSDT)

```c
// ntoskrnl/include/internal/ke.h
#define SSDT_MAX_ENTRIES 2

typedef struct _KSERVICE_TABLE_DESCRIPTOR {
    PULONG_PTR Base;      // Function pointers
    PULONG Count;         // Call counters (optional)
    ULONG Limit;          // Number of syscalls
    PUCHAR Number;        // Argument counts (bytes/8)
} KSERVICE_TABLE_DESCRIPTOR;

KSERVICE_TABLE_DESCRIPTOR KeServiceDescriptorTable[SSDT_MAX_ENTRIES];
KSERVICE_TABLE_DESCRIPTOR KeServiceDescriptorTableShadow[SSDT_MAX_ENTRIES];

// Table 0 = Core NT syscalls (ntoskrnl)
// Table 1 = Win32k syscalls (GUI subsystem)
```

### Syscall Table Generation

**Source**: `ntoskrnl/include/sysfuncs.h` (297 entries)
**Generated**: `ntoskrnl/include/internal/napi.h`

```c
// napi.h - Auto-generated from sysfuncs.h
#define SVC_(name, argcount) (ULONG_PTR)Nt##name,
ULONG_PTR MainSSDT[] = {
#include "sysfuncs.h"
};
#undef SVC_

#define SVC_(name, argcount) argcount * sizeof(void *),
UCHAR MainSSPT[] = {
#include "sysfuncs.h"
};

#define NUMBER_OF_SYSCALLS (sizeof(MainSSPT) / sizeof(MainSSPT[0]))
ULONG KiServiceLimit = NUMBER_OF_SYSCALLS;  // = 297
```

### Stub Generation

**User-mode stubs** (ntdll.dll): `ntoskrnl/ntdll.S` + `ntoskrnl/ntdll_nt6.S`
**Kernel-mode stubs** (Zw* entry points): `ntoskrnl/ex/zw.S`

```asm
; syscalls.inc:139-145 (STUB_U macro)
MACRO(STUB_U, Name, ArgCount)
    MAKE_LABEL Zw&Name, %ArgCount * 4
    START_PROC Nt&Name, %ArgCount * 4
    STUBCODE_U Name, SyscallId, %ArgCount
    .ENDP
    SyscallId = SyscallId + 1
ENDM
```

---

## Complete ReactOS NT Syscall Map (297 syscalls)

| # | Syscall | Args | Category | WuBuOS VSL Equivalent |
|---|---------|------|----------|----------------------|
| 1 | NtAcceptConnectPort | 6 | LPC | vsl_sys_socketpair + custom |
| 2 | NtAccessCheck | 8 | Security | — |
| 3 | NtAccessCheckAndAuditAlarm | 11 | Security | — |
| 4 | NtAccessCheckByType | 11 | Security | — |
| 5 | NtAccessCheckByTypeAndAuditAlarm | 16 | Security | — |
| 6 | NtAccessCheckByTypeResultList | 11 | Security | — |
| 7 | NtAccessCheckByTypeResultListAndAuditAlarm | 16 | Security | — |
| 8 | NtAccessCheckByTypeResultListAndAuditAlarmByHandle | 17 | Security | — |
| 9 | NtAddAtom | 3 | Atom | — |
| 10 | NtAddBootEntry | 2 | Boot | — |
| 11 | NtAddDriverEntry | 2 | Boot | — |
| 12 | NtAdjustGroupsToken | 6 | Token | — |
| 13 | NtAdjustPrivilegesToken | 6 | Token | — |
| 14 | NtAlertResumeThread | 2 | Thread | — |
| 15 | NtAlertThread | 1 | Thread | — |
| 16 | NtAllocateLocallyUniqueId | 1 | UniqueID | — |
| 17 | NtAllocateUserPhysicalPages | 3 | Memory | vsl_sys_mmap |
| 18 | NtAllocateUuids | 4 | RPC | — |
| 19 | NtAllocateVirtualMemory | 6 | Memory | **vsl_sys_mmap** ✓ |
| 20 | NtApphelpCacheControl | 2 | AppCompat | — |
| 21 | NtAreMappedFilesTheSame | 2 | Memory | — |
| 22 | NtAssignProcessToJobObject | 2 | Job | — |
| 23 | NtCallbackReturn | 3 | Callback | — |
| 24 | NtCancelDeviceWakeupRequest | 1 | Power | — |
| 25 | NtCancelIoFile | 2 | I/O | vsl_sys_io_cancel |
| 26 | NtCancelTimer | 2 | Timer | vsl_sys_timer_delete |
| 27 | NtClearEvent | 1 | Event | vsl_sys_event_clear |
| 28 | NtClose | 1 | Handle | **vsl_sys_close** ✓ |
| 29 | NtCloseObjectAuditAlarm | 3 | Security | — |
| 30 | NtCompactKeys | 2 | Registry | — |
| 31 | NtCompareTokens | 3 | Token | — |
| 32 | NtCompleteConnectPort | 1 | LPC | — |
| 33 | NtCompressKey | 1 | Registry | — |
| 34 | NtConnectPort | 8 | LPC | vsl_sys_connect |
| 35 | NtContinue | 2 | Exception | — |
| 36 | NtCreateDebugObject | 4 | Debug | — |
| 37 | NtCreateDirectoryObject | 3 | Object | vsl_sys_mkdir |
| 38 | NtCreateEvent | 5 | Event | vsl_sys_eventfd |
| 39 | NtCreateEventPair | 3 | EventPair | — |
| 40 | NtCreateFile | 11 | **File I/O** | **vsl_sys_open** ✓ |
| 41 | NtCreateIoCompletion | 4 | I/O Completion | vsl_sys_io_uring / epoll |
| 42 | NtCreateJobObject | 3 | Job | — |
| 43 | NtCreateJobSet | 3 | Job | — |
| 44 | NtCreateKey | 7 | Registry | vsl_sys_openat (Styx) |
| 45 | NtCreateMailslotFile | 8 | Mailslot | — |
| 46 | NtCreateMutant | 4 | Mutex | vsl_sys_futex |
| 47 | NtCreateNamedPipeFile | 14 | Pipe | vsl_sys_pipe |
| 48 | NtCreatePagingFile | 4 | Memory | — |
| 49 | NtCreatePort | 5 | LPC | vsl_sys_socketpair |
| 50 | NtCreateProcess | 8 | **Process** | **vsl_sys_fork + vsl_sys_execve** ✓ |
| 51 | NtCreateProcessEx | 9 | **Process** | **vsl_sys_clone + vsl_sys_execve** ✓ |
| 52 | NtCreateProfile | 9 | Profile | — |
| 53 | NtCreateSection | 7 | Section | **vsl_sys_mmap** ✓ |
| 54 | NtCreateSemaphore | 5 | Semaphore | vsl_sys_futex |
| 55 | NtCreateSymbolicLinkObject | 4 | Object | vsl_sys_symlink |
| 56 | NtCreateThread | 8 | **Thread** | **vsl_sys_clone** ✓ |
| 57 | NtCreateTimer | 4 | Timer | vsl_sys_timerfd |
| 58 | NtCreateToken | 13 | Token | — |
| 59 | NtCreateWaitablePort | 5 | LPC | — |
| 60 | NtDebugActiveProcess | 2 | Debug | — |
| 61 | NtDebugContinue | 3 | Debug | — |
| 62 | NtDelayExecution | 2 | Time | **vsl_sys_nanosleep** ✓ |
| 63 | NtDeleteAtom | 1 | Atom | — |
| 64 | NtDeleteBootEntry | 1 | Boot | — |
| 65 | NtDeleteDriverEntry | 1 | Boot | — |
| 66 | NtDeleteFile | 1 | File | **vsl_sys_unlink** ✓ |
| 67 | NtDeleteKey | 1 | Registry | vsl_sys_unlink |
| 68 | NtDeleteObjectAuditAlarm | 3 | Security | — |
| 69 | NtDeleteValueKey | 2 | Registry | — |
| 70 | NtDeviceIoControlFile | 10 | I/O Control | **vsl_sys_ioctl** ✓ |
| 71 | NtDisplayString | 1 | Debug | — |
| 72 | NtDuplicateObject | 7 | Handle | **vsl_sys_dup / vsl_sys_dup2** ✓ |
| 73 | NtDuplicateToken | 6 | Token | — |
| 74 | NtEnumerateBootEntries | 2 | Boot | — |
| 75 | NtEnumerateDriverEntries | 2 | Boot | — |
| 76 | NtEnumerateKey | 6 | Registry | vsl_sys_getdents |
| 77 | NtEnumerateSystemEnvironmentValuesEx | 3 | Env | — |
| 78 | NtEnumerateValueKey | 6 | Registry | — |
| 79 | NtExtendSection | 2 | Section | vsl_sys_mremap |
| 80 | NtFilterToken | 6 | Token | — |
| 81 | NtFindAtom | 3 | Atom | — |
| 82 | NtFlushBuffersFile | 2 | File | **vsl_sys_fsync** ✓ |
| 83 | NtFlushInstructionCache | 3 | Memory | — |
| 84 | NtFlushKey | 1 | Registry | — |
| 85 | NtFlushVirtualMemory | 4 | Memory | vsl_sys_msync |
| 86 | NtFlushWriteBuffer | 0 | Memory | — |
| 87 | NtFreeUserPhysicalPages | 3 | Memory | vsl_sys_munmap |
| 88 | NtFreeVirtualMemory | 4 | Memory | **vsl_sys_munmap** ✓ |
| 89 | NtFsControlFile | 10 | FS Control | vsl_sys_ioctl |
| 90 | NtGetContextThread | 2 | Thread | — |
| 91 | NtGetDevicePowerState | 2 | Power | — |
| 92 | NtGetPlugPlayEvent | 4 | PnP | — |
| 93 | NtGetWriteWatch | 7 | Memory | — |
| 94 | NtImpersonateAnonymousToken | 1 | Security | — |
| 95 | NtImpersonateClientOfPort | 2 | LPC | — |
| 96 | NtImpersonateThread | 3 | Thread | — |
| 97 | NtInitializeRegistry | 1 | Registry | — |
| 98 | NtInitiatePowerAction | 4 | Power | — |
| 99 | NtIsProcessInJob | 2 | Job | — |
|100 | NtIsSystemResumeAutomatic | 0 | Power | — |
|101 | NtListenPort | 2 | LPC | vsl_sys_listen |
|102 | NtLoadDriver | 1 | Driver | — |
|103 | NtLoadKey | 2 | Registry | — |
|104 | NtLoadKey2 | 3 | Registry | — |
|105 | NtLoadKeyEx | 4 | Registry | — |
|106 | NtLockFile | 10 | File | vsl_sys_fcntl (F_SETLK) |
|107 | NtLockProductActivationKeys | 2 | License | — |
|108 | NtLockRegistryKey | 1 | Registry | — |
|109 | NtLockVirtualMemory | 4 | Memory | vsl_sys_mlock |
|110 | NtMakePermanentObject | 1 | Object | — |
|111 | NtMakeTemporaryObject | 1 | Object | — |
|112 | NtMapUserPhysicalPages | 3 | Memory | vsl_sys_mmap |
|113 | NtMapUserPhysicalPagesScatter | 3 | Memory | — |
|114 | NtMapViewOfSection | 10 | Section | **vsl_sys_mmap** ✓ |
|115 | NtModifyBootEntry | 1 | Boot | — |
|116 | NtModifyDriverEntry | 1 | Boot | — |
|117 | NtNotifyChangeDirectoryFile | 9 | File Notify | **vsl_sys_inotify** ✓ |
|118 | NtNotifyChangeKey | 10 | Registry | — |
|119 | NtNotifyChangeMultipleKeys | 12 | Registry | — |
|120 | NtOpenDirectoryObject | 3 | Object | vsl_sys_openat |
|121 | NtOpenEvent | 3 | Event | vsl_sys_open |
|122 | NtOpenEventPair | 3 | EventPair | — |
|122 | NtOpenFile | 6 | **File** | **vsl_sys_open** ✓ |
|124 | NtOpenIoCompletion | 3 | I/O Completion | — |
|125 | NtOpenJobObject | 3 | Job | — |
|126 | NtOpenKey | 3 | Registry | vsl_sys_openat |
|127 | NtOpenMutant | 3 | Mutex | vsl_sys_open (futex) |
|128 | NtOpenObjectAuditAlarm | 12 | Security | — |
|129 | NtOpenProcess | 4 | **Process** | vsl_sys_open / procfs |
|130 | NtOpenProcessToken | 3 | Token | — |
|131 | NtOpenProcessTokenEx | 4 | Token | — |
|132 | NtOpenSection | 3 | Section | vsl_sys_open |
|133 | NtOpenSemaphore | 3 | Semaphore | vsl_sys_open (futex) |
|134 | NtOpenSymbolicLinkObject | 3 | Object | vsl_sys_readlink |
|135 | NtOpenThread | 4 | Thread | — |
|136 | NtOpenThreadToken | 4 | Token | — |
|137 | NtOpenThreadTokenEx | 5 | Token | — |
|138 | NtOpenTimer | 3 | Timer | vsl_sys_timerfd |
|139 | NtPlugPlayControl | 3 | PnP | — |
|140 | NtPowerInformation | 5 | Power | — |
|141 | NtPrivilegeCheck | 3 | Security | — |
|142 | NtPrivilegeObjectAuditAlarm | 6 | Security | — |
|143 | NtPrivilegedServiceAuditAlarm | 5 | Security | — |
|144 | NtProtectVirtualMemory | 5 | Memory | **vsl_sys_mprotect** ✓ |
|145 | NtPulseEvent | 2 | Event | vsl_sys_eventfd_write |
|146 | NtQueryAttributesFile | 2 | File | **vsl_sys_stat** ✓ |
|147 | NtQueryBootEntryOrder | 2 | Boot | — |
|148 | NtQueryBootOptions | 2 | Boot | — |
|149 | NtQueryDebugFilterState | 2 | Debug | — |
|150 | NtQueryDefaultLocale | 2 | Locale | — |
|151 | NtQueryDefaultUILanguage | 1 | Locale | — |
|152 | NtQueryDirectoryFile | 11 | File | **vsl_sys_getdents** ✓ |
|153 | NtQueryDirectoryObject | 7 | Object | — |
|154 | NtQueryDriverEntryOrder | 2 | Boot | — |
|155 | NtQueryEaFile | 9 | File | vsl_sys_getxattr |
|156 | NtQueryEvent | 5 | Event | — |
|157 | NtQueryFullAttributesFile | 2 | File | **vsl_sys_stat** ✓ |
|158 | NtQueryInformationAtom | 5 | Atom | — |
|159 | NtQueryInformationFile | 5 | File | vsl_sys_ioctl / fstat |
|160 | NtQueryInformationJobObject | 5 | Job | — |
|161 | NtQueryInformationPort | 5 | LPC | — |
|162 | NtQueryInformationProcess | 5 | **Process** | **vsl_sys_proc_info** ✓ |
|163 | NtQueryInformationThread | 5 | Thread | — |
|164 | NtQueryInformationToken | 5 | Token | — |
|165 | NtQueryInstallUILanguage | 1 | Locale | — |
|166 | NtQueryIntervalProfile | 2 | Profile | — |
|167 | NtQueryIoCompletion | 5 | I/O Completion | — |
|168 | NtQueryKey | 5 | Registry | vsl_sys_getdents |
|169 | NtQueryMultipleValueKey | 6 | Registry | — |
|170 | NtQueryMutant | 5 | Mutex | — |
|171 | NtQueryObject | 5 | Object | — |
|172 | NtQueryOpenSubKeys | 2 | Registry | — |
|173 | NtQueryOpenSubKeysEx | 4 | Registry | — |
|174 | NtQueryPerformanceCounter | 2 | Time | **vsl_sys_clock_gettime** ✓ |
|175 | NtQueryQuotaInformationFile | 9 | File | — |
|176 | NtQuerySection | 5 | Section | — |
|177 | NtQuerySecurityObject | 5 | Security | — |
|178 | NtQuerySemaphore | 5 | Semaphore | — |
|179 | NtQuerySymbolicLinkObject | 3 | Object | vsl_sys_readlink |
|180 | NtQuerySystemEnvironmentValue | 4 | Env | — |
|181 | NtQuerySystemEnvironmentValueEx | 5 | Env | — |
|182 | NtQuerySystemInformation | 4 | System | **vsl_sys_sysinfo** ✓ |
|183 | NtQuerySystemTime | 1 | Time | **vsl_sys_clock_gettime** ✓ |
|184 | NtQueryTimer | 5 | Timer | — |
|185 | NtQueryTimerResolution | 3 | Timer | — |
|186 | NtQueryValueKey | 6 | Registry | — |
|187 | NtQueryVirtualMemory | 6 | Memory | **vsl_sys_proc_vm_info** ✓ |
|188 | NtQueryVolumeInformationFile | 5 | File | vsl_sys_statfs |
|189 | NtQueueApcThread | 5 | APC | — |
|190 | NtRaiseException | 3 | Exception | — |
|191 | NtRaiseHardError | 6 | Error | — |
|192 | NtReadFile | 9 | **File Read** | **vsl_sys_read** ✓ |
|193 | NtReadFileScatter | 9 | File Read | vsl_sys_readv |
|194 | NtReadRequestData | 6 | LPC | — |
|195 | NtReadVirtualMemory | 5 | Memory | vsl_sys_process_vm_readv |
|196 | NtRegisterThreadTerminatePort | 1 | Thread | — |
|197 | NtReleaseMutant | 2 | Mutex | vsl_sys_futex (FUTEX_UNLOCK) |
|198 | NtReleaseSemaphore | 3 | Semaphore | vsl_sys_futex |
|199 | NtRemoveIoCompletion | 5 | I/O Completion | vsl_sys_epoll_wait |
|200 | NtRemoveProcessDebug | 2 | Debug | — |
|201 | NtRenameKey | 2 | Registry | vsl_sys_rename |
|202 | NtReplaceKey | 3 | Registry | — |
|203 | NtReplyPort | 2 | LPC | — |
|204 | NtReplyWaitReceivePort | 4 | LPC | — |
|205 | NtReplyWaitReceivePortEx | 5 | LPC | — |
|206 | NtReplyWaitReplyPort | 2 | LPC | — |
|207 | NtRequestDeviceWakeup | 1 | Power | — |
|208 | NtRequestPort | 2 | LPC | — |
|209 | NtRequestWaitReplyPort | 3 | LPC | — |
|210 | NtRequestWakeupLatency | 1 | Power | — |
|211 | NtResetEvent | 2 | Event | vsl_sys_eventfd |
|212 | NtResetWriteWatch | 3 | Memory | — |
|213 | NtRestoreKey | 3 | Registry | — |
|214 | NtResumeProcess | 1 | Process | vsl_sys_kill (SIGCONT) |
|215 | NtResumeThread | 2 | Thread | vsl_sys_kill (SIGCONT) |
|216 | NtSaveKey | 2 | Registry | — |
|217 | NtSaveKeyEx | 3 | Registry | — |
|218 | NtSaveMergedKeys | 3 | Registry | — |
|219 | NtSecureConnectPort | 9 | LPC | — |
|220 | NtSetBootEntryOrder | 2 | Boot | — |
|221 | NtSetBootOptions | 2 | Boot | — |
|222 | NtSetContextThread | 2 | Thread | — |
|223 | NtSetDebugFilterState | 3 | Debug | — |
|224 | NtSetDefaultHardErrorPort | 2 | Locale | — |
|225 | NtSetDefaultLocale | 2 | Locale | — |
|226 | NtSetDefaultUILanguage | 1 | Locale | — |
|227 | NtSetDriverEntryOrder | 2 | Boot | — |
|228 | NtSetEaFile | 4 | File | vsl_sys_setxattr |
|229 | NtSetEvent | 2 | Event | vsl_sys_eventfd_write |
|230 | NtSetEventBoostPriority | 1 | Event | — |
|231 | NtSetHighEventPair | 1 | EventPair | — |
|232 | NtSetHighWaitLowEventPair | 1 | EventPair | — |
|233 | NtSetInformationDebugObject | 5 | Debug | — |
|234 | NtSetInformationFile | 5 | File | vsl_sys_fcntl / ioctl |
|235 | NtSetInformationJobObject | 4 | Job | — |
|236 | NtSetInformationKey | 4 | Registry | — |
|237 | NtSetInformationObject | 4 | Object | — |
|238 | NtSetInformationProcess | 4 | Process | vsl_sys_prctl |
|239 | NtSetInformationThread | 4 | Thread | — |
|240 | NtSetInformationToken | 4 | Token | — |
|241 | NtSetIntervalProfile | 2 | Profile | — |
|242 | NtSetIoCompletion | 5 | I/O Completion | vsl_sys_epoll_ctl |
|243 | NtSetLdtEntries | 6 | LDT | — |
|244 | NtSetLowEventPair | 1 | EventPair | — |
|245 | NtSetLowWaitHighEventPair | 1 | EventPair | — |
|246 | NtSetQuotaInformationFile | 4 | File | — |
|247 | NtSetSecurityObject | 3 | Security | — |
|248 | NtSetSystemEnvironmentValue | 2 | Env | — |
|249 | NtSetSystemEnvironmentValueEx | 5 | Env | — |
|250 | NtSetSystemInformation | 3 | System | — |
|251 | NtSetSystemPowerState | 3 | Power | — |
|252 | NtSetSystemTime | 2 | Time | vsl_sys_clock_settime |
|253 | NtSetThreadExecutionState | 2 | Thread | — |
|254 | NtSetTimer | 7 | Timer | vsl_sys_timerfd_settime |
|255 | NtSetTimerResolution | 3 | Timer | — |
|256 | NtSetUuidSeed | 1 | UUID | — |
|257 | NtSetValueKey | 6 | Registry | — |
|258 | NtSetVolumeInformationFile | 5 | File | — |
|259 | NtShutdownSystem | 1 | Power | — |
|260 | NtSignalAndWaitForSingleObject | 4 | Sync | vsl_sys_futex_wait |
|261 | NtStartProfile | 1 | Profile | — |
|262 | NtStopProfile | 1 | Profile | — |
|263 | NtSuspendProcess | 1 | Process | vsl_sys_kill (SIGSTOP) |
|264 | NtSuspendThread | 2 | Thread | vsl_sys_kill (SIGSTOP) |
|265 | NtSystemDebugControl | 6 | Debug | — |
|266 | NtTerminateJobObject | 2 | Job | — |
|267 | NtTerminateProcess | 2 | **Process** | **vsl_sys_kill** ✓ |
|268 | NtTerminateThread | 2 | Thread | vsl_sys_tgkill |
|269 | NtTestAlert | 0 | Alert | — |
|270 | NtTraceEvent | 4 | ETW | — |
|271 | NtTranslateFilePath | 4 | File | — |
|272 | NtUnloadDriver | 1 | Driver | — |
|273 | NtUnloadKey | 1 | Registry | — |
|274 | NtUnloadKey2 | 2 | Registry | — |
|275 | NtUnloadKeyEx | 2 | Registry | — |
|276 | NtUnlockFile | 5 | File | vsl_sys_fcntl (F_SETLK) |
|277 | NtUnlockVirtualMemory | 4 | Memory | vsl_sys_munlock |
|278 | NtUnmapViewOfSection | 2 | Section | **vsl_sys_munmap** ✓ |
|279 | NtVdmControl | 2 | VDM | — |
|280 | NtWaitForDebugEvent | 4 | Debug | — |
|281 | NtWaitForMultipleObjects | 5 | Sync | **vsl_sys_poll/epoll** ✓ |
|282 | NtWaitForSingleObject | 3 | Sync | **vsl_sys_poll/futex** ✓ |
|283 | NtWaitHighEventPair | 1 | EventPair | — |
|284 | NtWaitLowEventPair | 1 | EventPair | — |
|285 | NtWriteFile | 9 | **File Write** | **vsl_sys_write** ✓ |
|286 | NtWriteFileGather | 9 | File Write | vsl_sys_writev |
|287 | NtWriteRequestData | 6 | LPC | — |
|288 | NtWriteVirtualMemory | 5 | Memory | vsl_sys_process_vm_writev |
|289 | NtYieldExecution | 0 | Thread | **vsl_sys_sched_yield** ✓ |
|290 | NtCreateKeyedEvent | 4 | KeyedEvent | vsl_sys_futex |
|291 | NtOpenKeyedEvent | 3 | KeyedEvent | vsl_sys_futex |
|292 | NtReleaseKeyedEvent | 4 | KeyedEvent | vsl_sys_futex |
|293 | NtWaitForKeyedEvent | 4 | KeyedEvent | vsl_sys_futex_wait |
|294 | NtQueryPortInformationProcess | 0 | LPC | — |
|295 | NtGetCurrentProcessorNumber | 0 | CPU | vsl_sys_getcpu |
|296 | NtWaitForMultipleObjects32 | 5 | Sync | vsl_sys_poll |

---

## WuBuOS VSL Syscall Mapping Status

### ✅ Implemented (17 syscalls with full delegation)

| VSL Syscall | Linux Syscall | NT Syscall Coverage |
|-------------|---------------|---------------------|
| vsl_sys_exit | exit_group | NtTerminateProcess |
| vsl_sys_getpid | getpid | NtQueryInformationProcess |
| vsl_sys_fork | clone | NtCreateProcess/NtCreateProcessEx |
| vsl_sys_clone | clone | NtCreateThread |
| vsl_sys_vfork | clone(CLONE_VM\|CLONE_VFORK) | — |
| vsl_sys_execve | execve | NtCreateProcessEx (via CreateProcess) |
| vsl_sys_wait4 | wait4 | NtWaitForSingleObject |
| vsl_sys_kill | kill | NtTerminateProcess/NtTerminateThread |
| vsl_sys_read | read | NtReadFile |
| vsl_sys_write | write | NtWriteFile |
| vsl_sys_open | openat | NtCreateFile/NtOpenFile |
| vsl_sys_close | close | NtClose |
| vsl_sys_lseek | lseek | NtSetInformationFile |
| vsl_sys_fstat | fstat | NtQueryInformationFile |
| vsl_sys_stat | stat | NtQueryAttributesFile |
| vsl_sys_ioctl | ioctl | NtDeviceIoControlFile/NtFsControlFile |
| vsl_sys_mmap | mmap | NtAllocateVirtualMemory/NtMapViewOfSection |
| vsl_sys_munmap | munmap | NtFreeVirtualMemory/NtUnmapViewOfSection |
| vsl_sys_mprotect | mprotect | NtProtectVirtualMemory |
| vsl_sys_fsync | fsync | NtFlushBuffersFile |
| vsl_sys_getdents | getdents64 | NtQueryDirectoryFile |
| vsl_sys_pipe | pipe2 | NtCreateNamedPipeFile |
| vsl_sys_dup | dup | NtDuplicateObject |
| vsl_sys_dup2 | dup2 | NtDuplicateObject |
| vsl_sys_fcntl | fcntl | NtLockFile/NtSetInformationFile |
| vsl_sys_socket | socket | NtCreateFile (AF_*) |
| vsl_sys_connect | connect | NtConnectPort |
| vsl_sys_bind | bind | — |
| vsl_sys_listen | listen | NtListenPort |
| vsl_sys_accept | accept | — |
| vsl_sys_sendto | sendto | NtWriteFile |
| vsl_sys_recvfrom | recvfrom | NtReadFile |
| vsl_sys_poll | ppoll | NtWaitForMultipleObjects |
| vsl_sys_epoll_create | epoll_create1 | NtCreateIoCompletion |
| vsl_sys_epoll_ctl | epoll_ctl | NtSetIoCompletion |
| vsl_sys_epoll_wait | epoll_wait | NtRemoveIoCompletion |
| vsl_sys_nanosleep | clock_nanosleep | NtDelayExecution |
| vsl_sys_clock_gettime | clock_gettime | NtQuerySystemTime/NtQueryPerformanceCounter |
| vsl_sys_timerfd_create | timerfd_create | NtCreateTimer |
| vsl_sys_timerfd_settime | timerfd_settime | NtSetTimer |
| vsl_sys_eventfd | eventfd2 | NtCreateEvent |
| vsl_sys_inotify_init | inotify_init1 | NtNotifyChangeDirectoryFile |
| vsl_sys_inotify_add_watch | inotify_add_watch | NtNotifyChangeDirectoryFile |
| vsl_sys_sched_yield | sched_yield | NtYieldExecution |
| vsl_sys_futex | futex | NtCreateMutant/NtWaitForSingleObject |
| vsl_sys_sigaction | rt_sigaction | NtSignalAndWaitForSingleObject |
| vsl_sys_sigprocmask | rt_sigprocmask | — |
| vsl_sys_rt_sigreturn | rt_sigreturn | — |

### 🔄 Partial / Planned

| VSL Syscall | Status | NT Target |
|-------------|--------|-----------|
| vsl_sys_mremap | Planned | NtExtendSection |
| vsl_sys_msync | Planned | NtFlushVirtualMemory |
| vsl_sys_mlock | Planned | NtLockVirtualMemory |
| vsl_sys_munlock | Planned | NtUnlockVirtualMemory |
| vsl_sys_getxattr | Planned | NtQueryEaFile |
| vsl_sys_setxattr | Planned | NtSetEaFile |
| vsl_sys_statfs | Planned | NtQueryVolumeInformationFile |
| vsl_sys_getcpu | Planned | NtGetCurrentProcessorNumber |
| vsl_sys_proc_info | Partial | NtQueryInformationProcess |
| vsl_sys_proc_vm_info | Partial | NtQueryVirtualMemory |

---

## WuBuOS Architecture Integration

### Layer Stack (TempleOS → Windows/Linux)

```
┌─────────────────────────────────────────────────────────────┐
│  TempleOS / HolyC Applications                              │
│  (ZealOS kernel personality - single user, ring-0, JIT)    │
├─────────────────────────────────────────────────────────────┤
│  Styx/9P Namespace                                          │
│  (Plan 9 file protocol - everything is a file)             │
├─────────────────────────────────────────────────────────────┤
│  VSL (Virtual Syscall Layer)                                │
│  NT syscall → Linux syscall translation                     │
│  Host delegation via syscall() / ioctl() / netlink         │
├─────────────────────────────────────────────────────────────┤
│  SteamOS / Arch Linux Host                                  │
│  NVK (NVIDIA Vulkan driver) + Proton/Wine                  │
│  Kernel 6.x + systemd + bubblewrap                         │
└─────────────────────────────────────────────────────────────┘
```

### Translation Path: NT → VSL → Linux

```
NtCreateFile (11 args)
    → vsl_sys_open(path, flags, mode)
    → openat(AT_FDCWD, path, flags, mode)
    → Returns host fd → maps to VSL_FD → returns VSL handle

NtReadFile (9 args)
    → vsl_sys_read(fd, buf, count)
    → read(fd, buf, count)
    → Returns bytes read / -errno

NtAllocateVirtualMemory (6 args)
    → vsl_sys_mmap(addr, size, prot, flags, fd, offset)
    → mmap(addr, size, prot, flags, fd, offset)
    → Returns mapped address

NtCreateProcess / NtCreateProcessEx (8/9 args)
    → vsl_sys_fork() → vsl_sys_execve(path, argv, envp)
    → fork() → execve()
    → PID mapping: host_pid ↔ vsl_pid
```

### ReactOS Study Value for WuBuOS

1. **SSDT Structure**: Clean function pointer table + arg count array → Direct model for VSL dispatch table
2. **Stub Generation**: Macro-based asm stubs → Template for VSL user-mode trampolines
3. **Argument Copying**: Fall-through switch for 0-16 args → VSL 6-register convention (a-f)
4. **Service Tables**: Core + Win32k separation → VSL: Core + Styx/9P + HolyC JIT tables
5. **Error Handling**: NTSTATUS ↔ errno mapping → VSL: -errno return convention
6. **Previous Mode**: KeGetPreviousMode() for user/kernel probe → VSL: validate pointers in handler
7. **GUI Thread Conversion**: KiConvertToGuiThread lazy init → VSL: HolyC session lazy init

---

## Implementation Priorities (From BATTLESHIP.md)

### Critical Tier (Active Workstreams)

| # | Workstream | ReactOS Reference | Status |
|---|------------|-------------------|--------|
| 1 | VSL Syscall Expansion | ntoskrnl/sysfuncs.lst (297) | 17/297 mapped |
| 2 | NT→VSL Argument Translation | KiSystemCallHandler arg copy | Need full mapping |
| 3 | Process/Thread Parity | NtCreateProcess/Thread | Partial |
| 4 | File I/O Parity | NtCreateFile/ReadFile/WriteFile | ✅ Core done |
| 5 | Memory Parity | NtAllocate/Free/ProtectVirtualMemory | ✅ Core done |
| 6 | Sync Primitives | NtCreateMutant/Event/Semaphore | Futex-based |
| 7 | Registry → Styx Mapping | NtCreateKey/OpenKey/QueryKey | Styx/9P namespace |

### Devil's Advocate Parity Audit (2026-07-04)

**New Rule**: EVERY parity gap with SteamOS/Ubuntu/Arch/TempleOS/ZealOS = REAL_GAP

ReactOS provides the **NT syscall specification** (297 calls) that Windows apps expect.
WuBuOS VSL must implement the **translation layer** for .wubu containers running Win32 apps via Proton/Wine.

---

## Code Patterns to Adopt from ReactOS

### 1. Syscall Table Generation (napi.h)
```c
// WuBuOS equivalent: vsl_syscall_table.h
#define VSL_SYSCALL(name, nargs) (int64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))vsl_sys_##name,
static vsl_syscall_fn_t vsl_syscall_table[] = {
#include "vsl_syscall_list.h"
};

#define VSL_SYSCALL(name, nargs) nargs,
static uint8_t vsl_syscall_nargs[] = {
#include "vsl_syscall_list.h"
};
```

### 2. User-Mode Trampoline (ntdll.S pattern)
```asm
; WuBuOS: vsl_user_trampoline.S (x86-64)
.macro VSL_STUB name, num
.global vsl_\name
vsl_\name:
    mov eax, \num          ; syscall number
    mov r10, rcx           ; Linux syscall uses r10 not rcx
    syscall
    ret
.endm

VSL_STUB open, 0
VSL_STUB read, 1
VSL_STUB write, 2
; ... generated from vsl_syscall_list.h
```

### 3. Kernel Entry Handler (traphandler.c pattern)
```c
// WuBuOS: vsl_syscall_entry.c
int64_t vsl_syscall_handler(uint64_t a, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    uint32_t sysnum = (uint32_t)a;  // Syscall number in first arg
    if (sysnum >= VSL_SYSCALL_COUNT) return -ENOSYS;
    
    // Validate args based on vsl_syscall_nargs[sysnum]
    // Call vsl_syscall_table[sysnum](b, c, d, e, f, 0)
    return vsl_syscall_table[sysnum](b, c, d, e, f, 0);
}
```

### 4. NTSTATUS ↔ errno Mapping
```c
// ReactOS: STATUS_SUCCESS=0, errno=0
// WuBuOS: Return -errno on error, positive on success
static int ntstatus_to_errno(NTSTATUS status) {
    switch (status) {
        case STATUS_SUCCESS: return 0;
        case STATUS_ACCESS_DENIED: return -EACCES;
        case STATUS_OBJECT_NAME_NOT_FOUND: return -ENOENT;
        case STATUS_INSUFFICIENT_RESOURCES: return -ENOMEM;
        case STATUS_INVALID_HANDLE: return -EBADF;
        // ... map all common codes
        default: return -EINVAL;
    }
}
```

---

## Win32 API → NT Syscall → VSL Mapping (Proton/Wine Layer)

```
Win32 API (kernel32.dll)
    │
    ▼
NTDLL (ntdll.dll) - user-mode) ──syscall──▶ NT Kernel
    │
    ▼
Wine/Proton: NTDLL → VSL Translation
    │
    ▼
WuBuOS VSL (vsl_sys_*) ──syscall()──▶ Linux Kernel
    │
    ▼
SteamOS/Arch Host (NVK, Proton, bubblewrap)
```

### Example: CreateFileW → NtCreateFile → vsl_sys_open

```c
// Wine's kernel32_CreateFileW
HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess,
                   DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile) {
    // Convert to NT
    NTSTATUS status = NtCreateFile(&handle, access, &objAttr, &iosb,
                                   &allocSize, attrs, share, disp, options,
                                   eaBuffer, eaLength);
    return handle;
}

// WuBuOS VSL translation in wubu_proton.c
NTSTATUS vsl_NtCreateFile(PHANDLE handle, ACCESS_MASK access, ...) {
    int flags = nt_access_to_linux_flags(access, share, disp, options);
    int mode = nt_attrs_to_linux_mode(attrs);
    int64_t vsl_fd = vsl_sys_open(path, flags, mode);
    if (vsl_fd < 0) return ntstatus_from_errno(vsl_fd);
    *handle = vsl_fd_to_handle(vsl_fd);
    return STATUS_SUCCESS;
}
```

---

## Files to Create / Extend

| File | Purpose | ReactOS Reference |
|------|---------|-------------------|
| `src/runtime/vsl/vsl_syscall_list.h` | Master syscall list (generate table) | `ntoskrnl/include/sysfuncs.h` |
| `src/runtime/vsl/vsl_syscall_table.c` | Dispatch table + arg counts | `ntoskrnl/include/internal/napi.h` |
| `src/runtime/vsl/vsl_user_trampoline.S` | User-mode syscall stubs | `ntoskrnl/ntdll.S` + `syscalls.inc` |
| `src/runtime/vsl/vsl_nt_mapping.c` | NT syscall → VSL translation | `ntoskrnl/io/iomgr/file.c` etc. |
| `src/runtime/wubu_proton.c` | Win32 API → VSL (PE loader) | Wine's NTDLL implementation |
| `src/bridge/wubu_syscall.c` | Syscall bridge handlers | `ntoskrnl/ke/amd64/traphandler.c` |

---

## Next Steps (Execute-First)

1. **Create vsl_syscall_list.h** from ReactOS sysfuncs.lst (297 entries)
2. **Generate vsl_syscall_table.c** with all 297 entries (stubs → real impl)
3. **Implement missing VSL syscalls** prioritized by Wine/Proton needs
4. **Add NTSTATUS ↔ errno mapping** table
5. **Build vsl_user_trampoline.S** for x86-64 (and aarch64 for Steam Deck)
6. **Test with Wine/Proton** simple Win32 app (notepad, solitaire)

---

## References

- ReactOS Source: `/home/wubu/reactos-study/reactos/`
- `ntoskrnl/include/sysfuncs.h` - 297 syscall definitions
- `ntoskrnl/include/internal/napi.h` - Generated SSDT
- `ntoskrnl/ke/amd64/traphandler.c` - Syscall entry handler
- `ntoskrnl/ntdll.S` - User stubs
- `sdk/include/asm/syscalls.inc` - Stub macros
- WuBuOS VSL: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/src/runtime/vsl/`
- BATTLESHIP.md v16: 1562 REAL_GAPs, Devil's Advocate parity audit

---

*Generated: 2026-07-04 | ReactOS commit: latest main | WuBuOS: mind-palace profile*