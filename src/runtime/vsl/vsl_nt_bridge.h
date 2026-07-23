/*
 * vsl_nt_bridge.h  --  WuBuOS NT Syscall → VSL Transliteration Layer
 *
 * Maps ReactOS NT syscall semantics (0x0-0x400+) to WuBuOS VSL syscall bridge.
 * Pipeline: NT syscall → VSL syscall → Styx9/9P → ZealOS kernel → TempleOS layer
 *
 * Source study: reactos-study/reactos/ntoskrnl/ + dll/ntdll/
 */

#ifndef WUBU_VSL_NT_BRIDGE_H
#define WUBU_VSL_NT_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "wubu_vsl.h"

/* ========================================================================
 * NT Status Codes (subset - maps to VSL errno)
 * ======================================================================== */
#define NT_STATUS_SUCCESS                0x00000000
#define NT_STATUS_BUFFER_TOO_SMALL       0xC0000023
#define NT_STATUS_INFO_LENGTH_MISMATCH   0xC0000004
#define NT_STATUS_ACCESS_DENIED          0xC0000022
#define NT_STATUS_INVALID_PARAMETER      0xC000000D
#define NT_STATUS_NO_MEMORY              0xC0000017
#define NT_STATUS_OBJECT_NAME_NOT_FOUND  0xC0000034
#define NT_STATUS_INSUFFICIENT_RESOURCES 0xC000009A
#define NT_STATUS_NOT_SAME_DEVICE        0xC00000D4
#define NT_STATUS_OBJECT_NAME_COLLISION  0xC0000035
#define NT_STATUS_FILE_NOT_FOUND         0xC000000F
#define NT_STATUS_NO_SUCH_FILE           0xC000000F
#define NT_STATUS_INVALID_HANDLE         0xC0000008
#define NT_STATUS_UNSUCCESSFUL           0xC0000001
#define NT_STATUS_NOT_IMPLEMENTED        0xC0000002
#define NT_STATUS_INVALID_DEVICE_REQUEST 0xC0000010
#define NT_STATUS_END_OF_FILE            0xC0000011
#define NT_STATUS_PENDING                0x00000103
#define NT_STATUS_MORE_ENTRIES           0x00000105
#define NT_STATUS_TIMEOUT                0x00000102
#define NT_STATUS_WAIT_0                 0x00000000
#define NT_STATUS_ALERTED                0x00000101
#define NT_STATUS_NO_MORE_FILES          0x8000001A
#define NT_STATUS_BUFFER_OVERFLOW        0x80000005

/* ========================================================================
 * NT System Call Numbers (from sysfuncs.lst - 297 syscalls)
 * ======================================================================== */

/* Process/Thread Management */
#define NT_SYSCALL_NTCREATEPROCESS           0x0030  /* NtCreateProcess */
#define NT_SYSCALL_NTCREATEPROCESSEX         0x0031  /* NtCreateProcessEx */
#define NT_SYSCALL_NTCREATETHREAD            0x0036  /* NtCreateThread */
#define NT_SYSCALL_NTCREATETHREADEX          0x00C5  /* NtCreateThreadEx (Vista+) */
#define NT_SYSCALL_NTOPENPROCESS             0x007D  /* NtOpenProcess */
#define NT_SYSCALL_NTOPENTHREAD              0x0085  /* NtOpenThread */
#define NT_SYSCALL_NTTERMINATEPROCESS        0x0108  /* NtTerminateProcess */
#define NT_SYSCALL_NTTERMINATETHREAD         0x0109  /* NtTerminateThread */
#define NT_SYSCALL_NTSUSPENDPROCESS          0x0105  /* NtSuspendProcess */
#define NT_SYSCALL_NTSUSPENDTHREAD           0x0106  /* NtSuspendThread */
#define NT_SYSCALL_NTRESUMEPROCESS           0x00D7  /* NtResumeProcess */
#define NT_SYSCALL_NTRESUMETHREAD            0x00D8  /* NtResumeThread */
#define NT_SYSCALL_NTGETCONTEXTTHREAD        0x005A  /* NtGetContextThread */
#define NT_SYSCALL_NTSETCONTEXTTHREAD        0x00DE  /* NtSetContextThread */
#define NT_SYSCALL_NTQUERYINFORMATIONPROCESS 0x00A2  /* NtQueryInformationProcess */
#define NT_SYSCALL_NTSETINFORMATIONPROCESS   0x00ED  /* NtSetInformationProcess */
#define NT_SYSCALL_NTQUERYINFORMATIONTHREAD  0x00A3  /* NtQueryInformationThread */
#define NT_SYSCALL_NTSETINFORMATIONTHREAD    0x00EE  /* NtSetInformationThread */
#define NT_SYSCALL_NTALERTTHREAD             0x000F  /* NtAlertThread */
#define NT_SYSCALL_NTALERTRESUMETHREAD       0x000E  /* NtAlertResumeThread */
#define NT_SYSCALL_NTTESTALERT               0x010E  /* NtTestAlert */
#define NT_SYSCALL_NTYIELDEXECUTION          0x0121  /* NtYieldExecution */

