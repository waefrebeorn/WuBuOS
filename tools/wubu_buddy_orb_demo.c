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
#include "wubu_buddy_emotion.h"
#include "wubutime.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <math.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
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
typedef void (*PFNglBlitFramebuffer)(GLint, GLint, GLint, GLint,
                                     GLint, GLint, GLint, GLint,
                                     GLbitfield, GLenum);
typedef void (*PFNglGenFramebuffers)(GLsizei, GLuint *);
typedef void (*PFNglBindFramebuffer)(GLenum, GLuint);
typedef void (*PFNglGenTextures)(GLsizei, GLuint *);
typedef void (*PFNglBindTexture)(GLenum, GLuint);
typedef void (*PFNglTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint,
                                GLenum, GLenum, const void *);
typedef void (*PFNglFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (*PFNglCheckFramebufferStatus)(GLenum);
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
    "uniform float uTime;\n"
    "uniform vec4  uEmo;\n"
    "uniform int   uCount;\n"
    "uniform vec4  uBlob[MAXB];\n"
    "uniform vec3  uBoxLo, uBoxHi;\n"
    "uniform vec3  uHead;\n"
    /* True distance estimator: smooth minimum of the component spheres.
     * The old 1/sqrt(field) heuristic is NOT Lipschitz -- it overshot through
     * thin regions and punched false holes in the surface. */
    "float smin(float a, float b, float k){\n"
    "  float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);\n"
    "  return mix(b, a, h) - k*h*(1.0-h); }\n"
    "float sdSeg2(vec2 p, vec2 a, vec2 b){\n"
    "  vec2 pa=p-a, ba=b-a; float h=clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);\n"
    "  return length(pa-ba*h); }\n"
    "float sdRoundCone(vec3 p, vec3 a, vec3 b, float r1, float r2){\n"
    "  vec3 ba=b-a; float l2=dot(ba,ba); float rr=r1-r2;\n"
    "  float a2=l2-rr*rr; float il2=1.0/l2;\n"
    "  vec3 pa=p-a; float y=dot(pa,ba); float z=y-l2;\n"
    "  vec3 xp=pa*l2-ba*y; float x2=dot(xp,xp);\n"
    "  float y2=y*y*l2; float z2=z*z*l2;\n"
    "  float k=sign(rr)*rr*rr*x2;\n"
    "  if (sign(z)*a2*z2>k) return sqrt(x2+z2)*il2-r2;\n"
    "  if (sign(y)*a2*y2<k) return sqrt(x2+y2)*il2-r1;\n"
    "  return (sqrt(x2*a2*il2)+y*rr)*il2-r1; }\n"    "float sdCapsule(vec3 p, vec3 a, vec3 b, float r){\n"
    "  vec3 pa=p-a, ba=b-a; float h=clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);\n"
    "  return length(pa-ba*h)-r; }\n"
    "float sdEllipsoid(vec3 p, vec3 c, vec3 r){\n"
    "  float k0 = length((p-c)/r);\n"
    "  return (k0-1.0)*min(r.x,min(r.y,r.z))/max(k0,1e-5); }\n"
    /* Explicit anatomy: a bean silhouette comes from relying on the metaball
     * cloud for the whole figure.  The form is now built: body ellipsoid that
     * still breathes through the blobs, a head blended in for a neck, and two
     * upright ears. */
    "float sdBlob(vec3 p){ float d = 1e6;\n"
    "  d = sdEllipsoid(p, vec3(0.0,-0.30,0.0), vec3(0.80,0.95,0.70));\n"
    "  for(int i=0;i<MAXB;i++){ if(i>=uCount) break;\n"
    "    d = smin(d, length(p-uBlob[i].xyz)-uBlob[i].w*1.42, 0.34); }\n"
    "  vec3 hd = uHead;                            /* head centre */\n"
    "  d = smin(d, length(p-hd)-0.62, 0.28);       /* neck blend */\n"
    "  float droop = 0.62*uEmo.y;                  /* stress drags ears */\n"
    "  d = min(d, sdRoundCone(p, hd+vec3(-0.28,0.40,0.0),\n"
    "                          hd+vec3(-0.42,1.45-droop,0.0), 0.155, 0.07));\n"
    "  d = min(d, sdRoundCone(p, hd+vec3( 0.28,0.40,0.0),\n"
    "                          hd+vec3( 0.42,1.45-droop,0.0), 0.155, 0.07));\n"
    "  return d; }\n"
    "float h31(vec3 p){ return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453); }\n"
    "float vnoise(vec3 p){\n"
    "  vec3 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);\n"
    "  return mix(mix(mix(h31(i),h31(i+vec3(1,0,0)),f.x),\n"
    "                 mix(h31(i+vec3(0,1,0)),h31(i+vec3(1,1,0)),f.x),f.y),\n"
    "             mix(mix(h31(i+vec3(0,0,1)),h31(i+vec3(1,0,1)),f.x),\n"
    "                 mix(h31(i+vec3(0,1,1)),h31(i+vec3(1,1,1)),f.x),f.y),f.z); }\n"
    "vec3 norm(vec3 p){ vec2 e=vec2(0.002,0.0);\n"
    "  return normalize(vec3(sdBlob(p+e.xyy)-sdBlob(p-e.xyy),\n"
    "                        sdBlob(p+e.yxy)-sdBlob(p-e.yxy),\n"
    "                        sdBlob(p+e.yyx)-sdBlob(p-e.yyx))); }\n"
    "void main(){\n"
    "  vec2 ndc = vUv*2.0-1.0; ndc.x *= uRes.x/uRes.y;\n"
    "  vec3 ro = uEye;\n"
    "  vec3 rd = normalize(uFwd + uFov*(ndc.x*uRight + ndc.y*uUp));\n"
    "  vec3 bg = mix(vec3(0.030,0.032,0.048), vec3(0.070,0.076,0.104),\n"
    "                smoothstep(-0.7,0.9,rd.y));\n"
    "  ro.y -= 0.16*uEmo.x*sin(uTime*6.2);   /* energy makes it hop */\n"
    "  float t=0.0; bool hit=false; vec3 p=ro;\n"
    "  for(int i=0;i<180;i++){\n"
    "    p = ro + rd*t;\n"
    "    float d = sdBlob(p);          /* honest distance, safe stride */\n"
    "    if (d < 0.0018){ hit=true; break; }\n"
    "    t += d*0.85;\n"
    "    if (t > 14.0) break;\n"
    "  }\n"
    "  vec3 col = bg;\n"
    "  if (hit){\n"
    "    vec3 n = norm(p);\n"
    "    vec3 q = p*15.0 + vec3(0.0, uTime*0.25, 0.0);\n"
    "    vec3 g = vec3(vnoise(q+vec3(0.03,0,0))-vnoise(q-vec3(0.03,0,0)),\n"
    "                  vnoise(q+vec3(0,0.03,0))-vnoise(q-vec3(0,0.03,0)),\n"
    "                  vnoise(q+vec3(0,0,0.03))-vnoise(q-vec3(0,0,0.03)));\n"
    "    n = normalize(n + 0.5*g);   /* living surface, not sterile plastic */\n"
    "    vec3 L = normalize(vec3(-0.45,0.75,0.55));\n"
    "    float dif = max(dot(n,L),0.0);\n"
    "    float hl  = 0.38;                       /* headlight: front never black */\n"
    "    vec3 h = normalize(L - rd);\n"
    "    vec3 L2 = normalize(vec3(0.6,-0.1,0.8));  /* camera-side fill */\n"
    "    float spec = pow(max(dot(n,h),0.0), 28.0);\n"
    "    float fres = pow(1.0 - max(dot(n,-rd),0.0), 3.0);\n"
    "    vec3 base = vec3(0.93,0.92,0.88)*(0.94+0.08*vnoise(p*16.0));\n"
    "    vec3 hd = uHead;                            /* head centre */\n"
    "    /* green aviator cap: the top hemisphere of the head */\n"
    "    float cap = smoothstep(hd.y+0.08, hd.y+0.22, p.y)\n"
    "              * (1.0-smoothstep(0.60, 0.72, length(p-hd)));\n"
    "    base = mix(base, vec3(0.14,0.42,0.22), cap);\n"
    "    /* red neckerchief tied where head meets body */\n"
    "    float scarf = smoothstep(hd.y-0.72, hd.y-0.58, p.y)\n"
    "                * (1.0-smoothstep(hd.y-0.40, hd.y-0.26, p.y))\n"
    "                * (1.0-smoothstep(0.62,0.80,length(p.xz-hd.xz)));\n"
    "    base = mix(base, vec3(0.72,0.10,0.12), scarf);\n"
    "    /* red W, four strokes drawn on the cap's front */\n"
    "    vec2 wp = vec2(p.x-hd.x, p.y-hd.y);\n"
    "    float wd = 1e3;\n"
    "    wd = min(wd, sdSeg2(wp, vec2(-0.20,0.48), vec2(-0.10,0.20)));\n"
    "    wd = min(wd, sdSeg2(wp, vec2(-0.10,0.20), vec2( 0.00,0.40)));\n"
    "    wd = min(wd, sdSeg2(wp, vec2( 0.00,0.40), vec2( 0.10,0.20)));\n"
    "    wd = min(wd, sdSeg2(wp, vec2( 0.10,0.20), vec2( 0.20,0.48)));\n"
    "    float wm = (1.0-smoothstep(0.018,0.034,wd)) * step(0.0, p.z-hd.z);\n"
    "    base = mix(base, vec3(0.93,0.10,0.10), wm);\n"
    "    /* eyes and nose: dark ovals on the front of the muzzle */\n"
    "    float front = step(0.05, p.z-hd.z);\n"
    "    float m = uEmo.w;\n"
    "    float ey = 1.35;                       /* base vertical stretch */\n"
    "    if (m > 0.5 && m < 1.5) ey = 1.9;      /* joy: happy squint */\n"
    "    else if (m > 1.5 && m < 2.5) ey = 2.2; /* excited: wide grin-eyes */\n"
    "    else if (m > 2.5 && m < 3.5) ey = 0.75;/* alert: open wide */\n"
    "    else if (m > 4.5) ey = 0.55;           /* sleepy: half closed */\n"
    "    float eL = length((wp-vec2(-0.20,-0.06))*vec2(1.0,ey));\n"
    "    float eR = length((wp-vec2( 0.20,-0.06))*vec2(1.0,ey));\n"
    "    float nose = length((wp-vec2(0.0,-0.24))*vec2(1.0,1.4));\n"
    "    base = mix(base, vec3(0.06,0.07,0.08),\n"
    "               front*(1.0-smoothstep(0.035,0.052,min(eL,eR))));\n"
    "    base = mix(base, vec3(0.45,0.06,0.07),\n"
    "               front*(1.0-smoothstep(0.022,0.034,nose)));\n"
    "    vec3 acc  = mix(vec3(1.00,0.74,0.30), vec3(1.00,0.32,0.14),\n                     clamp(uEmo.y*0.85 + uEmo.z*0.25, 0.0, 1.0));\n"
    "    col = base*(0.22 + hl*0.42 + dif*0.52 + 0.22*max(dot(n,L2),0.0))\n"
    "        + vec3(1.0)*spec*0.24 + acc*fres*0.95;\n"
    "  }\n"
    "  /* faint floor line so the render space reads as a place */\n"
    "  if (rd.y < -0.02){\n"
    "    float tf = (uBoxLo.y - ro.y)/rd.y;\n"
    "    if (tf>0.0 && tf<t+2.0){ vec3 fp = ro+rd*tf;\n"
    "      vec2 g = abs(fract(fp.xz*2.0)-0.5);\n"
    "      float line = smoothstep(0.48,0.5,max(g.x,g.y));\n"
    "      col += vec3(0.045,0.05,0.06)*line*smoothstep(6.0,1.0,tf); }\n"
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
    /* ONE orb per session. The desktop forks this binary on start, and it calls
     * setsid() so the orb survives a compositor restart -- which also means every
     * desktop start (including one that fails later, e.g. "no Wayland display")
     * used to leave another orb running: measured, two orbs at ~5% CPU each for
     * over three hours. An exclusive lock makes the second instance exit quietly
     * instead of doubling the animation cost on screen. */
    const char *rd = getenv("XDG_RUNTIME_DIR");
    char lockpath[512];
    snprintf(lockpath, sizeof lockpath, "%s/wubu-buddy.lock",
             (rd && *rd) ? rd : "/tmp");
    int lockfd = open(lockpath, O_CREAT | O_RDWR, 0600);
    if (lockfd >= 0 && flock(lockfd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "wubu-buddy: already running (%s)\n", lockpath);
        return 0;                      /* one orb per session; not an error */
    }

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
    void *p_blit     = glsym("glBlitFramebuffer");
    void *p_gfb      = glsym("glGenFramebuffers");
    void *p_bfb      = glsym("glBindFramebuffer");
    void *p_gtex     = glsym("glGenTextures");
    void *p_btex     = glsym("glBindTexture");
    void *p_teximg   = glsym("glTexImage2D");
    void *p_fbtex    = glsym("glFramebufferTexture2D");
    void *p_chkfb    = glsym("glCheckFramebufferStatus");
    wubu_buddy_emotion *emo = NULL;
    wubu_buddy_emotion_create(&emo);
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

    /* Render space: a real offscreen framebuffer.  The X screen here is only
     * 320x200, so drawing to the window would throw away almost all detail. */
    const int SW = 960, SH = 540;
    GLuint fbo = 0, rtex = 0;
    ((PFNglGenFramebuffers)p_gfb)(1, &fbo);
    ((PFNglBindFramebuffer)p_bfb)(GL_FRAMEBUFFER, fbo);
    ((PFNglGenTextures)p_gtex)(1, &rtex);
    ((PFNglBindTexture)p_btex)(GL_TEXTURE_2D, rtex);
    ((PFNglTexImage2D)p_teximg)(GL_TEXTURE_2D, 0, GL_RGBA, SW, SH, 0,
                               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ((PFNglFramebufferTexture2D)p_fbtex)(GL_FRAMEBUFFER,
                                        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                        rtex, 0);
    GLenum fbstat = ((PFNglCheckFramebufferStatus)p_chkfb)(GL_FRAMEBUFFER);
    if (fbstat != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "render-space FBO incomplete: 0x%x\n", fbstat);
        return 1;
    }
    ((PFNglBindFramebuffer)p_bfb)(GL_FRAMEBUFFER, 0);

    float blobs[MAXB][4];
    float lo[3], hi[3];
    wubu_buddy_orb_t *orb = wubu_buddy_orb_create(9);

    float yaw = 0.6f, pitch = 0.25f, rad = 3.4f, tsec = 0.0f;
    const int dbg_ears = getenv("WUBU_BUDDY_DEBUG_EARS") != NULL;
    float off_bob = 0.0f;
    int paused = 0;
    int sel = 0;                 /* selected blob for live sculpting */
    float off[WUBU_BUDDY_ORB_MAX_BLOBS][4] = {{0}}; /* dx,dy,dz,dr */
    /* Motion clock belongs to WuBuTIME, not to libc timing. */
    wubutime *tm = NULL;
    wubutime_create(&tm);
    uint64_t t_mono0 = wubutime_mono_ns(tm);
    uint64_t t_prev = t_mono0;

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
                if (k >= XK_1 && k <= XK_9) sel = (int)(k - XK_1);
                if (k == XK_w) off[sel][1] += 0.05f;   /* lift */
                if (k == XK_s) off[sel][1] -= 0.05f;
                if (k == XK_a) off[sel][0] -= 0.05f;
                if (k == XK_d) off[sel][0] += 0.05f;
                if (k == XK_bracketleft)  off[sel][3] -= 0.02f;
                if (k == XK_bracketright) off[sel][3] += 0.02f;
                if (k == XK_r) for (int j = 0; j < MAXB; ++j)
                    off[j][0] = off[j][1] = off[j][2] = off[j][3] = 0.0f;
            }
        }
        uint64_t t_now = wubutime_mono_ns(tm);
        float dt = (float)((int64_t)(t_now - t_prev)) / 1e9f;
        t_prev = t_now;
        if (!paused) tsec += dt;

        wubu_buddy_orb_set_time(orb, tsec);
        int n = wubu_buddy_orb_blob_count(orb);
        for (int i = 0; i < n; ++i) {
            float c[3], r;
            wubu_buddy_orb_get_blob(orb, i, c, &r);
            wubu_buddy_emotion_poll(emo, 0.5f);
        blobs[i][0]=c[0]+off[i][0]; blobs[i][1]=c[1]+off[i][1];
            blobs[i][2]=c[2]+off[i][2]; blobs[i][3]=r+off[i][3];
        }
        wubu_buddy_orb_render_box(orb, lo, hi);

        ((PFNglBindFramebuffer)p_bfb)(GL_FRAMEBUFFER, fbo);
        glClearColor(0.02f, 0.022f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, SW, SH);
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

        ((PFNglUniform1i)p_u1i)(((PFNglGetUniformLocation)p_loc)(prog,"uCount"), dbg_ears ? 0 : n);
        GLint l;
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uRes");
        ((PFNglUniform2f)p_u2f)(l, (GLfloat)SW, (GLfloat)SH);

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
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uTime");
        ((PFNglUniform1f)p_u1f)(l, tsec);
        /* Explicit anatomy: the head centre is authored, so the silhouette is
         * correct by construction instead of being read off the blob cloud. */
        float hd[3] = { 0.0f, 0.75f + 0.10f*off_bob, 0.0f };
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uHead");
        ((PFNglUniform3f)p_u3f)(l, hd[0], hd[1], hd[2]);

        float emo_v[4] = { wubu_buddy_emotion_energy(emo),
                           wubu_buddy_emotion_stress(emo),
                           wubu_buddy_emotion_warmth(emo),
                           wubu_buddy_emotion_mood(emo) };
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uEmo");
        ((PFNglUniform4fv)p_u4fv)(l, 1, emo_v);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uBoxLo");
        ((PFNglUniform3f)p_u3f)(l, lo[0], lo[1], lo[2]);
        l = ((PFNglGetUniformLocation)p_loc)(prog,"uBlob");
        ((PFNglUniform4fv)p_u4fv)(l, MAXB, &blobs[0][0]);

        ((PFNglBindVertexArray)p_bva)(vao);
        ((PFNglDrawArrays)p_draw)(GL_TRIANGLES, 0, 3);

        /* render-space box: 12 edges generated from gl_VertexID, no VBO */
        ((PFNglUseProgram)p_use)(bprog);
        float proj[16], view[16], vp[16];
        perspective(proj, 0.9f, (float)SW / (float)SH, 0.05f, 40.0f);
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

        ((PFNglBindFramebuffer)p_bfb)(GL_READ_FRAMEBUFFER, fbo);
        ((PFNglBindFramebuffer)p_bfb)(GL_DRAW_FRAMEBUFFER, 0);
        ((PFNglBlitFramebuffer)p_blit)(0, 0, SW, SH, 0, 0, W, H,
                                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
        /* live link: print what the buddy is feeling, straight from sensors */
        static float hud = 0.0f;
        if (tsec - hud > 1.5f) {
            hud = tsec;
            fprintf(stderr,
                "cpu %.1fC gpu %.1fC ssd %.1fC load %.2f | energy %.2f "
                "stress %.2f warmth %.2f mood %.0f\n",
                wubu_buddy_emotion_cpu_c(emo), wubu_buddy_emotion_gpu_c(emo),
                wubu_buddy_emotion_ssd_c(emo), wubu_buddy_emotion_load(emo),
                wubu_buddy_emotion_energy(emo), wubu_buddy_emotion_stress(emo),
                wubu_buddy_emotion_warmth(emo), wubu_buddy_emotion_mood(emo));
        }
        glXSwapBuffers(dpy, win);

        /* one-shot framebuffer dump: bypasses X window capture entirely */
        static int dumped = 0;
        if (!dumped && tsec > 1.0f) {
            dumped = 1;
            unsigned char *buf = malloc((size_t)SW * SH * 3);
            ((PFNglReadPixels)p_read)(0, 0, SW, SH, GL_RGB,
                                      GL_UNSIGNED_BYTE, buf);
            FILE *fp = fopen("/tmp/bod_snap.ppm", "wb");
            if (fp) {
                fprintf(fp, "P6\n%d %d\n255\n", SW, SH);
                /* glReadPixels is bottom-up; write top-down so the image is not
                 * upside down (every earlier audit was flipped). */
                for (int yy = SH - 1; yy >= 0; --yy)
                    fwrite(buf + (size_t)yy * SW * 3, 3, (size_t)SW, fp);
                fclose(fp);
                fprintf(stderr, "snapshot written %dx%d\n", SW, SH);
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
