/* wubu_buddy_orb_demo.c -- live shader orb + render-space box.
 *
 * Renders wubu_buddy (the metaball body from src/runtime/wubu_buddy_orb.c) by
 * sphere-tracing its field in a fragment shader: no bitmaps, no mesh assets,
 * the form is computed from blob state every frame.  The bounded render space
 * from wubu_buddy_orb_render_box() is drawn as a wire box around it.
 *
 * Controls: arrows/orbit drag = rotate, +/- = zoom, Space = freeze the form,
 *           Esc/q = quit.
 *
 * Build (no -dev packages needed):
 *   gcc -O2 -o tools/wubu_buddy_orb_demo tools/wubu_buddy_orb_demo.c \
 *       src/runtime/wubu_buddy_orb.c -I src/runtime \
 *       -l:libGL.so.1 -l:libX11.so.6 -lm
 */
#define _POSIX_C_SOURCE 200809L
#include "wubu_buddy_orb.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern GLXContext glXCreateContextAttribsARB(Display *, GLXFBConfig,
                                             GLXContext, Bool, const int *);

/* Modern-GL entry points Mesa's gl.h does not declare.  Signatures match the
 * OpenGL ABI; every one is resolved with glXGetProcAddress at runtime. */
typedef GLuint (*PFNglCreateShader)(GLenum type);
typedef void (*PFNglShaderSource)(GLuint s, GLsizei n,
                                          const char **src, const GLint *len);
typedef void (*PFNglCompileShader)(GLuint s);
typedef void (*PFNglGetShaderiv)(GLuint s, GLenum p, GLint *out);
typedef void (*PFNglGetShaderInfoLog)(GLuint s, GLsizei max,
                                              GLsizei *len, char *log);
typedef GLuint (*PFNglCreateProgram)(void);
typedef void (*PFNglAttachShader)(GLuint prog, GLuint shader);
typedef void (*PFNglLinkProgram)(GLuint prog);
typedef void (*PFNglGetProgramiv)(GLuint prog, GLenum p, GLint *out);
typedef void (*PFNglGetProgramInfoLog)(GLuint prog, GLsizei max,
                                               GLsizei *len, char *log);
typedef void (*PFNglUseProgram)(GLuint prog);
typedef GLint (*PFNglGetUniformLocation)(GLuint prog, const char *name);
typedef void (*PFNglUniform1f)(GLint l, GLfloat v);
typedef void (*PFNglUniform1i)(GLint l, GLint v);
typedef void (*PFNglUniform2f)(GLint l, GLfloat a, GLfloat b);
typedef void (*PFNglUniform3f)(GLint l, GLfloat a, GLfloat b, GLfloat c);
typedef void (*PFNglUniform4fv)(GLint l, GLsizei n, const GLfloat *v);
typedef void (*PFNglUniformMatrix4fv)(GLint l, GLsizei n, GLboolean tr,
                                              const GLfloat *v);
