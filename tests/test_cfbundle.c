/* SPDX-License-Identifier: MIT. Real import router + CFBundle/CFURL and temporary resource I/O. */
#define _POSIX_C_SOURCE 200809L
#include "foundation.h"
#include "url.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* Unrelated backends are absent in this CPU-only fixture. The CF table and
 * production import router are linked unmodified. No Vita/GPU execution claim. */
#define EMPTY(name) const import_entry name##_imports[]={{"_fixture_unused",0,0}};const int name##_imports_count=0
EMPTY(cf_memory);EMPTY(cfarray);EMPTY(cg_geometry);EMPTY(math_bridge);EMPTY(legacy_container);EMPTY(legacy_string);EMPTY(patch);EMPTY(libc_text);EMPTY(darwin);EMPTY(gles_float);EMPTY(gles);EMPTY(coreaudio);EMPTY(objc);EMPTY(eagl);
const import_entry *objc_class_import(const char *name){(void)name;return NULL;}
static char temporary_root[512];
static const char *map(const char *in,char *out,size_t n){
 const char *prefix="ux0:data/ironman2_macho/";
 if(!strncmp(in,prefix,strlen(prefix)))snprintf(out,n,"%s/%s",temporary_root,in+strlen(prefix));else snprintf(out,n,"%s",in);
 return out;
}
static uintptr_t address(const char *name){const import_entry *p=imports_lookup(name);assert(p && p->addr && p->data==IM2_IMPORT_FUNCTION);return p->addr;}
static Im2Imp method(void *o,const char *sel){Im2ObjcTarget t;assert(!im2_objc_lookup_target(o,sel,NULL,&t) && !t.guest && !t.stret_size);return t.imp;}
static void *send0(void *o,const char *sel){return (void *)((uintptr_t(*)(void *,const char *))method(o,sel))(o,sel);}
static void *send1(void *o,const char *sel,void *arg){return (void *)((uintptr_t(*)(void *,const char *,void *))method(o,sel))(o,sel,arg);}
int main(void){
 typedef void *(*Main)(void);typedef void *(*Copy)(void *);
 typedef void *(*Path)(void *,int32_t);typedef uint32_t(*Representation)(void *,uint32_t,uint8_t *,int32_t);
 Main main_bundle=(Main)address("_CFBundleGetMainBundle");Copy bundle_url=(Copy)address("_CFBundleCopyBundleURL"),resources_url=(Copy)address("_CFBundleCopyResourcesDirectoryURL");
 Path copy_path=(Path)address("_CFURLCopyFileSystemPath");Representation file_rep=(Representation)address("_CFURLGetFileSystemRepresentation");
 assert(cfbundle_imports_count==5 && !main_bundle() && !imports_lookup("_not_provided"));
 char dir[]="/tmp/im2-cfbundle-XXXXXX";assert(mkdtemp(dir));snprintf(temporary_root,sizeof(temporary_root),"%s",dir);
 char app[600],asset_path[700];snprintf(app,sizeof(app),"%s/IronMan2.app",dir);assert(!mkdir(app,0700));
 snprintf(asset_path,sizeof(asset_path),"%s/clip space.bin",app);const unsigned char payload[]={0,'I','M','2',0xff};
 FILE *f=fopen(asset_path,"wb");assert(f && fwrite(payload,1,sizeof(payload),f)==sizeof(payload) && !fclose(f));
 Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);assert(root);
 const char *bundle_path="ux0:data/ironman2_macho/IronMan2.app";
 Im2FoundationConfig cfg={bundle_path,NULL,NULL,map};assert(!im2_foundation_init(root,&cfg) && !im2_url_init(root,NULL));
 void *bundle=main_bundle();assert(bundle && bundle==main_bundle() && im2_objc_retain_count(bundle)==1);
 void *a=bundle_url(bundle),*b=resources_url(bundle);assert(a && b && im2_objc_retain_count(a)==1 && im2_objc_retain_count(b)==1);
 assert(!strcmp(im2_url_local_file_path(a),bundle_path));assert(!strcmp(im2_url_local_file_path(b),bundle_path));
 void *path=copy_path(a,0);assert(path && !strcmp(im2_string_utf8(path),bundle_path) && im2_objc_retain_count(path)==1);
 assert(!copy_path(a,1) && !copy_path(a,2) && !copy_path(a,-1));
 assert(!bundle_url(path) && !resources_url(NULL) && !copy_path(path,0));
 unsigned char out[512];memset(out,0xa5,sizeof(out));size_t length=strlen(bundle_path);
 assert(!file_rep(a,1,out,(int32_t)length) && out[0]==0xa5);
 assert(!file_rep(a,1,out,0) && !file_rep(a,1,out,-1) && !file_rep(a,0,NULL,1));
 assert(file_rep(a,1,out,(int32_t)length+1) && !strcmp((char *)out,bundle_path) && out[length+1]==0xa5);
 assert(file_rep(a,0,out,(int32_t)sizeof(out)));
 const char *native_asset="ux0:data/ironman2_macho/IronMan2.app/clip space.bin";
 void *asset=im2_url_create_file(native_asset,0);assert(asset && !strcmp(im2_url_local_file_path(asset),native_asset));
 void *bytes=im2_data_from_file(im2_url_local_file_path(asset),0);assert(bytes && im2_data_length(bytes)==sizeof(payload) && !memcmp(im2_data_bytes(bytes),payload,sizeof(payload)));
 assert(!im2_url_create_file("relative/file",0) && !im2_url_create_file("other0:data/file",0));
 assert(!im2_url_create_file(bundle_path,1)); /* Copy entry points above work without a pool. */
 void *pool=im2_objc_alloc(root,NULL);assert(pool && !im2_objc_pool_push(pool));
 assert(!strcmp(im2_string_utf8(send0(asset,"absoluteString")),"file:///ux0:data/ironman2_macho/IronMan2.app/clip%20space.bin"));
 void *from_text=send1(im2_objc_class_named("NSURL"),"URLWithString:",im2_string_create("file:///ux0:data/ironman2_macho/IronMan2.app/clip%20space.bin",1));
 assert(from_text && !strcmp(im2_url_local_file_path(from_text),native_asset));
 void *from_path=send1(im2_objc_class_named("NSURL"),"fileURLWithPath:",im2_string_create(native_asset,1));assert(from_path && !strcmp(im2_url_local_file_path(from_path),native_asset));
 void *file_data=send1(im2_objc_class_named("NSData"),"dataWithContentsOfURL:",from_path);assert(file_data && im2_data_length(file_data)==sizeof(payload) && !memcmp(im2_data_bytes(file_data),payload,sizeof(payload)));
 void *remote=send1(im2_objc_class_named("NSURL"),"URLWithString:",im2_string_create("https://example.test/clip",1));assert(remote && !copy_path(remote,0) && !file_rep(remote,1,out,sizeof(out)));
 void *server=send1(im2_objc_class_named("NSURL"),"URLWithString:",im2_string_create("file://server/ux0:data/file",1));assert(!server);
 const char *mounts[]={"ur0:/data/clip","app0:assets/clip","ux0:"};
 for(size_t i=0;i<3;i++){void *u=im2_url_create_file(mounts[i],0);assert(u && !strcmp(im2_url_local_file_path(u),mounts[i]));assert(!im2_objc_release(u));}
 assert(!im2_objc_pool_drain(pool));
 assert(!im2_objc_release(bytes) && !im2_objc_release(asset) && !im2_objc_release(path));
 assert(!im2_objc_release(a) && !im2_objc_release(b) && im2_objc_retain_count(bundle)==1);
 assert(!unlink(asset_path) && !rmdir(app) && !rmdir(dir));
 puts("PASS: actual CF import routing, borrowed/Get vs owned/Copy rules, Vita mount URL roundtrip, buffer bounds, remote rejection and real resource-file bytes");
}
