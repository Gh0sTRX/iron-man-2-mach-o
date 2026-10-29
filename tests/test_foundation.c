#define _POSIX_C_SOURCE 200809L
#include "foundation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char directory[1024];
static Im2Imp method(void *o,const char *selector){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,selector,NULL,&t));assert(!t.guest && !t.stret_size);return t.imp;}
static uintptr_t send0(void *o,const char *sel){return ((uintptr_t(*)(void *,const char *))method(o,sel))(o,sel);}
static void cleanup(void *o){((void(*)(void *,const char *))method(o,"dealloc"))(o,"dealloc");}
static uintptr_t allocate(Im2ObjcClass *self,const char *sel){(void)sel;return (uintptr_t)im2_objc_alloc(self,cleanup);}
static void dealloc(void *self,const char *sel){(void)sel;assert(!im2_objc_retain_count(self));}
static const char *map_path(const char *path,char *out,size_t size){if(!strncmp(path,"guest:",6))snprintf(out,size,"%s/%s",directory,path+6);else snprintf(out,size,"%s",path);return out;}
static void write_file(const char *path,const void *bytes,size_t n){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,n,f)==n);assert(!fclose(f));}
int main(void) {
    char temporary[]="/tmp/im2-foundation-XXXXXX";char *dir=mkdtemp(temporary);assert(dir);
    snprintf(directory,sizeof(directory),"%s",dir);char path[1200],sub[1200],cfgpath[1200];
    snprintf(path,sizeof(path),"%s/boot.bin",dir);const unsigned char payload[]={0,0xfe,'I','M','2'};write_file(path,payload,sizeof(payload));
    snprintf(sub,sizeof(sub),"%s/sub",dir);assert(!mkdir(sub,0700));
    char anim[1200];snprintf(anim,sizeof(anim),"%s/sub/anim.raw",dir);write_file(anim,"clip",4);
    const Im2MethodSpec root_instance[]={{"dealloc","v@:",(Im2Imp)dealloc,0}};
    const Im2MethodSpec root_class[]={{"alloc","@@:",(Im2Imp)allocate,0}};
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),root_instance,1,root_class,1);assert(root);
    snprintf(cfgpath,sizeof(cfgpath),"%s///",dir);
    Im2FoundationConfig cfg={cfgpath,NULL,NULL,map_path};
    assert(im2_foundation_init(NULL,&cfg));assert(im2_foundation_init(root->isa,&cfg));
    assert(!im2_foundation_init(root,&cfg));
    assert(im2_foundation_init(root,&cfg)); /* cannot replace a live Foundation registry */
    assert(!im2_string_create("needs pool",1));assert(!im2_data_create(payload,5,1));
    void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
    Im2String *s=im2_string_create("A\xc5\x9f\xf0\x9f\x99\x82",1);assert(s);
    assert(send0(s,"length")==4);assert(!strcmp((char *)send0(s,"UTF8String"),"A\xc5\x9f\xf0\x9f\x99\x82"));
    char out[16];memset(out,'x',sizeof(out));
    assert(im2_string_get_cstring(s,out,7,IM2_NS_UTF8));assert(out[0]=='x');
    assert(im2_string_get_cstring(s,out,sizeof(out),IM2_NS_ASCII));assert(out[0]=='x');
    assert(im2_string_get_cstring(s,out,sizeof(out),999));assert(out[0]=='x');
    assert(!im2_string_get_cstring(s,out,8,IM2_NS_UTF8));assert(!strcmp(out,s->str));
    typedef uintptr_t(*GetString)(void *,const char *,char *,size_t,uint32_t);
    assert(!((GetString)method(s,"getCString:maxLength:encoding:"))(s,"getCString:maxLength:encoding:",out,7,IM2_NS_UTF8));
    assert(((GetString)method(s,"getCString:maxLength:encoding:"))(s,"getCString:maxLength:encoding:",out,8,IM2_NS_UTF8));
    const char *invalid[]={"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)assert(!im2_string_create(invalid[i],1));
    Im2String *ascii=im2_string_create("ascii",1);assert(!im2_string_get_cstring(ascii,out,sizeof(out),IM2_NS_ASCII));
    assert(send0(im2_string_create("",1),"length")==0);
    void *allocated=(void *)send0(im2_objc_class_named("NSString"),"alloc");assert(allocated);
    typedef uintptr_t(*InitString)(void *,const char *,const char *);
    assert(((InitString)method(allocated,"initWithUTF8String:"))(allocated,"initWithUTF8String:","owned")== (uintptr_t)allocated);
    assert(!strcmp(im2_string_utf8(allocated),"owned"));assert(!im2_objc_release(allocated));
    uint8_t copied[]={1,2,3};void *data=im2_data_create(copied,3,1);assert(data);copied[0]=99;
    assert(((const uint8_t *)im2_data_bytes(data))[0]==1 && send0(data,"length")==3);
    memset(out,'x',sizeof(out));assert(im2_data_get_bytes(data,out,4));assert(out[0]=='x');
    assert(!im2_data_get_bytes(data,out,3) && out[0]==1);assert(im2_data_get_bytes(data,NULL,1));
    assert(!im2_data_create(NULL,1,0));assert(!im2_data_create(copied,IM2_FOUNDATION_MAX_DATA+1u,0));
    void *empty=im2_data_create(NULL,0,1);assert(empty && !im2_data_length(empty) && !im2_data_get_bytes(empty,NULL,0));
    void *file=im2_data_from_file("guest:boot.bin",1);assert(file && im2_data_length(file)==5 && !memcmp(im2_data_bytes(file),payload,5));
    assert(!im2_data_from_file(directory,1));
    assert(!im2_data_from_file("guest:missing.bin",1));assert(!im2_string_utf8(file));
    void *bundle=(void *)send0(im2_objc_class_named("NSBundle"),"mainBundle");assert(bundle);
    assert(!strcmp(im2_string_utf8((void *)send0(bundle,"resourcePath")),dir));
    char *found=im2_bundle_resource_path("boot","bin",NULL);assert(found && !strcmp(found,path));free(found);
    found=im2_bundle_resource_path("anim","raw","sub");assert(found && !strcmp(found,anim));free(found);
    assert(!im2_bundle_resource_path("missing","bin",NULL));assert(!im2_bundle_resource_path("../boot","bin",NULL));
    assert(!im2_bundle_resource_path("boot","../bin",NULL));assert(!im2_bundle_resource_path("boot","bin","/outside"));
    typedef uintptr_t(*Resource)(void *,const char *,void *,void *);
    Im2String *name=im2_string_create("boot",1),*ext=im2_string_create("bin",1);
    void *resource=(void *)((Resource)method(bundle,"pathForResource:ofType:"))(bundle,"pathForResource:ofType:",name,ext);
    assert(resource && !strcmp(im2_string_utf8(resource),path));
    typedef uintptr_t(*FileFactory)(void *,const char *,void *);
    void *from_method=(void *)((FileFactory)method(im2_objc_class_named("NSData"),"dataWithContentsOfFile:"))(im2_objc_class_named("NSData"),"dataWithContentsOfFile:",resource);
    assert(from_method && !memcmp(im2_data_bytes(from_method),payload,5));
    void *retained=(void *)send0(file,"copy");assert(retained==file && im2_objc_retain_count(file)==2);
    assert(!im2_objc_pool_drain(pool));assert(!im2_objc_retain_count(s));
    assert(im2_objc_retain_count(file)==1 && !memcmp(im2_data_bytes(file),payload,5));assert(!im2_objc_release(file));
    assert(im2_objc_retain_count(bundle)==1);
    assert(!unlink(path) && !unlink(anim) && !rmdir(sub) && !rmdir(dir));
    puts("PASS: actual Foundation class methods, UTF-8/UTF-16, CString failure, copied/file data, real bundle resource lookup and lifetimes");
}
