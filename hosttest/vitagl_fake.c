/* hosttest: vitaGL sobre OpenGL de Mesa (llvmpipe, perfil de compatibilidad =
 * pipeline fijo de GLES 1.1) en un pbuffer EGL sin pantalla ("surfaceless").
 * Las funciones solo de GLES 1.1 (float/punto fijo) se simulan al final. vglSwapBuffers lee el cuadro y lo manda
 * a la ventana SDL (window_sdl.c) y, cada HOSTTEST_FRAME_MS, a un PPM. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "vitaGL.h"
#include "window_sdl.h"
#include "macho.h"

/* CTouchPad::s_currentDeviceOrientation del juego (1 o 3), como uikit.c */
static int guest_orientation(void)
{
    static int *o;
    if (!o) o = (int *)macho_sym("__ZN9CTouchPad26s_currentDeviceOrientationE");
    return o ? *o : 3;
}

static EGLDisplay s_dpy;
static EGLSurface s_surf;
static EGLContext s_ctx;
static int s_w, s_h;
static uint32_t *s_pixels, *s_flipped;

GLboolean vglInitExtended(int legacy_pool_size, int width, int height, int ram_threshold, int msaa)
{
    static const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE };
    /* GL de escritorio, perfil de compatibilidad: Debian compila Mesa sin GLES 1.1 */
    static const EGLint ctx_attr[] = { EGL_NONE };
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_display =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLConfig cfg;
    EGLint n, surf_attr[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
    EGLContext ctx;
    (void)legacy_pool_size; (void)ram_threshold; (void)msaa;
    const char *step = "display";
    s_dpy = get_display ? get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL) : EGL_NO_DISPLAY;
    if (s_dpy == EGL_NO_DISPLAY || (step = "initialize", !eglInitialize(s_dpy, NULL, NULL))
            || (step = "config", !eglChooseConfig(s_dpy, cfg_attr, &cfg, 1, &n) || !n)
            || (step = "bind", !eglBindAPI(EGL_OPENGL_API))
            || (step = "pbuffer", (s_surf = eglCreatePbufferSurface(s_dpy, cfg, surf_attr)) == EGL_NO_SURFACE)
            || (step = "context", (ctx = eglCreateContext(s_dpy, cfg, EGL_NO_CONTEXT, ctx_attr)) == EGL_NO_CONTEXT)
            || (step = "make current", !eglMakeCurrent(s_dpy, s_surf, s_surf, ctx))) {
        fprintf(stderr, "[HOSTTEST] EGL/GLES1 no disponible: %s (error 0x%X)\n", step, eglGetError());
        exit(1);
    }
    s_ctx = ctx;
    s_w = width; s_h = height;
    s_pixels = malloc((size_t)width * height * 4);
    s_flipped = malloc((size_t)width * height * 4);
    fprintf(stderr, "[HOSTTEST] GLES %s (%s) en pbuffer %dx%d\n", glGetString(GL_VERSION),
            glGetString(GL_RENDERER), width, height);
    return GL_TRUE;
}
GLboolean vglInitWithCustomThreshold(int pool_size, int width, int height, int ram_threshold, int cdram_threshold,
                                     int phycont_threshold, int cdlg_threshold, int msaa)
{
    (void)cdram_threshold, (void)phycont_threshold, (void)cdlg_threshold;
    return vglInitExtended(pool_size, width, height, ram_threshold, msaa);
}

static void dump_ppm(void)
{
    static int every = -1;
    static unsigned shot;
    static time_t next;
    char path[256];
    FILE *f;
    int i;
    if (every < 0) every = getenv("HOSTTEST_FRAME_S") ? atoi(getenv("HOSTTEST_FRAME_S")) : 5;
    if (!every || time(NULL) < next) return;
    next = time(NULL) + every;
    mkdir("/data/frames", 0777);
    snprintf(path, sizeof path, "/data/frames/%04u.ppm", shot++);
    if (!(f = fopen(path, "wb"))) return;
    fprintf(f, "P6\n%d %d\n255\n", s_w, s_h);
    for (i = 0; i < s_w * s_h; i++) fwrite(&s_flipped[i], 1, 3, f);   /* RGBA -> RGB */
    fclose(f);
}