/* Memory Management */
#define NT_SYSCALL_NTALLOCATEVIRTUALMEMORY   0x0013  /* NtAllocateVirtualMemory */
#define NT_SYSCALL_NTFREEVIRTUALMEMORY       0x0058  /* NtFreeVirtualMemory */
#define NT_SYSCALL_NTRESETVIRTUALMEMORY      0x011B  /* NtResetVirtualMemory */
#define NT_SYSCALL_NTPROTECTVIRTUALMEMORY    0x0090  /* NtProtectVirtualMemory */
#define NT_SYSCALL_NTQUERYVIRTUALMEMORY      0x00BB  /* NtQueryVirtualMemory */
#define NT_SYSCALL_NTLOCKVIRTUALMEMORY       0x006D  /* NtLockVirtualMemory */
#define NT_SYSCALL_NTUNLOCKVIRTUALMEMORY     0x0119  /* NtUnlockVirtualMemory */
#define NT_SYSCALL_NTREADVIRTUALMEMORY       0x00C3  /* NtReadVirtualMemory */
#define NT_SYSCALL_NTWRITEVIRTUALMEMORY      0x0120  /* NtWriteVirtualMemory */
#define NT_SYSCALL_NTCREATESECTION           0x0035  /* NtCreateSection */
#define NT_SYSCALL_NTOPENSECTION             0x0082  /* NtOpenSection */
#define NT_SYSCALL_NTMAPVIEWOFSECTION        0x0072  /* NtMapViewOfSection */
#define NT_SYSCALL_NTUNMAPVIEWOFSECTION      0x0117  /* NtUnmapViewOfSection */
#define NT_SYSCALL_NTEXTENDSECTION           0x004F  /* NtExtendSection */
#define NT_SYSCALL_NTQUERYSECTION            0x00B6  /* NtQuerySection */
#define NT_SYSCALL_NTALLOCATEUSERPHYSICALPAGES 0x0011  /* NtAllocateUserPhysicalPages */
#define NT_SYSCALL_NTFREEUSERPHYSICALPAGES   0x0057  /* NtFreeUserPhysicalPages */
#define NT_SYSCALL_NTMAPUSERPHYSICALPAGES    0x0070  /* NtMapUserPhysicalPages */
#define NT_SYSCALL_NTMAPUSERPHYSICALPAGESSCATTER 0x0071  /* NtMapUserPhysicalPagesScatter */
#define NT_SYSCALL_NTGETWRITEWATCH           0x005E  /* NtGetWriteWatch */
#define NT_SYSCALL_NTRESETWRITEWATCH         0x00D2  /* NtResetWriteWatch */
#define NT_SYSCALL_NTFLUSHVIRTUALMEMORY      0x0055  /* NtFlushVirtualMemory */
#define NT_SYSCALL_NTFLUSHINSTRUCTIONCACHE   0x0053  /* NtFlushInstructionCache */

/* File/I/O Operations */
#define NT_SYSCALL_NTCREATEFILE              0x0028  /* NtCreateFile */
#define NT_SYSCALL_NTOPENFILE                0x007B  /* NtOpenFile */
#define NT_SYSCALL_NTCREATENAMEDPIPEFILE     0x002F  /* NtCreateNamedPipeFile */
#define NT_SYSCALL_NTCREATEMAILSLOTFILE      0x002D  /* NtCreateMailslotFile */
#define NT_SYSCALL_NTREADFILE                0x00C0  /* NtReadFile */
#define NT_SYSCALL_NTWRITEFILE               0x011D  /* NtWriteFile */
#define NT_SYSCALL_NTREADFILESCATTER         0x00C2  /* NtReadFileScatter */
#define NT_SYSCALL_NTWRITEFILEGATHER         0x011E  /* NtWriteFileGather */
#define NT_SYSCALL_NTDEVICEIOCONTROLFILE     0x0046  /* NtDeviceIoControlFile */
#define NT_SYSCALL_NTFSCTLFILE               0x0056  /* NtFsControlFile */
#define NT_SYSCALL_NTLOCKFILE                0x006A  /* NtLockFile */
#define NT_SYSCALL_NTUNLOCKFILE              0x011A  /* NtUnlockFile */
#define NT_SYSCALL_NTQUERYINFORMATIONFILE    0x009F  /* NtQueryInformationFile */
#define NT_SYSCALL_NTSETINFORMATIONFILE      0x00EA  /* NtSetInformationFile */
#define NT_SYSCALL_NTQUERYDIRECTORYFILE      0x0098  /* NtQueryDirectoryFile */
#define NT_SYSCALL_NTQUERYEAFILE             0x009B  /* NtQueryEaFile */
#define NT_SYSCALL_NTSETEAFILE               0x00E3  /* NtSetEaFile */
#define NT_SYSCALL_NTQUERYQUOTAINFORMATIONFILE 0x00AF  /* NtQueryQuotaInformationFile */
#define NT_SYSCALL_NTSETQUOTAINFORMATIONFILE 0x00F5  /* NtSetQuotaInformationFile */
#define NT_SYSCALL_NTQUERYVOLUMEINFORMATIONFILE 0x00B8  /* NtQueryVolumeInformationFile */
#define NT_SYSCALL_NTSETVOLUMEINFORMATIONFILE 0x00FE  /* NtSetVolumeInformationFile */
#define NT_SYSCALL_NTCLOSE                   0x001C  /* NtClose */
#define NT_SYSCALL_NTCANCELIOFILE            0x0018  /* NtCancelIoFile */
#define NT_SYSCALL_NTFLUSHBUFFERSFILE        0x0052  /* NtFlushBuffersFile */
#define NT_SYSCALL_NTQUERYATTRIBUTESFILE     0x0092  /* NtQueryAttributesFile */
#define NT_SYSCALL_NTQUERYFULLATTRIBUTESFILE 0x009D  /* NtQueryFullAttributesFile */
#define NT_SYSCALL_NTDELETEFILE              0x0042  /* NtDeleteFile */
#define NT_SYSCALL_NTREPLACEKEY              0x00CA  /* NtReplaceKey */
#define NT_SYSCALL_NTRESTOREKEY              0x00D0  /* NtRestoreKey */
#define NT_SYSCALL_NTSAVEKEY                 0x00D6  /* NtSaveKey */
#define NT_SYSCALL_NTSAVEKEYEX               0x00D7  /* NtSaveKeyEx */
#define NT_SYSCALL_NTSAVEMERGEDKEYS          0x00D8  /* NtSaveMergedKeys */
#define NT_SYSCALL_NTLOADKEY                 0x0066  /* NtLoadKey */
#define NT_SYSCALL_NTLOADKEY2                0x0067  /* NtLoadKey2 */
#define NT_SYSCALL_NTLOADKEYEX               0x0068  /* NtLoadKeyEx */
#define NT_SYSCALL_NTUNLOADKEY               0x0112  /* NtUnloadKey */
#define NT_SYSCALL_NTUNLOADKEY2              0x0113  /* NtUnloadKey2 */
#define NT_SYSCALL_NTUNLOADKEYEX             0x0114  /* NtUnloadKeyEx */
#define NT_SYSCALL_NTNOTIFYCHANGEDIRECTORYFILE 0x0075  /* NtNotifyChangeDirectoryFile */
#define NT_SYSCALL_NTNOTIFYCHANGEKEY         0x0076  /* NtNotifyChangeKey */
#define NT_SYSCALL_NTNOTIFYCHANGEMULTIPLEKEYS 0x0077  /* NtNotifyChangeMultipleKeys */

