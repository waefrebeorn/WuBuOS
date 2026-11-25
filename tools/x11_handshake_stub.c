/* x11_handshake_stub.c -- a socket that speaks ONLY the connection setup.
 *
 * Purpose: prove that a REAL X client accepts the bytes we emit. It listens on the
 * unix socket a client dials, reads the client's setup block, answers with our
 * Success reply, then reads whatever the client sends next and exits. A client
 * that accepts the reply moves on to its first request; one that rejects it prints
 * "cannot open display" and never gets that far. The difference is the whole
 * oracle -- this stub does not pretend to be a server. */
#include "wubu_x11_bridge.h"
#include "wubu_x11_request.h"
#include "wubu_x11_windows.h"
#include "wubu_x11_setup_reply.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


/* Expose is the packet every X client waits for: a window that is mapped and never
 * told to draw stays blank forever. xlogo proved this the hard way -- 40 requests,
 * a mapped window, and then silence, because nothing ever said "redraw now".
 *
 * 32-byte core event: type, detail, sequence, window, x, y, w, h, count, pad. */
static void send_expose(int c, uint16_t seq, int lsb, uint32_t win,
                        int x, int y, int w, int h) {
    uint8_t e[32];
    memset(e, 0, sizeof e);
    e[0] = 12;                    /* Expose */
    e[1] = 0;                     /* detail: none */
    if (lsb) { e[2] = (uint8_t)(seq & 0xff); e[3] = (uint8_t)(seq >> 8); }
    else     { e[2] = (uint8_t)(seq >> 8);   e[3] = (uint8_t)(seq & 0xff); }
#define PUT16(off, val) do { if (lsb) { e[off] = (uint8_t)((val) & 0xff); e[(off)+1] = (uint8_t)(((val) >> 8) & 0xff); } \
                            else { e[off] = (uint8_t)(((val) >> 8) & 0xff); e[(off)+1] = (uint8_t)((val) & 0xff); } } while (0)
#define PUT32(off, val) do { if (lsb) { e[off] = (uint8_t)((val) & 0xff); e[(off)+1] = (uint8_t)(((val) >> 8) & 0xff); \
                                     e[(off)+2] = (uint8_t)(((val) >> 16) & 0xff); e[(off)+3] = (uint8_t)(((val) >> 24) & 0xff); } \
                            else { e[off] = (uint8_t)(((val) >> 24) & 0xff); e[(off)+1] = (uint8_t)(((val) >> 16) & 0xff); \
                                   e[(off)+2] = (uint8_t)(((val) >> 8) & 0xff); e[(off)+3] = (uint8_t)((val) & 0xff); } } while (0)
    PUT32(4, win); PUT16(8, x); PUT16(10, y); PUT16(12, w); PUT16(14, h); PUT16(16, 0);
