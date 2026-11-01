/* SPDX-License-Identifier: MIT. Native VitaGL diagnostic only, no guest/game code.
 * This entry replaces the loader main at compile time. Never run as a mocked
 * GPU success test; the workspace only compiles/inspects it. A runtime report
 * still needs independent confirmation of physical-device provenance.
 */
#include <vitaGL.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gles.h"
#include "gles_oes.h"
#include "imports.h"
#include "hle.h"

const char im2_vitagl_environment_probe_marker[]="native-vitagl-environment-v2-no-guest";
#define RESULT_PATH DATA_PATH "vitagl_environment_probe.json"
int _newlib_heap_size_user=256*1024*1024;
unsigned int sceUserMainThreadStackSize=1024*1024;
static struct {
 const char *stage,*failure;
 int model,shader_file,ready_before,ready_after,strings,limits,fbo_complete;
 int clear_match,triangle_match,depth_match,formats_match,cleanup,complete;
 int color_width,color_height,color_format,depth_width,depth_height,depth_format;
 unsigned error,status,error_queries;int max_texture,framebuffer_binding;
 uint64_t started_us;
 char vendor[192],renderer[192],version[192];
 unsigned char clear_pixel[4],triangle_pixel[4],depth_pixel[4];
} result;
static struct {
 void (*gen_fb)(GLsizei,GLuint *),(*gen_rb)(GLsizei,GLuint *);
 void (*bind_fb)(GLenum,GLuint),(*bind_rb)(GLenum,GLuint);
 void (*attach)(GLenum,GLenum,GLenum,GLuint);
 GLenum (*status)(GLenum),(*error)(void);
 void (*parameter)(GLenum,GLenum,GLint *);
 void (*delete_fb)(GLsizei,const GLuint *),(*delete_rb)(GLsizei,const GLuint *);
} ops;
static GLuint fb,color,depth;
static void quoted(FILE *file,const char *text){
 fputc('"',file);
 for(const unsigned char *p=(const unsigned char *)(text?text:"");*p;p++){
  if(*p=='"' || *p=='\\'){fputc('\\',file);fputc(*p,file);}
  else if(*p<32)fprintf(file,"\\u%04x",*p);
  else fputc(*p,file);
 }
 fputc('"',file);
}
static int checkpoint(const char *stage){
 result.stage=stage;FILE *file=fopen(RESULT_PATH,"w");if(!file)return -1;
 fprintf(file,"{\n\"schema\":1,\"probe\":");quoted(file,im2_vitagl_environment_probe_marker);
 fprintf(file,",\n\"execution_origin\":\"unattested_native_runtime\",\"physical_hardware_confirmed\":false,");
 fprintf(file,"\n\"guest_executed\":false,\"gameplay_executed\":false,\"stage\":");quoted(file,result.stage);
 fprintf(file,",\"failure\":");quoted(file,result.failure);
 fprintf(file,",\n\"model\":%d,\"started_us\":%llu,\"shader_file_found\":%d,",result.model,(unsigned long long)result.started_us,result.shader_file);
 fprintf(file,"\n\"ready_before_init\":%d,\"ready_after_init\":%d,\"gl_strings\":%d,\"limits\":%d,\"max_texture\":%d,",result.ready_before,result.ready_after,result.strings,result.limits,result.max_texture);
 fprintf(file,"\n\"vendor\":");quoted(file,result.vendor);fprintf(file,",\"renderer\":");quoted(file,result.renderer);fprintf(file,",\"version\":");quoted(file,result.version);
 fprintf(file,",\n\"fbo_complete\":%d,\"fbo_status\":%u,\"native_framebuffer_binding\":%d,\"first_gl_error\":%u,\"gl_error_queries\":%u,",result.fbo_complete,result.status,result.framebuffer_binding,result.error,result.error_queries);
 fprintf(file,"\n\"clear_pixel\":[%u,%u,%u,%u],\"triangle_pixel\":[%u,%u,%u,%u],",result.clear_pixel[0],result.clear_pixel[1],result.clear_pixel[2],result.clear_pixel[3],result.triangle_pixel[0],result.triangle_pixel[1],result.triangle_pixel[2],result.triangle_pixel[3]);
 fprintf(file,"\n\"oes_color_storage\":[%d,%d,%d],\"oes_depth_storage\":[%d,%d,%d],\"oes_storage_metadata_match\":%d,\"native_format_precision_verified\":false,",result.color_width,result.color_height,result.color_format,result.depth_width,result.depth_height,result.depth_format,result.formats_match);
 fprintf(file,"\n\"depth_pixel\":[%u,%u,%u,%u],\"depth_occlusion_match\":%d,",result.depth_pixel[0],result.depth_pixel[1],result.depth_pixel[2],result.depth_pixel[3],result.depth_match);
 fprintf(file,"\n\"clear_match\":%d,\"triangle_match\":%d,\"cleanup_passed\":%d,\"gpu_checks_passed\":%d\n}\n",result.clear_match,result.triangle_match,result.cleanup,result.complete);
 int ok=!ferror(file);if(fclose(file))ok=0;return ok?0:-1;
}
static uintptr_t binding(const char *name){
 const import_entry *entry=imports_lookup(name);
 return entry && entry->addr && entry->data==IM2_IMPORT_FUNCTION?entry->addr:0;
}
static int bind_ops(void){
#define B(field,name,type) do{uintptr_t address=binding(name);if(!address)return -1;ops.field=(type)address;}while(0)
 B(gen_fb,"_glGenFramebuffersOES",void(*)(GLsizei,GLuint *));
 B(gen_rb,"_glGenRenderbuffersOES",void(*)(GLsizei,GLuint *));
 B(bind_fb,"_glBindFramebufferOES",void(*)(GLenum,GLuint));
 B(bind_rb,"_glBindRenderbufferOES",void(*)(GLenum,GLuint));
 B(attach,"_glFramebufferRenderbufferOES",void(*)(GLenum,GLenum,GLenum,GLuint));
 B(status,"_glCheckFramebufferStatusOES",GLenum(*)(GLenum));
 B(error,"_glGetError",GLenum(*)(void));
 B(parameter,"_glGetRenderbufferParameterivOES",void(*)(GLenum,GLenum,GLint *));
 B(delete_fb,"_glDeleteFramebuffersOES",void(*)(GLsizei,const GLuint *));
 B(delete_rb,"_glDeleteRenderbuffersOES",void(*)(GLsizei,const GLuint *));
#undef B
 return 0;
}
static int no_error(void){result.error_queries++;GLenum error=ops.error();if(error && !result.error)result.error=error;return error==GL_NO_ERROR;}
static int shader_exists(void){
 const char *paths[]={"ur0:/data/libshacccg.suprx","ur0:/data/external/libshacccg.suprx"};
 for(unsigned i=0;i<2;i++){SceIoStat st;if(!sceIoGetstat(paths[i],&st) && st.st_size>0)return 1;}
 return 0;
}
static int driver_string(GLenum name,char out[192]){
 const GLubyte *value=glGetString(name);if(!value || !*value)return 0;
 snprintf(out,192,"%s",(const char *)value);return 1;
}
static void release_objects(void){
 if(ops.bind_fb)ops.bind_fb(IM2_OES_FB,0);
 if(fb && ops.delete_fb){ops.delete_fb(1,&fb);fb=0;}
 if(color && ops.delete_rb){ops.delete_rb(1,&color);color=0;}
 if(depth && ops.delete_rb){ops.delete_rb(1,&depth);depth=0;}
}
int main(void){
 result.model=sceKernelGetModel();result.started_us=sceKernelGetProcessTimeWide();
 sceIoMkdir(DATA_PATH,0777);
 if(checkpoint("started")){sceKernelExitProcess(2);return 2;}
 result.ready_before=gles_eagl_ready();
 if(result.ready_before){result.failure="unexpected_preinitialized_gate";goto failed;}
 result.shader_file=shader_exists();
 if(!result.shader_file){result.failure="shader_compiler_file_missing";goto failed;}
 if(checkpoint("before_production_gles_init"))goto report_failed;
 gles_init(); /* Exactly the production VitaGL/OES/array initialization path. */
 result.ready_after=gles_eagl_ready();
 if(!result.ready_after){result.failure="production_gles_init_failed";goto failed;}
 if(bind_ops()){result.failure="production_oes_binding_missing";goto failed;}
 if(!no_error()){result.failure="initial_gl_error";goto failed;}
 result.strings=driver_string(GL_VENDOR,result.vendor) && driver_string(GL_RENDERER,result.renderer) && driver_string(GL_VERSION,result.version);
 int error_free=no_error();
 if(!result.strings || !error_free){result.failure="driver_identity_query_failed";goto failed;}
 glGetIntegerv(GL_MAX_TEXTURE_SIZE,&result.max_texture);result.limits=result.max_texture>=480;
 error_free=no_error();
 if(!result.limits || !error_free){result.failure="insufficient_texture_limit";goto failed;}
 if(checkpoint("before_real_oes_drawable"))goto report_failed;
 ops.gen_fb(1,&fb);ops.gen_rb(1,&color);ops.gen_rb(1,&depth);
 error_free=no_error();
 if(!fb || !color || !depth || !error_free){result.failure="oes_allocation_failed";goto failed;}
 ops.bind_fb(IM2_OES_FB,fb);ops.bind_rb(IM2_OES_RB,color);
 int storage_ok=gles_eagl_storage(IM2_OES_RB,IM2_OES_RGB565);error_free=no_error();
 if(!storage_ok || !error_free){result.failure="production_color_storage_failed";goto failed;}
 ops.parameter(IM2_OES_RB,0x8D42,&result.color_width);
 ops.parameter(IM2_OES_RB,0x8D43,&result.color_height);
 ops.parameter(IM2_OES_RB,0x8D44,&result.color_format);
 if(!no_error()){result.failure="color_storage_query_failed";goto failed;}
 ops.attach(IM2_OES_FB,IM2_OES_COLOR,IM2_OES_RB,color);
 if(!no_error()){result.failure="production_color_attach_failed";goto failed;}
 ops.bind_rb(IM2_OES_RB,depth);
 /* Depth uses the same production OES bridge as the original game. */
 uintptr_t storage=binding("_glRenderbufferStorageOES");
 if(!storage){result.failure="depth_storage_binding_missing";goto failed;}
 ((void(*)(GLenum,GLenum,GLsizei,GLsizei))storage)(IM2_OES_RB,IM2_OES_DEPTH16,320,480);
 ops.parameter(IM2_OES_RB,0x8D42,&result.depth_width);
 ops.parameter(IM2_OES_RB,0x8D43,&result.depth_height);
 ops.parameter(IM2_OES_RB,0x8D44,&result.depth_format);
 result.formats_match=result.color_width==320 && result.color_height==480 && result.color_format==IM2_OES_RGB565 && result.depth_width==320 && result.depth_height==480 && result.depth_format==IM2_OES_DEPTH16;
 error_free=no_error();
 if(!result.formats_match || !error_free){result.failure="oes_storage_metadata_mismatch";goto failed;}
 ops.attach(IM2_OES_FB,IM2_OES_DEPTH,IM2_OES_RB,depth);
 result.status=ops.status(IM2_OES_FB);result.fbo_complete=result.status==IM2_OES_COMPLETE;
 error_free=no_error();
 if(!result.fbo_complete || !error_free){result.failure="production_fbo_incomplete";goto failed;}
 glGetIntegerv(GL_FRAMEBUFFER_BINDING,&result.framebuffer_binding);error_free=no_error();
 if(result.framebuffer_binding<=0 || !error_free){result.failure="native_framebuffer_binding_not_confirmed";goto failed;}
 if(checkpoint("before_clear_readback"))goto report_failed;
 glViewport(0,0,16,16);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);
 glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPixelStorei(GL_PACK_ALIGNMENT,1);
 glClearColor(0.f,1.f,0.f,1.f);glClear(GL_COLOR_BUFFER_BIT);glFinish();
 memset(result.clear_pixel,0x5a,4);glReadPixels(8,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.clear_pixel);
 result.clear_match=result.clear_pixel[0]<=8 && result.clear_pixel[1]>=240 && result.clear_pixel[2]<=8 && result.clear_pixel[3]>=240;
 error_free=no_error();
 if(!result.clear_match || !error_free){result.failure="clear_readback_failed";goto failed;}
 if(checkpoint("before_fixed_pipeline_triangle"))goto report_failed;
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 const GLfloat vertices[]={-1.f,-1.f,1.f,-1.f,0.f,1.f};
 glDisableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glEnableClientState(GL_VERTEX_ARRAY);
 glVertexPointer(2,GL_FLOAT,0,vertices);glColor4f(1.f,0.f,0.f,1.f);glDrawArrays(GL_TRIANGLES,0,3);glFinish();
 memset(result.triangle_pixel,0x5a,4);glReadPixels(8,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.triangle_pixel);
 result.triangle_match=result.triangle_pixel[0]>=240 && result.triangle_pixel[1]<=8 && result.triangle_pixel[2]<=8 && result.triangle_pixel[3]>=240;
 error_free=no_error();
 if(!result.triangle_match || !error_free){result.failure="triangle_readback_failed";goto failed;}
 if(checkpoint("before_depth_occlusion_readback"))goto report_failed;
 glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
 glClearDepth(1.0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 const GLfloat near_vertices[]={-1.f,-1.f,0.f,1.f,-1.f,0.f,0.f,1.f,0.f};
 const GLfloat far_vertices[]={-1.f,-1.f,0.75f,1.f,-1.f,0.75f,0.f,1.f,0.75f};
 glVertexPointer(3,GL_FLOAT,0,near_vertices);glColor4f(1.f,0.f,0.f,1.f);glDrawArrays(GL_TRIANGLES,0,3);
 glVertexPointer(3,GL_FLOAT,0,far_vertices);glColor4f(0.f,0.f,1.f,1.f);glDrawArrays(GL_TRIANGLES,0,3);glFinish();
 memset(result.depth_pixel,0x5a,4);glReadPixels(8,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.depth_pixel);
 result.depth_match=result.depth_pixel[0]>=240 && result.depth_pixel[1]<=8 && result.depth_pixel[2]<=8 && result.depth_pixel[3]>=240;
 error_free=no_error();
 if(!result.depth_match || !error_free){result.failure="depth_occlusion_readback_failed";goto failed;}
 release_objects();result.cleanup=no_error();
 if(!result.cleanup){result.failure="cleanup_gl_error";goto failed;}
 result.complete=1;
 if(checkpoint("gpu_checks_passed_unattested_hardware"))goto report_failed;
 sceKernelExitProcess(0);return 0;
failed:
 release_objects();result.complete=0;
 if(checkpoint("failed"))goto report_failed;
 sceKernelExitProcess(1);return 1;
report_failed:
 release_objects();sceKernelExitProcess(2);return 2;
}
