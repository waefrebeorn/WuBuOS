/*
 * hello.c  --  WuBuOS test EFI application.
 *
 * Exercises the firmware for real: ConOut, memory map, AllocatePool,
 * GetTime, SetVariable/GetVariable, LocateHandleBuffer + SimpleFileSystem
 * open/read of its own binary, then ExitBootServices. Prints PASS/FAIL per
 * check and a summary, so booting it is the firmware's regression test.
 *
 * Linked flat at a fixed base and wrapped into PE32+ by mkpe.
 */

#include "../efi.h"

static EFI_SYSTEM_TABLE *ST;
static EFI_BOOT_SERVICES *BS;
static int g_pass, g_fail;

static void out(const CHAR16 *s) { ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)s); }

static void outhex(UINT64 v) {
    CHAR16 buf[19];
    const CHAR16 *d = u"0123456789ABCDEF";
    int i = 18;
    buf[18] = 0;
    if (!v) { out(u"0"); return; }
    while (v && i > 0) { buf[--i] = d[v & 0xF]; v >>= 4; }
    out(&buf[i]);
}

static void outdec(UINT64 v) {
    CHAR16 buf[21];
    int i = 20;
    buf[20] = 0;
    if (!v) { out(u"0"); return; }
    while (v && i > 0) { buf[--i] = (CHAR16)(u'0' + (v % 10)); v /= 10; }
    out(&buf[i]);
}

static void check(const CHAR16 *name, int ok) {
    out(ok ? u"  [PASS] " : u"  [FAIL] ");
    out(name);
    out(u"\r\n");
    if (ok) g_pass++; else g_fail++;
}

static EFI_GUID var_guid =
    { 0x11112222, 0x3333, 0x4444, { 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC } };

/* The payload links standalone, so it carries its own copies of the
 * well-known GUIDs rather than borrowing the firmware's symbols. */
