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
        case WUBU_X11_OP_CREATE_WINDOW: {
            /* wid(4) parent(4) x(2) y(2) w(2) h(2) border(2) class(2) visual(4) ... */
            uint32_t wid; int x, y, w, h;
            if (s.lsb_first) {
                wid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
                x = (int16_t)(body[8] | (body[9]<<8));
                y = (int16_t)(body[10] | (body[11]<<8));
                w = (int16_t)(body[12] | (body[13]<<8));
                h = (int16_t)(body[14] | (body[15]<<8));
            } else {
                wid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
                x = (int16_t)((body[8]<<8) | body[9]);
                y = (int16_t)((body[10]<<8) | body[11]);
                w = (int16_t)((body[12]<<8) | body[13]);
                h = (int16_t)((body[14]<<8) | body[15]);
            }
            if (wubu_x11_window_create(&table, wid, x, y, w, h) != 0)
                printf("CREATE refused id=%#x %dx%d\n", wid, w, h);
            else
                printf("CREATE id=%#x %dx%d\n", wid, w, h);
            handled++; continue;
        }
        case WUBU_X11_OP_MAP_WINDOW: {
            uint32_t wid;
            if (s.lsb_first) wid = (uint32_t)(body[0] | (body[1]<<8) | (body[2]<<16) | ((uint32_t)body[3]<<24));
            else wid = (uint32_t)(((uint32_t)body[0]<<24) | (body[1]<<16) | (body[2]<<8) | body[3]);
            wubu_x11_window_map(&table, wid, 1);
            printf("MAP id=%#x\n", wid);
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
        int best = -1, best_area = 0;
        for (int i = 0; i < WUBU_X11_MAX_WINDOWS; i++)
            if (table.win[i].in_use && table.win[i].mapped) {
                int a = table.win[i].w * table.win[i].h;
                if (a > best_area) { best_area = a; best = i; }
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