typedef void (*PFNglGenVertexArrays)(GLsizei n, GLuint *out);
typedef void (*PFNglBindVertexArray)(GLuint vao);
typedef void (*PFNglDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (*PFNglReadPixels)(GLint x, GLint y, GLsizei w, GLsizei h,
                                GLenum fmt, GLenum type, void *px);

#define MAXB WUBU_BUDDY_ORB_MAX_BLOBS
#define STR_(x) #x
#define STR(x) STR_(x)

static const char *VS_FULL = "#version 330 core\n"
    "out vec2 vUv;\n"
    "void main(){ vec2 p = vec2((gl_VertexID<<1)&2, gl_VertexID&2);\n"
    "  vUv = p; gl_Position = vec4(p*2.0-1.0, 0.0, 1.0); }\n";

static const char *FS_ORB = "#version 330 core\n"
    "precision highp float;\n"
    "#define MAXB " STR(WUBU_BUDDY_ORB_MAX_BLOBS) "\n"
    "in vec2 vUv; out vec4 frag;\n"
    "uniform vec2  uRes;\n"
    "uniform vec3  uEye, uRight, uUp, uFwd;\n"
    "uniform float uFov;\n"
    "uniform int   uCount;\n"
    "uniform vec4  uBlob[MAXB];\n"
    "uniform vec3  uBoxLo, uBoxHi;\n"
    "float field(vec3 p){ float f=0.0;\n"
    "  for(int i=0;i<MAXB;i++){ if(i>=uCount) break;\n"
    "    vec3 d=p-uBlob[i].xyz; f += uBlob[i].w*uBlob[i].w/max(dot(d,d),1e-6); }\n"
    "  return f; }\n"
    "vec3 norm(vec3 p){ vec2 e=vec2(0.004,0.0);\n"
    "  return normalize(vec3(field(p+e.xyy)-field(p-e.xyy),\n"
    "                        field(p+e.yxy)-field(p-e.yxy),\n"
    "                        field(p+e.yyx)-field(p-e.yyx))); }\n"
    "void main(){\n"
    "  vec2 ndc = vUv*2.0-1.0; ndc.x *= uRes.x/uRes.y;\n"
    "  vec3 ro = uEye;\n"
    "  vec3 rd = normalize(uFwd + uFov*(ndc.x*uRight + ndc.y*uUp));\n"
    "  vec3 bg = mix(vec3(0.030,0.032,0.048), vec3(0.070,0.076,0.104),\n"
    "                smoothstep(-0.7,0.9,rd.y));\n"
    "  float t=0.0; bool hit=false; vec3 p=ro;\n"
    "  for(int i=0;i<220;i++){\n"
    "    p = ro + rd*t;\n"
    "    float f = field(p);\n"
    "    if (f < 1.0){ t += 0.06; continue; }\n"
    "    float s = 0.9*(1.0 - inversesqrt(f));\n"
    "    if (s < 0.0075){ hit=true; break; }\n"
    "    t += max(s, 0.014);\n"
    "    if (t > 14.0) break;\n"
    "  }\n"
    "  vec3 col = bg;\n"
    "  if (hit){\n"
    "    vec3 n = norm(p);\n"
    "    vec3 L = normalize(vec3(-0.45,0.75,0.55));\n"
    "    float dif = max(dot(n,L),0.0);\n"
    "    float hd  = 0.38;                       /* headlight: front never black */\n"
    "    vec3 h = normalize(L - rd);\n"
    "    float spec = pow(max(dot(n,h),0.0), 42.0);\n"
    "    float fres = pow(1.0 - max(dot(n,-rd),0.0), 3.0);\n"
    "    vec3 base = vec3(0.93,0.92,0.88);\n"
    "    vec3 acc  = vec3(1.00,0.74,0.30);       /* one accent, amber rim */\n"
    "    col = base*(0.16 + hd*0.42 + dif*0.62) + vec3(1.0)*spec*0.55\n"
    "        + acc*fres*0.85;\n"
    "  }\n"
    "  /* faint floor line so the render space reads as a place */\n"
    "  if (rd.y < -0.02){\n"
    "    float tf = (uBoxLo.y - ro.y)/rd.y;\n"
    "    if (tf>0.0 && tf<t+2.0){ vec3 fp = ro+rd*tf;\n"
    "      vec2 g = abs(fract(fp.xz*2.0)-0.5);\n"
    "      float line = smoothstep(0.48,0.5,max(g.x,g.y));\n"
    "      col += vec3(0.10,0.11,0.13)*line*smoothstep(6.0,1.0,tf); }\n"
    "  }\n"
    "  col = pow(clamp(col,0.0,1.0), vec3(0.4545));\n"
    "  frag = vec4(col,1.0);\n"
    "}\n";

static const char *VS_BOX = "#version 330 core\n"
    "uniform mat4 uVP;\n"
    "uniform vec3 uLo, uHi;\n"
    "const int E[24] = int[24](0,1, 1,3, 3,2, 2,0, 4,5, 5,7, 7,6, 6,4,\n"
    "                        0,4, 1,5, 2,6, 3,7);\n"
    "vec3 corner(int i){ return vec3((i&1)!=0?uHi.x:uLo.x,\n"
    "                               ((i>>1)&1)!=0?uHi.y:uLo.y,\n"
    "                               ((i>>2)&1)!=0?uHi.z:uLo.z); }\n"
    "void main(){ gl_Position = uVP * vec4(corner(E[gl_VertexID]),1.0); }\n";

static const char *FS_BOX = "#version 330 core\n"
    "out vec4 frag;\n"
    "uniform vec3 uAccent;\n"
    "void main(){ frag = vec4(uAccent, 0.55); }\n";


/* Resolve modern entry points at runtime (Mesa exports them; headers do not). */
typedef void (*GLXGetProcAddressFn)(void);
static void *glsym(const char *n)
{
    return (void *)glXGetProcAddress((const GLubyte *)n);
}

static GLuint compile(GLenum kind, const char *src)
{
    GLuint (*create)(void) = (GLuint(*)(void))glsym(kind == GL_VERTEX_SHADER ?
        "glCreateShader" : "glCreateProgram");
    (void)create;
    return 0;
}

/* ---- minimal column-major 4x4 helpers ---- */
static void perspective(float *m, float fovy, float aspect, float zn, float zf)
{
    float f = 1.0f / tanf(fovy * 0.5f);
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = f / aspect; m[5] = f;
    m[10] = (zf + zn) / (zn - zf); m[11] = -1.0f;
    m[14] = 2.0f * zf * zn / (zn - zf);
}

static void look_at(float *m, const float eye[3], const float ctr[3],
                    const float up[3])
{
    float f[3] = {ctr[0]-eye[0], ctr[1]-eye[1], ctr[2]-eye[2]};
    float fl = sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
    f[0]/=fl; f[1]/=fl; f[2]/=fl;
    float s[3] = {f[1]*up[2]-f[2]*up[1], f[2]*up[0]-f[0]*up[2],
                  f[0]*up[1]-f[1]*up[0]};
    float sl = sqrtf(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);
    s[0]/=sl; s[1]/=sl; s[2]/=sl;
    float u[3] = {s[1]*f[2]-s[2]*f[1], s[2]*f[0]-s[0]*f[2],
                  s[0]*f[1]-s[1]*f[0]};
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0]=s[0]; m[1]=u[0]; m[2]=-f[0];
    m[4]=s[1]; m[5]=u[1]; m[6]=-f[1];
    m[8]=s[2]; m[9]=u[2]; m[10]=-f[2];
    m[12]=-(s[0]*eye[0]+s[1]*eye[1]+s[2]*eye[2]);
    m[13]=-(u[0]*eye[0]+u[1]*eye[1]+u[2]*eye[2]);
    m[14]= (f[0]*eye[0]+f[1]*eye[1]+f[2]*eye[2]);
    m[15]=1.0f;
}

