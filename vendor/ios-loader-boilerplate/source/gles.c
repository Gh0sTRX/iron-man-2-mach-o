/**
 * @file gles.c
 * @brief OpenGL ES 1.1 pipeline translation over vitaGL.
 * @details The game renders into a virtual 320x480 portrait renderbuffer created on iOS by EAGLView
 *          (with internal projection rotation to landscape). Here that renderbuffer is an FBO with a texture
 *          of the same size: viewport, scissor, and glCopyTexImage2D remain in iOS coordinate space untouched.
 *          During presentation (gles_present), the FBO is drawn rotated and scaled onto the native 960x544 screen.
 *
 *          Integer/pointer imports share the base ABI; float state calls are in gles_float.c.
 *          GL_FIXED vertex arrays are converted to float immediately before draw calls.
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vitaGL.h>

#include "gles.h"
#include "gles_oes.h"
#include "gles_arrays.h"
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
void gles_record_error(uint32_t e){if(e && !oes_error)oes_error=e;}
static GLenum g_glGetError(void){GLenum e=oes_error;if(e){oes_error=0;return e;}return glGetError();}

static Im2Arrays *arrays;
static GLint env_mode[2] = {GL_MODULATE,GL_MODULATE};
static unsigned server_unit;
static GLfixed color[4] = {0x10000,0x10000,0x10000,0x10000};
static uint32_t gles_draw_calls;
uint32_t gles_get_draw_calls(void){return gles_draw_calls;}
static void array_pointer(void *ctx,unsigned slot,int size,uint32_t type,int stride,const void *p){
    (void)ctx;
    if(slot==IM2_ARRAY_VERTEX)glVertexPointer(size,type,stride,p);
    else if(slot==IM2_ARRAY_COLOR)glColorPointer(size,type,stride,p);
    else if(slot==IM2_ARRAY_NORMAL)glNormalPointer(type,stride,p);
    else glTexCoordPointer(size,type,stride,p);
}
static void array_enable(void *ctx,unsigned slot,int on){
    (void)ctx;GLenum cap=slot==IM2_ARRAY_VERTEX?GL_VERTEX_ARRAY:slot==IM2_ARRAY_COLOR?GL_COLOR_ARRAY:slot==IM2_ARRAY_NORMAL?GL_NORMAL_ARRAY:GL_TEXTURE_COORD_ARRAY;
    if(on)glEnableClientState(cap);else glDisableClientState(cap);
}
static void array_unit(void *ctx,uint32_t unit){(void)ctx;glClientActiveTexture(unit);}
static void array_draw(void *ctx,uint32_t mode,int first,int count){(void)ctx;gles_draw_calls++;glDrawArrays(mode,first,count);}
static void array_elements(void *ctx,uint32_t mode,int count,uint32_t type,const void *p){(void)ctx;gles_draw_calls++;glDrawElements(mode,count,type,p);}
int gles_client_arrays_init(void){
    if(arrays)return 0;
    arrays=im2_arrays_create((Im2ArrayBackend){NULL,array_pointer,array_enable,array_unit,array_draw,array_elements});
    return arrays?0:-1;
}
void gles_client_arrays_restore(void){im2_arrays_restore(arrays);}
static void g_glVertexPointer(GLint n,GLenum t,GLsizei stride,const void *p){gles_record_error(im2_arrays_set(arrays,IM2_ARRAY_VERTEX,n,t,stride,p));}
static void g_glColorPointer(GLint n,GLenum t,GLsizei stride,const void *p){gles_record_error(im2_arrays_set(arrays,IM2_ARRAY_COLOR,n,t,stride,p));}
static void g_glNormalPointer(GLenum t,GLsizei stride,const void *p){gles_record_error(im2_arrays_set(arrays,IM2_ARRAY_NORMAL,3,t,stride,p));}
static void g_glTexCoordPointer(GLint n,GLenum t,GLsizei stride,const void *p){gles_record_error(im2_arrays_set(arrays,IM2_ARRAY_TEX0+im2_arrays_unit(arrays)-GL_TEXTURE0,n,t,stride,p));}
static void g_glClientActiveTexture(GLenum unit){gles_record_error(im2_arrays_client_unit(arrays,unit));}
static void g_glEnableClientState(GLenum cap){gles_record_error(im2_arrays_enable(arrays,cap,1));}
static void g_glDisableClientState(GLenum cap){gles_record_error(im2_arrays_enable(arrays,cap,0));}
static void g_glDrawArrays(GLenum mode,GLint first,GLsizei count){gles_record_error(im2_arrays_draw(arrays,mode,first,count));}
static void g_glDrawElements(GLenum mode,GLsizei count,GLenum type,const void *p){gles_record_error(im2_arrays_elements(arrays,mode,count,type,p));}
static void g_glGetPointerv(GLenum pname,void **p){gles_record_error(im2_arrays_get_pointer(arrays,pname,p));}

// ---------------------------------------------------------------- *x state

static void g_glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a) {
    color[0] = r, color[1] = g, color[2] = b, color[3] = a;
    glColor4x(r, g, b, a);
}

static void g_glActiveTexture(GLenum unit){
    if(unit<GL_TEXTURE0 || unit>GL_TEXTURE1){gles_record_error(GL_INVALID_ENUM);return;}
    server_unit=unit-GL_TEXTURE0;glActiveTexture(unit);
}
static int tex_target(GLenum target){if(target==GL_TEXTURE_ENV)return 1;gles_record_error(GL_INVALID_ENUM);return 0;}
static int tex_scale(GLenum pname){return pname==GL_RGB_SCALE || pname==GL_ALPHA_SCALE;}
static void g_glTexEnvi(GLenum target,GLenum pname,GLint value){
    if(!tex_target(target))return;
    if(pname==GL_TEXTURE_ENV_COLOR){gles_record_error(GL_INVALID_ENUM);return;}
    if(tex_scale(pname) && value!=1 && value!=2 && value!=4){gles_record_error(GL_INVALID_VALUE);return;}
    if(pname==GL_TEXTURE_ENV_MODE){
        if(value!=GL_MODULATE && value!=GL_REPLACE && value!=GL_DECAL && value!=GL_BLEND && value!=GL_ADD && value!=GL_COMBINE){gles_record_error(GL_INVALID_ENUM);return;}
        env_mode[server_unit]=value;
    }
    glTexEnvi(target,pname,value);
}
static void tex_float(GLenum target,GLenum pname,GLfloat value){
    if(!tex_target(target))return;
    if(pname==GL_TEXTURE_ENV_COLOR){gles_record_error(GL_INVALID_ENUM);return;}
    if(tex_scale(pname)){
        if(value!=1.f && value!=2.f && value!=4.f){gles_record_error(GL_INVALID_VALUE);return;}
        glTexEnvfv(target,pname,&value);return;
    }
    if(!isfinite(value) || value< -2147483648.f || value>=2147483648.f || (float)(GLint)value!=value){gles_record_error(GL_INVALID_ENUM);return;}
    g_glTexEnvi(target,pname,(GLint)value);
}
SOFTFP static void g_glTexEnvf(GLenum target,GLenum pname,GLfloat value){tex_float(target,pname,value);}
static void g_glTexEnvx(GLenum target,GLenum pname,GLfixed value){
    if(tex_scale(pname))tex_float(target,pname,(float)value*(1.f/65536.f));
    else g_glTexEnvi(target,pname,value);
}
static void g_glTexEnvfv(GLenum target,GLenum pname,const GLfloat *values){
    if(!tex_target(target))return;
    if(!values){gles_record_error(GL_INVALID_VALUE);return;}
    if(pname==GL_TEXTURE_ENV_COLOR){
        GLfloat v[4];for(int i=0;i<4;i++)v[i]=values[i]<0.f?0.f:values[i]>1.f?1.f:values[i];
        glTexEnvfv(target,pname,v);
    }else tex_float(target,pname,values[0]);
}
static void g_glTexEnvxv(GLenum target,GLenum pname,const GLfixed *values){
    if(!tex_target(target))return;
    if(!values){gles_record_error(GL_INVALID_VALUE);return;}
    if(pname==GL_TEXTURE_ENV_COLOR){
        GLfloat v[4];for(int i=0;i<4;i++)v[i]=(float)values[i]*(1.f/65536.f);
        g_glTexEnvfv(target,pname,v);
    }else g_glTexEnvx(target,pname,values[0]);
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


// ---------------------------------------------------------------- EAGLView framebuffer emulation

static uint32_t native_new_fb(void *ctx){(void)ctx;GLuint id=0;glGenFramebuffers(1,&id);return id;}
static void native_delete_fb(void *ctx,uint32_t id){(void)ctx;glDeleteFramebuffers(1,&id);}
static void native_bind_fb(void *ctx,uint32_t id){(void)ctx;glBindFramebuffer(GL_FRAMEBUFFER,id);}
static uint32_t native_storage(void *ctx,uint32_t format,int width,int height){
    (void)ctx;GLuint id=0;GLint previous=0;
    gles_record_error(glGetError());
    if(format!=IM2_OES_DEPTH16){
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
        glGenTextures(1,&id);if(!id){gles_record_error(glGetError());return 0;}glBindTexture(GL_TEXTURE_2D,id);
        GLenum pixel_format=format==IM2_OES_RGB565?GL_RGB:GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D,0,pixel_format,width,height,0,pixel_format,format==IM2_OES_RGB565?GL_UNSIGNED_SHORT_5_6_5:GL_UNSIGNED_BYTE,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D,(GLuint)previous);
    }else{
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&previous);
        glGenRenderbuffers(1,&id);if(!id){gles_record_error(glGetError());return 0;}glBindRenderbuffer(GL_RENDERBUFFER,id);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,width,height);
        glBindRenderbuffer(GL_RENDERBUFFER,(GLuint)previous);
    }
    GLenum error=glGetError();if(error){gles_record_error(error);if(id){if(format!=IM2_OES_DEPTH16)glDeleteTextures(1,&id);else glDeleteRenderbuffers(1,&id);}return 0;}
    return id;
}
static void native_delete_storage(void *ctx,uint32_t format,uint32_t id){(void)ctx;if(format!=IM2_OES_DEPTH16)glDeleteTextures(1,&id);else glDeleteRenderbuffers(1,&id);}
static int native_attach(void *ctx,uint32_t fb,uint32_t attachment,uint32_t format,uint32_t id){
    (void)ctx;gles_record_error(glGetError());glBindFramebuffer(GL_FRAMEBUFFER,fb);
    if(format!=IM2_OES_DEPTH16)glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,id,0);
    else glFramebufferRenderbuffer(GL_FRAMEBUFFER,attachment,GL_RENDERBUFFER,id);
    GLenum error=glGetError();gles_record_error(error);return !error;
}
static uint32_t native_check(void *ctx,uint32_t fb){(void)ctx;glBindFramebuffer(GL_FRAMEBUFFER,fb);return glCheckFramebufferStatus(GL_FRAMEBUFFER);}
static void g_glGenFramebuffersOES(GLsizei n,GLuint *ids){gles_record_error(im2_oes_gen(oes,IM2_OES_FB,n,ids));}
static void g_glGenRenderbuffersOES(GLsizei n,GLuint *ids){gles_record_error(im2_oes_gen(oes,IM2_OES_RB,n,ids));}
static void g_glBindFramebufferOES(GLenum target,GLuint id){gles_record_error(target==IM2_OES_FB?im2_oes_bind(oes,target,id):IM2_OES_ENUM);}
static void g_glBindRenderbufferOES(GLenum target,GLuint id){gles_record_error(target==IM2_OES_RB?im2_oes_bind(oes,target,id):IM2_OES_ENUM);}
static void g_glDeleteFramebuffersOES(GLsizei n,const GLuint *ids){gles_record_error(im2_oes_delete(oes,IM2_OES_FB,n,ids));}
static void g_glDeleteRenderbuffersOES(GLsizei n,const GLuint *ids){gles_record_error(im2_oes_delete(oes,IM2_OES_RB,n,ids));}
static void g_glRenderbufferStorageOES(GLenum target,GLenum format,GLsizei w,GLsizei h){gles_record_error(target==IM2_OES_RB?im2_oes_storage(oes,format,w,h):IM2_OES_ENUM);}
static void g_glFramebufferRenderbufferOES(GLenum target,GLenum attachment,GLenum rbtarget,GLuint id){gles_record_error(target==IM2_OES_FB && rbtarget==IM2_OES_RB?im2_oes_attach(oes,attachment,id):IM2_OES_ENUM);}
static GLenum g_glCheckFramebufferStatusOES(GLenum target){if(target!=IM2_OES_FB){gles_record_error(IM2_OES_ENUM);return 0;}return im2_oes_status(oes);}
static void g_glGetRenderbufferParameterivOES(GLenum target,GLenum pname,GLint *value){gles_record_error(target==IM2_OES_RB?im2_oes_parameter(oes,pname,value):IM2_OES_ENUM);}
static int virtual_integer(GLenum pname,GLint *value){
    if(pname==GL_MAX_TEXTURE_UNITS)*value=2; /* The compatibility layer tracks two units. */
    else if(pname==0x8CA6u)*value=(GLint)im2_oes_binding(oes,IM2_OES_FB);
    else if(pname==0x8CA7u)*value=(GLint)im2_oes_binding(oes,IM2_OES_RB);
    else return im2_arrays_parameter(arrays,pname,value);
    return 1;
}
static void g_glGetIntegerv(GLenum pname,GLint *value){
    if(!value){gles_record_error(IM2_OES_VALUE);return;}
    if(!virtual_integer(pname,value))glGetIntegerv(pname,value);
}
static void g_glGetFloatv(GLenum pname,GLfloat *value){
    if(!value){gles_record_error(GL_INVALID_VALUE);return;}
    GLint v;if(virtual_integer(pname,&v))*value=(GLfloat)v;else glGetFloatv(pname,value);
}
static void g_glGetBooleanv(GLenum pname,GLboolean *value){
    if(!value){gles_record_error(GL_INVALID_VALUE);return;}
    GLint v;if(virtual_integer(pname,&v))*value=(GLboolean)(v!=0);else glGetBooleanv(pname,value);
}

