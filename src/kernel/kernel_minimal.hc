/*
 * kernel_minimal.hc  --  Minimal HolyC Kernel (parser-compatible)
 * 
 * Only uses syntax supported by current HolyC parser:
 * - Function definitions with I64/U64 return
 * - Basic types (I64, U64, U8*)
 * - Function calls
 * - While loops, if statements
 * - Basic arithmetic
 */

// Types
typedef unsigned long I64;
typedef unsigned char U8;

// Forward declarations (declarations without extern)
I64 VBEFillRect(I64 x, I64 y, I64 w, I64 h, I64 color);
I64 VBEDrawText(I64 x, I64 y, U8 *str, I64 color, I64 scale);
I64 VBESwap();
I64 WMCreateWin(I64 x, I64 y, I64 w, I64 h, U8 *title);
I64 WMRender();
I64 Sleep(I64 ms);

I64 KernelMain() {
    I64 w = 1024;
    I64 h = 768;
    VBESwap();
    VBEFillRect(0, 0, w, h, 0x000080);
    VBEDrawText(50, 50, "WuBuOS HolyC Kernel", 0xFFFFFF, 2);
    VBEDrawText(50, 100, "Running from HolyC!", 0xFFFF00, 1);
    VBEDrawText(50, 130, "VBE syscalls working!", 0x00FF00, 1);
    I64 win = WMCreateWin(200, 200, 400, 300, "HolyC Window");
    VBESwap();
    while (1) {
        WMRender();
        Sleep(16);
    }
    return 0;
}

I64 _start() {
    KernelMain();
    return 0;
}