#undef PUT16
#undef PUT32
    size_t off = 0;
    while (off < sizeof e) {
        ssize_t r = write(c, e + off, sizeof e - off);
        if (r <= 0) return;
        off += (size_t)r;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s /path/to/socket\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);   /* a crash must not hide the last line */
    unlink(argv[1]);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 2; }
    struct sockaddr_un a; memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", argv[1]);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); return 2; }
    if (listen(fd, 1) < 0) { perror("listen"); return 2; }
    printf("READY\n"); fflush(stdout);

    int c = accept(fd, NULL, NULL);
    if (c < 0) { perror("accept"); return 2; }

    uint8_t in[4096];
    size_t got = 0;
    /* the setup block's size is only known after its first 12 bytes */
    while (got < WUBU_X11_SETUP_FIXED) {
        ssize_t n = read(c, in + got, sizeof in - got);
        if (n <= 0) { printf("FAIL client closed before setup\n"); return 1; }
        got += (size_t)n;
    }
    /* The block's size is not known until its lengths are read, and the lengths
     * are in the CLIENT's byte order -- so compute the needed total from the first
     * 12 bytes before parsing anything, and only then parse the whole block. */
    int lsb = (in[0] == 'l');
    uint16_t name_len  = lsb ? (uint16_t)(in[6] | (in[7] << 8))
                             : (uint16_t)((in[6] << 8) | in[7]);
    uint16_t data_len  = lsb ? (uint16_t)(in[8] | (in[9] << 8))
                             : (uint16_t)((in[8] << 8) | in[9]);
    size_t need = WUBU_X11_SETUP_FIXED + ((name_len + 3u) & ~3u) + ((data_len + 3u) & ~3u);
    while (got < need) {
        ssize_t n = read(c, in + got, sizeof in - got);
        if (n <= 0) { printf("FAIL client closed mid-setup\n"); return 1; }
        got += (size_t)n;
    }
    WubuX11Setup s;
    if (wubu_x11_parse_setup(in, got, &s) != 0) { printf("FAIL setup unparsable\n"); return 1; }
    printf("SETUP order=%s version=%u.%u auth=\"%s\"\n",
           s.lsb_first ? "lsb" : "msb", s.major, s.minor, s.auth_name);

    /* resource ids MUST be base | (n & mask): 0x00400000 root, visual and colormap */
    WubuX11ScreenCfg cfg = { 1024, 768, 271, 203, 24, 0x00400001, 0x00400021, 0x00400020 };
    uint8_t out[WUBU_X11_REPLY_MAX];
    int n = wubu_x11_build_setup_reply(out, sizeof out, &cfg, s.lsb_first);
    if (n <= 0) { printf("FAIL reply encode %d\n", n); return 1; }
    if (write(c, out, (size_t)n) != n) { printf("FAIL short write\n"); return 1; }
    printf("REPLY sent %d bytes\n", n); fflush(stdout);

    /* From here the client drives. Every reply must echo the request's SEQUENCE
     * number -- a client matches replies by it and stalls silently otherwise, with
     * no error to diagnose. */
    /* A real resource table, so a client's CreateWindow is a window and its
     * PutImage lands in that window's pixels. */
    static WubuX11Windows table;
    wubu_x11_windows_init(&table);

    /* Graphics contexts: a fill is only a colour if the GC says which colour. The
     * table is small and fixed for the same reason the windows are. */
    struct { int in_use; uint32_t id; uint32_t fg; } gc[64];
    memset(gc, 0, sizeof gc);

    int handled = 0, unknown = 0;
    uint16_t seq = 0;
    for (;;) {
        uint8_t h[4];
        size_t got_h = 0;
        while (got_h < 4) {
            ssize_t r = read(c, h + got_h, 4 - got_h);
            if (r <= 0) goto out;
            got_h += (size_t)r;
        }
        WubuX11Request rq; size_t need = 0;
        if (wubu_x11_parse_request(h, 4, &rq, &need, s.lsb_first) != 0) goto out;
        uint8_t body[65536];
        size_t have = 0, want = need - 4;
        if (getenv("WUBU_X11_TRACE")) printf("REQ op=%u len=%zu need=%zu\n", (unsigned)rq.opcode, (size_t)rq.length, need);
        while (have < want) {
            ssize_t r = read(c, body + have, want - have);
            if (r <= 0) goto out;
            have += (size_t)r;
        }
        seq++;
        int n = -1;
        uint8_t rep[256];
        switch (rq.opcode) {
        case WUBU_X11_OP_QUERY_EXTENSION:
            n = wubu_x11_reply_query_extension(rep, sizeof rep, seq, 0, s.lsb_first);
            break;
        case WUBU_X11_OP_LIST_EXTENSIONS:
            n = wubu_x11_reply_list_extensions(rep, sizeof rep, seq, NULL, 0, s.lsb_first);
            break;
        case WUBU_X11_OP_INTERN_ATOM:
            n = wubu_x11_reply_atom(rep, sizeof rep, seq, 100, s.lsb_first);
            break;
        case WUBU_X11_OP_GET_ATOM_NAME:
            n = wubu_x11_reply_atom_name(rep, sizeof rep, seq, "ATOM", s.lsb_first);
            break;
        case WUBU_X11_OP_GET_GEOMETRY:
            n = wubu_x11_reply_geometry(rep, sizeof rep, seq, cfg.root_window,
                                        0, 0, cfg.width, cfg.height, 0, s.lsb_first);
            break;
        case 2: {   /* ChangeWindowAttributes: wid(4) value-mask(4) values... */
            uint32_t wid, vmask;
            if (s.lsb_first) {
                wid   = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                vmask = (uint32_t)(body[4] | (body[5]<<8) | (body[6]<<16) | ((uint32_t)body[7]<<24));
            } else {
                wid   = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                vmask = (uint32_t)(((uint32_t)body[4]<<24) | (body[5]<<16) | (body[6]<<8) | body[7]);
            }
            /* CWEventMask is bit 11. Values follow the mask in bit order, and each
             * attribute has its own width, so the offset is the sum of the widths of
             * the set bits below it -- assuming one does is how a client's window
             * ends up with a nonsense mask. */
            static const int w_[15] = {4,4,4,4,1,1,1,4,4,1,1,4,4,4,4};
            int off2 = 0;
            for (int b = 0; b < 11; b++) if (vmask & (1u << b)) off2 += w_[b];
            if (vmask & (1u << 11)) {
                uint32_t em;
                if (s.lsb_first) em = (uint32_t)(body[8+off2] | (body[9+off2]<<8) | (body[10+off2]<<16) | ((uint32_t)body[11+off2]<<24));
                else em = (uint32_t)(((uint32_t)body[8+off2]<<24) | (body[9+off2]<<16) | (body[10+off2]<<8) | body[11+off2]);
                WubuX11Window *wn = wubu_x11_window_find(&table, wid);
                if (wn) wn->event_mask = em;
                printf("CHANGEATTR id=%#x event-mask=%#x\n", wid, em);
                /* A client that selects ExposureMask on an already-mapped window
                 * expects Expose straight away -- that is the contract Xt relies on,
                 * and waiting for a map that already happened means waiting forever. */
                if (wn && wn->mapped && (em & (1u << 15))) {
                    send_expose(c, seq, s.lsb_first, wid, 0, 0, wn->w, wn->h);
                    printf("EXPOSE id=%#x %dx%d (on select)\n", wid, wn->w, wn->h);
                }
            }
            handled++; continue;
        }
        case WUBU_X11_OP_CREATE_WINDOW: {
            /* wid(4) parent(4) x(2) y(2) w(2) h(2) border(2) class(2) visual(4) ... */
            uint32_t wid, parent; int x, y, w, h;
            if (s.lsb_first) {
                wid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                parent = (uint32_t)(body[4] | (body[5]<<8) | (body[6]<<16) | ((uint32_t)body[7]<<24));
                x = (int16_t)(body[8] | (body[9]<<8));
                y = (int16_t)(body[10] | (body[11]<<8));
                w = (int16_t)(body[12] | (body[13]<<8));
                h = (int16_t)(body[14] | (body[15]<<8));
            } else {
                wid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                parent = (uint32_t)(((uint32_t)body[4]<<24) | (body[5]<<16) | (body[6]<<8) | body[7]);
                x = (int16_t)((body[8]<<8) | body[9]);
                y = (int16_t)((body[10]<<8) | body[11]);
                w = (int16_t)((body[12]<<8) | body[13]);
                h = (int16_t)((body[14]<<8) | body[15]);
            }
            /* The value list after the visual is where Xt puts CWEventMask (bit 11),
             * and it is why a client that "sets no attributes" still expects Expose.
             * Values follow the mask in bit order with their own widths. */
            uint32_t emask = 0;
            if (have >= 28) {
                uint32_t vmask;
                if (s.lsb_first) vmask = (uint32_t)(body[24] | (body[25]<<8) | (body[26]<<16) | ((uint32_t)body[27]<<24));
                else vmask = (uint32_t)(((uint32_t)body[24]<<24) | (body[25]<<16) | (body[26]<<8) | body[27]);
                static const int vw_[15] = {4,4,4,4,1,1,1,4,4,1,1,4,4,4,4};
                int voff = 0;
                for (int b = 0; b < 11; b++) if (vmask & (1u << b)) voff += vw_[b];
                if ((vmask & (1u << 11)) && have >= (size_t)(28 + voff + 4)) {
                    const uint8_t *q = body + 28 + voff;
                    if (s.lsb_first) emask = (uint32_t)(q[0] | (q[1]<<8) | (q[2]<<16) | ((uint32_t)q[3]<<24));
                    else emask = (uint32_t)(((uint32_t)q[0]<<24) | (q[1]<<16) | (q[2]<<8) | q[3]);
                }
            }
            if (wubu_x11_window_create_ex(&table, wid, parent, x, y, w, h) != 0)
                printf("CREATE refused id=%#x %dx%d\n", wid, w, h);
            else
            {
                WubuX11Window *wn = wubu_x11_window_find(&table, wid);
                if (wn) wn->event_mask = emask;
                (void)0;
            }
            handled++; continue;
        }
        case 9: {   /* MapSubwindows: parentage matters -- mapping the parent is not
                     * mapping its children, and a child that selected ExposureMask
                     * will wait forever if nobody ever maps it. */
            uint32_t wid;
            if (s.lsb_first) wid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
            else wid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
            int mapped = 0;
            for (int i = 0; i < WUBU_X11_MAX_WINDOWS; i++) {
                WubuX11Window *wn = &table.win[i];
                if (!wn->in_use || wn->parent != wid) continue;
                wn->mapped = 1; mapped++;
                if (wn->event_mask & (1u << 15)) { send_expose(c, seq, s.lsb_first, wn->id, 0, 0, wn->w, wn->h); printf("EXPOSE id=%#x (subwindow)\n", wn->id); }
            }
            printf("MAPSUBWIN parent=%#x children=%d\n", wid, mapped);
            handled++; continue;
        }
        case WUBU_X11_OP_MAP_WINDOW: {
            uint32_t wid;
            if (s.lsb_first) wid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
            else wid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
            wubu_x11_window_map(&table, wid, 1);
            printf("MAP id=%#x\n", wid);
            {
                WubuX11Window *wn = wubu_x11_window_find(&table, wid);
                if (wn && (wn->event_mask & (1u << 15))) {   /* ExposureMask */
                    send_expose(c, seq, s.lsb_first, wid, 0, 0, wn->w, wn->h);
                    printf("EXPOSE id=%#x %dx%d\n", wid, wn->w, wn->h);
                }
            }
            handled++; continue;
        }
        case 55: {  /* CreateGC: gcid(4) drawable(4) mask(4) values... */
            uint32_t gid, mask;
            if (s.lsb_first) {
                gid  = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                mask = (uint32_t)(body[8] | (body[9]<<8) | (body[10]<<16) | ((uint32_t)body[11]<<24));
            } else {
                gid  = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                mask = (uint32_t)(((uint32_t)body[8]<<24) | (body[9]<<16) | (body[10]<<8) | body[11]);
            }
            int slot = -1;
            for (int i = 0; i < 64; i++) if (!gc[i].in_use) { slot = i; break; }
            if (slot < 0) { printf("CREATEGC refused (table full)\n"); handled++; continue; }
            gc[slot].in_use = 1; gc[slot].id = gid; gc[slot].fg = 0x000000;
            /* GCForeground is bit 2; values follow the mask in bit order. */
            static const int gw[23] = {4,4,4,4,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,4};
            int off3 = 0;
            for (int b = 0; b < 2; b++) if (mask & (1u << b)) off3 += gw[b];
            if (mask & (1u << 2)) {
                const uint8_t *q = body + 12 + off3;
                if (s.lsb_first) gc[slot].fg = (uint32_t)(q[0] | (q[1]<<8) | (q[2]<<16) | ((uint32_t)q[3]<<24));
                else gc[slot].fg = (uint32_t)(((uint32_t)q[0]<<24) | (q[1]<<16) | (q[2]<<8) | q[3]);
            }
            printf("CREATEGC id=%#x fg=%#06x\n", gid, gc[slot].fg);
            handled++; continue;
        }
        case 56: {  /* ChangeGC: gcid(4) mask(4) values... */
            uint32_t gid, mask;
            if (s.lsb_first) {
                gid  = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                mask = (uint32_t)(body[4] | (body[5]<<8) | (body[6]<<16) | ((uint32_t)body[7]<<24));
            } else {
                gid  = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                mask = (uint32_t)(((uint32_t)body[4]<<24) | (body[5]<<16) | (body[6]<<8) | body[7]);
            }
            for (int i = 0; i < 64; i++) if (gc[i].in_use && gc[i].id == gid) {
                static const int gw[23] = {4,4,4,4,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,4};
                int off3 = 0;
                for (int b = 0; b < 2; b++) if (mask & (1u << b)) off3 += gw[b];
                if (mask & (1u << 2)) {
                    const uint8_t *q = body + 8 + off3;
                    if (s.lsb_first) gc[i].fg = (uint32_t)(q[0] | (q[1]<<8) | (q[2]<<16) | ((uint32_t)q[3]<<24));
                    else gc[i].fg = (uint32_t)(((uint32_t)q[0]<<24) | (q[1]<<16) | (q[2]<<8) | q[3]);
                }
                printf("CHANGEGC id=%#x fg=%#06x\n", gid, gc[i].fg);
            }
            handled++; continue;
        }
        case 60: {  /* FreeGC */
            uint32_t gid;
            if (s.lsb_first) gid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
            else gid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
            for (int i = 0; i < 64; i++) if (gc[i].in_use && gc[i].id == gid) gc[i].in_use = 0;
            handled++; continue;
        }
        case 69: {  /* PolyFillRectangle: drawable(4) gc(4) then x,y,w,h (2 each) */
            uint32_t dw, gid, fg = 0;
            if (s.lsb_first) {
                dw  = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                gid = (uint32_t)(body[4] | (body[5]<<8) | (body[6]<<16) | ((uint32_t)body[7]<<24));
            } else {
                dw  = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                gid = (uint32_t)(((uint32_t)body[4]<<24) | (body[5]<<16) | (body[6]<<8) | body[7]);
            }
            for (int i = 0; i < 64; i++) if (gc[i].in_use && gc[i].id == gid) fg = gc[i].fg;
            WubuX11Window *wn = wubu_x11_window_find(&table, dw);
            if (!wn) { printf("FILLRECT unknown drawable %#x\n", dw); handled++; continue; }
            int drew = 0;
            for (size_t o = 8; o + 8 <= have; o += 8) {
                int rx, ry, rw, rh;
                if (s.lsb_first) { rx = (int16_t)(body[o]|(body[o+1]<<8)); ry = (int16_t)(body[o+2]|(body[o+3]<<8));
                                   rw = (int16_t)(body[o+4]|(body[o+5]<<8)); rh = (int16_t)(body[o+6]|(body[o+7]<<8)); }
                else { rx = (int16_t)((body[o]<<8)|body[o+1]); ry = (int16_t)((body[o+2]<<8)|body[o+3]);
                       rw = (int16_t)((body[o+4]<<8)|body[o+5]); rh = (int16_t)((body[o+6]<<8)|body[o+7]); }
                if (rw <= 0 || rh <= 0) continue;
                for (int yy = ry; yy < ry + rh; yy++) {
                    if (yy < 0 || yy >= wn->h) continue;
                    uint8_t *row = wn->pixels + ((size_t)yy * wn->w) * 4u;
                    for (int xx = rx; xx < rx + rw; xx++) {
                        if (xx < 0 || xx >= wn->w) continue;
                        row[xx*4+0] = (uint8_t)((fg >> 16) & 0xff);
                        row[xx*4+1] = (uint8_t)((fg >> 8) & 0xff);
                        row[xx*4+2] = (uint8_t)(fg & 0xff);
                        row[xx*4+3] = 255;
                    }
                }
                printf("    rect %d,%d %dx%d\n", rx, ry, rw, rh);
                drew++;
            }
            printf("FILLRECT id=%#x fg=%#06x rects=%d\n", dw, fg, drew);
            handled++; continue;
        }
        case 72: {   /* PutImage: wid(4) gc(4) w(2) h(2) dx(2) dy(2) pad(1) depth(1) pad(2) data */
            uint32_t wid; int iw, ih, dx, dy, depth;
            if (s.lsb_first) {
                wid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                iw = (int16_t)(body[8] | (body[9]<<8));
                ih = (int16_t)(body[10] | (body[11]<<8));
                dx = (int16_t)(body[12] | (body[13]<<8));
                dy = (int16_t)(body[14] | (body[15]<<8));
                depth = body[17];
            } else {
                wid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                iw = (int16_t)((body[8]<<8) | body[9]);
                ih = (int16_t)((body[10]<<8) | body[11]);
                dx = (int16_t)((body[12]<<8) | body[13]);
                dy = (int16_t)((body[14]<<8) | body[15]);
                depth = body[17];
            }
            int rc = wubu_x11_window_put_image(&table, wid, dx, dy, iw, ih,
                                               body + 20, have > 20 ? have - 20 : 0,
                                               s.lsb_first, depth ? depth : 24);
            printf("PUTIMAGE id=%#x %dx%d at %d,%d depth=%d rc=%d\n", wid, iw, ih, dx, dy, depth, rc);
            handled++; continue;
        }
        case WUBU_X11_OP_GET_PROPERTY:
            n = wubu_x11_reply_property_missing(rep, sizeof rep, seq, s.lsb_first);
            break;
        case 3:   /* GetWindowAttributes */
            n = wubu_x11_reply_window_attrs(rep, sizeof rep, seq, cfg.root_visual,
                                            cfg.default_colormap, s.lsb_first);
            break;
        case 101: /* GetKeyboardMapping: 1 keysym per keycode for the range asked */
            n = wubu_x11_reply_keymap(rep, sizeof rep, seq, 1, 8, s.lsb_first);
            break;
        case 97:  /* QueryBestSize */
            n = wubu_x11_reply_best_size(rep, sizeof rep, seq, cfg.width, cfg.height, s.lsb_first);
            break;
        case 107: /* GetScreenSaver */
            n = wubu_x11_reply_screen_saver(rep, sizeof rep, seq, s.lsb_first);
            break;
        case 110: /* ListHosts */
            n = wubu_x11_reply_list_hosts(rep, sizeof rep, seq, s.lsb_first);
            break;
        case 106: /* GetPointerControl */
            n = wubu_x11_reply_pointer_control(rep, sizeof rep, seq, s.lsb_first);
            break;
        case WUBU_X11_OP_GET_INPUT_FOCUS:
            n = wubu_x11_reply_input_focus(rep, sizeof rep, seq, 1, s.lsb_first);
            break;
        default: {
            if (!wubu_x11_op_expects_reply(rq.opcode)) {
                /* Mutating request: consume it, say nothing. An error here would
                 * abort a client that did nothing wrong. */
                handled++;
                continue;
            }
            /* Unimplemented (error code 17): 32 bytes, same header shape. Every
             * field in the client's byte order, exactly like a reply. */
            memset(rep, 0, 32);
            rep[0] = 0; rep[1] = 17;
            if (s.lsb_first) { rep[2] = (uint8_t)seq; rep[3] = (uint8_t)(seq >> 8); }
            else             { rep[2] = (uint8_t)(seq >> 8); rep[3] = (uint8_t)seq; }
            rep[10] = rq.opcode;               /* minor opcode slot carries the major */
            n = 32; unknown++;
            break;
        }
        }
        if (n <= 0) goto out;
        if (write(c, rep, (size_t)n) != n) goto out;
        handled++;
        if (unknown > 64) goto out;            /* do not spin on a client we cannot serve */
    }
