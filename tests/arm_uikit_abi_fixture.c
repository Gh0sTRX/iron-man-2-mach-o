/* SPDX-License-Identifier: MIT. Only class registry/allocator dependencies are fixtures;
 * CGRect argument handling and sidecar getters are the actual production module. */
#include "objc_runtime.h"
#include "uikit_hierarchy.h"
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
static Im2ObjcClass classes[16];static Im2ClassRW rw[16];static Im2ClassRO ro[16];
static size_t class_count,used;static unsigned char heap[8192] __attribute__((aligned(16)));
static void *receiver;static Im2ObjcClass *view_class;
uintptr_t fixture_methods[4];
void *memset(void *p,int value,size_t n){unsigned char *s=p;while(n--)*s++=(unsigned char)value;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *s=p;const unsigned char *t=q;while(n--)*s++=*t++;return p;}
int strcmp(const char *a,const char *b){while(*a && *a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
void *calloc(size_t n,size_t size){if(n>8192 || size>8192 || n*size>sizeof(heap)-used)return NULL;size_t length=(n*size+15)&~15u;if(length>sizeof(heap)-used)return NULL;void *p=heap+used;used+=length;return memset(p,0,length);}
void free(void *p){(void)p;}
__attribute__((noreturn)) void abort(void){__asm__ volatile("bkpt #0");for(;;){}}
int pthread_mutex_lock(pthread_mutex_t *p){(void)p;return 0;}
int pthread_mutex_unlock(pthread_mutex_t *p){(void)p;return 0;}
Im2ObjcClass *im2_objc_register_class(const char *name,Im2ObjcClass *super,size_t size,const Im2MethodSpec *im,size_t ni,const Im2MethodSpec *cm,size_t nc){
 (void)cm;(void)nc;if(class_count==16)return NULL;size_t i=class_count++;
 ro[i].name=name;ro[i].instance_size=(uint32_t)size;rw[i].ro=ro+i;classes[i].data=rw+i;classes[i].superclass=super;
 if(!strcmp(name,"UIView")){view_class=classes+i;for(size_t j=0;j<ni;j++){
  if(!strcmp(im[j].selector,"initWithFrame:"))fixture_methods[0]=(uintptr_t)im[j].imp;
  if(!strcmp(im[j].selector,"setFrame:"))fixture_methods[1]=(uintptr_t)im[j].imp;
  if(!strcmp(im[j].selector,"setBounds:"))fixture_methods[2]=(uintptr_t)im[j].imp;
  if(!strcmp(im[j].selector,"bounds"))fixture_methods[3]=(uintptr_t)im[j].imp;
 }}return classes+i;
}
const Im2ObjcClass *im2_objc_class_metadata(const void *p){for(size_t i=0;i<class_count;i++)if(p==classes+i)return classes+i;return NULL;}
Im2ObjcClass *im2_objc_object_class(const void *p){return p==receiver?view_class:NULL;}
size_t im2_objc_registration_checkpoint(void){return class_count;}
int im2_objc_registration_rollback(size_t cp){class_count=cp;return 0;}
void *im2_objc_retain(void *p){return p;}
int im2_objc_release(void *p){(void)p;return 0;}
size_t im2_objc_retain_count(const void *p){return p?1:0;}
int fixture_setup(void *self){receiver=self;Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,4,NULL,0,NULL,0);return im2_uikit_hierarchy_init(root,NULL);}
int fixture_read(Im2Rect32 *out){
 if(im2_uikit_view_frame(receiver,out))return -1;
 ((void(*)(Im2Rect32 *,void *,const char *))fixture_methods[3])(out+1,receiver,"bounds");return 0;
}