void vglSwapBuffers(GLboolean has_commondialog)
{
    GLint fb;
    int y;
    (void)has_commondialog;
    /* se lee del framebuffer de la ventana, no del FBO que el juego tenga puesto */
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_OES, &fb);
    {
        static int once;
        if (!once++) {
            GLint vp[4];
            glBindFramebufferOES(GL_FRAMEBUFFER_OES, 1);
            fprintf(stderr, "[HOSTTEST] FBO 1 status 0x%X\n", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
            glBindFramebufferOES(GL_FRAMEBUFFER_OES, (GLuint)fb);
            glGetIntegerv(GL_VIEWPORT, vp);
            (void)vp;
        }
    }
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
    glReadPixels(0, 0, s_w, s_h, GL_RGBA, GL_UNSIGNED_BYTE, s_pixels);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, (GLuint)fb);
    /* gles.c gira el FBO retrato 320x480 a paisaje (±90° según la orientación del
     * juego) con la convención de vitaGL, que guarda las texturas FBO con el eje Y
     * al revés que Mesa. Compuesto con el giro, en Mesa sale espejado en Y con la
     * orientación 3 y en X con la 1 (comprobado con la pantalla de título). */
    for (y = 0; y < s_h; y++) {
        int x;
        if (guest_orientation() == 1) {
            const uint32_t *src = &s_pixels[y * s_w];
            for (x = 0; x < s_w; x++) s_flipped[y * s_w + x] = src[s_w - 1 - x];
        } else {
            memcpy(&s_flipped[y * s_w], &s_pixels[(s_h - 1 - y) * s_w], (size_t)s_w * 4);
        }
    }
    hosttest_window_frame(s_flipped, s_w, s_h, s_w);
    dump_ppm();
    eglSwapBuffers(s_dpy, s_surf);
}

void vglUseTripleBuffering(GLboolean usage) { (void)usage; }
size_t vglMemFree(vglMemType type) { (void)type; return 64u << 20; }

/* GL_OES_framebuffer_object = FBO del núcleo de GL 3.0 (mismo nombre sin "OES"). */
#define OES(ret, name, params, args) \
    ret name params { \
        static ret (*fn) params; \
        char core[64]; \
        if (!fn) { snprintf(core, sizeof core, "%s", #name); core[strlen(core) - 3] = 0; \
                   fn = (ret (*) params)eglGetProcAddress(core); } \
        return fn args; \
    }
OES(void, glGenFramebuffersOES, (GLsizei n, GLuint *i), (n, i))
OES(void, glDeleteFramebuffersOES, (GLsizei n, const GLuint *i), (n, i))
OES(void, glBindFramebufferOES, (GLenum t, GLuint f), (t, f))
OES(void, glFramebufferTexture2DOES, (GLenum t, GLenum a, GLenum tt, GLuint x, GLint l), (t, a, tt, x, l))
OES(void, glFramebufferRenderbufferOES, (GLenum t, GLenum a, GLenum rt, GLuint r), (t, a, rt, r))
OES(GLenum, glCheckFramebufferStatusOES, (GLenum t), (t))
OES(void, glGetFramebufferAttachmentParameterivOES, (GLenum t, GLenum a, GLenum p, GLint *v), (t, a, p, v))
OES(void, glGenRenderbuffersOES, (GLsizei n, GLuint *i), (n, i))
OES(void, glDeleteRenderbuffersOES, (GLsizei n, const GLuint *i), (n, i))
OES(void, glBindRenderbufferOES, (GLenum t, GLuint r), (t, r))
OES(void, glRenderbufferStorageOES, (GLenum t, GLenum f, GLsizei w, GLsizei h), (t, f, w, h))
OES(void, glGetRenderbufferParameterivOES, (GLenum t, GLenum p, GLint *v), (t, p, v))

/* FBO con los nombres de GLES 2 que vitaGL acepta también en GLES 1 */
void glGenFramebuffers(GLsizei n, GLuint *ids) { glGenFramebuffersOES(n, ids); }
void glDeleteFramebuffers(GLsizei n, const GLuint *ids) { glDeleteFramebuffersOES(n, ids); }
void glBindFramebuffer(GLenum t, GLuint fb) { glBindFramebufferOES(t, fb); }
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l) { glFramebufferTexture2DOES(t, a, tt, tex, l); }
void glFramebufferRenderbuffer(GLenum t, GLenum a, GLenum rt, GLuint rb) { glFramebufferRenderbufferOES(t, a, rt, rb); }
GLenum glCheckFramebufferStatus(GLenum t) { return glCheckFramebufferStatusOES(t); }
void glGetFramebufferAttachmentParameteriv(GLenum t, GLenum a, GLenum p, GLint *v) { glGetFramebufferAttachmentParameterivOES(t, a, p, v); }
void glGenRenderbuffers(GLsizei n, GLuint *ids) { glGenRenderbuffersOES(n, ids); }
void glDeleteRenderbuffers(GLsizei n, const GLuint *ids) { glDeleteRenderbuffersOES(n, ids); }
void glBindRenderbuffer(GLenum t, GLuint rb) { glBindRenderbufferOES(t, rb); }
void glRenderbufferStorage(GLenum t, GLenum f, GLsizei w, GLsizei h) { glRenderbufferStorageOES(t, f, w, h); }
void glGetRenderbufferParameteriv(GLenum t, GLenum p, GLint *v) { glGetRenderbufferParameterivOES(t, p, v); }

