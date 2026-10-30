/**
 * @file gles.c
 * @brief OpenGL ES 1.1 pipeline translation over vitaGL.
 * @details The game renders into a virtual 320x480 portrait renderbuffer created on iOS by EAGLView
 *          (with internal projection rotation to landscape). Here that renderbuffer is an FBO with a texture
 *          of the same size: viewport, scissor, and glCopyTexImage2D remain in iOS coordinate space untouched.
 *          During presentation (gles_present), the FBO is drawn rotated and scaled onto the native 960x544 screen.
 *
 *          All imported gl* functions take only integer/pointer arguments (*x variants): no SoftFP issues.
 *          GL_FIXED vertex arrays are converted to float immediately before draw calls.
 */
#include <stdlib.h>
#include <string.h>
#include <vitaGL.h>

#include "gles.h"
#include "gles_oes.h"
#include "display_geometry.h"
#include "hle.h"
#include "imports.h"
#include "objc.h"
#include "log.h"

#define FB_W IM2_FB_WIDTH
#define FB_H IM2_FB_HEIGHT
#define SCR_W IM2_SCREEN_WIDTH
#define SCR_H IM2_SCREEN_HEIGHT
// Destination rectangle: 480x320 scaled to Vita screen height (1.7x) and centered horizontally.
#define DST_H IM2_PRESENT_HEIGHT
#define DST_W IM2_PRESENT_WIDTH
#define DST_X IM2_PRESENT_X

#define GL_FRAMEBUFFER_COMPLETE_OES 0x8CD5
#define GL_RENDERBUFFER_WIDTH_OES 0x8D42
#define GL_RENDERBUFFER_HEIGHT_OES 0x8D43

static GLuint fbo, fbo_tex;
static int fbo_ready;
static Im2Oes *oes;
static GLenum oes_error;
static void gles_error(GLenum e){if(e && !oes_error)oes_error=e;}
static GLenum g_glGetError(void){GLenum e=oes_error;if(e){oes_error=0;return e;}return glGetError();}

// Cached state overwritten during blit that glGet cannot fully query:
typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    const void *ptr;
} array_ptr;
static array_ptr vtx, tc[2];
static int client_unit; // glClientActiveTexture - GL_TEXTURE0
static GLint env_mode = GL_MODULATE;
static GLfixed color[4] = {0x10000, 0x10000, 0x10000, 0x10000};

// ---------------------------------------------------------------- GL_FIXED arrays

static float *conv_buf[3];
static int conv_cap[3];

static const void *fixed_to_float(const array_ptr *a, int n, int slot) {
    int need = n * a->size;
    if (need > conv_cap[slot]) {
        conv_buf[slot] = realloc(conv_buf[slot], need * sizeof(float));
        conv_cap[slot] = need;
    }
    int step = a->stride ? a->stride / 4 : a->size;
    const int32_t *src = a->ptr;
    float *dst = conv_buf[slot];
    for (int i = 0; i < n; i++)
        for (int c = 0; c < a->size; c++)
            dst[i * a->size + c] = src[i * step + c] * (1.0f / 65536.0f);
    return dst;
}

static void apply_vertex(int n) {
    if (!vtx.size || (vtx.type == GL_FIXED && !n)) // GL_FIXED arrays are converted at draw time
        return;
    if (vtx.type == GL_FIXED)
        glVertexPointer(vtx.size, GL_FLOAT, 0, fixed_to_float(&vtx, n, 0));
    else
        glVertexPointer(vtx.size, vtx.type, vtx.stride, vtx.ptr);
}

static void apply_texcoord(int unit, int n) {
    array_ptr *a = &tc[unit];
    if (!a->size || (a->type == GL_FIXED && !n))
        return;
    glClientActiveTexture(GL_TEXTURE0 + unit);
    if (a->type == GL_FIXED)
        glTexCoordPointer(a->size, GL_FLOAT, 0, fixed_to_float(a, n, 1 + unit));
    else
        glTexCoordPointer(a->size, a->type, a->stride, a->ptr);
    glClientActiveTexture(GL_TEXTURE0 + client_unit);
}

static void g_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) {
    vtx = (array_ptr){size, type, stride, ptr};
    apply_vertex(0);
}

static void g_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) {
    tc[client_unit] = (array_ptr){size, type, stride, ptr};
    apply_texcoord(client_unit, 0);
}

static void g_glClientActiveTexture(GLenum unit) {
    client_unit = (unit - GL_TEXTURE0) & 1;
    glClientActiveTexture(unit);
}

static uint32_t gles_draw_calls;

