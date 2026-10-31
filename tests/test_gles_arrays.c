/* SPDX-License-Identifier: MIT. Real conversion engine, guarded client memory. */
#define _GNU_SOURCE
#include "gles_arrays.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#define FIXED 0x140c
#define FLOAT 0x1406
#define VERTEX 0x8074
#define TEX 0x8078
#define T0 0x84c0
static int fail_realloc;
void *__real_realloc(void *,size_t);
void *__wrap_realloc(void *p,size_t n){if(fail_realloc){fail_realloc=0;return NULL;}return __real_realloc(p,n);}
typedef struct {const void *ptr[5];int size[5],stride[5],on[5];uint32_t type[5],unit,mode,index_type;const void *indices;int first,count;unsigned draws,bindings;} Backend;
static void pointer(void *c,unsigned s,int size,uint32_t type,int stride,const void *p){Backend *b=c;b->bindings++;b->ptr[s]=p;b->size[s]=size;b->type[s]=type;b->stride[s]=stride;}
static void enable(void *c,unsigned s,int on){((Backend *)c)->on[s]=on;}
static void unit(void *c,uint32_t u){((Backend *)c)->unit=u;}
static void draw(void *c,uint32_t m,int first,int count){Backend *b=c;b->draws++;b->mode=m;b->first=first;b->count=count;}
static void elements(void *c,uint32_t m,int count,uint32_t t,const void *p){Backend *b=c;b->draws++;b->mode=m;b->count=count;b->index_type=t;b->indices=p;}
static Im2Arrays *create(Backend *b){return im2_arrays_create((Im2ArrayBackend){b,pointer,enable,unit,draw,elements});}
static void raw(void *p,int32_t n){memcpy(p,&n,4);}
int main(void){
 Backend b={0};Im2Arrays *a=create(&b);assert(a);int value;void *p;
 assert(!im2_arrays_draw(a,4,0,3) && !b.draws);
 assert(im2_arrays_parameter(a,0x807a,&value) && value==4);
 assert(im2_arrays_parameter(a,0x807b,&value) && value==FLOAT);
 unsigned char v[30];memset(v,0,sizeof(v));
 for(unsigned i=0;i<3;i++){raw(v+i*10,(int32_t)(i+1)*65536);raw(v+i*10+4,-(int32_t)(i+1)*32768);}
 assert(!im2_arrays_set(a,0,2,FIXED,10,v));assert(!im2_arrays_enable(a,VERTEX,1));
 assert(!im2_arrays_draw(a,4,1,2) && b.draws==1 && b.first==1 && b.count==2 && b.type[0]==FLOAT && b.stride[0]==0);
 const float *f=b.ptr[0];assert(f[2]==2.f && f[3]==-1.f && f[4]==3.f && f[5]==-1.5f);
 assert(!im2_arrays_get_pointer(a,0x8090,&p) && p==v);
 assert(im2_arrays_parameter(a,0x807b,&value) && value==FIXED);assert(im2_arrays_parameter(a,0x807c,&value) && value==10);
 unsigned char indices[]={2,0,2};assert(!im2_arrays_elements(a,4,3,0x1401,indices));
 f=b.ptr[0];assert(f[0]==1.f && f[1]==-.5f && f[4]==3.f && b.indices==indices && b.index_type==0x1401);
 unsigned char ix16[7];uint16_t x[]={2,1,0};memcpy(ix16+1,x,6);assert(!im2_arrays_elements(a,4,3,0x1403,ix16+1) && b.indices==ix16+1);
 int32_t col[12]={65536,0,32768,65536,0,65536,0,65536,32768,32768,65536,65536};
 int32_t norm[9]={0,0,65536,65536,0,0,0,65536,0};
 int32_t uv0[6]={0,65536,65536,0,32768,32768},uv1[6]={65536,65536,0,0,16384,49152};
 assert(!im2_arrays_set(a,1,4,FIXED,0,col) && !im2_arrays_enable(a,0x8076,1));
 assert(!im2_arrays_set(a,2,3,FIXED,0,norm) && !im2_arrays_enable(a,0x8075,1));
 assert(!im2_arrays_set(a,3,2,FIXED,0,uv0) && !im2_arrays_enable(a,TEX,1));
 assert(!im2_arrays_client_unit(a,T0+1) && !im2_arrays_set(a,4,2,FIXED,0,uv1) && !im2_arrays_enable(a,TEX,1));
 assert(!im2_arrays_elements(a,5,3,0x1401,indices) && b.unit==T0+1);
 assert(((const float *)b.ptr[1])[2]==.5f && ((const float *)b.ptr[2])[2]==1.f);
 assert(((const float *)b.ptr[3])[4]==.5f && ((const float *)b.ptr[4])[5]==.75f);
 memset(b.ptr,0,sizeof(b.ptr));memset(b.on,0,sizeof(b.on));im2_arrays_restore(a);
 for(unsigned s=0;s<5;s++){assert(b.ptr[s] && b.on[s]);}
 assert(b.unit==T0+1);
 assert(im2_arrays_client_unit(a,T0+2)==0x500 && im2_arrays_unit(a)==T0+1);
 unsigned binds=b.bindings;assert(im2_arrays_set(a,0,1,FIXED,0,v)==0x501 && b.bindings==binds);
 assert(im2_arrays_set(a,0,2,FIXED,-1,v)==0x501);assert(im2_arrays_set(a,2,3,0x1401,0,v)==0x500);
 assert(im2_arrays_enable(a,0xdead,1)==0x500 && im2_arrays_get_pointer(a,0xdead,&p)==0x500);
 assert(im2_arrays_get_pointer(a,0x8090,NULL)==0x501);
 unsigned before=b.draws;
 assert(im2_arrays_draw(a,7,0,1)==0x500 && im2_arrays_draw(a,4,-1,1)==0x501);
 assert(im2_arrays_draw(a,4,INT_MAX,1)==0x505 && im2_arrays_draw(a,4,0,-1)==0x501);
 assert(!im2_arrays_draw(a,4,0,0) && !im2_arrays_elements(a,4,0,0x1403,NULL));
 assert(im2_arrays_elements(a,4,1,0x1405,indices)==0x500);
 assert(im2_arrays_elements(a,4,1,0x1403,NULL)==0x502);
 assert(im2_arrays_elements(a,4,1048577,0x1403,indices)==0x505);
 assert(im2_arrays_elements(a,4,3,0x1403,(void *)(UINTPTR_MAX-2))==0x502);
 assert(b.draws==before);
 assert(!im2_arrays_set(a,2,3,FIXED,0,NULL));assert(im2_arrays_draw(a,4,0,3)==0x502 && b.draws==before);
 assert(!im2_arrays_enable(a,0x8075,0));assert(!im2_arrays_draw(a,4,0,3)); // disabled NULL normal is not read
 im2_arrays_destroy(a);
 // OOM must preserve the old conversion allocation and allow a later retry.
 memset(&b,0,sizeof(b));a=create(&b);assert(a);
 assert(!im2_arrays_set(a,0,2,FIXED,10,v) && !im2_arrays_enable(a,VERTEX,1));
 assert(!im2_arrays_draw(a,4,0,1));f=b.ptr[0];before=b.draws;binds=b.bindings;fail_realloc=1;
 assert(im2_arrays_draw(a,4,0,3)==0x505 && b.draws==before && b.bindings==binds && f[0]==1.f);
 assert(!im2_arrays_draw(a,4,0,3) && ((const float *)b.ptr[0])[4]==3.f);
 assert(!im2_arrays_set(a,0,2,FIXED,INT_MAX,(void *)(UINTPTR_MAX-8)));
 assert(im2_arrays_draw(a,4,0,3)==0x502);
 // Raw float/byte arrays are forwarded, not converted or normalized on the CPU.
 float raw_vertex[]={1,2,3,4};unsigned char colors[]={255,0,127,255,0,255,0,255};
 assert(!im2_arrays_set(a,0,2,FLOAT,0,raw_vertex));assert(!im2_arrays_set(a,1,4,0x1401,0,colors) && !im2_arrays_enable(a,0x8076,1));
 assert(!im2_arrays_draw(a,1,0,2) && b.ptr[0]==raw_vertex && b.ptr[1]==colors && b.type[1]==0x1401);
 im2_arrays_destroy(a);
 // Only the selected index lives on a readable page; all earlier vertices are protected.
 // Conversion must not read vertices that the draw does not reference.
 size_t page=(size_t)sysconf(_SC_PAGESIZE);assert(page%8==0 && page/8<=65535);unsigned selected=(unsigned)(page/8);
 unsigned char *memory=mmap(NULL,page*2,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);
 assert(!mprotect(memory+page,page,PROT_READ|PROT_WRITE));raw(memory+page,-65536);raw(memory+page+4,32768);
 memset(&b,0,sizeof(b));a=create(&b);assert(a && !im2_arrays_set(a,0,2,FIXED,0,memory) && !im2_arrays_enable(a,VERTEX,1));
 assert(!im2_arrays_draw(a,4,(int)selected,1));f=b.ptr[0];assert(f[selected*2]==-1.f && f[selected*2+1]==.5f);
 uint16_t sparse[]={(uint16_t)selected,(uint16_t)selected};assert(!im2_arrays_elements(a,4,2,0x1403,sparse));assert(((const float *)b.ptr[0])[selected*2]==-1.f);
 im2_arrays_destroy(a);assert(!munmap(memory,page*2));
 puts("PASS: fixed vertex/color/normal/two-texture conversion, byte strides and unaligned indices, sparse guard-page reads, state restoration/queries, passthrough, OOM recovery and arithmetic/enum/count guards");
}