out:
    /* Dump the largest mapped window so the pixels can be checked from outside:
     * a framebuffer nobody can look at is not evidence of anything. */
    {
        /* Pick the window that was actually DRAWN INTO, not the biggest one: an
         * outer shell that never received a fill is a black rectangle, and dumping
         * it looks exactly like "drawing does not work". */
        int best = -1, best_painted = -1;
        for (int i = 0; i < WUBU_X11_MAX_WINDOWS; i++) {
            WubuX11Window *w = &table.win[i];
            if (!w->in_use || !w->mapped) continue;
            int painted = 0;
            for (int p = 0; p < w->w * w->h; p++)
                if (w->pixels[p*4] || w->pixels[p*4+1] || w->pixels[p*4+2]) painted++;
            if (painted > best_painted) { best_painted = painted; best = i; }
        }
        if (best >= 0) {
            FILE *f = fopen("/tmp/x11_stub_frame.ppm", "wb");
            if (f) {
                WubuX11Window *w = &table.win[best];
                fprintf(f, "P6\n%d %d\n255\n", w->w, w->h);
                for (int i = 0; i < w->w * w->h; i++)
                    fwrite(w->pixels + i * 4, 1, 3, f);
                fclose(f);
                printf("FRAME id=%#x %dx%d -> /tmp/x11_stub_frame.ppm\n", w->id, w->w, w->h);
            }
        }
    }
    printf("LOOP handled=%d unknown=%d\n", handled, unknown);
    printf(handled > 0 ? "PASS client accepted the reply and drove %d request(s)\n"
                       : "PARTIAL client closed after setup\n", handled);
    close(c); close(fd); unlink(argv[1]);
    return handled > 0 ? 0 : 1;
}