/* Synchronization Objects */
#define NT_SYSCALL_NTCREATEEVENT             0x0026  /* NtCreateEvent */
#define NT_SYSCALL_NTOPENEVENT               0x0079  /* NtOpenEvent */
#define NT_SYSCALL_NTPULSEEVENT              0x0091  /* NtPulseEvent */
#define NT_SYSCALL_NTSETEVENT                0x00E5  /* NtSetEvent */
#define NT_SYSCALL_NTRESETEVENT              0x00D1  /* NtResetEvent */
#define NT_SYSCALL_NTCREATEMUTANT            0x002E  /* NtCreateMutant */
#define NT_SYSCALL_NTOPENMUTANT              0x0087  /* NtOpenMutant */
#define NT_SYSCALL_NTRELEASEMUTANT           0x00C5  /* NtReleaseMutant */
#define NT_SYSCALL_NTCREATESEMAPHORE         0x0034  /* NtCreateSemaphore */
#define NT_SYSCALL_NTOPENSEMAPHORE           0x0083  /* NtOpenSemaphore */
#define NT_SYSCALL_NTRELEASESEMAPHORE        0x00C6  /* NtReleaseSemaphore */
#define NT_SYSCALL_NTQUERYSEMAPHORE          0x00AE  /* NtQuerySemaphore */
#define NT_SYSCALL_NTCREATETIMER             0x0037  /* NtCreateTimer */
#define NT_SYSCALL_NTOPENTIMER               0x0089  /* NtOpenTimer */
#define NT_SYSCALL_NTSETTIMER                0x00F6  /* NtSetTimer */
#define NT_SYSCALL_NTSETTIMERRESOLUTION      0x00F7  /* NtSetTimerResolution */
#define NT_SYSCALL_NTQUERYTIMER              0x00B9  /* NtQueryTimer */
#define NT_SYSCALL_NTQUERYTIMERRESOLUTION    0x00BA  /* NtQueryTimerResolution */
#define NT_SYSCALL_NTCANCELTIMER             0x0019  /* NtCancelTimer */
#define NT_SYSCALL_NTCREATEWAITABLEPORT      0x0039  /* NtCreateWaitablePort */
#define NT_SYSCALL_NTCREATEPORT              0x0038  /* NtCreatePort */
#define NT_SYSCALL_NTCONNECTPORT             0x0023  /* NtConnectPort */
#define NT_SYSCALL_NTLISTENPORT              0x0069  /* NtListenPort */
#define NT_SYSCALL_NTACCEPTCONNECTPORT       0x0001  /* NtAcceptConnectPort */
#define NT_SYSCALL_NTCOMPLETECONNECTPORT     0x0022  /* NtCompleteConnectPort */
#define NT_SYSCALL_NTREQUESTPORT             0x00C9  /* NtRequestPort */
#define NT_SYSCALL_NTREQUESTWAITREPLYPORT    0x00CA  /* NtRequestWaitReplyPort */
#define NT_SYSCALL_NTREPLYPORT               0x00CC  /* NtReplyPort */
#define NT_SYSCALL_NTREPLYWAITRECEIVEPORT    0x00CD  /* NtReplyWaitReceivePort */
#define NT_SYSCALL_NTREPLYWAITRECEIVEPORTEX  0x00CE  /* NtReplyWaitReceivePortEx */
#define NT_SYSCALL_NTREPLYWAITREPLYPORT      0x00CF  /* NtReplyWaitReplyPort */