static void mat4_mul(float *o, const float *a, const float *b)
{
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float v = 0.0f;
            for (int k = 0; k < 4; ++k)
                v += a[k*4+r] * b[c*4+k];
            t[c*4+r] = v;
        }
    for (int i = 0; i < 16; ++i) o[i] = t[i];
}

static void normalize3(float v[3])
{
    float l = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]) ?: 1.0f;
    v[0]/=l; v[1]/=l; v[2]/=l;
}

int main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no X display\n"); return 1; }

    static int fbattr[] = {GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
                           GLX_RENDER_TYPE, GLX_RGBA_BIT,
                           GLX_DOUBLEBUFFER, True, GLX_DEPTH_SIZE, 24, None};
    int nfb = 0;
    GLXFBConfig *fbs = glXChooseFBConfig(dpy, DefaultScreen(dpy), fbattr, &nfb);
    if (!fbs || nfb <= 0) { fprintf(stderr, "no fbconfig\n"); return 1; }
    GLXFBConfig fb = fbs[0];

    int ctxattr[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                     GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                     GLX_CONTEXT_PROFILE_MASK_ARB,
                     GLX_CONTEXT_CORE_PROFILE_BIT_ARB, None};
    GLXContext ctx = glXCreateContextAttribsARB(dpy, fb, 0, True, ctxattr);
    if (!ctx) { fprintf(stderr, "no core context\n"); return 1; }

    XVisualInfo *vi = glXGetVisualFromFBConfig(dpy, fb);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                                   vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                     PointerMotionMask;
    int W = DisplayWidth(dpy, vi->screen);
    int H = DisplayHeight(dpy, vi->screen);
    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, W, H, 0,
                               vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);
    XSizeHints hints;
    hints.flags = PPosition | PSize;
    hints.x = 0; hints.y = 0; hints.width = W; hints.height = H;
    XSetWMNormalHints(dpy, win, &hints);
    XMapWindow(dpy, win);
    XMoveResizeWindow(dpy, win, 0, 0, W, H);
    glXMakeCurrent(dpy, win, ctx);

    /* pull every modern entry point we need */
    void *p_create   = glsym("glCreateShader");
    void *p_src      = glsym("glShaderSource");
    void *p_comp     = glsym("glCompileShader");
    void *p_gsi      = glsym("glGetShaderiv");
    void *p_gsil     = glsym("glGetShaderInfoLog");
    void *p_cprog    = glsym("glCreateProgram");
    void *p_attach   = glsym("glAttachShader");
    void *p_link     = glsym("glLinkProgram");
    void *p_gpi      = glsym("glGetProgramiv");
    void *p_use      = glsym("glUseProgram");
    void *p_loc      = glsym("glGetUniformLocation");
    void *p_u1f      = glsym("glUniform1f");
    void *p_u1i      = glsym("glUniform1i");
    void *p_u3f      = glsym("glUniform3f");
    void *p_u4fv     = glsym("glUniform4fv");
    void *p_gva      = glsym("glGenVertexArrays");
    void *p_bva      = glsym("glBindVertexArray");
    void *p_gbuf     = glsym("glGenBuffers");
    void *p_bbuf     = glsym("glBindBuffer");
    void *p_bufdata  = glsym("glBufferData");
    void *p_enatt    = glsym("glEnableVertexAttribArray");
    void *p_vap      = glsym("glVertexAttribPointer");
    void *p_draw     = glsym("glDrawArrays");
    void *p_u2f      = glsym("glUniform2f");
    void *p_um4      = glsym("glUniformMatrix4fv");
    void *p_read     = glsym("glReadPixels");
    void *p_gpil     = glsym("glGetProgramInfoLog");

    #define FN(name) ((typeof(&name))name)

    #define COMPILE_CHECK(sh, tag) do { GLint okc = 0; \
        ((PFNglGetShaderiv)p_gsi)((sh), GL_COMPILE_STATUS, &okc); \
        if (!okc) { char lg[2048]; GLsizei ln = 0; \
            ((PFNglGetShaderInfoLog)p_gsil)((sh), sizeof(lg), &ln, lg); \
            fprintf(stderr, "%s compile failed: %s\n", tag, lg); return 1; } } while (0)

    GLuint vs_orb = ((PFNglCreateShader)p_create)(GL_VERTEX_SHADER);
    ((PFNglShaderSource)p_src)(vs_orb, 1, &VS_FULL, NULL);
    ((PFNglCompileShader)p_comp)(vs_orb);
    COMPILE_CHECK(vs_orb, "orb vertex");
    GLuint fs_orb = ((PFNglCreateShader)p_create)(GL_FRAGMENT_SHADER);
    ((PFNglShaderSource)p_src)(fs_orb, 1, &FS_ORB, NULL);
    ((PFNglCompileShader)p_comp)(fs_orb);
    COMPILE_CHECK(fs_orb, "orb fragment");
    GLuint prog = ((PFNglCreateProgram)p_cprog)();
    ((PFNglAttachShader)p_attach)(prog, vs_orb);
    ((PFNglAttachShader)p_attach)(prog, fs_orb);
    ((PFNglLinkProgram)p_link)(prog);

    GLint ok = 0;
    ((PFNglGetProgramiv)p_gpi)(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei len = 0;
        ((PFNglGetProgramInfoLog)p_gpil)(prog, sizeof(log), &len, log);
        fprintf(stderr, "orb program link failed: %s\n", log);
        return 1;
    }

    GLuint vs_box = ((PFNglCreateShader)p_create)(GL_VERTEX_SHADER);
    ((PFNglShaderSource)p_src)(vs_box, 1, &VS_BOX, NULL);
    ((PFNglCompileShader)p_comp)(vs_box);
    COMPILE_CHECK(vs_box, "box vertex");
    GLuint fs_box = ((PFNglCreateShader)p_create)(GL_FRAGMENT_SHADER);
    ((PFNglShaderSource)p_src)(fs_box, 1, &FS_BOX, NULL);
    ((PFNglCompileShader)p_comp)(fs_box);
    COMPILE_CHECK(fs_box, "box fragment");
    GLuint bprog = ((PFNglCreateProgram)p_cprog)();
    ((PFNglAttachShader)p_attach)(bprog, vs_box);
    ((PFNglAttachShader)p_attach)(bprog, fs_box);
    ((PFNglLinkProgram)p_link)(bprog);
    ((PFNglGetProgramiv)p_gpi)(bprog, GL_LINK_STATUS, &ok);
    if (!ok) { fprintf(stderr, "box program link failed\n"); return 1; }

    GLuint vao = 0;
    ((PFNglGenVertexArrays)p_gva)(1, &vao);
    ((PFNglBindVertexArray)p_bva)(vao);

    float blobs[MAXB][4];
    float lo[3], hi[3];
    wubu_buddy_orb_t *orb = wubu_buddy_orb_create(9);

    float yaw = 0.6f, pitch = 0.25f, rad = 3.4f, tsec = 0.0f;
    int paused = 0;
    struct timespec t0, tn;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress) {
                KeySym k = XKeycodeToKeysym(dpy, ev.xkey.keycode, 0);
                if (k == XK_Escape || k == XK_q) { goto done; }
                if (k == XK_space) paused = !paused;
                if (k == XK_Left)  yaw -= 0.12f;
                if (k == XK_Right) yaw += 0.12f;
                if (k == XK_Up)    pitch += 0.08f;
                if (k == XK_Down)  pitch -= 0.08f;
                if (k == XK_equal || k == XK_plus) rad -= 0.18f;
                if (k == XK_minus) rad += 0.18f;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &tn);
        float dt = (float)(tn.tv_sec - t0.tv_sec) +
                   (float)(tn.tv_nsec - t0.tv_nsec) / 1e9f;
        t0 = tn;
        if (!paused) tsec += dt;

        wubu_buddy_orb_set_time(orb, tsec);
        int n = wubu_buddy_orb_blob_count(orb);
        for (int i = 0; i < n; ++i) {
            float c[3], r;
            wubu_buddy_orb_get_blob(orb, i, c, &r);
            blobs[i][0]=c[0]; blobs[i][1]=c[1]; blobs[i][2]=c[2]; blobs[i][3]=r;
        }
        wubu_buddy_orb_render_box(orb, lo, hi);

        glClearColor(0.02f, 0.022f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, W, H);
        ((PFNglUseProgram)p_use)(prog);

        float eye[3] = { rad*sinf(yaw)*cosf(pitch),
                         rad*sinf(pitch),
                         rad*cosf(yaw)*cosf(pitch) };
        float fwd[3] = { -eye[0], -eye[1], -eye[2] };
        normalize3(fwd);
        float up[3] = {0,1,0};
        float right[3] = { fwd[1]*up[2]-fwd[2]*up[1],
                           fwd[2]*up[0]-fwd[0]*up[2],
                           fwd[0]*up[1]-fwd[1]*up[0] };
        normalize3(right);
        float rup[3] = { right[1]*fwd[2]-right[2]*fwd[1],
                         right[2]*fwd[0]-right[0]*fwd[2],
                         right[0]*fwd[1]-right[1]*fwd[0] };

        ((PFNglUniform1i)p_u1i)(((PFNglGetUniformLocation)p_loc)(prog,"uCount"), n);
        GLint l;
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uRes");
        ((PFNglUniform2f)p_u2f)(l, (GLfloat)W, (GLfloat)H);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uEye");
        ((PFNglUniform3f)p_u3f)(l, eye[0], eye[1], eye[2]);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uFwd");
        ((PFNglUniform3f)p_u3f)(l, fwd[0], fwd[1], fwd[2]);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uRight");
        ((PFNglUniform3f)p_u3f)(l, right[0], right[1], right[2]);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uUp");
        ((PFNglUniform3f)p_u3f)(l, rup[0], rup[1], rup[2]);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uFov");
        ((PFNglUniform1f)p_u1f)(l, 0.62f);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uBoxLo");
        ((PFNglUniform3f)p_u3f)(l, lo[0], lo[1], lo[2]);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uBlob");
        ((PFNglUniform4fv)p_u4fv)(l, MAXB, &blobs[0][0]);

        ((PFNglBindVertexArray)p_bva)(vao);
        ((PFNglDrawArrays)p_draw)(GL_TRIANGLES, 0, 3);

        /* render-space box: 12 edges generated from gl_VertexID, no VBO */
        ((PFNglUseProgram)p_use)(bprog);
        float proj[16], view[16], vp[16];
        perspective(proj, 0.9f, (float)W / (float)H, 0.05f, 40.0f);
        look_at(view, eye, (float[3]){0, 0, 0}, (float[3]){0, 1, 0});
        mat4_mul(vp, proj, view);
        l = ((PFNglGetUniformLocation)p_loc)(bprog,"uVP");
        ((PFNglUniformMatrix4fv)p_um4)(l, 1, GL_FALSE, vp);
        l = ((PFNglGetUniformLocation)p_loc)(bprog,"uLo");
        ((PFNglUniform3f)p_u3f)(l, lo[0], lo[1], lo[2]);
        l = ((PFNglGetUniformLocation)p_loc)(bprog,"uHi");
        ((PFNglUniform3f)p_u3f)(l, hi[0], hi[1], hi[2]);
        l = ((PFNglGetUniformLocation)p_loc)(bprog,"uAccent");
        ((PFNglUniform3f)p_u3f)(l, 1.0f, 0.74f, 0.30f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        ((PFNglDrawArrays)p_draw)(GL_LINES, 0, 24);
        glDisable(GL_BLEND);

        glXSwapBuffers(dpy, win);

        /* one-shot framebuffer dump: bypasses X window capture entirely */
        static int dumped = 0;
        if (!dumped && tsec > 1.0f) {
            dumped = 1;
            unsigned char *buf = malloc((size_t)W * H * 3);
            ((PFNglReadPixels)p_read)(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, buf);
            FILE *fp = fopen("/tmp/bod_snap.ppm", "wb");
            if (fp) {
                fprintf(fp, "P6\n%d %d\n255\n", W, H);
                fwrite(buf, 3, (size_t)W * H, fp);
                fclose(fp);
                fprintf(stderr, "snapshot written %dx%d\n", W, H);
            }
            free(buf);
        }
    }
done:
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, ctx);
    XCloseDisplay(dpy);
    wubu_buddy_orb_destroy(orb);
    return 0;
}