uint32_t gles_get_draw_calls(void) {
    return gles_draw_calls;
}

static void g_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    gles_draw_calls++;
    int n = first + count;
    if (vtx.type == GL_FIXED)
        apply_vertex(n);
    for (int u = 0; u < 2; u++)
        if (tc[u].type == GL_FIXED)
            apply_texcoord(u, n);
    glDrawArrays(mode, first, count);
}

static void g_glGetPointerv(GLenum pname, void **out) {
    if (pname == GL_VERTEX_ARRAY_POINTER)
        *out = (void *)vtx.ptr;
    else if (pname == GL_TEXTURE_COORD_ARRAY_POINTER)
        *out = (void *)tc[client_unit].ptr;
    else
        glGetPointerv(pname, out);
}

// ---------------------------------------------------------------- *x state

static void g_glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a) {
    color[0] = r, color[1] = g, color[2] = b, color[3] = a;
    glColor4x(r, g, b, a);
}

static void g_glTexEnvi(GLenum target, GLenum pname, GLint v) {
    if (pname == GL_TEXTURE_ENV_MODE)
        env_mode = v;
    glTexEnvi(target, pname, v);
}

static void g_glTexEnvxv(GLenum target, GLenum pname, const GLfixed *v) {
    if (pname == GL_TEXTURE_ENV_MODE) // enum value, not fixed point
        return g_glTexEnvi(target, pname, v[0]);
    glTexEnvxv(target, pname, (GLfixed *)v);
}

// Texture parameters used by the engine are enums (filters, wrap modes): pass unscaled.
static void g_glTexParameterx(GLenum target, GLenum pname, GLfixed v) { glTexParameteri(target, pname, v); }

static void g_glGetFixedv(GLenum pname, GLfixed *out) {
    float f[16];
    int n = 1;
    if (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX || pname == GL_TEXTURE_MATRIX)
        n = 16;
    else if (pname == GL_CURRENT_COLOR) {
        memcpy(out, color, sizeof(color));
        return;
    }
    glGetFloatv(pname, f);
    for (int i = 0; i < n; i++)
        out[i] = (GLfixed)(f[i] * 65536.0f);
}

static void g_glGetLightxv(GLenum light, GLenum pname, GLfixed *out) { memset(out, 0, 4 * sizeof(GLfixed)); }

// ---------------------------------------------------------------- EAGLView framebuffer emulation