/* Registry/Key Operations */
#define NT_SYSCALL_NTCREATEKEY               0x002C  /* NtCreateKey */
#define NT_SYSCALL_NTOPENKEY                 0x0080  /* NtOpenKey */
#define NT_SYSCALL_NTDELETEKEY               0x0043  /* NtDeleteKey */
#define NT_SYSCALL_NTRENAMEKEY               0x00CB  /* NtRenameKey */
#define NT_SYSCALL_NTDELETEVALUEKEY          0x0044  /* NtDeleteValueKey */
#define NT_SYSCALL_NTQUERYKEY                0x00A8  /* NtQueryKey */
#define NT_SYSCALL_NTQUERYVALUEKEY           0x00BC  /* NtQueryValueKey */
#define NT_SYSCALL_NTQUERYMULTIPLEVALUEKEY   0x00AA  /* NtQueryMultipleValueKey */
#define NT_SYSCALL_NTENUMERATEKEY            0x004C  /* NtEnumerateKey */
#define NT_SYSCALL_NTENUMERATEVALUEKEY       0x004D  /* NtEnumerateValueKey */
#define NT_SYSCALL_NTSETVALUEKEY             0x0102  /* NtSetValueKey */
#define NT_SYSCALL_NTFLUSHKEY                0x0054  /* NtFlushKey */
#define NT_SYSCALL_NTCOMPACTKEYS             0x001E  /* NtCompactKeys */
#define NT_SYSCALL_NTOPENKEYEX               0x0081  /* NtOpenKeyEx */

/* Object Manager */
#define NT_SYSCALL_NTCREATEDIRECTORYOBJECT   0x0029  /* NtCreateDirectoryObject */
#define NT_SYSCALL_NTOPENDIRECTORYOBJECT     0x007E  /* NtOpenDirectoryObject */
#define NT_SYSCALL_NTQUERYDIRECTORYOBJECT    0x0099  /* NtQueryDirectoryObject */
#define NT_SYSCALL_NTCREATESYMBOLICLINKOBJECT 0x0033  /* NtCreateSymbolicLinkObject */
#define NT_SYSCALL_NTOPENSYMBOLICLINKOBJECT  0x0084  /* NtOpenSymbolicLinkObject */
#define NT_SYSCALL_NTQUERYSYMBOLICLINKOBJECT 0x00B3  /* NtQuerySymbolicLinkObject */
#define NT_SYSCALL_NTMAKEPERMANENTOBJECT     0x006F  /* NtMakePermanentObject */
#define NT_SYSCALL_NTMAKETEMPORARYOBJECT     0x006E  /* NtMakeTemporaryObject */
#define NT_SYSCALL_NTQUERYOBJECT             0x00AB  /* NtQueryObject */
#define NT_SYSCALL_NTSETINFORMATIONOBJECT    0x00E7  /* NtSetInformationObject */
#define NT_SYSCALL_NTDUPLICATEOBJECT         0x0048  /* NtDuplicateObject */
#define NT_SYSCALL_NTQUERYOPENSUBKEYS        0x00B2  /* NtQueryOpenSubKeys */
#define NT_SYSCALL_NTQUERYOPENSUBKEYSEX      0x00B3  /* NtQueryOpenSubKeysEx */
#define NT_SYSCALL_NTLOCKREGISTRYKEY         0x006C  /* NtLockRegistryKey */

/* Security/Access Control */
#define NT_SYSCALL_NTACCESSCHECK             0x0002  /* NtAccessCheck */
#define NT_SYSCALL_NTACCESSCHECKBYTYPE       0x0004  /* NtAccessCheckByType */
#define NT_SYSCALL_NTACCESSCHECKBYTYPERESULTLIST 0x0006  /* NtAccessCheckByTypeResultList */
#define NT_SYSCALL_NTACCESSCHECKANDAUDITALARM 0x0003  /* NtAccessCheckAndAuditAlarm */
#define NT_SYSCALL_NTACCESSCHECKBYTYPEANDAUDITALARM 0x0005  /* NtAccessCheckByTypeAndAuditAlarm */
#define NT_SYSCALL_NTACCESSCHECKBYTYPERESULTLISTANDAUDITALARM 0x0007  /* NtAccessCheckByTypeResultListAndAuditAlarm */
#define NT_SYSCALL_NTACCESSCHECKBYTYPERESULTLISTANDAUDITALARMBYHANDLE 0x0008  /* NtAccessCheckByTypeResultListAndAuditAlarmByHandle */
#define NT_SYSCALL_NTOPENOBJECTAUDITALARM    0x007C  /* NtOpenObjectAuditAlarm */
#define NT_SYSCALL_NTCLOSEOBJECTAUDITALARM   0x001D  /* NtCloseObjectAuditAlarm */
#define NT_SYSCALL_NTDELETEOBJECTAUDITALARM  0x0045  /* NtDeleteObjectAuditAlarm */
#define NT_SYSCALL_NTPRIVILEGECHECK          0x008E  /* NtPrivilegeCheck */
#define NT_SYSCALL_NTPRIVILEGEOBJECTAUDITALARM 0x008F  /* NtPrivilegeObjectAuditAlarm */
#define NT_SYSCALL_NTPRIVILEGESERVICEAUDITALARM 0x0090  /* NtPrivilegedServiceAuditAlarm */
#define NT_SYSCALL_NTSETSECURITYOBJECT       0x00E8  /* NtSetSecurityObject */
#define NT_SYSCALL_NTQUERYSECURITYOBJECT     0x00B7  /* NtQuerySecurityObject */
#define NT_SYSCALL_NTFILTERTOKEN             0x0050  /* NtFilterToken */
#define NT_SYSCALL_NTCOMPARETOKENS           0x001F  /* NtCompareTokens */
#define NT_SYSCALL_NTOPENPROCESSTOKEN        0x007E  /* NtOpenProcessToken */
#define NT_SYSCALL_NTOPENPROCESSTOKENEX      0x0081  /* NtOpenProcessTokenEx */
#define NT_SYSCALL_NTOPENTHREADTOKEN         0x0086  /* NtOpenThreadToken */
#define NT_SYSCALL_NTOPENTHREADTOKENEX       0x0087  /* NtOpenThreadTokenEx */
#define NT_SYSCALL_NTSETINFORMATIONTOKEN     0x00F0  /* NtSetInformationToken */
#define NT_SYSCALL_NTQUERYINFORMATIONTOKEN   0x00A4  /* NtQueryInformationToken */
#define NT_SYSCALL_NTADJUSTPRIVILEGESTOKEN   0x000D  /* NtAdjustPrivilegesToken */
#define NT_SYSCALL_NTADJUSTGROUPSTOKEN       0x000C  /* NtAdjustGroupsToken */

