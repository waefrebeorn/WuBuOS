/*
 * kernel.hc  --  WuBuOS HolyC Kernel Source
 * 
 * Minimal TempleOS-style kernel in HolyC.
 * This is what the HolyC compiler should be able to compile to ELF.
 */

// ============================================================
// HolyC Kernel Headers (minimal ZealOS subset)
// ============================================================

#define NULL        0
#define TRUE        1
#define FALSE       0

typedef unsigned char   U8;
typedef unsigned short  U16;
typedef unsigned int    U32;
typedef unsigned long   U64;
typedef signed char     I8;
typedef signed short    I16;
typedef signed int      I32;
typedef signed long     I64;
typedef char            Bool;

// ============================================================
// VBE Framebuffer (maps to sys_vbe_* syscalls)
// ============================================================

extern I64 VBEFillRect(I64 x, I64 y, I64 w, I64 h, I64 color);
extern I64 VBEFillCircle(I64 x, I64 y, I64 r, I64 color);
extern I64 VBEDrawText(I64 x, I64 y, U8 *str, I64 color, I64 scale);
extern I64 VBEDrawChar(I64 x, I64 y, I64 c, I64 color, I64 scale);
extern I64 VBEVLine(I64 x, I64 y1, I64 y2, I64 color);
extern I64 VBEHLine(I64 x1, I64 x2, I64 y, I64 color);
extern I64 VBETextWidth(U8 *str, I64 scale);
extern I64 VBESwap();

// ============================================================
// Window Manager (maps to sys_wm_* syscalls)
// ============================================================

extern I64 WMCreateWin(I64 x, I64 y, I64 w, I64 h, U8 *title);
extern I64 WMDestroyWin(I64 id);
extern I64 WMSetFocus(I64 id);
extern I64 WMGetFocused();
extern I64 WMRender();

// ============================================================
// File I/O (maps to sys_file_* syscalls)
// ============================================================

extern I64 FileOpen(U8 *path, I64 mode);
extern I64 FileRead(I64 fd, U8 *buf, I64 len);
extern I64 FileWrite(I64 fd, U8 *buf, I64 len);
extern I64 FileClose(I64 fd);

// ============================================================
// Styx/9P (maps to sys_styx_* syscalls)
// ============================================================

extern I64 StyxOpen(U8 *path, I64 mode);
extern I64 StyxRead(I64 fid, I64 offset, I64 count, U8 *buf);
extern I64 StyxWrite(I64 fid, I64 offset, I64 count, U8 *buf);

// ============================================================
// Containers (maps to sys_container_* syscalls)
// ============================================================

extern I64 ContainerCreate(U8 *name, U8 *args, U8 *env);
extern I64 ContainerDestroy(I64 id);
extern I64 ContainerExec(I64 id, U8 *cmd, U8 *args);

// ============================================================
// Time/Utils
// ============================================================

extern I64 GetTime();
extern I64 Sleep(I64 ms);

// ============================================================
// Kernel Entry Point
// ============================================================

U0 KernelMain() {
    // Initialize VBE framebuffer
    I64 w = 1024, h = 768;
    VBESwap();  // Initialize double-buffer
    
    // Clear to dark blue
    VBEFillRect(0, 0, w, h, 0x000080);
    
    // Draw title
    VBEDrawText(50, 50, "WuBuOS HolyC Kernel", 0xFFFFFF, 2);
    VBEDrawText(50, 100, "Running from HolyC!", 0xFFFF00, 1);
    VBEDrawText(50, 130, "VBE syscalls working!", 0x00FF00, 1);
    
    // Create a window
    I64 win = WMCreateWin(200, 200, 400, 300, "HolyC Window");
    
    VBESwap();  // Flip to display
    
    // Simple message loop
    while (TRUE) {
        WMRender();
        Sleep(16);  // ~60 FPS
    }
}

// Export entry point for ELF
U0 _start() {
    KernelMain();
}
