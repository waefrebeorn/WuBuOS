/*
 * efi.h  --  WuBuFW: UEFI interface types, implemented from scratch in C11.
 *
 * No EDK2, no gnu-efi, no OVMF. Every structure here is written against the
 * UEFI 2.10 specification layout so that stock PE32+ EFI applications
 * (BOOTX64.EFI) can be loaded and executed by our own firmware.
 *
 * ABI: UEFI uses the Microsoft x64 calling convention. On SysV toolchains we
 * mark every function pointer and every implementation with EFIAPI
 * (__attribute__((ms_abi))). Getting this wrong is silent stack corruption,
 * so it is applied uniformly.
 */

#ifndef WUBUFW_EFI_H
#define WUBUFW_EFI_H

#include <stdint.h>
#include <stddef.h>

#define EFIAPI __attribute__((ms_abi))

typedef uint8_t   BOOLEAN;
typedef int64_t   INTN;
typedef uint64_t  UINTN;
typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;
typedef int8_t    INT8;
typedef int16_t   INT16;
typedef int32_t   INT32;
typedef int64_t   INT64;
typedef uint16_t  CHAR16;
typedef char      CHAR8;
typedef void      VOID;
typedef UINTN     EFI_STATUS;
typedef VOID     *EFI_HANDLE;
typedef VOID     *EFI_EVENT;
typedef UINT64    EFI_PHYSICAL_ADDRESS;
typedef UINT64    EFI_VIRTUAL_ADDRESS;
typedef UINT64    EFI_LBA;
typedef UINTN     EFI_TPL;

#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL ((void *)0)
#endif

/* -- Status codes (high bit set = error) ------------------------------- */
#define EFI_ERROR_BIT              ((UINTN)1 << 63)
#define EFIERR(x)                  (EFI_ERROR_BIT | (UINTN)(x))
#define EFI_SUCCESS                0
#define EFI_LOAD_ERROR             EFIERR(1)
#define EFI_INVALID_PARAMETER      EFIERR(2)
#define EFI_UNSUPPORTED            EFIERR(3)
#define EFI_BAD_BUFFER_SIZE        EFIERR(4)
#define EFI_BUFFER_TOO_SMALL       EFIERR(5)
#define EFI_NOT_READY              EFIERR(6)
#define EFI_DEVICE_ERROR           EFIERR(7)
#define EFI_WRITE_PROTECTED        EFIERR(8)
#define EFI_OUT_OF_RESOURCES       EFIERR(9)
#define EFI_NOT_FOUND              EFIERR(14)
#define EFI_ACCESS_DENIED          EFIERR(15)
#define EFI_TIMEOUT                EFIERR(18)
#define EFI_ABORTED                EFIERR(21)
#define EFI_SECURITY_VIOLATION     EFIERR(26)
#define EFI_ERROR(s)               (((UINTN)(s) & EFI_ERROR_BIT) != 0)

/* -- GUID -------------------------------------------------------------- */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* -- Table header ------------------------------------------------------ */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

#define EFI_SYSTEM_TABLE_SIGNATURE   0x5453595320494249ULL /* "IBI SYST" */
#define EFI_BOOT_SERVICES_SIGNATURE  0x56524553544f4f42ULL /* "BOOTSERV" */
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552ULL /* "RUNTSERV" */
#define EFI_2_100_SYSTEM_TABLE_REVISION ((2 << 16) | 100)

/* -- Memory ------------------------------------------------------------ */
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

#define EFI_PAGE_SIZE  4096
#define EFI_PAGE_SHIFT 12

#define EFI_MEMORY_UC   0x0000000000000001ULL
#define EFI_MEMORY_WC   0x0000000000000002ULL
#define EFI_MEMORY_WT   0x0000000000000004ULL
#define EFI_MEMORY_WB   0x0000000000000008ULL
#define EFI_MEMORY_RUNTIME 0x8000000000000000ULL