/* System Information/Control */
#define NT_SYSCALL_NTQUERYSYSTEMINFORMATION  0x00B2  /* NtQuerySystemInformation */
#define NT_SYSCALL_NTSETSYSTEMINFORMATION    0x00EB  /* NtSetSystemInformation */
#define NT_SYSCALL_NTQUERYSYSTEMTIME         0x00B3  /* NtQuerySystemTime */
#define NT_SYSCALL_NTSETSYSTEMTIME         0x00EC  /* NtSetSystemTime */
#define NT_SYSCALL_NTDELAYEXECUTION        0x003E  /* NtDelayExecution */
#define NT_SYSCALL_NTQUERYPERFORMANCECOUNTER 0x00A6  /* NtQueryPerformanceCounter */
#define NT_SYSCALL_NTQUERYINTERVALPROFILE    0x00A6  /* NtQueryIntervalProfile */
#define NT_SYSCALL_NTSETINTERVALPROFILE      0x00E1  /* NtSetIntervalProfile */
#define NT_SYSCALL_NTQUERYDEFAULTLOCALE      0x0094  /* NtQueryDefaultLocale */
#define NT_SYSCALL_NTSETDEFAULTLOCALE        0x00E2  /* NtSetDefaultLocale */
#define NT_SYSCALL_NTQUERYDEFAULTUILANGUAGE  0x0095  /* NtQueryDefaultUILanguage */
#define NT_SYSCALL_NTSETDEFAULTUILANGUAGE    0x00E3  /* NtSetDefaultUILanguage */
#define NT_SYSCALL_NTQUERYINSTALLUILANGUAGE  0x00A5  /* NtQueryInstallUILanguage */
#define NT_SYSCALL_NTQUERYSYSTEMENVIRONMENTVALUE 0x00B4  /* NtQuerySystemEnvironmentValue */
#define NT_SYSCALL_NTQUERYSYSTEMENVIRONMENTVALUEEX 0x00B5  /* NtQuerySystemEnvironmentValueEx */
#define NT_SYSCALL_NTSETSYSTEMENVIRONMENTVALUE 0x00F4  /* NtSetSystemEnvironmentValue */
#define NT_SYSCALL_NTSETSYSTEMENVIRONMENTVALUEEX 0x00F5  /* NtSetSystemEnvironmentValueEx */
#define NT_SYSCALL_NTENUMERATESYSTEMENVIRONMENTVALUESEX 0x004E  /* NtEnumerateSystemEnvironmentValuesEx */
#define NT_SYSCALL_NTINITIALIZEREGISTRY      0x0061  /* NtInitializeRegistry */
#define NT_SYSCALL_NTSHUTDOWNSYSTEM          0x00D9  /* NtShutdownSystem */
#define NT_SYSCALL_NTPOWERINFORMATION        0x008D  /* NtPowerInformation */
#define NT_SYSCALL_NTINITIATEPOWERACTION     0x0062  /* NtInitiatePowerAction */
#define NT_SYSCALL_NTSETSYSTEMPOWERSTATE     0x00F1  /* NtSetSystemPowerState */

/* Plug and Play */
#define NT_SYSCALL_NTPLUGPLAYCONTROL         0x008F  /* NtPlugPlayControl */
#define NT_SYSCALL_NTGETPLUGPLAYEVENT        0x005C  /* NtGetPlugPlayEvent */

