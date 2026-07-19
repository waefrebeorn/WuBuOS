/* wubu_dos_emu.c -- WuBuOS 8086/DOS shim public API (create/destroy/load/run/capture). */
#include "wubu_dos_emu_internal.h"

int wubu_dos_emu_exit_code(const WubuDosEmu *e) { return e ? e->exit_code : -1; }

void wubu_dos_emu_regs(const WubuDosEmu *e, uint16_t *ax, uint16_t *bx, uint16_t *cx, uint16_t *dx,
                       uint16_t *si, uint16_t *di, uint16_t *ip, uint16_t *flags, uint16_t *cs) {
    if (!e) return;
    if (ax) *ax = e->ax;
    if (bx) *bx = e->bx;
    if (cx) *cx = e->cx;
    if (dx) *dx = e->dx;
    if (si) *si = e->si;
    if (di) *di = e->di;
    if (ip) *ip = e->ip;
    if (flags) *flags = e->flags;
    if (cs) *cs = e->cs;
}


int wubu_dos_emu_load_com(WubuDosEmu *e, const uint8_t *data, size_t size) {
    if (!e || !data || size == 0) return -1;
    if (size > WUBU_DOS_MEM_SIZE - 0x100) return -1;
    memset(e->mem, 0, WUBU_DOS_MEM_SIZE);
    /* Build a real 80-byte PSP at seg 0x1000:0. */
    e->psp_seg = e->ds;
    wr8(e, e->ds, 0, 0xCD); wr8(e, e->ds, 1, 0x20); /* int 20h at PSP:0 */
    wr8(e, e->ds, 2, 0x9A);                          /* far JMP (unused) */
    wr16(e, e->ds, 0x16, 0);                          /* parent PSP = 0 */
    wr16(e, e->ds, 0x2C, 0);                          /* environment seg = 0 */
    wr16(e, e->ds, 0x80, 0x80);                       /* DTA = PSP:0x80 */
    /* Default command tail (CR only) so a program that reads PSP:0x80 sees an
     * empty line; real cmdline could be injected via a dedicated API. */
    wr8(e, e->ds, 0x80, 0x0D);
    /* Copy program to 0x100. */
    for (size_t i = 0; i < size; i++) wr8(e, e->cs, (uint16_t)(0x100 + i), data[i]);
    e->ip = 0x100;
    e->state = WUBU_DOS_RUNNING;
    return 0;
}

void wubu_dos_emu_destroy(WubuDosEmu *e) { free(e); }


int wubu_dos_emu_load_exe(WubuDosEmu *e, const uint8_t *data, size_t size) {
    if (!e || !data || size < 28) return -1;
    if (data[0] != 'M' && data[0] != 'Z') return -1;
    uint16_t hdr_size = (uint16_t)(data[8] | (data[9] << 8));   /* paragraphs */
    uint16_t min_extra = (uint16_t)(data[10] | (data[11] << 8));
    uint16_t init_ss   = (uint16_t)(data[14] | (data[15] << 8));
    uint16_t init_sp   = (uint16_t)(data[16] | (data[17] << 8));
    uint16_t init_ip   = (uint16_t)(data[20] | (data[21] << 8));
    uint16_t init_cs   = (uint16_t)(data[22] | (data[23] << 8));
    uint16_t reloc_off = (uint16_t)(data[24] | (data[25] << 8));
    uint16_t reloc_cnt = (uint16_t)(data[26] | (data[27] << 8));
    uint32_t img_base = (uint32_t)hdr_size * 16;
    uint32_t img_bytes = (uint32_t)size - img_base;
    memset(e->mem, 0, WUBU_DOS_MEM_SIZE);
    /* Load image at seg 0. */
    for (uint32_t i = 0; i < img_bytes && (img_base + i) < WUBU_DOS_MEM_SIZE; i++)
        e->mem[img_base + i] = data[img_base + i];
    /* Apply relocations (seg:off pairs, add hdr_size). */
    for (int i = 0; i < reloc_cnt; i++) {
        uint32_t ro = (uint32_t)reloc_off + (uint32_t)i * 4;
        if (ro + 3 >= size) break;
        uint16_t off = (uint16_t)(data[ro] | (data[ro + 1] << 8));
        uint16_t seg = (uint16_t)(data[ro + 2] | (data[ro + 3] << 8));
        uint32_t a = ((uint32_t)seg << 4) + off;
        if (a + 1 < WUBU_DOS_MEM_SIZE) {
            uint16_t v = (uint16_t)(e->mem[a] | (e->mem[a + 1] << 8));
            v = (uint16_t)(v + hdr_size);
            e->mem[a] = (uint8_t)v; e->mem[a + 1] = (uint8_t)(v >> 8);
        }
    }
    e->cs = hdr_size + init_cs;
    e->ds = hdr_size; e->es = hdr_size; e->ss = hdr_size + init_ss;
    e->sp = init_sp ? init_sp : 0xFFFE;
    e->psp_seg = hdr_size;                            /* EXE PSP lives at image base */
    e->ip = init_ip;
    e->ax = e->bx = e->cx = e->dx = 0;
    e->state = WUBU_DOS_RUNNING;
    (void)min_extra;
    return 0;
}


