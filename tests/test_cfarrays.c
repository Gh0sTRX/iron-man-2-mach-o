/* SPDX-License-Identifier: MIT. Actual CF import router/lifetime, opaque borrowed values. */
#include "cfarrays.h"
#include "imports.h"
#include "foundation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define EMPTY(name) const import_entry name##_imports[]={{"_unused",0,0}};const int name##_imports_count=0
EMPTY(patch);EMPTY(libc_text);EMPTY(darwin);EMPTY(gles_float);EMPTY(gles);EMPTY(coreaudio);EMPTY(objc);EMPTY(eagl);EMPTY(cfbundle);
EMPTY(legacy_string);EMPTY(legacy_container);EMPTY(math_bridge);EMPTY(cg_geometry);
const import_entry *objc_class_import(const char *n){(void)n;return NULL;}
void fatal_error(const char *fmt,...){assert(strstr(fmt,"CFArray") || strstr(fmt,"CFRelease") || strstr(fmt,"CFRetain") || strstr(fmt,"CF array"));_Exit(73);}
static uintptr_t address(const char *n){const import_entry *p=imports_lookup(n);assert(p && p->data==0 && p->addr);return p->addr;}
typedef void *(*Create)(const void *,int32_t,const void *);
typedef void (*Append)(void *,const void *);
typedef int32_t(*Count)(const void *);
typedef const void *(*At)(const void *,int32_t);
typedef void (*Release)(void *);
typedef void *(*Retain)(void *);
static Create create;static Append append;static Count count;static At at;static Release release,clear;static Retain retain;
static unsigned destroyed;
static void destroy(void *v){(void)v;destroyed++;}
static void rejected(unsigned test,void *array){
 pid_t child=fork();assert(child>=0);
 if(!child){
  switch(test){
   case 0:create((void *)1,0,NULL);break;
   case 1:create(NULL,1,NULL);break;
   case 2:create(NULL,-1,NULL);break;
   case 3:create(NULL,0,(void *)1);break;
   case 4:at(array,-1);break;
   case 5:at(array,1);break;
   case 6:append((void *)1,(void *)2);break;
   case 7:release((void *)1);break;
   case 8:retain(NULL);break;
   case 9:count(NULL);break;
   case 10:for(unsigned i=0;i<65536;i++)append(array,(void *)(uintptr_t)i);break;
   default:abort();
  }
  _Exit(1);
 }
 int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==73);
}
int main(void){
 create=(Create)address("_CFArrayCreateMutable");append=(Append)address("_CFArrayAppendValue");
 count=(Count)address("_CFArrayGetCount");at=(At)address("_CFArrayGetValueAtIndex");
 clear=(Release)address("_CFArrayRemoveAllValues");retain=(Retain)address("_CFRetain");release=(Release)address("_CFRelease");
 const import_entry *allocator=imports_lookup("_kCFAllocatorDefault");assert(allocator && allocator->data==1 && !*(const void *const *)allocator->addr);
 assert(im2_cfarrays_init(NULL));
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root && !im2_cfarrays_init(root));assert(im2_cfarrays_init(root));
 Im2FoundationConfig cfg={"/tmp",NULL,NULL,NULL};assert(!im2_foundation_init(root,&cfg));
 // Create/Get work without an autorelease pool and never consume object values.
 void *a=create(*(const void *const *)allocator->addr,0,NULL);assert(a && count(a)==0 && im2_objc_retain_count(a)==1);
 void *v=im2_objc_alloc(root,destroy);assert(v);append(a,v);append(a,v);assert(count(a)==2 && at(a,0)==v && at(a,1)==v && im2_objc_retain_count(v)==1);
 release(v);assert(destroyed==1);clear(a);assert(count(a)==0 && destroyed==1); // stale values must not be touched
 for(unsigned i=0;i<1025;i++)append(a,(void *)(uintptr_t)(0x10000+i));
 for(unsigned i=0;i<1025;i++)assert(at(a,(int32_t)i)==(void *)(uintptr_t)(0x10000+i));
 clear(a);append(a,NULL);assert(count(a)==1 && !at(a,0));clear(a);
 append(a,a);assert(im2_objc_retain_count(a)==1);clear(a); // no retaining self-cycle
 assert(retain(a)==a && im2_objc_retain_count(a)==2);release(a);assert(im2_objc_retain_count(a)==1);
 // Shared production CF lifetime functions also handle ordinary Foundation values.
 void *s=im2_string_create("borrowed",0);append(a,s);assert(im2_objc_retain_count(s)==1);retain(s);release(s);clear(a);release(s);
 append(a,(void *)0x1234);
 for(unsigned i=0;i<11;i++)rejected(i,a);
 release(a);assert(!im2_objc_retain_count(a));
 void *b=create(NULL,0,NULL);v=im2_objc_alloc(root,destroy);append(b,v);release(b);assert(destroyed==1 && im2_objc_retain_count(v)==1);release(v);assert(destroyed==2);
 assert(cfarray_imports_count==6 && cf_memory_imports_count==2);
 puts("PASS: actual CF import routing, NULL allocator DATA, Create/Get/retain/release rules, non-retained opaque/duplicate/stale/self values, growth/clear/destruction, Foundation lifetime and 11 rejection cases");
}