/* Debug/Exception */
#define NT_SYSCALL_NTCREATEDEBUGOBJECT       0x002A  /* NtCreateDebugObject */
#define NT_SYSCALL_NTDEBUGACTIVEPROCESS      0x003E  /* NtDebugActiveProcess */
#define NT_SYSCALL_NTDEBUGCONTINUE           0x003F  /* NtDebugContinue */
#define NT_SYSCALL_NTREMOVEPROCESSDEBUG      0x00C8  /* NtRemoveProcessDebug */
#define NT_SYSCALL_NTWAITFORDEBUGEVENT       0x011B  /* NtWaitForDebugEvent */
#define NT_SYSCALL_NTWAITFORINGLEOBJECT      0x011D  /* NtWaitForSingleObject */
#define NT_SYSCALL_NTWAITFORMULTIPLEOBJECTS  0x011C  /* NtWaitForMultipleObjects */
#define NT_SYSCALL_NTSIGNALANDWAITFORSINGLEOBJECT 0x00DA  /* NtSignalAndWaitForSingleObject */
#define NT_SYSCALL_NTWAITHIGHEVENTPAIR       0x0123  /* NtWaitHighEventPair */
#define NT_SYSCALL_NTWAITLOWEVENTPAIR        0x0124  /* NtWaitLowEventPair */
#define NT_SYSCALL_NTSETHIGHEVENTPAIR        0x00E6  /* NtSetHighEventPair */
#define NT_SYSCALL_NTSETLOWEVENTPAIR         0x00E7  /* NtSetLowEventPair */
#define NT_SYSCALL_NTSETHIGHWAITLOWEVENTPAIR 0x00E8  /* NtSetHighWaitLowEventPair */
#define NT_SYSCALL_NTSETLOWWAITHIGHEVENTPAIR 0x00E9  /* NtSetLowWaitHighEventPair */
#define NT_SYSCALL_NTCREATEEVENTPAIR         0x002B  /* NtCreateEventPair */
#define NT_SYSCALL_NTOPENEVENTPAIR           0x0080  /* NtOpenEventPair */

/* Profiling */
#define NT_SYSCALL_NTCREATEPROFILE           0x0032  /* NtCreateProfile */
#define NT_SYSCALL_NTSTARTPROFILE            0x00D5  /* NtStartProfile */
#define NT_SYSCALL_NTSTOPPROFILE             0x00D6  /* NtStopProfile */
#define NT_SYSCALL_NTQUERYINTERVALPROFILE    0x00A6  /* NtQueryIntervalProfile */
#define NT_SYSCALL_NTSETINTERVALPROFILE      0x00E1  /* NtSetIntervalProfile */

/* LPC/ALPC */
#define NT_SYSCALL_NTREQUESTDEVICEWAKEUP     0x00C7  /* NtRequestDeviceWakeup */
#define NT_SYSCALL_NTCANCELDEVICEWAKEUPREQUEST 0x0017  /* NtCancelDeviceWakeupRequest */
#define NT_SYSCALL_NTREQUESTWAKEUPLATENCY    0x00CB  /* NtRequestWakeupLatency */

/* Memory/Working Set */
#define NT_SYSCALL_NTQUERYWORKINGSET         0x00A7  /* NtQueryWorkingSet */
#define NT_SYSCALL_NTQUERYWORKINGSETEX       0x00A8  /* NtQueryWorkingSetEx */
#define NT_SYSCALL_NTLOCKPAGABLESECTIONBYHANDLE 0x006B  /* NtLockPagableSectionByHandle */

/* Job Objects */
#define NT_SYSCALL_NTCREATEJOBOBJECT         0x0030  /* NtCreateJobObject */
#define NT_SYSCALL_NTOPENJOBOBJECT           0x0084  /* NtOpenJobObject */
#define NT_SYSCALL_NTASSIGNPROCESSTOJOBOBJECT 0x000E  /* NtAssignProcessToJobObject */
#define NT_SYSCALL_NTTERMINATEJOBOBJECT      0x0107  /* NtTerminateJobObject */
#define NT_SYSCALL_NTQUERYINFORMATIONJOBOBJECT 0x00A0  /* NtQueryInformationJobObject */
#define NT_SYSCALL_NTSETINFORMATIONJOBOBJECT 0x00EB  /* NtSetInformationJobObject */
#define NT_SYSCALL_NTISPROCESSINJOB          0x0063  /* NtIsProcessInJob */
#define NT_SYSCALL_NTCREATEJOBSET            0x0031  /* NtCreateJobSet */

/* Tokens/Authentication */
#define NT_SYSCALL_NTCREATETOKEN             0x003A  /* NtCreateToken */
#define NT_SYSCALL_NTDUPLICATETOKEN          0x0049  /* NtDuplicateToken */
#define NT_SYSCALL_NTIMPERSONATEANONYMOUSTOKEN 0x005E  /* NtImpersonateAnonymousToken */
#define NT_SYSCALL_NTIMPERSONATECLIENTOFPORT 0x005F  /* NtImpersonateClientOfPort */
#define NT_SYSCALL_NTIMPERSONATETHREAD       0x0060  /* NtImpersonateThread */

/* Thread Pool (Vista+) */
#define NT_SYSCALL_NTTPALLOCPOOL             0x0047  /* TpAllocPool (via ntdll) */
#define NT_SYSCALL_NTTPRELEASEPOOL           0x0048  /* TpReleasePool */
#define NT_SYSCALL_NTTPSETPOOLMINTHREADS     0x0049  /* TpSetPoolMinThreads */
#define NT_SYSCALL_NTTPSETPOOLMAXTHREADS     0x004A  /* TpSetPoolMaxThreads */
#define NT_SYSCALL_NTTPQUERYPOOLSTACKINFORMATION 0x004B  /* TpQueryPoolStackInformation */
#define NT_SYSCALL_NTTPSETPOOLSTACKINFORMATION 0x004C  /* TpSetPoolStackInformation */