static uint32_t native_new_fb(void *ctx){(void)ctx;GLuint id=0;glGenFramebuffers(1,&id);return id;}
static void native_delete_fb(void *ctx,uint32_t id){(void)ctx;glDeleteFramebuffers(1,&id);}
static void native_bind_fb(void *ctx,uint32_t id){(void)ctx;glBindFramebuffer(GL_FRAMEBUFFER,id);}
static uint32_t native_storage(void *ctx,uint32_t format,int width,int height){
    (void)ctx;GLuint id=0;GLint previous=0;
    gles_error(glGetError());
    if(format!=IM2_OES_DEPTH16){
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
        glGenTextures(1,&id);if(!id){gles_error(glGetError());return 0;}glBindTexture(GL_TEXTURE_2D,id);
        GLenum pixel_format=format==IM2_OES_RGB565?GL_RGB:GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D,0,pixel_format,width,height,0,pixel_format,format==IM2_OES_RGB565?GL_UNSIGNED_SHORT_5_6_5:GL_UNSIGNED_BYTE,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D,(GLuint)previous);
    }else{
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&previous);
        glGenRenderbuffers(1,&id);if(!id){gles_error(glGetError());return 0;}glBindRenderbuffer(GL_RENDERBUFFER,id);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,width,height);
        glBindRenderbuffer(GL_RENDERBUFFER,(GLuint)previous);
    }
    GLenum error=glGetError();if(error){gles_error(error);if(id){if(format!=IM2_OES_DEPTH16)glDeleteTextures(1,&id);else glDeleteRenderbuffers(1,&id);}return 0;}
    return id;
}
static void native_delete_storage(void *ctx,uint32_t format,uint32_t id){(void)ctx;if(format!=IM2_OES_DEPTH16)glDeleteTextures(1,&id);else glDeleteRenderbuffers(1,&id);}
static int native_attach(void *ctx,uint32_t fb,uint32_t attachment,uint32_t format,uint32_t id){
    (void)ctx;gles_error(glGetError());glBindFramebuffer(GL_FRAMEBUFFER,fb);
    if(format!=IM2_OES_DEPTH16)glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,id,0);
    else glFramebufferRenderbuffer(GL_FRAMEBUFFER,attachment,GL_RENDERBUFFER,id);
    GLenum error=glGetError();gles_error(error);return !error;
}
static uint32_t native_check(void *ctx,uint32_t fb){(void)ctx;glBindFramebuffer(GL_FRAMEBUFFER,fb);return glCheckFramebufferStatus(GL_FRAMEBUFFER);}
static void g_glGenFramebuffersOES(GLsizei n,GLuint *ids){gles_error(im2_oes_gen(oes,IM2_OES_FB,n,ids));}
static void g_glGenRenderbuffersOES(GLsizei n,GLuint *ids){gles_error(im2_oes_gen(oes,IM2_OES_RB,n,ids));}
static void g_glBindFramebufferOES(GLenum target,GLuint id){gles_error(target==IM2_OES_FB?im2_oes_bind(oes,target,id):IM2_OES_ENUM);}
static void g_glBindRenderbufferOES(GLenum target,GLuint id){gles_error(target==IM2_OES_RB?im2_oes_bind(oes,target,id):IM2_OES_ENUM);}
static void g_glDeleteFramebuffersOES(GLsizei n,const GLuint *ids){gles_error(im2_oes_delete(oes,IM2_OES_FB,n,ids));}
static void g_glDeleteRenderbuffersOES(GLsizei n,const GLuint *ids){gles_error(im2_oes_delete(oes,IM2_OES_RB,n,ids));}
static void g_glRenderbufferStorageOES(GLenum target,GLenum format,GLsizei w,GLsizei h){gles_error(target==IM2_OES_RB?im2_oes_storage(oes,format,w,h):IM2_OES_ENUM);}
static void g_glFramebufferRenderbufferOES(GLenum target,GLenum attachment,GLenum rbtarget,GLuint id){gles_error(target==IM2_OES_FB && rbtarget==IM2_OES_RB?im2_oes_attach(oes,attachment,id):IM2_OES_ENUM);}
static GLenum g_glCheckFramebufferStatusOES(GLenum target){if(target!=IM2_OES_FB){gles_error(IM2_OES_ENUM);return 0;}return im2_oes_status(oes);}
static void g_glGetRenderbufferParameterivOES(GLenum target,GLenum pname,GLint *value){gles_error(target==IM2_OES_RB?im2_oes_parameter(oes,pname,value):IM2_OES_ENUM);}
static void g_glGetIntegerv(GLenum pname,GLint *value){
    if(!value){gles_error(IM2_OES_VALUE);return;}
    if(pname==0x8CA6u)*value=(GLint)im2_oes_binding(oes,IM2_OES_FB);
    else if(pname==0x8CA7u)*value=(GLint)im2_oes_binding(oes,IM2_OES_RB);
    else glGetIntegerv(pname,value);
}

// ---------------------------------------------------------------- init / present

void gles_init(void) {
    vglUseTripleBuffering(GL_FALSE);
    fbo_ready=vglInitExtended(0,SCR_W,SCR_H,8*1024*1024,SCE_GXM_MULTISAMPLE_NONE);
    if(!fbo_ready){l_error("vitaGL initialization failed");return;}
    oes=im2_oes_create((Im2OesBackend){NULL,native_new_fb,native_delete_fb,native_bind_fb,
        native_storage,native_delete_storage,native_attach,native_check});
    if(!oes){fbo_ready=0;l_error("OES framebuffer registry allocation failed");return;}
    glViewport(0,0,FB_W,FB_H);
    l_info("vitaGL initialized; guest must allocate and attach its OES drawable before presentation");
}

int gles_eagl_ready(void){return fbo_ready && oes;}
int gles_eagl_storage(uint32_t target,uint32_t format){
    return target==IM2_OES_RB && gles_eagl_ready() &&
        !im2_oes_storage(oes,format,FB_W,FB_H);
}
int gles_eagl_present(uint32_t target){
    if(target!=IM2_OES_RB || !gles_eagl_ready() || !im2_oes_drawable(oes,&fbo,&fbo_tex))return 0;
    gles_present();return 1;
}

