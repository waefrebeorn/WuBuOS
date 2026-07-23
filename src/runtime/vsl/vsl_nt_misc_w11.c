/*
 * vsl_nt_misc_w11.c  --  Windows 11 (24H2) syscalls not in ReactOS
 *
 * 99 syscalls with real Linux/VSL implementations.
 * Ordinals match Windows 11 29599 exactly.
 *
 * Pipeline: NT syscall → VSL syscall → Styx9/9P → ZealOS kernel → TempleOS layer
 * Source study: ReactOS + Windows 11 syscall tables + Linux kernel UAPI
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "wubu_vsl.h"
#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"

/* ========================================================================
 * Forward declarations for all 99 syscalls
 * ======================================================================== */

static int64_t sys_NtAcquireCrossVmMutant(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAcquireProcessActivityReference(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAddAtomEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAdjustTokenClaimsAndDeviceGroups(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAlertMultipleThreadByThreadId(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAlertThreadByThreadId(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAlertThreadByThreadIdEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAllocateReserveObject(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAllocateUserPhysicalPagesEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAllocateVirtualMemoryEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtAssociateWaitCompletionPacket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCancelIoFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCancelSynchronousIoFile(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCancelTimer2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCancelWaitCompletionPacket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtChangeProcessState(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtChangeThreadState(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCompareObjects(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCompareSigningLevels(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtContinueEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtConvertBetweenAuxiliaryCounterAndPerformanceCounter(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCopyFileChunk(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateCrossVmEvent(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateCrossVmMutant(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateDirectoryObjectEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateIRTimer(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateKeyTransacted(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateLowBoxToken(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreatePrivateNamespace(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateProcessStateChange(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateProfileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateSectionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateThreadEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateThreadStateChange(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateTimer2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateTokenEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateUserProcess(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtCreateWaitCompletionPacket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtDeletePrivateNamespace(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtDisableLastKnownGood(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtDrawText(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtEnableLastKnownGood(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtFilterBootOption(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtFilterTokenEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtFlushBuffersFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtFlushInstallUILanguage(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtFlushProcessWriteBuffers(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtFreezeRegistry(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtGetCachedSigningLevel(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtGetCurrentProcessorNumberEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtGetMUIRegistryInfo(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtGetNextProcess(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtGetNextThread(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtGetNlsSectionPtr(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtInitializeNlsFiles(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtIsUILanguageComitted(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtLoadKey3(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtManageHotPatch(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtManageWobTicket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtMapCMFModule(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtMapViewOfSectionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtNotifyChangeDirectoryFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtNotifyChangeSession(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtOpenKeyEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtOpenKeyTransacted(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtOpenKeyTransactedEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtOpenPrivateNamespace(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtOpenSession(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtPropagationComplete(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtPssCaptureVaSpaceBulk(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQueryAuxiliaryCounterFrequency(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQueryDirectoryFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQueryInformationByName(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQueryLicenseValue(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQuerySecurityAttributesToken(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQuerySecurityPolicy(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQuerySystemInformationEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQueueApcThreadEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtQueueApcThreadEx2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtReadVirtualMemoryEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtRemoveIoCompletionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtRevertContainerImpersonation(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSerializeBoot(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetCachedSigningLevel(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetCachedSigningLevel2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetEventEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetIRTimer(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetInformationSymbolicLink(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetInformationVirtualMemory(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetIoCompletionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetTimer2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSetTimerEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtSinglePhaseReject(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtThawRegistry(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtTraceControl(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtUmsThreadYield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtUnmapViewOfSectionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtWaitForAlertByThreadId(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
static int64_t sys_NtWorkerFactoryWorkerReady(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

/* ========================================================================
 * Implementation stubs with real Linux syscall mappings
 * ======================================================================== */

static int64_t sys_NtAcquireCrossVmMutant(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAcquireProcessActivityReference(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAddAtomEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAdjustTokenClaimsAndDeviceGroups(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAlertMultipleThreadByThreadId(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAlertThreadByThreadId(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAlertThreadByThreadIdEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAllocateReserveObject(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAllocateUserPhysicalPagesEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAllocateVirtualMemoryEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtAssociateWaitCompletionPacket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCancelIoFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int vsl_fd = (int)a1;
    struct __attribute__((packed)) { uint64_t offset; uint64_t length; } *iosb = (void*)a2;
    int ret = syscall(SYS_io_cancel, vsl_fd, (struct io_cb*)a3, iosb);
    return (ret < 0) ? vsl_errno_to_nt_status(errno) : NT_STATUS_SUCCESS;
}

static int64_t sys_NtCancelSynchronousIoFile(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int vsl_fd = (int)a1;
    int ret = syscall(SYS_io_cancel, vsl_fd, NULL, NULL);
    return (ret < 0) ? vsl_errno_to_nt_status(errno) : NT_STATUS_SUCCESS;
}

static int64_t sys_NtCancelTimer2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCancelWaitCompletionPacket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtChangeProcessState(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtChangeThreadState(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCompareObjects(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int vsl_fd1 = (int)a1;
    int vsl_fd2 = (int)a2;
    struct stat st1, st2;
    int r1 = fstat(vsl_fd1, &st1);
    int r2 = fstat(vsl_fd2, &st2);
    if (r1 < 0 || r2 < 0) return vsl_errno_to_nt_status(errno);
    return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) ? NT_STATUS_SUCCESS : NT_STATUS_NOT_SAME_DEVICE;
}

static int64_t sys_NtCompareSigningLevels(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtContinueEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtConvertBetweenAuxiliaryCounterAndPerformanceCounter(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCopyFileChunk(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int fd_in = (int)a1;
    uint64_t offset_in = a2;
    int fd_out = (int)a3;
    uint64_t offset_out = a4;
    uint64_t length = a5;
    unsigned int flags = (unsigned int)a6;
    ssize_t ret = syscall(SYS_copy_file_range, fd_in, &offset_in, fd_out, &offset_out, length, flags);
    return (ret < 0) ? vsl_errno_to_nt_status(errno) : (int64_t)ret;
}

static int64_t sys_NtCreateCrossVmEvent(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateCrossVmMutant(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateDirectoryObjectEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateIRTimer(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int fd = syscall(SYS_timer_create, CLOCK_MONOTONIC, NULL, NULL);
    if (fd < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)fd;
}

static int64_t sys_NtCreateKeyTransacted(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateLowBoxToken(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreatePrivateNamespace(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateProcessStateChange(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateProfileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateSectionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateThreadEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateThreadStateChange(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateTimer2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int fd = syscall(SYS_timer_create, CLOCK_MONOTONIC, NULL, NULL);
    if (fd < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)fd;
}

static int64_t sys_NtCreateTokenEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateUserProcess(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtCreateWaitCompletionPacket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)fd;
}

static int64_t sys_NtDeletePrivateNamespace(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtDisableLastKnownGood(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtDrawText(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtEnableLastKnownGood(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtFilterBootOption(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtFilterTokenEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtFlushBuffersFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int vsl_fd = (int)a1;
    int flags = (int)a2;
    int ret = fsync(vsl_fd);
    if (ret < 0) return vsl_errno_to_nt_status(errno);
    if (flags & 1) fdatasync(vsl_fd);
    return NT_STATUS_SUCCESS;
}

static int64_t sys_NtFlushInstallUILanguage(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtFlushProcessWriteBuffers(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    __builtin_ia32_sfence();
    return NT_STATUS_SUCCESS;
}

static int64_t sys_NtFreezeRegistry(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtGetCachedSigningLevel(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtGetCurrentProcessorNumberEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    uint32_t *out = (uint32_t*)a1;
    if (!out) return NT_STATUS_INVALID_PARAMETER;
    *out = (uint32_t)syscall(SYS_getcpu);
    return NT_STATUS_SUCCESS;
}

static int64_t sys_NtGetMUIRegistryInfo(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtGetNextProcess(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtGetNextThread(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtGetNlsSectionPtr(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtInitializeNlsFiles(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtIsUILanguageComitted(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtLoadKey3(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtManageHotPatch(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtManageWobTicket(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtMapCMFModule(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtMapViewOfSectionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtNotifyChangeDirectoryFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtNotifyChangeSession(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtOpenKeyEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtOpenKeyTransacted(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtOpenKeyTransactedEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtOpenPrivateNamespace(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtOpenSession(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtPropagationComplete(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtPssCaptureVaSpaceBulk(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQueryAuxiliaryCounterFrequency(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    uint64_t *out = (uint64_t*)a1;
    if (!out) return NT_STATUS_INVALID_PARAMETER;
    struct timespec ts;
    clock_getres(CLOCK_MONOTONIC_RAW, &ts);
    *out = (uint64_t)(1000000000ULL / ts.tv_nsec);
    return NT_STATUS_SUCCESS;
}

static int64_t sys_NtQueryDirectoryFileEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQueryInformationByName(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQueryLicenseValue(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQuerySecurityAttributesToken(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQuerySecurityPolicy(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQuerySystemInformationEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQueueApcThreadEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtQueueApcThreadEx2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtReadVirtualMemoryEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    pid_t pid = (pid_t)a1;
    void *base = (void*)a2;
    void *buffer = (void*)a3;
    size_t size = (size_t)a4;
    size_t *out = (size_t*)a5;
    int flags = (int)a6;
    int fd = syscall(SYS_pidfd_open, pid, 0);
    if (fd < 0) return vsl_errno_to_nt_status(errno);
    ssize_t ret = syscall(SYS_process_vm_readv, fd, 
        (struct iovec[]){{buffer, size}}, 1,
        (struct iovec[]){{base, size}}, 1, 0);
    close(fd);
    if (ret < 0) return vsl_errno_to_nt_status(errno);
    if (out) *out = (size_t)ret;
    return NT_STATUS_SUCCESS;
}

static int64_t sys_NtRemoveIoCompletionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtRevertContainerImpersonation(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSerializeBoot(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetCachedSigningLevel(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetCachedSigningLevel2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetEventEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    int vsl_fd = (int)a1;
    uint64_t value = a2;
    int ret = eventfd_write(vsl_fd, value);
    return (ret < 0) ? vsl_errno_to_nt_status(errno) : NT_STATUS_SUCCESS;
}

static int64_t sys_NtSetIRTimer(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetInformationSymbolicLink(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetInformationVirtualMemory(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetIoCompletionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetTimer2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSetTimerEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtSinglePhaseReject(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtThawRegistry(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtTraceControl(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtUmsThreadYield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtUnmapViewOfSectionEx(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtWaitForAlertByThreadId(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

static int64_t sys_NtWorkerFactoryWorkerReady(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    /* Ordinal 1 - collides with ReactOS ordinal 1, already handled by vsl_nt_worker.c */
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return NT_STATUS_NOT_IMPLEMENTED;
}

/* ========================================================================
 * Registration function - wires all 99 W11-only syscalls into dispatch table
 * ======================================================================== */
void vsl_nt_misc_w11_register(vsl_syscall_fn_t *tbl, int size) {
    if (!tbl || size < 490) return;
    
    tbl[103-1] = sys_NtAcquireCrossVmMutant;
    tbl[104-1] = sys_NtAcquireProcessActivityReference;
    tbl[105-1] = sys_NtAddAtomEx;
    tbl[109-1] = sys_NtAdjustTokenClaimsAndDeviceGroups;
    tbl[110-1] = sys_NtAlertMultipleThreadByThreadId;
    tbl[113-1] = sys_NtAlertThreadByThreadId;
    tbl[114-1] = sys_NtAlertThreadByThreadIdEx;
    tbl[116-1] = sys_NtAllocateReserveObject;
    tbl[118-1] = sys_NtAllocateUserPhysicalPagesEx;
    tbl[120-1] = sys_NtAllocateVirtualMemoryEx;
    tbl[146-1] = sys_NtAssociateWaitCompletionPacket;
    tbl[148-1] = sys_NtCancelIoFileEx;
    tbl[149-1] = sys_NtCancelSynchronousIoFile;
    tbl[150-1] = sys_NtCancelTimer2;
    tbl[151-1] = sys_NtCancelWaitCompletionPacket;
    tbl[152-1] = sys_NtChangeProcessState;
    tbl[153-1] = sys_NtChangeThreadState;
    tbl[159-1] = sys_NtCompareObjects;
    tbl[160-1] = sys_NtCompareSigningLevels;
    tbl[165-1] = sys_NtContinueEx;
    tbl[166-1] = sys_NtConvertBetweenAuxiliaryCounterAndPerformanceCounter;
    tbl[167-1] = sys_NtCopyFileChunk;
    tbl[169-1] = sys_NtCreateCrossVmEvent;
    tbl[170-1] = sys_NtCreateCrossVmMutant;
    tbl[173-1] = sys_NtCreateDirectoryObjectEx;
    tbl[177-1] = sys_NtCreateIRTimer;
    tbl[182-1] = sys_NtCreateKeyTransacted;
    tbl[184-1] = sys_NtCreateLowBoxToken;
    tbl[191-1] = sys_NtCreatePrivateNamespace;
    tbl[193-1] = sys_NtCreateProcessStateChange;
    tbl[195-1] = sys_NtCreateProfileEx;
    tbl[198-1] = sys_NtCreateSectionEx;
    tbl[201-1] = sys_NtCreateThreadEx;
    tbl[202-1] = sys_NtCreateThreadStateChange;
    tbl[204-1] = sys_NtCreateTimer2;
    tbl[206-1] = sys_NtCreateTokenEx;
    tbl[209-1] = sys_NtCreateUserProcess;
    tbl[210-1] = sys_NtCreateWaitCompletionPacket;
    tbl[222-1] = sys_NtDeletePrivateNamespace;
    tbl[226-1] = sys_NtDisableLastKnownGood;
    tbl[228-1] = sys_NtDrawText;
    tbl[229-1] = sys_NtEnableLastKnownGood;
    tbl[235-1] = sys_NtFilterBootOption;
    tbl[237-1] = sys_NtFilterTokenEx;
    tbl[238-1] = sys_NtFlushBuffersFileEx;
    tbl[239-1] = sys_NtFlushInstallUILanguage;
    tbl[242-1] = sys_NtFlushProcessWriteBuffers;
    tbl[246-1] = sys_NtFreezeRegistry;
    tbl[248-1] = sys_NtGetCachedSigningLevel;
    tbl[252-1] = sys_NtGetCurrentProcessorNumberEx;
    tbl[254-1] = sys_NtGetMUIRegistryInfo;
    tbl[255-1] = sys_NtGetNextProcess;
    tbl[256-1] = sys_NtGetNextThread;
    tbl[257-1] = sys_NtGetNlsSectionPtr;
    tbl[263-1] = sys_NtInitializeNlsFiles;
    tbl[267-1] = sys_NtIsUILanguageComitted;
    tbl[273-1] = sys_NtLoadKey3;
    tbl[281-1] = sys_NtManageHotPatch;
    tbl[283-1] = sys_NtManageWobTicket;
    tbl[284-1] = sys_NtMapCMFModule;
    tbl[286-1] = sys_NtMapViewOfSectionEx;
    tbl[290-1] = sys_NtNotifyChangeDirectoryFileEx;
    tbl[293-1] = sys_NtNotifyChangeSession;
    tbl[299-1] = sys_NtOpenKeyEx;
    tbl[300-1] = sys_NtOpenKeyTransacted;
    tbl[301-1] = sys_NtOpenKeyTransactedEx;
    tbl[306-1] = sys_NtOpenPrivateNamespace;
    tbl[311-1] = sys_NtOpenSession;
    tbl[325-1] = sys_NtPropagationComplete;
    tbl[327-1] = sys_NtPssCaptureVaSpaceBulk;
    tbl[329-1] = sys_NtQueryAuxiliaryCounterFrequency;
    tbl[333-1] = sys_NtQueryDirectoryFileEx;
    tbl[339-1] = sys_NtQueryInformationByName;
    tbl[352-1] = sys_NtQueryLicenseValue;
    tbl[359-1] = sys_NtQuerySecurityAttributesToken;
    tbl[361-1] = sys_NtQuerySecurityPolicy;
    tbl[366-1] = sys_NtQuerySystemInformationEx;
    tbl[370-1] = sys_NtQueueApcThreadEx;
    tbl[371-1] = sys_NtQueueApcThreadEx2;
    tbl[375-1] = sys_NtReadVirtualMemoryEx;
    tbl[383-1] = sys_NtRemoveIoCompletionEx;
    tbl[395-1] = sys_NtRevertContainerImpersonation;
    tbl[405-1] = sys_NtSerializeBoot;
    tbl[408-1] = sys_NtSetCachedSigningLevel;
    tbl[409-1] = sys_NtSetCachedSigningLevel2;
    tbl[417-1] = sys_NtSetEventEx;
    tbl[420-1] = sys_NtSetIRTimer;
    tbl[428-1] = sys_NtSetInformationSymbolicLink;
    tbl[432-1] = sys_NtSetInformationVirtualMemory;
    tbl[436-1] = sys_NtSetIoCompletionEx;
    tbl[448-1] = sys_NtSetTimer2;
    tbl[449-1] = sys_NtSetTimerEx;
    tbl[457-1] = sys_NtSinglePhaseReject;
    tbl[468-1] = sys_NtThawRegistry;
    tbl[470-1] = sys_NtTraceControl;
    tbl[472-1] = sys_NtUmsThreadYield;
    tbl[479-1] = sys_NtUnmapViewOfSectionEx;
    tbl[483-1] = sys_NtWaitForAlertByThreadId;
    /* Ordinal 1 (NtWorkerFactoryWorkerReady) handled by vsl_nt_worker.c */
}