typedef struct {
    UINT32               Type;
    UINT32               Pad;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS  VirtualStart;
    UINT64               NumberOfPages;
    UINT64               Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* -- Time -------------------------------------------------------------- */
typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

typedef struct {
    UINT32  Resolution;
    UINT32  Accuracy;
    BOOLEAN SetsToZero;
} EFI_TIME_CAPABILITIES;

/* -- Simple Text Output ------------------------------------------------ */
typedef struct {
    INT32   MaxMode;
    INT32   Mode;
    INT32   Attribute;
    INT32   CursorColumn;
    INT32   CursorRow;
    BOOLEAN CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct { UINT8 Addr[32]; } EFI_MAC_ADDRESS;
typedef union {
    UINT8   Addr[4];
} EFI_IPv4_ADDRESS;
typedef union {
    UINT8   Addr[16];
} EFI_IPv6_ADDRESS;
typedef union {
    UINT32            v4;
    EFI_IPv4_ADDRESS  v4u;
    EFI_IPv6_ADDRESS  v6u;
    UINT64            Addr[2];
} EFI_IP_ADDRESS;

/* -- Simple Network Protocol ----------------------------------------- */
typedef enum {
    EfiSimpleNetworkStopped, EfiSimpleNetworkStarted, EfiSimpleNetworkInitialized
} EFI_SIMPLE_NETWORK_STATE;

typedef struct {
    UINT32 State;
    UINT32 HwAddressSize;
    UINT32 MediaHeaderSize;
    UINT32 MaxPacketSize;
    UINT32 NvRamSize;
    UINT32 NvRamAccessSize;
    UINT32 ReceiveFilterMask;
    UINT32 ReceiveFilterSetting;
    UINT32 MaxMCastFilterCount;
    UINT32 MCastFilterCount;
    UINT8  MCastFilter[16][32];
    EFI_MAC_ADDRESS BroadcastAddress;
    EFI_MAC_ADDRESS PermanentAddress;
    EFI_MAC_ADDRESS CurrentAddress;
    EFI_MAC_ADDRESS *MulticastAddress;
    UINT8  IfType;
    BOOLEAN MacAddressChangeable;
    BOOLEAN MultipleTxSupported;
    UINT8  MediaPresentSupported;
    UINT8  MediaPresent;
} EFI_SIMPLE_NETWORK_MODE;

struct _EFI_SIMPLE_NETWORK_PROTOCOL;
typedef struct _EFI_SIMPLE_NETWORK_PROTOCOL EFI_SIMPLE_NETWORK_PROTOCOL;
struct _EFI_SIMPLE_NETWORK_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Start)(EFI_SIMPLE_NETWORK_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Stop)(EFI_SIMPLE_NETWORK_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Initialize)(EFI_SIMPLE_NETWORK_PROTOCOL *This, UINTN ExtraRx, UINTN ExtraTx);
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_NETWORK_PROTOCOL *This, BOOLEAN Verifier);
    EFI_STATUS (EFIAPI *Shutdown)(EFI_SIMPLE_NETWORK_PROTOCOL *This);
    EFI_STATUS (EFIAPI *ReceiveFilters)(EFI_SIMPLE_NETWORK_PROTOCOL *This, UINT32 Enable, UINT32 Disable, BOOLEAN Reset, UINTN McastCount, EFI_MAC_ADDRESS *McastAddr);
    EFI_STATUS (EFIAPI *StationAddress)(EFI_SIMPLE_NETWORK_PROTOCOL *This, BOOLEAN Reset, EFI_MAC_ADDRESS *New);
    EFI_STATUS (EFIAPI *Statistics)(EFI_SIMPLE_NETWORK_PROTOCOL *This, BOOLEAN Reset, UINTN *StatSize, VOID *StatTable);
    EFI_STATUS (EFIAPI *MCastIPtoMAC)(EFI_SIMPLE_NETWORK_PROTOCOL *This, BOOLEAN IsIPv6, EFI_IP_ADDRESS *Ip, EFI_MAC_ADDRESS *Mac);
    EFI_STATUS (EFIAPI *NvData)(EFI_SIMPLE_NETWORK_PROTOCOL *This, BOOLEAN Read, UINTN Offset, UINTN Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *GetStatus)(EFI_SIMPLE_NETWORK_PROTOCOL *This, UINT32 *IrqStatus, VOID **TxBuf);
    EFI_STATUS (EFIAPI *Transmit)(EFI_SIMPLE_NETWORK_PROTOCOL *This, UINTN HeaderSize, UINTN BufferSize, VOID *Buffer, EFI_MAC_ADDRESS *Src, EFI_MAC_ADDRESS *Dst, UINT16 *Protocol);
    EFI_STATUS (EFIAPI *Receive)(EFI_SIMPLE_NETWORK_PROTOCOL *This, UINTN *HeaderSize, UINTN *BufferSize, VOID *Buffer, EFI_MAC_ADDRESS *Src, EFI_MAC_ADDRESS *Dst, UINT16 *Protocol);
    EFI_SIMPLE_NETWORK_MODE *Mode;
    EFI_STATUS (EFIAPI *WaitForPacket)(EFI_SIMPLE_NETWORK_PROTOCOL *This, UINTN *Mask);
};

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN Extended);
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
    EFI_STATUS (EFIAPI *TestString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
    EFI_STATUS (EFIAPI *QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber, UINTN *Cols, UINTN *Rows);
    EFI_STATUS (EFIAPI *SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber);
    EFI_STATUS (EFIAPI *SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Attribute);
    EFI_STATUS (EFIAPI *ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    EFI_STATUS (EFIAPI *SetCursorPosition)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Col, UINTN Row);
    EFI_STATUS (EFIAPI *EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN Visible);
    SIMPLE_TEXT_OUTPUT_MODE *Mode;
};