void gles_present(void) {
    static const GLenum caps[] = {GL_BLEND, GL_ALPHA_TEST, GL_SCISSOR_TEST, GL_DEPTH_TEST, GL_TEXTURE_2D};
    GLboolean on[5], tca, va, ca, na;
    GLint tex, mode, active, vp[4];
    for (int i = 0; i < 5; i++)
        on[i] = glIsEnabled(caps[i]);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
    tca = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
    va = glIsEnabled(GL_VERTEX_ARRAY);
    ca = glIsEnabled(GL_COLOR_ARRAY);
    na = glIsEnabled(GL_NORMAL_ARRAY);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    glGetIntegerv(GL_VIEWPORT, vp);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCR_W, SCR_H);
    for (int i = 0; i < 4; i++)
        glDisable(caps[i]);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrthof(0, SCR_W, SCR_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);
    // Screen corners (top-left, top-right, bottom-left, bottom-right) -> vertical FBO texture.
    // Assumed landscape-right rotation (top edge of FBO aligns to left edge of screen).
    // Adjust UV mapping table if target game renders mirrored or inverted.
    static const float uv[] = {1, 1, 1, 0, 0, 1, 0, 0};
    const float xy[] = {DST_X, 0, DST_X + DST_W, 0, DST_X, SCR_H, DST_X + DST_W, SCR_H};
    glVertexPointer(2, GL_FLOAT, 0, xy);
    glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    vglSwapBuffers(ime_active()); // GL_TRUE while system dialog is active

    // Restore guest OpenGL state.
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(mode);
    im2_oes_bind(oes,IM2_OES_FB,im2_oes_binding(oes,IM2_OES_FB));
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env_mode);
    for (int i = 0; i < 5; i++)
        on[i] ? glEnable(caps[i]) : glDisable(caps[i]);
    if (!va)
        glDisableClientState(GL_VERTEX_ARRAY);
    if (!tca)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (ca)
        glEnableClientState(GL_COLOR_ARRAY);
    if (na)
        glEnableClientState(GL_NORMAL_ARRAY);
    glColor4x(color[0], color[1], color[2], color[3]);
    apply_vertex(0);
    apply_texcoord(0, 0);
    glClientActiveTexture(GL_TEXTURE0 + client_unit);
    glActiveTexture(active);
}

// ---------------------------------------------------------------- import table

#undef F
#define F(name, fn) {"_" #name, (uintptr_t)(fn), 0}
#undef D
#define D(name) {"_" #name, (uintptr_t)(name), 0}

const import_entry gles_imports[] = {
    // Integer/pointer-only GLES1 calls share the ARM register ABI.
    // Floating-point calls require explicit iOS SoftFP bridges; do not map directly.
    D(glColorMask), D(glCullFace), D(glDepthFunc), D(glFrontFace), D(glPixelStorei),
    D(glActiveTexture), D(glBindTexture), D(glBlendFunc), D(glClear), D(glCopyTexImage2D),
    D(glCopyTexSubImage2D), D(glDeleteTextures), D(glDepthMask), D(glDisable), D(glDisableClientState),
    D(glEnable), D(glEnableClientState), D(glGenTextures), F(glGetIntegerv,g_glGetIntegerv), F(glGetError,g_glGetError), D(glIsEnabled),
    D(glLoadIdentity), D(glLoadMatrixx), D(glMatrixMode), D(glOrthox), D(glPopMatrix), D(glPushMatrix),
    D(glRotatex), D(glScalex), D(glScissor), D(glShadeModel), D(glTexImage2D), D(glTexParameteri),
    D(glTexSubImage2D), D(glTranslatex), D(glViewport),
    F(glColor4x, g_glColor4x), F(glTexEnvi, g_glTexEnvi), F(glTexEnvxv, g_glTexEnvxv),
    F(glTexParameterx, g_glTexParameterx), F(glGetFixedv, g_glGetFixedv), F(glGetLightxv, g_glGetLightxv),
    F(glVertexPointer, g_glVertexPointer), F(glTexCoordPointer, g_glTexCoordPointer),
    F(glClientActiveTexture, g_glClientActiveTexture), F(glDrawArrays, g_glDrawArrays),
    F(glGetPointerv, g_glGetPointerv),
    F(glGenFramebuffersOES, g_glGenFramebuffersOES), F(glGenRenderbuffersOES, g_glGenRenderbuffersOES),
    F(glBindFramebufferOES, g_glBindFramebufferOES), F(glBindRenderbufferOES, g_glBindRenderbufferOES),
    F(glDeleteFramebuffersOES, g_glDeleteFramebuffersOES), F(glDeleteRenderbuffersOES, g_glDeleteRenderbuffersOES),
    F(glRenderbufferStorageOES, g_glRenderbufferStorageOES), F(glFramebufferRenderbufferOES, g_glFramebufferRenderbufferOES),
    F(glCheckFramebufferStatusOES, g_glCheckFramebufferStatusOES),
    F(glGetRenderbufferParameterivOES, g_glGetRenderbufferParameterivOES),
};
const int gles_imports_count = sizeof(gles_imports) / sizeof(gles_imports[0]);