// ---------------------------------------------------------------- init / present

void gles_init(void) {
    vglUseTripleBuffering(GL_FALSE);
    fbo_ready=vglInitExtended(0,SCR_W,SCR_H,8*1024*1024,SCE_GXM_MULTISAMPLE_NONE);
    if(!fbo_ready){l_error("vitaGL initialization failed");return;}
    if(gles_client_arrays_init()){fbo_ready=0;l_error("Client array registry allocation failed");return;}
    oes=im2_oes_create((Im2OesBackend){NULL,native_new_fb,native_delete_fb,native_bind_fb,
        native_storage,native_delete_storage,native_attach,native_check});
    if(!oes){im2_arrays_destroy(arrays);arrays=NULL;fbo_ready=0;l_error("OES framebuffer registry allocation failed");return;}
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
    static const GLenum caps[] = {GL_BLEND,GL_ALPHA_TEST,GL_SCISSOR_TEST,GL_DEPTH_TEST,GL_FOG,GL_LIGHTING,GL_CULL_FACE,GL_STENCIL_TEST};
    GLboolean on[8],texture_on[2],mask[4];GLfloat clear[4];
    GLint tex, mode, active, vp[4];
    for(int i=0;i<8;i++)on[i]=glIsEnabled(caps[i]);
    glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE1);texture_on[1]=glIsEnabled(GL_TEXTURE_2D);glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);texture_on[0]=glIsEnabled(GL_TEXTURE_2D);
    glClientActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    glGetIntegerv(GL_VIEWPORT, vp);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCR_W, SCR_H);
    for(int i=0;i<8;i++)glDisable(caps[i]);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0.f,0.f,0.f,1.f);
    glEnable(GL_TEXTURE_2D);
    glClientActiveTexture(GL_TEXTURE1);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
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
    glMatrixMode(GL_TEXTURE);glPopMatrix();
    glMatrixMode(mode);
    im2_oes_bind(oes,IM2_OES_FB,im2_oes_binding(oes,IM2_OES_FB));
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env_mode[0]);
    for(int i=0;i<8;i++){if(on[i])glEnable(caps[i]);else glDisable(caps[i]);}
    if(texture_on[0])glEnable(GL_TEXTURE_2D);else glDisable(GL_TEXTURE_2D);
    glColorMask(mask[0],mask[1],mask[2],mask[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
    glColor4x(color[0],color[1],color[2],color[3]);
    gles_client_arrays_restore();
    glActiveTexture(GL_TEXTURE1);if(texture_on[1])glEnable(GL_TEXTURE_2D);else glDisable(GL_TEXTURE_2D);
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
    F(glActiveTexture,g_glActiveTexture), D(glBindTexture), D(glBlendFunc), D(glClear), D(glCopyTexImage2D),
    D(glCopyTexSubImage2D), D(glDeleteTextures), D(glDepthMask), D(glDisable), F(glDisableClientState,g_glDisableClientState),
    D(glEnable), F(glEnableClientState,g_glEnableClientState), D(glGenTextures), F(glGetIntegerv,g_glGetIntegerv), F(glGetError,g_glGetError), D(glIsEnabled),
    D(glLoadIdentity), D(glLoadMatrixx), D(glMatrixMode), D(glOrthox), D(glPopMatrix), D(glPushMatrix),
    D(glRotatex), D(glScalex), D(glScissor), D(glShadeModel), D(glTexImage2D), D(glTexParameteri),
    D(glTexSubImage2D), D(glTranslatex), D(glViewport),
    F(glGetFloatv,g_glGetFloatv), F(glGetBooleanv,g_glGetBooleanv), D(glGetString), D(glHint), D(glReadPixels), D(glCompressedTexImage2D),
    F(glTexEnvf,g_glTexEnvf), F(glTexEnvfv,g_glTexEnvfv), F(glTexEnvx,g_glTexEnvx),
    F(glColor4x, g_glColor4x), F(glTexEnvi, g_glTexEnvi), F(glTexEnvxv, g_glTexEnvxv),
    F(glTexParameterx, g_glTexParameterx), F(glGetFixedv, g_glGetFixedv),
    F(glColorPointer,g_glColorPointer), F(glNormalPointer,g_glNormalPointer), F(glDrawElements,g_glDrawElements),
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