/* Solo GLES 1.1: versiones float y en punto fijo 16.16 sobre las de escritorio */
void glOrtho(double, double, double, double, double, double);
void glFrustum(double, double, double, double, double, double);
void glClipPlane(GLenum, const double *);
#define FX(x) ((GLfloat)(x) / 65536.0f)
void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { glOrtho(l, r, b, t, n, f); }
void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { glFrustum(l, r, b, t, n, f); }
void glClipPlanef(GLenum p, const GLfloat *e)
{
    double d[4] = { e[0], e[1], e[2], e[3] };
    glClipPlane(p, d);
}
void glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    glOrtho(FX(l), FX(r), FX(b), FX(t), FX(n), FX(f));
}
void glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a) { glColor4f(FX(r), FX(g), FX(b), FX(a)); }
void glLoadMatrixx(const GLfixed *m)
{
    GLfloat f[16];
    int i;
    for (i = 0; i < 16; i++) f[i] = FX(m[i]);
    glLoadMatrixf(f);
}
void glRotatex(GLfixed a, GLfixed x, GLfixed y, GLfixed z) { glRotatef(FX(a), FX(x), FX(y), FX(z)); }
void glScalex(GLfixed x, GLfixed y, GLfixed z) { glScalef(FX(x), FX(y), FX(z)); }
void glTranslatex(GLfixed x, GLfixed y, GLfixed z) { glTranslatef(FX(x), FX(y), FX(z)); }
/* los parámetros de modo (GL_MODULATE...) van como enteros, no como 16.16 */
void glTexEnvx(GLenum t, GLenum p, GLfixed v) { glTexEnvi(t, p, v); }
void glTexEnvxv(GLenum t, GLenum p, const GLfixed *v)
{
    GLfloat f[4];
    int i;
    if (p != GL_TEXTURE_ENV_COLOR) { glTexEnvi(t, p, v[0]); return; }
    for (i = 0; i < 4; i++) f[i] = FX(v[i]);
    glTexEnvfv(t, p, f);
}