/* -- Simple Text Input ------------------------------------------------- */
typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN Extended);
    EFI_STATUS (EFIAPI *ReadKeyStroke)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);
    EFI_EVENT  WaitForKey;
};

/* -- Device path ------------------------------------------------------- */
typedef struct {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
} EFI_DEVICE_PATH_PROTOCOL;

#define EFI_DP_TYPE_MEDIA      0x04
#define EFI_DP_MEDIA_FILEPATH  0x04
#define EFI_DP_TYPE_END        0x7F
#define EFI_DP_END_ENTIRE      0xFF

/* -- Block IO ---------------------------------------------------------- */
typedef struct {
    UINT32  MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32  BlockSize;
    UINT32  IoAlign;
    EFI_LBA LastBlock;
} EFI_BLOCK_IO_MEDIA;

struct _EFI_BLOCK_IO_PROTOCOL;
typedef struct _EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;
struct _EFI_BLOCK_IO_PROTOCOL {
    UINT64 Revision;
    EFI_BLOCK_IO_MEDIA *Media;
    EFI_STATUS (EFIAPI *Reset)(EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN Extended);
    EFI_STATUS (EFIAPI *ReadBlocks)(EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba, UINTN Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *WriteBlocks)(EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba, UINTN Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *FlushBlocks)(EFI_BLOCK_IO_PROTOCOL *This);
};

/* -- File protocol ----------------------------------------------------- */
#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL
#define EFI_FILE_READ_ONLY   0x01
#define EFI_FILE_DIRECTORY   0x10

struct _EFI_FILE_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
struct _EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **New, CHAR16 *Name, UINT64 Mode, UINT64 Attr);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Delete)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL *This, UINTN *Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *Write)(EFI_FILE_PROTOCOL *This, UINTN *Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL *This, UINT64 *Pos);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL *This, UINT64 Pos);
    EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL *This, EFI_GUID *Type, UINTN *Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *SetInfo)(EFI_FILE_PROTOCOL *This, EFI_GUID *Type, UINTN Size, VOID *Buffer);
    EFI_STATUS (EFIAPI *Flush)(EFI_FILE_PROTOCOL *This);
};

typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64   Attribute;
    CHAR16   FileName[1];
} EFI_FILE_INFO;

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_PROTOCOL **Root);
};

/* -- Loaded image ------------------------------------------------------ */
struct _EFI_SYSTEM_TABLE;
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;

