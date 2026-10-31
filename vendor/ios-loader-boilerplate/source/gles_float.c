/* SPDX-License-Identifier: MIT. iOS base-AAPCS -> Vita hard-float state calls.
 * Forward real state commands to VitaGL; no rendering/device proof is implied.
 */
#include <vitaGL.h>
#include "imports.h"
#include "gles.h"
SOFTFP static void gf_alpha(GLenum fn,GLfloat ref){glAlphaFunc(fn,ref);}
SOFTFP static void gf_clear(GLfloat r,GLfloat g,GLfloat b,GLfloat a){glClearColor(r,g,b,a);}
SOFTFP static void gf_depth(GLfloat depth){glClearDepthf(depth);}
SOFTFP static void gf_range(GLfloat near,GLfloat far){glDepthRangef(near,far);}
SOFTFP static void gf_fog(GLenum pname,GLfloat value){glFogf(pname,value);}
SOFTFP static void gf_frustum(GLfloat l,GLfloat r,GLfloat b,GLfloat t,GLfloat n,GLfloat f){glFrustumf(l,r,b,t,n,f);}
SOFTFP static void gf_ortho(GLfloat l,GLfloat r,GLfloat b,GLfloat t,GLfloat n,GLfloat f){glOrthof(l,r,b,t,n,f);}
SOFTFP static void gf_rotate(GLfloat a,GLfloat x,GLfloat y,GLfloat z){glRotatef(a,x,y,z);}
SOFTFP static void gf_scale(GLfloat x,GLfloat y,GLfloat z){glScalef(x,y,z);}
SOFTFP static void gf_translate(GLfloat x,GLfloat y,GLfloat z){glTranslatef(x,y,z);}
SOFTFP static void gf_material(GLenum face,GLenum pname,GLfloat value){glMaterialf(face,pname,value);}
static int supported_light(GLenum light,GLenum pname){
    if(light<GL_LIGHT0 || light>GL_LIGHT7){gles_record_error(GL_INVALID_ENUM);return 0;}
    switch(pname){
        case GL_AMBIENT:case GL_DIFFUSE:case GL_SPECULAR:case GL_POSITION:
        case GL_CONSTANT_ATTENUATION:case GL_LINEAR_ATTENUATION:case GL_QUADRATIC_ATTENUATION:return 1;
        default:gles_record_error(GL_INVALID_ENUM);return 0; /* SDK spot parameters unsupported. */
    }
}
static void gf_light_vector(GLenum light,GLenum pname,const GLfloat *value){
    if(!supported_light(light,pname))return;
    if(!value){gles_record_error(GL_INVALID_VALUE);return;}
    if((pname==GL_CONSTANT_ATTENUATION || pname==GL_LINEAR_ATTENUATION || pname==GL_QUADRATIC_ATTENUATION) && value[0]<0){gles_record_error(GL_INVALID_VALUE);return;}
    glLightfv(light,pname,value);
}
SOFTFP static void gf_light_scalar(GLenum light,GLenum pname,GLfloat value){
    /* A scalar must never be handed to an SDK vector case that reads four floats. */
    if(pname!=GL_CONSTANT_ATTENUATION && pname!=GL_LINEAR_ATTENUATION && pname!=GL_QUADRATIC_ATTENUATION){
        gles_record_error(GL_INVALID_ENUM);return;
    }
    gf_light_vector(light,pname,&value);
}
const import_entry gles_float_imports[]={
 F(glAlphaFunc,gf_alpha),F(glClearColor,gf_clear),F(glClearDepthf,gf_depth),
 F(glDepthRangef,gf_range),F(glFogf,gf_fog),F(glFrustumf,gf_frustum),F(glOrthof,gf_ortho),
 F(glRotatef,gf_rotate),F(glScalef,gf_scale),F(glTranslatef,gf_translate),
 F(glMaterialf,gf_material),F(glLightf,gf_light_scalar),F(glLightfv,gf_light_vector),
 /* Pointer/integer-only APIs already share the register ABI; float arrays are data. */
 F(glFogfv,glFogfv),F(glLightModelfv,glLightModelfv),F(glLoadMatrixf,glLoadMatrixf),
 F(glMaterialfv,glMaterialfv),F(glMultMatrixf,glMultMatrixf),F(glClearColorx,glClearColorx),
};
const int gles_float_imports_count=sizeof(gles_float_imports)/sizeof(gles_float_imports[0]);