size_t wubu_dos_emu_text(const WubuDosEmu *e, char *out, size_t out_size) {
    if (!e || !out || out_size == 0) return 0;
    size_t n = 0;
    for (int y = 0; y < WUBU_DOS_TEXT_ROWS; y++) {
        for (int x = 0; x < WUBU_DOS_TEXT_COLS; x++) {
            char c = (char)e->text[y][x];
            if (c == 0) c = ' ';
            if (n + 1 < out_size) out[n++] = c;
        }
        if (n + 1 < out_size) out[n++] = '\n';
    }
    out[n] = '\0';
    return n;
}

int wubu_dos_emu_step(WubuDosEmu *e) {
    if (!e || e->state != WUBU_DOS_RUNNING) return -1;
    int r = step(e);
    e->steps++;
    return r;
}

void wubu_dos_emu_key(WubuDosEmu *e, uint8_t ascii) {
    if (!e) return;
    /* Map a few common ASCII bytes to plausible scancodes for INT 16h. */
    static const uint8_t sc[256] = {0};
    (void)sc;
    uint8_t scan = 0x1E; /* default 'A' style; good enough for ah=0 top byte */
    if (ascii >= 'a' && ascii <= 'z') scan = (uint8_t)(0x1E + (ascii - 'a'));
    else if (ascii >= 'A' && ascii <= 'Z') scan = (uint8_t)(0x1E + (ascii - 'A'));
    else if (ascii >= '0' && ascii <= '9') scan = (uint8_t)(0x02 + (ascii - '0'));
    else if (ascii == ' ') scan = 0x39;
    else if (ascii == '\r' || ascii == '\n') scan = 0x1C;
    else if (ascii == '\b') scan = 0x0E;
    else if (ascii == '\t') scan = 0x0F;
    else if (ascii == 0x1B) scan = 0x01;
    int next = (e->ktail + 1) & 63;
    if (next == e->khead) return; /* full */
    e->kbuf[e->ktail] = ascii; e->kscan[e->ktail] = scan; e->ktail = next;
}

WubuDosEmuState wubu_dos_emu_run(WubuDosEmu *e, uint64_t max_steps) {
    if (!e) return WUBU_DOS_ERROR;
    /* Safety ceiling so a runaway/ill-formed program can never hang the host
     * process (e.g. a COM that falls through into a zeroed region and loops
     * on no-op instructions). max_steps==0 means "no caller-imposed limit",
     * not "unbounded". */
    uint64_t hard_cap = (max_steps != 0) ? max_steps : 20000000ULL;
    while (e->state == WUBU_DOS_RUNNING) {
        if (e->steps >= hard_cap) { e->state = WUBU_DOS_ERROR; break; }
        int r = step(e);
        e->steps++;
        if (r < 0) break;
    }
    return e->state;
}

WubuDosEmu *wubu_dos_emu_create(void) {
    WubuDosEmu *e = (WubuDosEmu *)calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->state = WUBU_DOS_RUNNING;
    e->ds = e->es = e->ss = 0x1000; /* PSP segment; COM loads at 0x100 */
    e->cs = 0x1000;
    e->sp = 0xFFFE;
    e->cur_attr = 0x07; /* light gray on black */
    return e;
}

uint16_t wubu_dos_emu_peek16(const WubuDosEmu *e, uint16_t seg, uint16_t off) {
    if (!e) return 0;
    return rd16((WubuDosEmu *)e, seg, off);
}

size_t wubu_dos_emu_frame_rgba(const WubuDosEmu *e, uint8_t *rgba, int *out_w, int *out_h) {
    if (!e || !rgba) return 0;
    int W = WUBU_DOS_TEXT_COLS * WUBU_DOS_CELL_W;
    int H = WUBU_DOS_TEXT_ROWS * WUBU_DOS_CELL_H;
    /* palette: attr low nibble = fg, high = bg (very rough CGA-ish) */
    static const uint8_t pal[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},
        {170,85,0},{170,170,170},{85,85,85},{85,85,255},{85,255,85},
        {85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
    };
    for (int y = 0; y < H; y++) {
        int row = y / WUBU_DOS_CELL_H;
        int gl = y % WUBU_DOS_CELL_H;
        for (int x = 0; x < W; x++) {
            int col = x / WUBU_DOS_CELL_W;
            int gc = x % WUBU_DOS_CELL_W;
            uint8_t ch = e->text[row][col];
            uint8_t at = e->attr[row][col];
            int bit = 0;
            if (ch >= WUBU_DOS_FONT_FIRST && ch <= WUBU_DOS_FONT_LAST) {
                int idx = ch - WUBU_DOS_FONT_FIRST;
                int gy = gl * 7 / WUBU_DOS_CELL_H;       /* map 16 -> 7 */
                int gx = gc * 5 / WUBU_DOS_CELL_W;       /* map 8 -> 5 */
                if (gx < 5 && gy < 7) bit = (wubu_dos_font[idx][gy] >> (4 - gx)) & 1;
            }
            const uint8_t *fg = pal[at & 0x0F];
            const uint8_t *bg = pal[(at >> 4) & 0x0F];
            uint8_t r = bit ? fg[0] : bg[0];
            uint8_t g = bit ? fg[1] : bg[1];
            uint8_t b = bit ? fg[2] : bg[2];
            size_t o = ((size_t)y * W + x) * 4;
            rgba[o] = r; rgba[o+1] = g; rgba[o+2] = b; rgba[o+3] = 255;
        }
    }
    if (out_w) *out_w = W;
    if (out_h) *out_h = H;
    return (size_t)W * H * 4;
}
