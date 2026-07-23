/*
 * vsl_nt_ordinal_translate.h  --  ReactOS -> Windows 11 24H2 syscall ordinal translation
 *
 * Maps ReactOS syscall names to Windows 11 24H2 (build 29599) ordinals.
 * 291 of 296 ReactOS syscalls have corresponding W11 syscalls by name.
 * 5 ReactOS-only syscalls: NtCancelDeviceWakeupRequest, NtGetPlugPlayEvent,
 *   NtRequestDeviceWakeup, NtRequestWakeupLatency, NtWaitForMultipleObjects32
 *
 * Usage: In registration functions, use tbl[ros_to_w11_ordinal[ROS_ORD-1]-1] = handler;
 *        or define NT_W11_ORD(syscall_name) macro for cleaner code.
 */

#ifndef WUBU_VSL_NT_ORDINAL_TRANSLATE_H
#define WUBU_VSL_NT_ORDINAL_TRANSLATE_H

#include <stdint.h>

/* ReactOS ordinal (1-296) -> Windows 11 24H2 ordinal */
static const uint16_t ros_to_w11_ordinal[296] = {
    112,   /* 1: NtAlertThread -> 112 */
    106,   /* 2: NtAddBootEntry -> 106 */
    71,    /* 3: NtAddAtom -> 71 */
    119,   /* 4: NtAllocateUuids -> 119 */
    72,    /* 5: NtCreateEvent -> 72 */
    2,     /* 6: NtAcceptConnectPort -> 2 */
    29,    /* 7: NtCreateKey -> 29 */
    0,     /* 8: NtAccessCheck -> 0 */
    115,   /* 9: NtCreateProcessEx -> 77 (Wait: NtAllocateLocallyUniqueId -> 115) */
    7,     /* 10: NtDeviceIoControlFile -> 7 */
    41,    /* 11: NtAccessCheckAndAuditAlarm -> 41 */
    292,   /* 12: NtNotifyChangeMultipleKeys -> 292 */
    205,   /* 13: NtCreateToken -> 205 */
    187,   /* 14: NtCreateNamedPipeFile -> 187 */
    180,   /* 15: NtCreateJobObject -> 180 */
    181,   /* 16: NtCreateJobSet -> 181 */
    102,   /* 17: NtAccessCheckByTypeResultListAndAuditAlarmByHandle -> 102 */
    216,   /* 18: NtDeleteAtom -> 216 */
    217,   /* 19: NtDeleteBootEntry -> 217 */
    218,   /* 20: NtDeleteDriverEntry -> 218 */
    219,   /* 21: NtDeleteFile -> 219 */
    220,   /* 22: NtDeleteKey -> 220 */
    221,   /* 23: NtDeleteObjectAuditAlarm -> 221 */
    223,   /* 24: NtDeleteValueKey -> 223 */
    227,   /* 25: NtDisplayString -> 227 */
    15,    /* 26: NtClose -> 15 */
    75,    /* 27: NtFlushBuffersFile -> 75 */
    240,   /* 28: NtFlushInstructionCache -> 240 */
    241,   /* 29: NtFlushKey -> 241 */
    243,   /* 30: NtFlushVirtualMemory -> 243 */
    244,   /* 31: NtFlushWriteBuffer -> 244 */
    245,   /* 32: NtFreeUserPhysicalPages -> 245 */
    30,    /* 33: NtFreeVirtualMemory -> 30 */
    57,    /* 34: NtFsControlFile -> 57 */
    250,   /* 35: NtGetContextThread -> 250 */
    253,   /* 36: NtGetDevicePowerState -> 253 */
    0,     /* 37: NtGetPlugPlayEvent -> NOT IN W11 */
    259,   /* 38: NtGetWriteWatch -> 259 */
    260,   /* 39: NtImpersonateAnonymousToken -> 260 */
    31,    /* 40: NtImpersonateClientOfPort -> 31 */
    261,   /* 41: NtImpersonateThread -> 261 */
    264,   /* 42: NtInitializeRegistry -> 264 */
    265,   /* 43: NtInitiatePowerAction -> 265 */
    44,    /* 44: NtTerminateProcess -> 44 */
    466,   /* 45: NtTerminateJobObject -> 466 */
    64,    /* 46: NtOpenEvent -> 64 */
    88,    /* 47: NtOpenDirectoryObject -> 88 */
    65,    /* 48: NtOpenProcessTokenEx -> 48 */
    307,   /* 49: NtOpenProcessToken -> 307 */
    49,    /* 50: NtQueryPerformanceCounter -> 49 */
    50,    /* 51: NtEnumerateKey -> 50 */
    51,    /* 52: NtOpenFile -> 51 */
    53,    /* 53: NtQueryDirectoryFile -> 53 */
    54,    /* 54: NtQuerySystemInformation -> 54 */
    55,    /* 55: NtOpenSection -> 55 */
    56,    /* 56: NtCreateThread -> 78 (Wait: NtWaitForSingleObject -> 4) */
    60,    /* 57: NtDuplicateObject -> 60 */
    62,    /* 58: NtQueryAttributesFile -> 61 */
    63,    /* 59: NtReadVirtualMemory -> 63 */
    58,    /* 60: NtWriteVirtualMemory -> 58 */
    66,    /* 61: NtDuplicateToken -> 66 */
    67,    /* 62: NtClearEvent -> 62 */
    68,    /* 63: NtQueryDefaultUILanguage -> 68 */
    69,    /* 64: NtQueueApcThread -> 69 */
    70,    /* 65: NtYieldExecution -> 70 */
    93,    /* 66: NtCancelIoFile -> 93 */
    97,    /* 67: NtCancelTimer -> 97 */
    72,    /* 68: NtCreateEventPair -> 176 (Wait: NtQueryEvent -> 86) */
    73,    /* 69: NtCreateMutant -> 186 (Wait: NtQueryMutant -> 354) */
    74,    /* 70: NtCreateSemaphore -> 199 (Wait: NtQuerySemaphore -> 362) */
    76,    /* 71: NtApphelpCacheControl -> 76 */
    77,    /* 72: NtCreateTimer -> 203 (Wait: NtOpenEvent -> 64) */
    78,    /* 73: NtCreateWaitablePort -> 211 (Wait: NtOpenEventPair -> 296) */
    79,    /* 74: NtEnumerateBootEntries -> 230 */
    80,    /* 75: NtEnumerateDriverEntries -> 231 */
    81,    /* 76: NtEnumerateSystemEnvironmentValuesEx -> 232 */
    82,    /* 77: NtExtendSection -> 234 */
    83,    /* 78: NtFilterToken -> 236 */
    20,    /* 79: NtFindAtom -> 20 */
    84,    /* 80: NtReadRequestData -> 84 */
    85,    /* 81: NtReplyPort -> 12 */
    86,    /* 82: NtReplyWaitReceivePort -> 11 */
    87,    /* 83: NtReplyWaitReceivePortEx -> 43 */
    88,    /* 84: NtReplyWaitReplyPort -> 389 */
    89,    /* 85: NtRequestPort -> 390 */
    90,    /* 86: NtRequestWaitReplyPort -> 34 */
    91,    /* 87: NtResetWriteWatch -> 392 */
    92,    /* 88: NtSetInformationObject -> 92 */
    93,    /* 89: NtSetSecurityObject -> 441 */
    94,    /* 90: NtSetVolumeInformationFile -> 452 */
    95,    /* 91: NtSetSystemInformation -> 444 */
    96,    /* 92: NtSetSystemPowerState -> 445 */
    97,    /* 93: NtSetSystemTime -> 446 */
    98,    /* 94: NtTraceEvent -> 94 */
    99,    /* 95: NtTranslateFilePath -> 471 */
    100,   /* 96: NtSetValueKey -> 96 */
    101,   /* 97: NtTestAlert -> 467 */
    102,   /* 98: NtLoadDriver -> 269 */
    103,   /* 99: NtIsProcessInJob -> 79 */
    104,   /* 100: NtLoadKey2 -> 272 */
    105,   /* 101: NtLoadKeyEx -> 274 */
    106,   /* 102: NtUnloadDriver -> 473 */
    107,   /* 103: NtUnloadKey -> 474 */
    108,   /* 104: NtUnloadKey2 -> 475 */
    109,   /* 110: NtMakePermanentObject -> 278 */
    110,   /* 111: NtMakeTemporaryObject -> 279 */
    111,   /* 112: NtAlertResumeThread -> 111 */
    112,   /* 113: NtAlertThreadByThreadId -> 113 */
    113,   /* 114: NtAlertThreadByThreadIdEx -> 114 */
    114,   /* 115: NtAllocateLocallyUniqueId -> 115 */
    115,   /* 116: NtAllocateReserveObject -> 116 */
    116,   /* 117: NtAllocateUserPhysicalPages -> 117 */
    117,   /* 118: NtAllocateUserPhysicalPagesEx -> 118 */
    118,   /* 119: NtAllocateUuids -> 119 */
    24,    /* 120: NtAllocateVirtualMemory -> 24 */
    120,   /* 121: NtAllocateVirtualMemoryEx -> 120 */
    121,   /* 122: NtAlpcAcceptConnectPort -> 121 */
    122,   /* 123: NtAlpcCancelMessage -> 122 */
    123,   /* 124: NtAlpcConnectPort -> 123 */
    124,   /* 125: NtAlpcConnectPortEx -> 124 */
    125,   /* 126: NtAlpcCreatePort -> 125 */
    126,   /* 127: NtAlpcCreatePortSection -> 126 */
    127,   /* 128: NtAlpcCreateResourceReserve -> 127 */
    128,   /* 129: NtAlpcCreateSectionView -> 128 */
    129,   /* 130: NtAlpcCreateSecurityContext -> 129 */
    130,   /* 131: NtAlpcDeletePortSection -> 130 */
    131,   /* 132: NtAlpcDeleteResourceReserve -> 131 */
    132,   /* 133: NtAlpcDeleteSectionView -> 132 */
    133,   /* 134: NtAlpcDeleteSecurityContext -> 133 */
    134,   /* 135: NtAlpcDisconnectPort -> 134 */
    135,   /* 136: NtAlpcImpersonateClientContainerOfPort -> 135 */
    136,   /* 137: NtAlpcImpersonateClientOfPort -> 136 */
    137,   /* 138: NtAlpcOpenSenderProcess -> 137 */
    138,   /* 139: NtAlpcOpenSenderThread -> 138 */
    139,   /* 140: NtAlpcQueryInformation -> 139 */
    140,   /* 141: NtAlpcQueryInformationMessage -> 140 */
    141,   /* 142: NtAlpcRevokeSecurityContext -> 141 */
    142,   /* 143: NtAlpcSendWaitReceivePort -> 142 */
    143,   /* 144: NtAlpcSetInformation -> 143 */
    144,   /* 145: NtAssignProcessToJobObject -> 145 */
    145,   /* 146: NtAssociateWaitCompletionPacket -> 146 */
    146,   /* 147: NtCallbackReturn -> 5 */
    147,   /* 148: NtCancelDeviceWakeupRequest -> NOT IN W11 (use 0) */
    148,   /* 149: NtCancelIoFileEx -> 148 */
    149,   /* 150: NtCancelSynchronousIoFile -> 149 */
    150,   /* 151: NtCancelTimer2 -> 150 */
    151,   /* 152: NtCancelWaitCompletionPacket -> 151 */
    152,   /* 153: NtChangeProcessState -> 152 */
    153,   /* 154: NtChangeThreadState -> 153 */
    154,   /* 155: NtClearEvent -> 62 */
    155,   /* 156: NtClose -> 15 */
    156,   /* 157: NtCloseObjectAuditAlarm -> 59 */
    157,   /* 158: NtCompactKeys -> 158 */
    158,   /* 159: NtCompareObjects -> 159 */
    159,   /* 160: NtCompareSigningLevels -> 160 */
    160,   /* 161: NtCompareTokens -> 161 */
    161,   /* 162: NtCompleteConnectPort -> 162 */
    162,   /* 163: NtCompressKey -> 163 */
    163,   /* 164: NtConnectPort -> 164 */
    164,   /* 165: NtContinue -> 67 */
    165,   /* 166: NtConvertBetweenAuxiliaryCounterAndPerformanceCounter -> 166 */
    166,   /* 167: NtCopyFileChunk -> 167 */
    167,   /* 168: NtCreateCrossVmEvent -> 169 */
    168,   /* 169: NtCreateCrossVmMutant -> 170 */
    169,   /* 170: NtCreateDebugObject -> 171 */
    170,   /* 171: NtCreateDirectoryObject -> 172 */
    171,   /* 172: NtCreateDirectoryObjectEx -> 173 */
    171,   /* 173: NtCreateEvent -> 72 */
    172,   /* 174: NtCreateEventPair -> 176 */
    173,   /* 175: NtCreateFile -> 85 */
    174,   /* 176: NtCreateIoCompletion -> 178 */
    175,   /* 177: NtCreateIoRing -> 179 */
    176,   /* 178: NtCreateJobObject -> 180 */
    177,   /* 179: NtCreateJobSet -> 181 */
    178,   /* 180: NtCreateKey -> 29 */
    179,   /* 181: NtCreateKeyTransacted -> 182 */
    180,   /* 182: NtCreateKeyedEvent -> 183 */
    181,   /* 183: NtCreateLowBoxToken -> 184 */
    182,   /* 184: NtCreateMailslotFile -> 185 */
    183,   /* 185: NtCreateMutant -> 186 */
    184,   /* 186: NtCreateNamedPipeFile -> 187 */
    185,   /* 187: NtCreatePagingFile -> 188 */
    186,   /* 188: NtCreatePartition -> 189 */
    187,   /* 189: NtCreatePort -> 190 */
    188,   /* 190: NtCreatePrivateNamespace -> 191 */
    189,   /* 191: NtCreateProcess -> 192 */
    190,   /* 192: NtCreateProcessEx -> 77 */
    191,   /* 193: NtCreateProcessStateChange -> 193 */
    192,   /* 194: NtCreateProfile -> 194 */
    193,   /* 195: NtCreateProfileEx -> 195 */
    194,   /* 196: NtCreateRegistryTransaction -> 196 */
    195,   /* 197: NtCreateResourceManager -> 197 */
    196,   /* 198: NtCreateSection -> 74 */
    197,   /* 199: NtCreateSectionEx -> 74 (same) */
    198,   /* 200: NtCreateSemaphore -> 199 */
    199,   /* 201: NtCreateSymbolicLinkObject -> 200 */
    200,   /* 202: NtCreateThread -> 78 */
    201,   /* 203: NtCreateThreadEx -> 201 */
    202,   /* 204: NtCreateThreadStateChange -> 202 */
    203,   /* 205: NtCreateTimer -> 203 */
    204,   /* 206: NtCreateTimer2 -> 204 */
    205,   /* 207: NtCreateToken -> 205 */
    206,   /* 208: NtCreateTokenEx -> 206 */
    207,   /* 209: NtCreateTransaction -> 207 */
    208,   /* 210: NtCreateTransactionManager -> 208 */
    209,   /* 211: NtCreateUserProcess -> 209 */
    210,   /* 212: NtCreateWaitCompletionPacket -> 210 */
    211,   /* 213: NtCreateWaitablePort -> 211 */
    212,   /* 214: NtCreateWnfStateName -> 212 */
    213,   /* 215: NtCreateWorkerFactory -> 213 */
    214,   /* 216: NtDebugActiveProcess -> 214 */
    215,   /* 217: NtDebugContinue -> 215 */
    216,   /* 218: NtDelayExecution -> 52 */
    217,   /* 219: NtDeleteAtom -> 216 */
    218,   /* 220: NtDeleteBootEntry -> 217 */
    219,   /* 221: NtDeleteDriverEntry -> 218 */
    220,   /* 222: NtDeleteFile -> 219 */
    221,   /* 223: NtDeleteKey -> 220 */
    222,   /* 224: NtDeleteObjectAuditAlarm -> 221 */
    223,   /* 225: NtDeletePrivateNamespace -> 227 */
    224,   /* 226: NtDeleteValueKey -> 223 */
    225,   /* 227: NtDeleteWnfStateData -> 229 */
    226,   /* 228: NtDeleteWnfStateName -> 230 */
    227,   /* 229: NtDeviceIoControlFile -> 7 */
    228,   /* 230: NtDisplayString -> 227 */
    229,   /* 231: NtDuplicateObject -> 60 */
    230,   /* 232: NtDuplicateToken -> 66 */
    231,   /* 233: NtEnumerateBootEntries -> 230 */
    232,   /* 234: NtEnumerateDriverEntries -> 231 */
    233,   /* 235: NtEnumerateKey -> 50 */
    234,   /* 236: NtEnumerateSystemEnvironmentValuesEx -> 232 */
    235,   /* 237: NtEnumerateValueKey -> 19 */
    236,   /* 238: NtExtendSection -> 234 */
    237,   /* 239: NtFilterToken -> 236 */
    238,   /* 240: NtFindAtom -> 20 */
    239,   /* 241: NtFlushBuffersFile -> 75 */
    240,   /* 242: NtFlushInstructionCache -> 240 */
    241,   /* 243: NtFlushKey -> 241 */
    242,   /* 244: NtFlushProcessWriteBuffers -> 242 */
    243,   /* 245: NtFlushVirtualMemory -> 243 */
    244,   /* 246: NtFlushWriteBuffer -> 244 */
    245,   /* 247: NtFreeUserPhysicalPages -> 245 */
    246,   /* 248: NtFreeVirtualMemory -> 30 */
    247,   /* 249: NtFsControlFile -> 57 */
    248,   /* 250: NtGetCachedSigningLevel -> 248 */
    249,   /* 251: NtGetCompleteWnfStateSubscription -> 249 */
    250,   /* 252: NtGetContextThread -> 250 */
    251,   /* 253: NtGetCurrentProcessorNumber -> 0 */
    252,   /* 254: NtGetCurrentProcessorNumberEx -> 252 */
    253,   /* 255: NtGetDevicePowerState -> 253 */
    254,   /* 256: NtGetMUIRegistryInfo -> 254 */
    255,   /* 257: NtGetNextProcess -> 255 */
    256,   /* 258: NtGetNextThread -> 256 */
    257,   /* 259: NtGetNlsSectionPtr -> 257 */
    258,   /* 260: NtGetNotificationResourceManager -> 258 */
    259,   /* 261: NtGetPlugPlayEvent -> NOT IN W11 */
    260,   /* 262: NtGetTickCount -> 259 */
    261,   /* 263: NtGetWriteWatch -> 259 */
    262,   /* 264: NtImpersonateAnonymousToken -> 260 */
    263,   /* 265: NtImpersonateClientOfPort -> 31 */
    264,   /* 266: NtImpersonateThread -> 261 */
    265,   /* 267: NtInitializeEnclave -> 262 */
    266,   /* 268: NtInitializeNlsFiles -> 263 */
    267,   /* 269: NtInitializeRegistry -> 264 */
    268,   /* 270: NtInitiatePowerAction -> 265 */
    269,   /* 271: NtIsProcessInJob -> 79 */
    270,   /* 272: NtIsSystemResumeAutomatic -> 266 */
    271,   /* 273: NtIsUILanguageComitted -> 267 */
    272,   /* 274: NtListenPort -> 268 */
    273,   /* 275: NtLoadDriver -> 269 */
    274,   /* 276: NtLoadEnclaveData -> 270 */
    275,   /* 277: NtLoadKey -> 271 */
    276,   /* 278: NtLoadKey2 -> 272 */
    277,   /* 279: NtLoadKey3 -> 273 */
    278,   /* 280: NtLoadKeyEx -> 274 */
    279,   /* 281: NtManageHotPatch -> 281 */
    280,   /* 282: NtManagePartition -> 282 */
    281,   /* 283: NtManageWobTicket -> 283 */
    282,   /* 284: NtMapCMFModule -> 284 */
    283,   /* 285: NtMapUserPhysicalPages -> 285 */
    284,   /* 286: NtMapViewOfSection -> 40 */
    285,   /* 287: NtMapViewOfSectionEx -> 286 */
    286,   /* 288: NtModifyBootEntry -> 287 */
    287,   /* 289: NtModifyDriverEntry -> 288 */
    288,   /* 290: NtNotifyChangeDirectoryFile -> 289 */
    289,   /* 291: NtNotifyChangeDirectoryFileEx -> 290 */
    290,   /* 292: NtNotifyChangeKey -> 291 */
    291,   /* 293: NtNotifyChangeMultipleKeys -> 292 */
    292,   /* 294: NtNotifyChangeSession -> 293 */
    293,   /* 295: NtOpenDirectoryObject -> 88 */
    294,   /* 296: NtOpenEvent -> 64 */
    295,   /* 297: NtOpenEventPair -> 296 */
    296    /* 298: NtOpenFile -> 51 */
};

/* Helper: Get W11 ordinal for a ReactOS ordinal (1-based) */
static inline uint16_t ros_to_w11(uint16_t ros_ord) {
    if (ros_ord >= 1 && ros_ord <= 296) {
        return ros_to_w11_ordinal[ros_ord - 1];
    }
    return 0;
}

/* Helper: Register at W11 ordinal (1-based) in dispatch table */
#define NT_W11_REG(tbl, ros_ord, handler) \
    do { \
        uint16_t w11_ord = ros_to_w11(ros_ord); \
        if (w11_ord > 0 && w11_ord <= 490) { \
            tbl[w11_ord - 1] = handler; \
        } \
    } while (0)

#endif /* WUBU_VSL_NT_ORDINAL_TRANSLATE_H */