typedef struct {
    UINT32            Revision;
    EFI_HANDLE        ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE        DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    VOID             *Reserved;
    UINT32            LoadOptionsSize;
    VOID             *LoadOptions;
    VOID             *ImageBase;
    UINT64            ImageSize;
    EFI_MEMORY_TYPE   ImageCodeType;
    EFI_MEMORY_TYPE   ImageDataType;
    EFI_STATUS (EFIAPI *Unload)(EFI_HANDLE ImageHandle);
} EFI_LOADED_IMAGE_PROTOCOL;

/* -- Graphics output --------------------------------------------------- */
typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 RedMask, GreenMask, BlueMask, ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
    UINT32                    Version;
    UINT32                    HorizontalResolution;
    UINT32                    VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK         PixelInformation;
    UINT32                    PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN  SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct { UINT8 Blue, Green, Red, Reserved; } EFI_GRAPHICS_OUTPUT_BLT_PIXEL;
typedef enum { EfiBltVideoFill, EfiBltVideoToBltBuffer, EfiBltBufferToVideo, EfiBltVideoToVideo, EfiGraphicsOutputBltOperationMax } EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

struct _EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode, UINTN *SizeOfInfo, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFI_STATUS (EFIAPI *SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode);
    EFI_STATUS (EFIAPI *Blt)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buf,
                             EFI_GRAPHICS_OUTPUT_BLT_OPERATION Op, UINTN sx, UINTN sy, UINTN dx, UINTN dy,
                             UINTN w, UINTN h, UINTN delta);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

/* -- Configuration table ----------------------------------------------- */
typedef struct {
    EFI_GUID VendorGuid;
    VOID    *VendorTable;
} EFI_CONFIGURATION_TABLE;

/* -- Events / TPL ------------------------------------------------------ */
#define TPL_APPLICATION 4
#define TPL_CALLBACK    8
#define TPL_NOTIFY      16
#define TPL_HIGH_LEVEL  31

#define EVT_TIMER                         0x80000000
#define EVT_RUNTIME                       0x40000000
#define EVT_NOTIFY_WAIT                   0x00000100
#define EVT_NOTIFY_SIGNAL                 0x00000200
#define EVT_SIGNAL_EXIT_BOOT_SERVICES     0x00000201
#define EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE 0x60000202

typedef VOID (EFIAPI *EFI_EVENT_NOTIFY)(EFI_EVENT Event, VOID *Context);
typedef enum { TimerCancel, TimerPeriodic, TimerRelative } EFI_TIMER_DELAY;
typedef enum { EFI_NATIVE_INTERFACE } EFI_INTERFACE_TYPE;
typedef enum { AllHandles, ByRegisterNotify, ByProtocol } EFI_LOCATE_SEARCH_TYPE;
typedef enum { EfiResetCold, EfiResetWarm, EfiResetShutdown, EfiResetPlatformSpecific } EFI_RESET_TYPE;

/* -- Runtime services -------------------------------------------------- */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    EFI_STATUS (EFIAPI *GetTime)(EFI_TIME *Time, EFI_TIME_CAPABILITIES *Caps);
    EFI_STATUS (EFIAPI *SetTime)(EFI_TIME *Time);
    EFI_STATUS (EFIAPI *GetWakeupTime)(BOOLEAN *Enabled, BOOLEAN *Pending, EFI_TIME *Time);
    EFI_STATUS (EFIAPI *SetWakeupTime)(BOOLEAN Enable, EFI_TIME *Time);
    EFI_STATUS (EFIAPI *SetVirtualAddressMap)(UINTN MapSize, UINTN DescSize, UINT32 DescVersion, EFI_MEMORY_DESCRIPTOR *Map);
    EFI_STATUS (EFIAPI *ConvertPointer)(UINTN DebugDisposition, VOID **Address);
    EFI_STATUS (EFIAPI *GetVariable)(CHAR16 *Name, EFI_GUID *Guid, UINT32 *Attr, UINTN *Size, VOID *Data);
    EFI_STATUS (EFIAPI *GetNextVariableName)(UINTN *NameSize, CHAR16 *Name, EFI_GUID *Guid);
    EFI_STATUS (EFIAPI *SetVariable)(CHAR16 *Name, EFI_GUID *Guid, UINT32 Attr, UINTN Size, VOID *Data);
    EFI_STATUS (EFIAPI *GetNextHighMonotonicCount)(UINT32 *HighCount);
    VOID       (EFIAPI *ResetSystem)(EFI_RESET_TYPE Type, EFI_STATUS Status, UINTN DataSize, VOID *Data);
    EFI_STATUS (EFIAPI *UpdateCapsule)(VOID **CapsuleHeaderArray, UINTN CapsuleCount, EFI_PHYSICAL_ADDRESS ScatterGatherList);
    EFI_STATUS (EFIAPI *QueryCapsuleCapabilities)(VOID **CapsuleHeaderArray, UINTN CapsuleCount, UINT64 *MaximumCapsuleSize, EFI_RESET_TYPE *ResetType);
    EFI_STATUS (EFIAPI *QueryVariableInfo)(UINT32 Attr, UINT64 *MaxStorage, UINT64 *RemainingStorage, UINT64 *MaxVarSize);
} EFI_RUNTIME_SERVICES;

