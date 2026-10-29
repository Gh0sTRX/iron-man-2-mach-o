/* SPDX-License-Identifier: MIT. Independently authored; no Apple/Swift implementation copied. */
#include "foundation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define PATH_LIMIT 4095u
typedef struct { Im2ObjcClass *isa;uint8_t *bytes;uint32_t length; } Data;
typedef struct { Im2ObjcClass *isa;char *path; } Bundle;
static Im2ObjcClass *strings,*data_class,*bundles;
static Bundle *main_bundle;
static Im2FoundationConfig config;
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(offsetof(Im2String,str)==8,"ARM32 string C pointer offset");
_Static_assert(sizeof(Im2String)==16,"ARM32 string object prefix");
#endif
static char *duplicate(const char *s) {
    size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p;
}
static int kind_of(const void *object,Im2ObjcClass *expected) {
    if(!object || !expected)return 0;
    Im2ObjcClass *cls=im2_objc_object_class(object);
    for(unsigned i=0;cls && i<128;i++) {
        if(cls==expected)return 1;
        const Im2ObjcClass *m=im2_objc_class_metadata(cls);if(!m)return 0;
        cls=m->superclass;
    }
    return 0;
}
static void fail(const char *message) {
    if(config.fail)config.fail(message);
    abort(); /* A returning error callback must not turn unsupported behavior into success. */
}
int im2_utf8_utf16_length(const char *text,size_t bytes,uint32_t *length) {
    if(!length || (bytes && !text) || bytes>UINT32_MAX)return -1;
    uint32_t units=0;
    for(size_t i=0;i<bytes;) {
        uint32_t c=(unsigned char)text[i++],min=0;unsigned n=0;
        if(c<0x80) {units++;continue;}
        if(c>=0xc2 && c<=0xdf){n=1;c&=31;min=0x80;}
        else if(c>=0xe0 && c<=0xef){n=2;c&=15;min=0x800;}
        else if(c>=0xf0 && c<=0xf4){n=3;c&=7;min=0x10000;}
        else return -1;
        if(n>bytes-i)return -1;
        for(unsigned j=0;j<n;j++) {
            uint32_t b=(unsigned char)text[i++];if((b&0xc0)!=0x80)return -1;
            c=(c<<6)|(b&63);
        }
        if(c<min || c>0x10ffff || (c>=0xd800 && c<=0xdfff))return -1;
        units+=c>0xffff?2u:1u;
    }
    *length=units;return 0;
}
static void string_cleanup(void *o){free(((Im2String *)o)->str);((Im2String *)o)->str=NULL;}
static void data_cleanup(void *o){free(((Data *)o)->bytes);((Data *)o)->bytes=NULL;}
static void bundle_cleanup(void *o){free(((Bundle *)o)->path);((Bundle *)o)->path=NULL;}
static void *finish(void *o,int autorelease) {
    if(o && autorelease && !im2_objc_autorelease(o)){im2_objc_release(o);return NULL;}
    return o;
}
static int string_assign(Im2String *o,const char *text,uint32_t encoding) {
    if(!kind_of(o,strings) || !text)return -1;
    size_t bytes=strlen(text);uint32_t units;
    if(bytes>IM2_FOUNDATION_MAX_DATA || im2_utf8_utf16_length(text,bytes,&units))return -1;
    if(encoding==IM2_NS_ASCII) {for(size_t i=0;i<bytes;i++)if((unsigned char)text[i]>=128)return -1;}
    else if(encoding!=IM2_NS_UTF8)return -1;
    char *copy=duplicate(text);if(!copy)return -1;
    free(o->str);o->str=copy;o->bytes=(uint32_t)bytes;o->flags=0x7c8;return 0;
}
Im2String *im2_string_create(const char *text,int autorelease) {
    if(!strings || !text)return NULL;
    Im2String *o=im2_objc_alloc(strings,string_cleanup);if(!o)return NULL;
    if(string_assign(o,text,IM2_NS_UTF8)){im2_objc_release(o);return NULL;}
    return finish(o,autorelease);
}
const char *im2_string_utf8(const void *o) {
    if(!kind_of(o,strings))return NULL;
    return ((const Im2String *)o)->str?((const Im2String *)o)->str:"";
}
int im2_string_get_cstring(const void *o,char *out,size_t capacity,uint32_t encoding) {
    const char *s=im2_string_utf8(o);if(!s || !out)return -1;
    size_t n=strlen(s);if(capacity<=n)return -1;
    if(encoding==IM2_NS_ASCII){for(size_t i=0;i<n;i++)if((unsigned char)s[i]>=128)return -1;}
    else if(encoding!=IM2_NS_UTF8)return -1;
    memcpy(out,s,n+1);return 0;
}
static int data_assign(Data *o,const void *bytes,size_t n) {
    if(!kind_of(o,data_class) || (n && !bytes) || n>IM2_FOUNDATION_MAX_DATA)return -1;
    uint8_t *copy=n?malloc(n):NULL;if(n && !copy)return -1;
    if(n)memcpy(copy,bytes,n);
    free(o->bytes);o->bytes=copy;o->length=(uint32_t)n;return 0;
}
void *im2_data_create(const void *bytes,size_t n,int autorelease) {
    if(!data_class)return NULL;
    Data *o=im2_objc_alloc(data_class,data_cleanup);if(!o)return NULL;
    if(data_assign(o,bytes,n)){im2_objc_release(o);return NULL;}
    return finish(o,autorelease);
}
static const char *mapped_path(const char *path,char buffer[PATH_LIMIT+1]) {
    if(!path || strlen(path)>PATH_LIMIT)return NULL;
    const char *mapped=config.map_path?config.map_path(path,buffer,PATH_LIMIT+1):path;
    return mapped && strlen(mapped)<PATH_LIMIT?mapped:NULL;
}
void *im2_data_from_file(const char *path,int autorelease) {
    char mapped[PATH_LIMIT+1];const char *p=mapped_path(path,mapped);if(!p)return NULL;
    struct stat st;if(stat(p,&st) || !S_ISREG(st.st_mode))return NULL;
    FILE *f=fopen(p,"rb");if(!f)return NULL;
    Data *o=NULL;
    if(fseek(f,0,SEEK_END))goto done;
    long n=ftell(f);if(n<0 || (unsigned long)n>IM2_FOUNDATION_MAX_DATA || fseek(f,0,SEEK_SET))goto done;
    o=im2_objc_alloc(data_class,data_cleanup);if(!o)goto done;
    o->bytes=n?malloc((size_t)n):NULL;o->length=(uint32_t)n;
    if((n && !o->bytes) || (n && fread(o->bytes,1,(size_t)n,f)!=(size_t)n) || ferror(f)) {im2_objc_release(o);o=NULL;}
done:
    if(fclose(f) && o){im2_objc_release(o);o=NULL;}
    return finish(o,autorelease);
}
const void *im2_data_bytes(const void *o){return kind_of(o,data_class)?((const Data *)o)->bytes:NULL;}
size_t im2_data_length(const void *o){return kind_of(o,data_class)?((const Data *)o)->length:0;}
int im2_data_get_bytes(const void *o,void *out,size_t n) {
    if(!kind_of(o,data_class) || (n && !out) || n>((const Data *)o)->length)return -1;
    if(n)memcpy(out,((const Data *)o)->bytes,n);
    return 0;
}
static int relative(const char *s) {
    if(!s || !*s || s[0]=='/' || strchr(s,':') || strchr(s,'\\'))return 0;
    const char *p=s;
    while(*p){const char *end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);
        if(!n || (n==1 && p[0]=='.') || (n==2 && p[0]=='.' && p[1]=='.'))return 0;
        if(!end)return 1;
        p=end+1;if(!*p)return 0;
    }
    return 0;
}
static char *resource_path(const Bundle *b,const char *name,const char *ext,const char *dir) {
    if(!b || !b->path || !relative(name) || (dir && *dir && !relative(dir)))return NULL;
    if(ext && (strchr(ext,'/') || strchr(ext,'\\') || strchr(ext,':')))return NULL;
    size_t n=strlen(b->path)+strlen(name)+3;
    if(ext)n+=strlen(ext);
    if(dir)n+=strlen(dir)+1;
    if(n>PATH_LIMIT)return NULL;
    char *path=malloc(n);if(!path)return NULL;
    snprintf(path,n,"%s/%s%s%s%s%s",b->path,dir?dir:"",dir && *dir?"/":"",name,ext && *ext?".":"",ext?ext:"");
    struct stat st;if(stat(path,&st) || !S_ISREG(st.st_mode)){free(path);return NULL;}
    return path;
}
char *im2_bundle_resource_path(const char *n,const char *e,const char *d){return resource_path(main_bundle,n,e,d);}
static uintptr_t string_factory(void *self,const char *sel,const char *text){(void)self;(void)sel;return (uintptr_t)im2_string_create(text,1);}
static uintptr_t string_init(Im2String *self,const char *sel,const char *s){(void)sel;if(string_assign(self,s,IM2_NS_UTF8)){im2_objc_release(self);return 0;}return (uintptr_t)self;}
static uintptr_t string_init_encoding(Im2String *self,const char *sel,const char *s,uint32_t encoding){(void)sel;if(string_assign(self,s,encoding)){im2_objc_release(self);return 0;}return (uintptr_t)self;}
static uintptr_t string_utf8(void *self,const char *sel){(void)sel;return (uintptr_t)im2_string_utf8(self);}
static uintptr_t string_length(void *self,const char *sel){(void)sel;const char *s=im2_string_utf8(self);uint32_t n=0;if(!s || im2_utf8_utf16_length(s,strlen(s),&n))fail("Invalid NSString UTF-8");return n;}
static uintptr_t string_get(void *self,const char *sel,char *out,size_t n,uint32_t enc){(void)sel;return im2_string_get_cstring(self,out,n,enc)==0;}
static uintptr_t string_cencoding(void *self,const char *sel,uint32_t enc){(void)sel;const char *s=im2_string_utf8(self);if(!s)return 0;if(enc==IM2_NS_ASCII){for(const unsigned char *p=(const unsigned char *)s;*p;p++)if(*p>=128)return 0;}else if(enc!=IM2_NS_UTF8)return 0;return (uintptr_t)s;}
static uintptr_t immutable_copy(void *self,const char *sel){(void)sel;return (uintptr_t)im2_objc_retain(self);}
static void string_dealloc(void *self,const char *sel){(void)sel;string_cleanup(self);}
static uintptr_t data_factory(void *self,const char *sel,const void *b,size_t n){(void)self;(void)sel;return (uintptr_t)im2_data_create(b,n,1);}
static uintptr_t data_empty(void *self,const char *sel){(void)self;(void)sel;return (uintptr_t)im2_data_create(NULL,0,1);}
static uintptr_t data_file(void *self,const char *sel,void *path){(void)self;(void)sel;return (uintptr_t)im2_data_from_file(im2_string_utf8(path),1);}
static uintptr_t data_init(Data *self,const char *sel,const void *b,size_t n){(void)sel;if(data_assign(self,b,n)){im2_objc_release(self);return 0;}return (uintptr_t)self;}
static uintptr_t data_bytes(void *self,const char *sel){(void)sel;return (uintptr_t)im2_data_bytes(self);}
static uintptr_t data_length(void *self,const char *sel){(void)sel;return im2_data_length(self);}
static void data_get(void *self,const char *sel,void *out,size_t n){(void)sel;if(im2_data_get_bytes(self,out,n))fail("NSData getBytes:length: range rejected");}
static void data_dealloc(void *self,const char *sel){(void)sel;data_cleanup(self);}
static uintptr_t bundle_main(void *self,const char *sel){(void)self;(void)sel;return (uintptr_t)main_bundle;}
static uintptr_t bundle_path(Bundle *self,const char *sel){(void)sel;return (uintptr_t)(self->path?im2_string_create(self->path,1):NULL);}
static uintptr_t bundle_resource_dir(Bundle *self,const char *sel,void *name,void *ext,void *dir) {
    (void)sel;const char *n=im2_string_utf8(name),*e=ext?im2_string_utf8(ext):NULL,*d=dir?im2_string_utf8(dir):NULL;
    if(!n || (ext && !e) || (dir && !d))return 0;
    char *p=resource_path(self,n,e,d);if(!p)return 0;
    Im2String *s=im2_string_create(p,1);free(p);return (uintptr_t)s;
}
static uintptr_t bundle_resource(Bundle *self,const char *sel,void *name,void *ext){return bundle_resource_dir(self,sel,name,ext,NULL);}
static void bundle_dealloc(void *self,const char *sel){(void)sel;bundle_cleanup(self);}
#define M(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_foundation_init(Im2ObjcClass *root,const Im2FoundationConfig *cfg) {
    const Im2ObjcClass *parent=im2_objc_class_metadata(root);
    if(!parent || (parent->data->ro->flags&1u))return -1;
    if(strings || !cfg || !cfg->bundle_path || !*cfg->bundle_path || strlen(cfg->bundle_path)>PATH_LIMIT)return -1;
    size_t checkpoint=im2_objc_registration_checkpoint();config=*cfg;
    const Im2MethodSpec sim[]={M("UTF8String","*@:",string_utf8),M("length","I@:",string_length),
        M("initWithUTF8String:","@@:*",string_init),M("initWithCString:encoding:","@@:*I",string_init_encoding),
        M("getCString:maxLength:encoding:","c@:*II",string_get),M("cStringUsingEncoding:","*@:I",string_cencoding),
        M("copy","@@:",immutable_copy),M("dealloc","v@:",string_dealloc)};
    Im2MethodSpec scm[]={M("stringWithUTF8String:","@@:*",string_factory),M("stringWithFormat:","@@:@",cfg->string_format)};
    strings=im2_objc_register_class("NSString",root,sizeof(Im2String),sim,8,scm,cfg->string_format?2:1);
    const Im2MethodSpec dim[]={M("initWithBytes:length:","@@:^vI",data_init),M("bytes","^v@:",data_bytes),M("length","I@:",data_length),
        M("getBytes:length:","v@:^vI",data_get),M("copy","@@:",immutable_copy),M("dealloc","v@:",data_dealloc)};
    const Im2MethodSpec dcm[]={M("data","@@:",data_empty),M("dataWithBytes:length:","@@:^vI",data_factory),M("dataWithContentsOfFile:","@@:@",data_file)};
    data_class=strings?im2_objc_register_class("NSData",root,sizeof(Data),dim,6,dcm,3):NULL;
    const Im2MethodSpec bim[]={M("bundlePath","@@:",bundle_path),M("resourcePath","@@:",bundle_path),
        M("pathForResource:ofType:","@@:@@",bundle_resource),M("pathForResource:ofType:inDirectory:","@@:@@@",bundle_resource_dir),M("dealloc","v@:",bundle_dealloc)};
    const Im2MethodSpec bcm[]={M("mainBundle","@@:",bundle_main)};
    bundles=data_class?im2_objc_register_class("NSBundle",root,sizeof(Bundle),bim,5,bcm,1):NULL;
    if(bundles)main_bundle=im2_objc_alloc(bundles,bundle_cleanup);
    if(main_bundle)main_bundle->path=duplicate(cfg->bundle_path);
    if(!main_bundle || !main_bundle->path) {
        if(main_bundle)im2_objc_release(main_bundle);
        main_bundle=NULL;
        (void)im2_objc_registration_rollback(checkpoint);strings=data_class=bundles=NULL;return -1;
    }
    size_t len=strlen(main_bundle->path);while(len>1 && main_bundle->path[len-1]=='/')main_bundle->path[--len]=0;
    return 0;
}