EFI_GUID gEfiLoadedImageProtocolGuid =
    { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiSimpleFileSystemProtocolGuid =
    { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiBlockIoProtocolGuid =
    { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiDevicePathProtocolGuid =
    { 0x09576E91, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiSimpleTextOutProtocolGuid =
    { 0x387477C2, 0x69C7, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiSimpleTextInProtocolGuid =
    { 0x387477C1, 0x69C7, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiGraphicsOutputProtocolGuid =
    { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } };
EFI_GUID gEfiFileInfoGuid =
    { 0x09576E92, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID gEfiAcpi20TableGuid =
    { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } };

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    ST = st;
    BS = st->BootServices;

    out(u"\r\n");
    out(u"=== WuBuOS EFI payload: firmware conformance run ===\r\n");
    out(u"Firmware vendor: ");
    out(st->FirmwareVendor ? st->FirmwareVendor : u"(none)");
    out(u"\r\n");

    check(u"SystemTable signature", st->Hdr.Signature == EFI_SYSTEM_TABLE_SIGNATURE);
    check(u"BootServices present", BS != NULL &&
          BS->Hdr.Signature == EFI_BOOT_SERVICES_SIGNATURE);
    check(u"RuntimeServices present", st->RuntimeServices != NULL);

    /* --- AllocatePool / FreePool --- */
    void *pool = NULL;
    EFI_STATUS s = BS->AllocatePool(EfiLoaderData, 4096, &pool);
    check(u"AllocatePool 4KB", !EFI_ERROR(s) && pool != NULL);
    if (pool) {
        unsigned char *p = pool;
        for (int i = 0; i < 4096; i++) p[i] = (unsigned char)(i & 0xFF);
        int ok = 1;
        for (int i = 0; i < 4096; i++) if (p[i] != (unsigned char)(i & 0xFF)) ok = 0;
        check(u"Pool memory readback", ok);
        check(u"FreePool", !EFI_ERROR(BS->FreePool(pool)));
    }

    /* --- AllocatePages --- */
    EFI_PHYSICAL_ADDRESS pg = 0;
    s = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 16, &pg);
    check(u"AllocatePages 16 pages", !EFI_ERROR(s) && pg != 0 && (pg & 0xFFF) == 0);
    if (pg) check(u"FreePages", !EFI_ERROR(BS->FreePages(pg, 16)));

    /* --- Memory map --- */
    UINTN msize = 0, mkey = 0, dsize = 0;
    UINT32 dver = 0;
    s = BS->GetMemoryMap(&msize, NULL, &mkey, &dsize, &dver);
    check(u"GetMemoryMap sizing", s == EFI_BUFFER_TOO_SMALL && msize > 0);
    out(u"        map bytes=");  outdec(msize);
    out(u" descsize=");          outdec(dsize);
    out(u"\r\n");

    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN alloc = msize + 4 * dsize;
    if (!EFI_ERROR(BS->AllocatePool(EfiLoaderData, alloc, (void **)&map))) {
        UINTN got = alloc;
        s = BS->GetMemoryMap(&got, map, &mkey, &dsize, &dver);
        check(u"GetMemoryMap fetch", !EFI_ERROR(s));
        UINT64 conv = 0;
        UINTN n = got / dsize;
        for (UINTN i = 0; i < n; i++) {
            EFI_MEMORY_DESCRIPTOR *d =
                (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + i * dsize);
            if (d->Type == EfiConventionalMemory) conv += d->NumberOfPages;
        }
        out(u"        entries="); outdec(n);
        out(u" free MB=");        outdec(conv * 4096 / (1024 * 1024));
        out(u"\r\n");
        check(u"Conventional memory > 16MB", conv * 4096 > 16u * 1024 * 1024);
    }

    /* --- Stall + monotonic count --- */
    UINT64 c1 = 0, c2 = 0;
    BS->GetNextMonotonicCount(&c1);
    BS->Stall(2000);
    BS->GetNextMonotonicCount(&c2);
    check(u"Monotonic count increments", c2 > c1);

    /* --- Events + timer (real elapsed time, not a tick counter) --- */
    EFI_EVENT tev = NULL;
    s = BS->CreateEvent(EVT_TIMER, TPL_APPLICATION, NULL, NULL, &tev);
    check(u"CreateEvent(EVT_TIMER)", !EFI_ERROR(s) && tev != NULL);
    if (tev) {
        /* 50ms relative timer: must not be ready immediately, must be
         * ready after stalling past its deadline. */
        s = BS->SetTimer(tev, TimerRelative, 500000);   /* 100ns units */
        check(u"SetTimer relative", !EFI_ERROR(s));
        check(u"Timer not ready early", BS->CheckEvent(tev) == EFI_NOT_READY);
        BS->Stall(80000);
        check(u"Timer fires after deadline", BS->CheckEvent(tev) == EFI_SUCCESS);
        check(u"CloseEvent", !EFI_ERROR(BS->CloseEvent(tev)));
    }

    /* --- Runtime: GetTime --- */
    EFI_TIME t;
    s = ST->RuntimeServices->GetTime(&t, NULL);
    check(u"RT GetTime", !EFI_ERROR(s) && t.Year >= 2000 && t.Month >= 1 && t.Month <= 12);
    out(u"        RTC ");   outdec(t.Year);
    out(u"-");              outdec(t.Month);
    out(u"-");              outdec(t.Day);
    out(u" ");              outdec(t.Hour);
    out(u":");              outdec(t.Minute);
    out(u"\r\n");

    /* --- Runtime: variables --- */
    UINT8 vdata[8] = { 'W','u','B','u','F','W','!',0 };
    s = ST->RuntimeServices->SetVariable(u"WubuTest", &var_guid, 0x7, sizeof(vdata), vdata);
    check(u"RT SetVariable", !EFI_ERROR(s));
    UINT8 rdata[8] = {0};
    UINTN rsize = sizeof(rdata);
    UINT32 rattr = 0;
    s = ST->RuntimeServices->GetVariable(u"WubuTest", &var_guid, &rattr, &rsize, rdata);
    int vok = !EFI_ERROR(s) && rsize == sizeof(vdata);
    for (UINTN i = 0; vok && i < rsize; i++) if (rdata[i] != vdata[i]) vok = 0;
    check(u"RT GetVariable roundtrip", vok);

    /* --- LoadedImage --- */
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    s = BS->HandleProtocol(image, &gEfiLoadedImageProtocolGuid, (void **)&li);
    check(u"HandleProtocol LoadedImage", !EFI_ERROR(s) && li != NULL);
    if (li) {
        out(u"        image base=0x"); outhex((UINT64)(UINTN)li->ImageBase);
        out(u" size=");                outdec(li->ImageSize);
        out(u"\r\n");
    }

    /* --- Filesystem --- */
    UINTN nfs = 0;
    EFI_HANDLE *fsh = NULL;
    s = BS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &nfs, &fsh);
    check(u"LocateHandleBuffer SimpleFS", !EFI_ERROR(s) && nfs > 0);
    if (!EFI_ERROR(s) && nfs > 0) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        if (!EFI_ERROR(BS->HandleProtocol(fsh[0], &gEfiSimpleFileSystemProtocolGuid, (void **)&fs))) {
            EFI_FILE_PROTOCOL *root = NULL;
            check(u"OpenVolume", !EFI_ERROR(fs->OpenVolume(fs, &root)) && root != NULL);
            if (root) {
                EFI_FILE_PROTOCOL *f = NULL;
                s = root->Open(root, &f, u"\\EFI\\BOOT\\BOOTX64.EFI", EFI_FILE_MODE_READ, 0);
                check(u"Open BOOTX64.EFI", !EFI_ERROR(s) && f != NULL);
                if (f) {
                    UINT8 hdr[64];
                    UINTN rn = sizeof(hdr);
                    s = f->Read(f, &rn, hdr);
                    check(u"Read 64 bytes", !EFI_ERROR(s) && rn == sizeof(hdr));
                    check(u"File starts with MZ", hdr[0] == 'M' && hdr[1] == 'Z');
                    UINT64 pos = 0;
                    f->GetPosition(f, &pos);
                    check(u"GetPosition after read", pos == 64);
                    check(u"SetPosition rewind", !EFI_ERROR(f->SetPosition(f, 0)));
                    f->Close(f);
                }
            }
        }
    }

    /* --- Summary --- */
    out(u"\r\n=== results: ");
    outdec((UINT64)g_pass);
    out(u" passed, ");
    outdec((UINT64)g_fail);
    out(u" failed ===\r\n");

    if (g_fail == 0) out(u"WUBUFW_SELFTEST_OK\r\n");
    else             out(u"WUBUFW_SELFTEST_FAIL\r\n");

    /* --- ExitBootServices (last: console may stop working) --- */
    UINTN esize = 0, ekey = 0, edsz = 0;
    UINT32 edver = 0;
    BS->GetMemoryMap(&esize, NULL, &ekey, &edsz, &edver);
    EFI_MEMORY_DESCRIPTOR *emap = NULL;
    if (!EFI_ERROR(BS->AllocatePool(EfiLoaderData, esize + 4 * edsz, (void **)&emap))) {
        UINTN got = esize + 4 * edsz;
        if (!EFI_ERROR(BS->GetMemoryMap(&got, emap, &ekey, &edsz, &edver))) {
            out(u"Calling ExitBootServices...\r\n");
            if (EFI_ERROR(BS->ExitBootServices(image, ekey)))
                out(u"  [FAIL] ExitBootServices\r\n");
        }
    }

    for (;;) __asm__ volatile("cli; hlt");
    return EFI_SUCCESS;
}