/* -- Boot services ----------------------------------------------------- */
typedef struct {
    EFI_TABLE_HEADER Hdr;

    EFI_TPL    (EFIAPI *RaiseTPL)(EFI_TPL NewTpl);
    VOID       (EFIAPI *RestoreTPL)(EFI_TPL OldTpl);

    EFI_STATUS (EFIAPI *AllocatePages)(EFI_ALLOCATE_TYPE Type, EFI_MEMORY_TYPE MemType, UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN Pages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *MapSize, EFI_MEMORY_DESCRIPTOR *Map, UINTN *MapKey, UINTN *DescSize, UINT32 *DescVersion);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size, VOID **Buffer);
    EFI_STATUS (EFIAPI *FreePool)(VOID *Buffer);

    EFI_STATUS (EFIAPI *CreateEvent)(UINT32 Type, EFI_TPL NotifyTpl, EFI_EVENT_NOTIFY NotifyFunction, VOID *NotifyContext, EFI_EVENT *Event);
    EFI_STATUS (EFIAPI *SetTimer)(EFI_EVENT Event, EFI_TIMER_DELAY Type, UINT64 TriggerTime);
    EFI_STATUS (EFIAPI *WaitForEvent)(UINTN NumberOfEvents, EFI_EVENT *Event, UINTN *Index);
    EFI_STATUS (EFIAPI *SignalEvent)(EFI_EVENT Event);
    EFI_STATUS (EFIAPI *CloseEvent)(EFI_EVENT Event);
    EFI_STATUS (EFIAPI *CheckEvent)(EFI_EVENT Event);

    EFI_STATUS (EFIAPI *InstallProtocolInterface)(EFI_HANDLE *Handle, EFI_GUID *Protocol, EFI_INTERFACE_TYPE Type, VOID *Interface);
    EFI_STATUS (EFIAPI *ReinstallProtocolInterface)(EFI_HANDLE Handle, EFI_GUID *Protocol, VOID *Old, VOID *New);
    EFI_STATUS (EFIAPI *UninstallProtocolInterface)(EFI_HANDLE Handle, EFI_GUID *Protocol, VOID *Interface);
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, VOID **Interface);
    VOID      *Reserved;
    EFI_STATUS (EFIAPI *RegisterProtocolNotify)(EFI_GUID *Protocol, EFI_EVENT Event, VOID **Registration);
    EFI_STATUS (EFIAPI *LocateHandle)(EFI_LOCATE_SEARCH_TYPE SearchType, EFI_GUID *Protocol, VOID *SearchKey, UINTN *BufferSize, EFI_HANDLE *Buffer);
    EFI_STATUS (EFIAPI *LocateDevicePath)(EFI_GUID *Protocol, EFI_DEVICE_PATH_PROTOCOL **DevicePath, EFI_HANDLE *Device);
    EFI_STATUS (EFIAPI *InstallConfigurationTable)(EFI_GUID *Guid, VOID *Table);

    EFI_STATUS (EFIAPI *LoadImage)(BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle, EFI_DEVICE_PATH_PROTOCOL *DevicePath, VOID *SourceBuffer, UINTN SourceSize, EFI_HANDLE *ImageHandle);
    EFI_STATUS (EFIAPI *StartImage)(EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData);
    EFI_STATUS (EFIAPI *Exit)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus, UINTN ExitDataSize, CHAR16 *ExitData);
    EFI_STATUS (EFIAPI *UnloadImage)(EFI_HANDLE ImageHandle);
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);

    EFI_STATUS (EFIAPI *GetNextMonotonicCount)(UINT64 *Count);
    EFI_STATUS (EFIAPI *Stall)(UINTN Microseconds);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(UINTN Timeout, UINT64 WatchdogCode, UINTN DataSize, CHAR16 *WatchdogData);

    EFI_STATUS (EFIAPI *ConnectController)(EFI_HANDLE Controller, EFI_HANDLE *DriverImageHandle, EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath, BOOLEAN Recursive);
    EFI_STATUS (EFIAPI *DisconnectController)(EFI_HANDLE Controller, EFI_HANDLE DriverImageHandle, EFI_HANDLE ChildHandle);

    EFI_STATUS (EFIAPI *OpenProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, VOID **Interface, EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle, UINT32 Attributes);
    EFI_STATUS (EFIAPI *CloseProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle);
    EFI_STATUS (EFIAPI *OpenProtocolInformation)(EFI_HANDLE Handle, EFI_GUID *Protocol, VOID **EntryBuffer, UINTN *EntryCount);

    EFI_STATUS (EFIAPI *ProtocolsPerHandle)(EFI_HANDLE Handle, EFI_GUID ***ProtocolBuffer, UINTN *ProtocolBufferCount);
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(EFI_LOCATE_SEARCH_TYPE SearchType, EFI_GUID *Protocol, VOID *SearchKey, UINTN *NoHandles, EFI_HANDLE **Buffer);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, VOID *Registration, VOID **Interface);
    EFI_STATUS (EFIAPI *InstallMultipleProtocolInterfaces)(EFI_HANDLE *Handle, ...);
    EFI_STATUS (EFIAPI *UninstallMultipleProtocolInterfaces)(EFI_HANDLE Handle, ...);

    EFI_STATUS (EFIAPI *CalculateCrc32)(VOID *Data, UINTN DataSize, UINT32 *Crc32);

    VOID       (EFIAPI *CopyMem)(VOID *Destination, VOID *Source, UINTN Length);
    VOID       (EFIAPI *SetMem)(VOID *Buffer, UINTN Size, UINT8 Value);
    EFI_STATUS (EFIAPI *CreateEventEx)(UINT32 Type, EFI_TPL NotifyTpl, EFI_EVENT_NOTIFY NotifyFunction, const VOID *NotifyContext, const EFI_GUID *EventGroup, EFI_EVENT *Event);
} EFI_BOOT_SERVICES;

struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER                 Hdr;
    CHAR16                          *FirmwareVendor;
    UINT32                           FirmwareRevision;
    EFI_HANDLE                       ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *ConIn;
    EFI_HANDLE                       ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                       StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES            *RuntimeServices;
    EFI_BOOT_SERVICES               *BootServices;
    UINTN                            NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE         *ConfigurationTable;
};

typedef EFI_STATUS (EFIAPI *EFI_IMAGE_ENTRY_POINT)(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

/* -- Well-known GUIDs (defined in fw_guid.c) --------------------------- */
extern EFI_GUID gEfiLoadedImageProtocolGuid;
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiSimpleNetworkProtocolGuid;
extern EFI_GUID gEfiPciRootBridgeIoProtocolGuid;
extern EFI_GUID gEfiLoadedImageProtocolGuid;
extern EFI_GUID gEfiBlockIoProtocolGuid;
extern EFI_GUID gEfiDevicePathProtocolGuid;
extern EFI_GUID gEfiSimpleTextOutProtocolGuid;
extern EFI_GUID gEfiSimpleTextInProtocolGuid;
extern EFI_GUID gEfiGraphicsOutputProtocolGuid;
extern EFI_GUID gEfiFileInfoGuid;
extern EFI_GUID gEfiAcpi20TableGuid;

#endif /* WUBUFW_EFI_H */