/* Diagnóstico: formatos comprimidos que pide el juego (PVRTC no existe en Mesa). */
void glCompressedTexImage2D(GLenum target, GLint level, GLenum fmt, GLsizei w, GLsizei h,
                            GLint border, GLsizei size, const void *data)
{
    static void (*real)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
    static int logged;
    GLenum err;
    if (!real) real = (void (*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *))
                     eglGetProcAddress("glCompressedTexImage2D");
    if (fmt >= 0x8B90 && fmt <= 0x8B99) { /* OES_compressed_paletted_texture: Mesa no lo tiene, vitaGL sí */
        static const int esz[5] = {3, 4, 2, 2, 2}; /* RGB8, RGBA8, R5_G6_B5, RGBA4, RGB5_A1 */
        int k = (fmt - 0x8B90) % 5, bits = fmt < 0x8B95 ? 4 : 8, n = 1 << bits, i;
        const uint8_t *pal = data, *idx = pal + n * esz[k];
        uint8_t *out = malloc((size_t)w * h * 4);
        for (i = 0; i < w * h; i++) {
            int c = bits == 8 ? idx[i] : (i & 1 ? idx[i / 2] & 15 : idx[i / 2] >> 4);
            const uint8_t *e = pal + c * esz[k];
            uint16_t v = e[0] | e[1] << 8;
            uint8_t *o = out + i * 4;
            switch (k) {
            case 0: o[0] = e[0], o[1] = e[1], o[2] = e[2], o[3] = 255; break;
            case 1: memcpy(o, e, 4); break;
            case 2: o[0] = (v >> 11) << 3, o[1] = ((v >> 5) & 63) << 2, o[2] = (v & 31) << 3, o[3] = 255; break;
            case 3: o[0] = (v >> 12) << 4, o[1] = ((v >> 8) & 15) << 4, o[2] = ((v >> 4) & 15) << 4, o[3] = (v & 15) << 4; break;
            default: o[0] = (v >> 11) << 3, o[1] = ((v >> 6) & 31) << 3, o[2] = ((v >> 1) & 31) << 3, o[3] = v & 1 ? 255 : 0;
            }
        }
        glTexImage2D(target, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, out); /* solo el nivel 0 */
        free(out);
        return;
    }
    real(target, level, fmt, w, h, border, size, data);
    if ((err = glGetError()) != GL_NO_ERROR && logged++ < 10)
        fprintf(stderr, "[HOSTTEST] glCompressedTexImage2D formato 0x%04X %dx%d: error GL 0x%X\n", fmt, w, h, err);
}

/* BIA renderiza desde su hilo de juego (ThreadRoutine): en la Vita el contexto de vitaGL no está
 * atado a un hilo, en EGL sí. gl_thread.c pasa el contexto del hilo principal al hilo del juego. */
void hosttest_gl_release(void) { eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); }
void hosttest_gl_acquire(void)
{
    if (!eglMakeCurrent(s_dpy, s_surf, s_surf, s_ctx))
        fprintf(stderr, "[HOSTTEST] eglMakeCurrent en el hilo del juego: 0x%X\n", eglGetError());
}

/* GL_FIXED en arrays de vértices: vitaGL lo acepta (lo convierte en el shader), el GL de escritorio de
 * Mesa no (INVALID_ENUM -> nada se dibuja). Se guarda el puntero y se convierte a float al dibujar. */
typedef struct { GLint size; GLenum type; GLsizei stride; const void *ptr; float *buf; int cap; } fx_array;
static fx_array s_fx[4]; /* 0 vértices, 1 texcoord unidad 0, 2 color, 3 texcoord unidad 1 (por unidad, como vitaGL) */
static void (*real_ptr[4])(GLint, GLenum, GLsizei, const void *);
static int s_cunit; /* glClientActiveTexture - GL_TEXTURE0 */
void glClientActiveTexture(GLenum unit)
{
    static void (*real)(GLenum);
    if (!real) real = (void (*)(GLenum))eglGetProcAddress("glClientActiveTexture");
    s_cunit = (unit - GL_TEXTURE0) & 1;
    real(unit);
}

static void fx_set(int k, const char *name, GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    if (!real_ptr[k]) real_ptr[k] = (void (*)(GLint, GLenum, GLsizei, const void *))eglGetProcAddress(name);
    s_fx[k] = (fx_array){size, type, stride, ptr, s_fx[k].buf, s_fx[k].cap};
    if (type != GL_FIXED) real_ptr[k](size, type, stride, ptr);
}
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) { fx_set(0, "glVertexPointer", size, type, stride, ptr); }
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) { fx_set(s_cunit ? 3 : 1, "glTexCoordPointer", size, type, stride, ptr); }
void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) { fx_set(2, "glColorPointer", size, type, stride, ptr); }