/* Keyed Events (Vista+) */
#define NT_SYSCALL_NTCREATEKEYEDEVENT        0x003B  /* NtCreateKeyedEvent */
#define NT_SYSCALL_NTOPENKEYEDEVENT          0x0088  /* NtOpenKeyedEvent */
#define NT_SYSCALL_NTRELEASEKEYEDEVENT       0x00C7  /* NtReleaseKeyedEvent */
#define NT_SYSCALL_NTWAITFORKEYEDEVENT       0x011F  /* NtWaitForKeyedEvent */

/* Transaction Manager (Vista+) */
#define NT_SYSCALL_NTCREATETRANSACTION       0x003C  /* NtCreateTransaction */
#define NT_SYSCALL_NTOPENTRANSACTION         0x0089  /* NtOpenTransaction */
#define NT_SYSCALL_NTCOMMITTRANSACTION       0x0024  /* NtCommitTransaction */
#define NT_SYSCALL_NTROLLBACKTRANSACTION     0x00D3  /* NtRollbackTransaction */
#define NT_SYSCALL_NTQUERYINFORMATIONTRANSACTION 0x00A1  /* NtQueryInformationTransaction */
#define NT_SYSCALL_NTSETINFORMATIONTRANSACTION 0x00EC  /* NtSetInformationTransaction */

/* Registry Transaction (Vista+) */
#define NT_SYSCALL_NTCREATEKEYTRANSACTED     0x003D  /* NtCreateKeyTransacted */
#define NT_SYSCALL_NTOPENKEYTRANSACTED       0x008A  /* NtOpenKeyTransacted */

/* System Debug Control */
#define NT_SYSCALL_NTSYSTEMDEBUGCONTROL      0x00D9  /* NtSystemDebugControl */

/* Trace/Event */
#define NT_SYSCALL_NTTRACEEVENT              0x010F  /* NtTraceEvent */
#define NT_SYSCALL_NTTRACECONTROL            0x010E  /* NtTraceControl */

/* Boot/Driver Entries */
#define NT_SYSCALL_NTADDBOOTENTRY            0x000A  /* NtAddBootEntry */
#define NT_SYSCALL_NTDELETEBOOTENTRY         0x0040  /* NtDeleteBootEntry */
#define NT_SYSCALL_NTMODIFYBOOTENTRY         0x0073  /* NtModifyBootEntry */
#define NT_SYSCALL_NTQUERYBOOTENTRYORDER     0x00A7  /* NtQueryBootEntryOrder */
#define NT_SYSCALL_NTSETBOOTENTRYORDER       0x00E0  /* NtSetBootEntryOrder */
#define NT_SYSCALL_NTQUERYBOOTOPTIONS        0x00A8  /* NtQueryBootOptions */
#define NT_SYSCALL_NTSETBOOTOPTIONS          0x00E1  /* NtSetBootOptions */
#define NT_SYSCALL_NTADDDriverENTRY          0x000B  /* NtAddDriverEntry */
#define NT_SYSCALL_NTDELETEDRIVERENTRY       0x0041  /* NtDeleteDriverEntry */
#define NT_SYSCALL_NTMODIFYDRIVERENTRY       0x0074  /* NtModifyDriverEntry */
#define NT_SYSCALL_NTENUMERATEBOOTENTRIES    0x004A  /* NtEnumerateBootEntries */
#define NT_SYSCALL_NTENUMERATEDRIVERENTRIES  0x004B  /* NtEnumerateDriverEntries */
#define NT_SYSCALL_NTQUERYDRIVERENTRYORDER   0x00A4  /* NtQueryDriverEntryOrder */
#define NT_SYSCALL_NTSETDRIVERENTRYORDER     0x00DD  /* NtSetDriverEntryOrder */

/* Driver/Module */
#define NT_SYSCALL_NTLOADDRIVER              0x0064  /* NtLoadDriver */
#define NT_SYSCALL_NTUNLOADDRIVER            0x0115  /* NtUnloadDriver */

/* Application Help */
#define NT_SYSCALL_NTAPPHELPCACHECONTROL     0x0014  /* NtApphelpCacheControl */

/* ========================================================================
 * NT → VSL Syscall Mapping Table
 * ======================================================================== */

typedef struct {
    uint16_t nt_syscall_num;      /* NT syscall number (0x0-0x400+) */
    const char *nt_name;          /* NT syscall name */
    uint16_t vsl_syscall_num;     /* VSL syscall number (or 0xFFFF for multi-call) */
    uint8_t flags;                /* Mapping flags */
} vsl_nt_syscall_map_t;

/* Mapping flags */
#define VSL_NT_MAP_DIRECT          0x01  /* Direct 1:1 mapping to VSL syscall */
#define VSL_NT_MAP_COMPOSITE       0x02  /* Maps to multiple VSL syscalls */
#define VSL_NT_MAP_EMULATED        0x04  /* Fully emulated in userspace */
#define VSL_NT_MAP_STUB            0x08  /* Stub - not yet implemented */
#define VSL_NT_MAP_UNSUPPORTED     0x10  /* Not supported in WuBuOS architecture */
#define VSL_NT_MAP_STYX9           0x20  /* Routes through Styx9/9P namespace */

/* ========================================================================
 * NT Object Types → VSL/Styx9 Type Mapping
 * ======================================================================== */

