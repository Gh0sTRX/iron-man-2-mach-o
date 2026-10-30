/* SPDX-License-Identifier: MIT. Backend fixtures model GPU resources, not Vita execution. */
#include "gles_oes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint32_t next,bound,status;int live_fb,live_storage,created,fail_create,fail_storage,fail_attach;} Gpu;
static uint32_t create_fb(void *p){Gpu *g=p;if(++g->created==g->fail_create)return 0;g->live_fb++;return ++g->next;}
static void delete_fb(void *p,uint32_t id){assert(id);((Gpu *)p)->live_fb--;}
static void bind_fb(void *p,uint32_t id){((Gpu *)p)->bound=id;}
static uint32_t storage(void *p,uint32_t format,int w,int h){Gpu *g=p;assert(format==IM2_OES_RGBA8 || format==IM2_OES_RGB565 || format==IM2_OES_DEPTH16);assert(w>0 && h>0);if(g->fail_storage)return 0;g->live_storage++;return ++g->next;}
static void delete_storage(void *p,uint32_t format,uint32_t id){(void)format;assert(id);((Gpu *)p)->live_storage--;}
static int attach(void *p,uint32_t f,uint32_t point,uint32_t format,uint32_t id){(void)format;(void)id;Gpu *g=p;assert(f && (point==IM2_OES_COLOR || point==IM2_OES_DEPTH));g->bound=f;return !g->fail_attach;}
static uint32_t check(void *p,uint32_t f){Gpu *g=p;g->bound=f;return g->status;}
int main(void){
 Gpu g={.next=100,.status=IM2_OES_COMPLETE};
 Im2Oes *s=im2_oes_create((Im2OesBackend){&g,create_fb,delete_fb,bind_fb,storage,delete_storage,attach,check});assert(s);
 assert(!im2_oes_create((Im2OesBackend){0}));
 uint32_t f[2]={99,99},r[3]={0},native_fb=0,texture=0;
 g.fail_create=2;assert(im2_oes_gen(s,IM2_OES_FB,2,f)==IM2_OES_MEMORY);assert(f[0]==99 && f[1]==99 && !g.live_fb);
 g.fail_create=0;assert(!im2_oes_gen(s,IM2_OES_FB,2,f));assert(f[0]!=f[1] && g.live_fb==2);
 assert(!im2_oes_gen(s,IM2_OES_RB,3,r));assert(!im2_oes_bind(s,IM2_OES_RB,r[0]));
 assert(!im2_oes_drawable(s,&native_fb,&texture));
 int value=-1;assert(!im2_oes_parameter(s,0x8D42,&value) && !value);
 assert(im2_oes_storage(s,123,320,480)==IM2_OES_ENUM);assert(im2_oes_storage(s,IM2_OES_RGBA8,0,480)==IM2_OES_VALUE);
 g.fail_storage=1;assert(im2_oes_storage(s,IM2_OES_RGBA8,320,480)==IM2_OES_MEMORY);assert(!g.live_storage);
 g.fail_storage=0;assert(!im2_oes_storage(s,IM2_OES_RGB565,320,480));
 assert(!im2_oes_parameter(s,0x8D42,&value) && value==320);assert(!im2_oes_parameter(s,0x8D43,&value) && value==480);
 assert(!im2_oes_parameter(s,0x8D44,&value) && value==IM2_OES_RGB565);
 assert(!im2_oes_bind(s,IM2_OES_FB,f[0]));uint32_t first_native=g.bound;
 assert(im2_oes_status(s)==IM2_OES_MISSING);
 g.fail_attach=1;assert(im2_oes_attach(s,IM2_OES_COLOR,r[0])==IM2_OES_OPERATION);assert(im2_oes_status(s)==IM2_OES_MISSING);
 g.fail_attach=0;assert(!im2_oes_attach(s,IM2_OES_COLOR,r[0]));assert(im2_oes_status(s)==IM2_OES_COMPLETE);
 assert(im2_oes_storage(s,IM2_OES_RGBA8,320,480)==IM2_OES_OPERATION); /* Attached resize fails safely. */
 assert(im2_oes_drawable(s,&native_fb,&texture) && native_fb==first_native && texture);
 assert(!im2_oes_bind(s,IM2_OES_FB,f[1]));uint32_t second_native=g.bound;assert(second_native!=first_native);
 assert(im2_oes_status(s)==IM2_OES_MISSING);assert(im2_oes_drawable(s,&native_fb,&texture));assert(g.bound==second_native);
 assert(!im2_oes_bind(s,IM2_OES_RB,r[1]));assert(!im2_oes_storage(s,IM2_OES_DEPTH16,160,240));
 assert(!im2_oes_bind(s,IM2_OES_FB,f[0]));assert(!im2_oes_attach(s,IM2_OES_DEPTH,r[1]));assert(im2_oes_status(s)==IM2_OES_DIMENSIONS);
 assert(!im2_oes_attach(s,IM2_OES_DEPTH,0));assert(!im2_oes_storage(s,IM2_OES_DEPTH16,320,480));assert(!im2_oes_attach(s,IM2_OES_DEPTH,r[1]));
 assert(im2_oes_status(s)==IM2_OES_COMPLETE);assert(im2_oes_attach(s,IM2_OES_COLOR,r[1])==IM2_OES_OPERATION);
 assert(!im2_oes_bind(s,IM2_OES_RB,r[0]));g.status=IM2_OES_INCOMPLETE;assert(!im2_oes_drawable(s,&native_fb,&texture));g.status=IM2_OES_COMPLETE;
 assert(im2_oes_bind(s,IM2_OES_FB,999)==IM2_OES_OPERATION && im2_oes_binding(s,IM2_OES_FB)==f[0]);
 assert(im2_oes_bind(s,0,0)==IM2_OES_ENUM);assert(im2_oes_gen(s,IM2_OES_FB,-1,f)==IM2_OES_VALUE);
 assert(im2_oes_gen(s,IM2_OES_FB,65,f)==IM2_OES_MEMORY);assert(im2_oes_parameter(s,123,&value)==IM2_OES_ENUM);
 assert(!im2_oes_delete(s,IM2_OES_RB,1,r));assert(!im2_oes_drawable(s,&native_fb,&texture));assert(!im2_oes_binding(s,IM2_OES_RB));
 assert(im2_oes_status(s)==IM2_OES_MISSING);assert(!im2_oes_delete(s,IM2_OES_RB,1,r)); /* Unknown/already deleted names ignored. */
 assert(!im2_oes_delete(s,IM2_OES_FB,1,f));assert(!im2_oes_binding(s,IM2_OES_FB) && !g.bound);
 assert(!im2_oes_bind(s,IM2_OES_RB,r[2]));assert(!im2_oes_storage(s,IM2_OES_RGBA8,32,32));
 im2_oes_destroy(s);assert(!g.live_fb && !g.live_storage && !g.bound);
 puts("PASS: OES namespaces, native bindings, color/depth storage, dimensions, completeness, allocation rollback, errors, deletion and presentation selection");
}