static void fx_convert(int count)
{
    GLint vbo = 0;
    int k, i, c;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    for (k = 0; k < 4; k++) {
        fx_array *a = &s_fx[k];
        if (a->type != GL_FIXED || !a->ptr || vbo) continue; /* ponytail: FIXED dentro de un VBO no se convierte */
        if (count * a->size > a->cap) a->buf = realloc(a->buf, (a->cap = count * a->size) * sizeof(float));
        for (i = 0; i < count; i++) {
            const int32_t *src = (const int32_t *)((const uint8_t *)a->ptr + i * (a->stride ? a->stride : a->size * 4));
            for (c = 0; c < a->size; c++) a->buf[i * a->size + c] = src[c] / 65536.0f;
        }
        int cu = s_cunit;
        if (k == 1 || k == 3) glClientActiveTexture(k == 3 ? GL_TEXTURE1 : GL_TEXTURE0);
        real_ptr[k](a->size, GL_FLOAT, 0, a->buf);
        if (k == 1 || k == 3) glClientActiveTexture(GL_TEXTURE0 + cu);
    }
}
/* HOSTTEST_GLTRACE=N: las primeras N llamadas de dibujo con su estado (diagnóstico de pantalla negra) */
static void fx_trace(const char *fn, GLenum mode, GLsizei count)
{
    static int left = -1;
    GLint tex = 0, fb = 0, vbo = 0, vp[4];
    GLenum err = glGetError();
    if (left < 0) left = getenv("HOSTTEST_GLTRACE") ? atoi(getenv("HOSTTEST_GLTRACE")) : 0;
    if (!left) return;
    left--;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_OES, &fb);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_VIEWPORT, vp);
    fprintf(stderr, "[GLTRACE] %s mode=%u n=%d vtx=%d/0x%X tc=0x%X col=0x%X tex=%d fb=%d vbo=%d vp=%d,%d,%d,%d tex2d=%d blend=%d err=0x%X\n",
            fn, mode, count, s_fx[0].size, s_fx[0].type, s_fx[1].type, s_fx[2].type, tex, fb, vbo, vp[0], vp[1], vp[2], vp[3],
            glIsEnabled(GL_TEXTURE_2D), glIsEnabled(GL_BLEND), err);
    if (s_fx[0].type == GL_SHORT && s_fx[0].ptr) {
        const int16_t *v = s_fx[0].ptr;
        const uint8_t *c = s_fx[2].ptr;
        GLfloat pm[16], mv[16];
        glGetFloatv(GL_PROJECTION_MATRIX, pm);
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
        fprintf(stderr, "   v0=%d,%d,%d col0=%d,%d,%d,%d carr=%d P=%.4f,%.4f,%.3f,%.3f MV=%.3f,%.3f,%.3f,%.3f,%.1f,%.1f\n", v[0], v[1], v[2],
                c ? c[0] : -1, c ? c[1] : -1, c ? c[2] : -1, c ? c[3] : -1, glIsEnabled(GL_COLOR_ARRAY),
                pm[0], pm[5], pm[12], pm[13], mv[0], mv[1], mv[4], mv[5], mv[12], mv[13]);
        GLint sc[4], cm[4]; GLboolean cmask[4];
        glGetIntegerv(GL_SCISSOR_BOX, sc);
        glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
        glGetIntegerv(GL_BLEND_SRC, &cm[0]); glGetIntegerv(GL_BLEND_DST, &cm[1]); glGetIntegerv(GL_DEPTH_FUNC, &cm[2]);
        glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &cm[3]);
        GLfloat dr[2]; glGetFloatv(GL_DEPTH_RANGE, dr);
        fprintf(stderr, "   blend=%X/%X depthfunc=%X env=%X depthrange=%.2f,%.2f varr=%d tcarr=%d\n", cm[0], cm[1], cm[2], cm[3], dr[0], dr[1],
                glIsEnabled(GL_VERTEX_ARRAY), glIsEnabled(GL_TEXTURE_COORD_ARRAY));
        fprintf(stderr, "   Pcol3=%.4f,%.4f,%.4f,%.4f MV2=%.3f,%.3f,%.3f MV3=%.3f,%.3f,%.3f,%.3f\n", pm[3], pm[7], pm[11], pm[15], mv[2], mv[6], mv[8], mv[3], mv[7], mv[11], mv[15]);
        fprintf(stderr, "   zclip(v0)=%.7f mv10=%.6f mv14=%.6f\n", pm[2]*(mv[0]*v[0]+mv[4]*v[1]+mv[8]*v[2]+mv[12]) + pm[6]*(mv[1]*v[0]+mv[5]*v[1]+mv[9]*v[2]+mv[13])
                + pm[10]*(mv[2]*v[0]+mv[6]*v[1]+mv[10]*v[2]+mv[14]) + pm[14], mv[10], mv[14]);
        fprintf(stderr, "   P1=%.4f P4=%.4f P10=%.7f P14=%.7f depth=%d scissor=%d(%d,%d,%d,%d) alpha=%d cull=%d fog=%d light=%d cmask=%d%d%d%d\n",
                pm[1], pm[4], pm[10], pm[14], glIsEnabled(GL_DEPTH_TEST), glIsEnabled(GL_SCISSOR_TEST), sc[0], sc[1], sc[2], sc[3],
                glIsEnabled(GL_ALPHA_TEST), glIsEnabled(GL_CULL_FACE), glIsEnabled(GL_FOG), glIsEnabled(GL_LIGHTING),
                cmask[0], cmask[1], cmask[2], cmask[3]);
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    static void (*real)(GLenum, GLint, GLsizei);
    if (!real) real = (void (*)(GLenum, GLint, GLsizei))eglGetProcAddress("glDrawArrays");
    fx_convert(first + count);
    real(mode, first, count);
    fx_trace("glDrawArrays", mode, count);
}
/* Copia CPU de los buffers de índices: con un EBO ligado, fx_convert necesita el índice máximo
 * (sin ella convertía 0 vértices -> texcoords GL_FIXED en NULL -> todo con el texel (0,0)). */