typedef enum {
    NT_OBJECT_TYPE_UNKNOWN = 0,
    NT_OBJECT_TYPE_PROCESS,
    NT_OBJECT_TYPE_THREAD,
    NT_OBJECT_TYPE_FILE,
    NT_OBJECT_TYPE_DIRECTORY,
    NT_OBJECT_TYPE_SYMBOLIC_LINK,
    NT_OBJECT_TYPE_EVENT,
    NT_OBJECT_TYPE_MUTANT,
    NT_OBJECT_TYPE_SEMAPHORE,
    NT_OBJECT_TYPE_TIMER,
    NT_OBJECT_TYPE_KEY,
    NT_OBJECT_TYPE_KEYED_EVENT,
    NT_OBJECT_TYPE_SECTION,
    NT_OBJECT_TYPE_JOB,
    NT_OBJECT_TYPE_TOKEN,
    NT_OBJECT_TYPE_PORT,
    NT_OBJECT_TYPE_WAITABLE_PORT,
    NT_OBJECT_TYPE_DEBUG_OBJECT,
    NT_OBJECT_TYPE_PROFILE,
    NT_OBJECT_TYPE_TRANSACTION,
    NT_OBJECT_TYPE_TRANSACTION_MANAGER,
    NT_OBJECT_TYPE_RESOURCE_MANAGER,
    NT_OBJECT_TYPE_ENLISTMENT,
    NT_OBJECT_TYPE_TM,
    NT_OBJECT_TYPE_IO_COMPLETION,
    NT_OBJECT_TYPE_EVENT_PAIR,
    NT_OBJECT_TYPE_WORK_ITEM,
} nt_object_type_t;

/* ========================================================================
 * NT → VSL Bridge Context
 * ======================================================================== */

typedef struct {
    /* Process context */
    uint32_t current_pid;
    uint32_t current_tid;
    
    /* Handle table (NT handle → VSL fd/Styx fid) */
    struct {
        uint32_t nt_handle;
        int vsl_fd;           /* VSL file descriptor */
        uint64_t styx_fid;    /* Styx9 fid */
        uint64_t data;        /* opaque payload: pid_t for PROC/THREAD, mmap base for SECTION/VMEM */
        nt_object_type_t type;
        bool valid;
    } handle_table[4096];
    
    /* Memory management */
    struct {
        void *process_heap;
        size_t heap_size;
        void *shared_memory_base;
    } memory;
    
    /* Synchronization */
    struct {
        /* Critical sections, SRW locks, condition variables */
    } sync;
    
    /* Registry */
    struct {
        /* NT registry hive → Styx9 namespace mapping */
    } registry;
    
    /* I/O */
    struct {
        /* IRP tracking */
    } io;

    /* WNF (Windows Notification Facility) */
    uint32_t wnf_notify_event;
    
} vsl_nt_bridge_ctx_t;

/* ========================================================================
 * Public API
 * ======================================================================== */

/* Initialize NT bridge */
int vsl_nt_bridge_init(vsl_nt_bridge_ctx_t *ctx);

/* Shutdown NT bridge */
void vsl_nt_bridge_shutdown(vsl_nt_bridge_ctx_t *ctx);

/* Dispatch NT syscall through VSL */
int64_t vsl_nt_syscall_dispatch(vsl_nt_bridge_ctx_t *ctx,
                                 uint16_t nt_syscall_num,
                                 uint64_t *args,  /* NT syscall args (up to 10) */
                                 int n_args);

/* Handle translation */
int vsl_nt_handle_to_vsl_fd(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle, int *out_vsl_fd);
int vsl_nt_handle_to_styx_fid(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle, uint64_t *out_styx_fid);
/* Returns the opaque payload stored in the handle slot (pid_t for PROC/THREAD,
 * mmap base for SECTION/VMEM, etc.). Returns 0 and -1 if the handle is invalid. */
int vsl_nt_handle_to_data(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle, uint64_t *out_data);
uint32_t vsl_nt_allocate_handle(vsl_nt_bridge_ctx_t *ctx, int vsl_fd, uint64_t styx_fid, nt_object_type_t type);
int vsl_nt_free_handle(vsl_nt_bridge_ctx_t *ctx, uint32_t nt_handle);

/* Object type mapping */
nt_object_type_t vsl_nt_object_type_from_name(const char *nt_type_name);
const char *vsl_nt_object_type_name(nt_object_type_t type);

/* Status translation */
int vsl_nt_status_to_errno(uint32_t nt_status);
uint32_t vsl_errno_to_nt_status(int errno_val);

/* Memory management — implemented as NT syscalls (Batch 4). These are the
 * real transliterated handlers dispatched via g_nt_dispatch[] (raw syscall
 * arg ABI), not the old ctx-rich helper prototypes which were never wired. */
int64_t vsl_nt_allocate_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_free_virtual_memory(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* Thread/Process — implemented as NT syscalls (Batch 4). */
int64_t vsl_nt_create_thread(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t vsl_nt_create_process(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* File/I/O helpers */
int vsl_nt_create_file(vsl_nt_bridge_ctx_t *ctx,
                        uint32_t *out_file_handle,
                        uint32_t desired_access,
                        void *object_attributes,
                        void *io_status_block,
                        void *allocation_size,
                        uint32_t file_attributes,
                        uint32_t share_access,
                        uint32_t create_disposition,
                        uint32_t create_options,
                        void *ea_buffer,
                        uint32_t ea_length);

#endif /* WUBU_VSL_NT_BRIDGE_H */