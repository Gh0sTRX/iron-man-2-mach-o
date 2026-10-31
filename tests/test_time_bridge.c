/* SPDX-License-Identifier: MIT. Controlled native wall clock; production CF/NSDate. */
#include "time_bridge.h"
#include "imports.h"
#include <sys/time.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <sys/wait.h>
#include <unistd.h>
static int failed;static int64_t seconds=978307200;static int microseconds=250000;
int __wrap_gettimeofday(struct timeval *tv,void *zone){(void)zone;if(failed){errno=EIO;return -1;}tv->tv_sec=seconds;tv->tv_usec=microseconds;return 0;}
void fatal_error(const char *message,...){assert(message && *message);_Exit(73);}
#define EMPTY(name) const import_entry name##_imports[]={{"_unused",0,0}};const int name##_imports_count=0
EMPTY(patch);EMPTY(darwin);EMPTY(gles);EMPTY(gles_float);EMPTY(coreaudio);EMPTY(objc);EMPTY(eagl);EMPTY(cfbundle);EMPTY(cfarray);EMPTY(cf_memory);
EMPTY(legacy_stream);EMPTY(legacy_string);EMPTY(legacy_container);EMPTY(math_bridge);EMPTY(cg_geometry);EMPTY(libc_text);
const import_entry *objc_class_import(const char *n){(void)n;return NULL;}
static uintptr_t address(const char *n){const import_entry *p=imports_lookup(n);assert(p && p->data==0 && p->addr);return p->addr;}
static Im2Imp method(void *o,const char *s){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,s,NULL,&t) && !t.guest && !t.stret_size);return t.imp;}
static uintptr_t send0(void *o,const char *s){return ((uintptr_t(*)(void*,const char*))method(o,s))(o,s);}
static uint64_t bits(double d){uint64_t n;memcpy(&n,&d,8);return n;}
static double unpack(uint64_t n){double d;memcpy(&d,&n,8);return d;}
static double get0(void *o,const char *s){return unpack(((uint64_t(*)(void*,const char*))method(o,s))(o,s));}
static void *send_double(void *o,const char *s,double v){return (void *)((uintptr_t(*)(void*,const char*,uint64_t))method(o,s))(o,s,bits(v));}
static int compare(void *a,void *b){return ((int32_t(*)(void*,const char*,void*))method(a,"compare:"))(a,"compare:",b);}
static int32_t field(void *t,int i){int32_t v;memcpy(&v,(unsigned char*)t+4*i,4);return v;}
int main(void){
 double (*cf)(void)=(double(*)(void))address("_CFAbsoluteTimeGetCurrent");
 double (*diff)(int32_t,int32_t)=(double(*)(int32_t,int32_t))address("_difftime");
 int32_t (*now)(int32_t*)=(int32_t(*)(int32_t*))address("_time");
 void *(*gmt)(const int32_t*)=(void*(*)(const int32_t*))address("_gmtime");
 assert(cf()==.25 && diff(INT32_MAX,INT32_MIN)==4294967295.);
 uint32_t out[]={0xa5a5a5a5,0xa5a5a5a5};assert(now((int32_t*)out)==978307200 && out[0]==978307200 && out[1]==0xa5a5a5a5);
 int32_t epoch=0;void *t=gmt(&epoch);assert(t && field(t,5)==70 && field(t,4)==0 && field(t,3)==1 && field(t,6)==4 && field(t,9)==0);
 const char *zone;memcpy(&zone,(unsigned char*)t+40,sizeof(zone));assert(!strcmp(zone,"GMT"));
 epoch=951782400;t=gmt(&epoch);assert(field(t,5)==100 && field(t,4)==1 && field(t,3)==29 && field(t,7)==59);
 epoch=INT32_MAX;t=gmt(&epoch);assert(field(t,5)==138 && field(t,4)==0 && field(t,3)==19 && field(t,0)==7);
 assert(!gmt(NULL) && errno==EINVAL);
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root && !im2_dates_init(root));assert(im2_dates_init(root));
 void *cls=im2_objc_class_named("NSDate");assert(cls && get0(cls,"timeIntervalSinceReferenceDate")==.25);
 void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 void *a=(void *)send0(cls,"date"),*b=send_double(cls,"dateWithTimeIntervalSinceReferenceDate:",0.0);assert(a && b);
 assert(get0(a,"timeIntervalSince1970")==978307200.25 && get0(b,"timeIntervalSince1970")==978307200.);
 assert(compare(a,b)==1 && compare(b,a)==-1 && compare(a,a)==0);
 double delta=unpack(((uint64_t(*)(void*,const char*,void*))method(a,"timeIntervalSinceDate:"))(a,"timeIntervalSinceDate:",b));assert(delta==.25);
 assert(send0(a,"copy")== (uintptr_t)a && im2_objc_retain_count(a)==2);assert(!im2_objc_release(a));
 void *d=im2_objc_alloc(cls,NULL);assert(d && send_double(d,"initWithTimeIntervalSince1970:",-1.)==d && get0(d,"timeIntervalSince1970")==-1.);
 assert(send_double(d,"initWithTimeIntervalSinceReferenceDate:",2.)==d && get0(d,"timeIntervalSinceReferenceDate")==2.);
 assert(send_double(d,"initWithTimeIntervalSinceReferenceDate:",1e-9)==d && get0(d,"timeIntervalSinceReferenceDate")==1e-9);
 assert(!im2_objc_release(d));
 assert(!im2_objc_pool_drain(pool) && !im2_objc_retain_count(a) && !im2_objc_retain_count(b));
 failed=1;out[0]=0xa5a5a5a5;assert(now((int32_t*)out)==-1 && out[0]==0xa5a5a5a5 && errno==EIO);
 pid_t child=fork();assert(child>=0);if(!child){cf();_Exit(1);}int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==73);
 failed=0;seconds=(int64_t)INT32_MAX+1;assert(now(NULL)==-1 && errno==EOVERFLOW);
 microseconds=1000000;assert(now(NULL)==-1 && errno==EINVAL);
 assert(time_bridge_imports_count==4);
 puts("PASS: CF/Unix epochs and fractions, signed32 time/difftime bounds, Darwin UTC tm/leap year, NSDate init/factory/comparison/raw double ABI, copy/pool lifetime and clock failure guards");
}
