/* Minimal HolyC test - no typedefs, no forward declarations */

U64 VBEFillRect(U64 x, U64 y, U64 w, U64 h, U64 color) { return 0; }
U64 VBEDrawText(U64 x, U64 y, U8 *str, U64 color, U64 scale) { return 0; }
U64 VBESwap() { return 0; }
U64 WMCreateWin(U64 x, U64 y, U64 w, U64 h, U8 *title) { return 0; }
U64 WMRender() { return 0; }
U64 Sleep(U64 ms) { return 0; }

U64 KernelMain() {
    U64 w = 1024;
    U64 h = 768;
    VBESwap();
    VBEFillRect(0, 0, w, h, 0x000080);
    VBEDrawText(50, 50, "WuBuOS HolyC Kernel", 0xFFFFFF, 2);
    VBEDrawText(50, 100, "Running from HolyC!", 0xFFFF00, 1);
    VBEDrawText(50, 130, "VBE syscalls working!", 0x00FF00, 1);
    U64 win = WMCreateWin(200, 200, 400, 300, "HolyC Window");
    VBESwap();
    while (1) {
        WMRender();
        Sleep(16);
    }
    return 0;
}

U64 _start() {
    KernelMain();
    return 0;
}