#define EB_MAX 512
static struct { GLuint name; uint8_t *data; GLsizeiptr size; } s_eb[EB_MAX];
static GLuint s_ebo;
static uint8_t **eb_slot(GLuint name, GLsizeiptr **size)
{
    int i, free_i = -1;
    for (i = 0; i < EB_MAX; i++) {
        if (s_eb[i].name == name) { *size = &s_eb[i].size; return &s_eb[i].data; }
        if (!s_eb[i].name && free_i < 0) free_i = i;
    }
    if (free_i < 0) return NULL;
    s_eb[free_i].name = name;
    *size = &s_eb[free_i].size;
    return &s_eb[free_i].data;
}
void glBindBuffer(GLenum target, GLuint b)
{
    static void (*real)(GLenum, GLuint);
    if (!real) real = (void (*)(GLenum, GLuint))eglGetProcAddress("glBindBuffer");
    if (target == GL_ELEMENT_ARRAY_BUFFER) s_ebo = b;
    real(target, b);
}
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    static void (*real)(GLenum, GLsizeiptr, const void *, GLenum);
    GLsizeiptr *sz;
    uint8_t **d;
    if (!real) real = (void (*)(GLenum, GLsizeiptr, const void *, GLenum))eglGetProcAddress("glBufferData");
    if (target == GL_ELEMENT_ARRAY_BUFFER && s_ebo && (d = eb_slot(s_ebo, &sz))) {
        *d = realloc(*d, size);
        *sz = size;
        if (data) memcpy(*d, data, size); else memset(*d, 0, size);
    }
    real(target, size, data, usage);
}
void glBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, const void *data)
{
    static void (*real)(GLenum, GLintptr, GLsizeiptr, const void *);
    GLsizeiptr *sz;
    uint8_t **d;
    if (!real) real = (void (*)(GLenum, GLintptr, GLsizeiptr, const void *))eglGetProcAddress("glBufferSubData");
    if (target == GL_ELEMENT_ARRAY_BUFFER && s_ebo && (d = eb_slot(s_ebo, &sz)) && *d && off + size <= *sz)
        memcpy(*d + off, data, size);
    real(target, off, size, data);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *idx)
{
    static void (*real)(GLenum, GLsizei, GLenum, const void *);
    GLint ebo = 0;
    int n = 0, i;
    const void *ix = idx;
    if (!real) real = (void (*)(GLenum, GLsizei, GLenum, const void *))eglGetProcAddress("glDrawElements");
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
    if (ebo) {
        GLsizeiptr *sz;
        uint8_t **d = eb_slot(ebo, &sz);
        ix = d && *d && (GLsizeiptr)(uintptr_t)idx < *sz ? *d + (uintptr_t)idx : NULL;
    }
    if (ix)
        for (i = 0; i < count; i++) {
            int v = type == GL_UNSIGNED_BYTE ? ((const uint8_t *)ix)[i] : ((const uint16_t *)ix)[i];
            if (v >= n) n = v + 1;
        }
    fx_convert(n);
    real(mode, count, type, idx);
    fx_trace("glDrawElements", mode, count);
}

