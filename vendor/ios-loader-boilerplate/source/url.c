/* SPDX-License-Identifier: MIT. Independently authored partial NSURL.
 * No networking or filesystem side effects. Unsupported URL forms return nil.
 */
#include "url.h"
#include "foundation.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define URL_LIMIT 4096u
typedef struct {
    Im2ObjcClass *isa;
    char *absolute;
    char *scheme;
    char *path;
} Im2URL;
static Im2ObjcClass *url_class;

static char *copy_span(const char *text,size_t n) {
    char *p=malloc(n+1);
    if(p){memcpy(p,text,n);p[n]=0;}
    return p;
}
static int hex_digit(unsigned char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
static char *decode_path(const char *encoded,size_t len) {
    char *decoded=malloc(len+1);
    if(!decoded)return NULL;
    size_t out=0;
    for(size_t i=0;i<len;i++) {
        unsigned char c=(unsigned char)encoded[i];
        if(c=='%') {
            if(i+2>=len){free(decoded);return NULL;}
            int hi=hex_digit((unsigned char)encoded[i+1]);
            int lo=hex_digit((unsigned char)encoded[i+2]);
            if(hi<0 || lo<0 || (hi==0 && lo==0)){free(decoded);return NULL;}
            c=(unsigned char)(hi*16+lo);i+=2;
        }
        decoded[out++]=(char)c;
    }
    decoded[out]=0;
    return decoded;
}
/* NSString supports binary-safe UTF-8 buffers. A URL must never silently
 * accept the prefix before an embedded NUL and discard the remaining bytes. */
static const char *url_input(void *string) {
    const char *value=im2_string_utf8(string);
    if(!value || strlen(value)!=im2_string_byte_length(string))return NULL;
    return value;
}
static void destroy_url(void *object) {
    Im2URL *u=object;
    free(u->absolute);free(u->scheme);free(u->path);
    u->absolute=u->scheme=u->path=NULL;
}
static int url_assign(Im2URL *u,const char *text) {
    if(!text)return -1;
    size_t n=strlen(text);
    if(!n || n>URL_LIMIT)return -1;
    size_t colon=0;
    if(!isalpha((unsigned char)text[0]))return -1;
    for(;colon<n && text[colon]!=':';colon++) {
        unsigned char c=(unsigned char)text[colon];
        if(!isalnum(c) && c!='+' && c!='-' && c!='.')return -1;
    }
    if(colon==n || colon>=64)return -1;
    for(size_t i=0;i<n;i++)if((unsigned char)text[i]<=0x20 || (unsigned char)text[i]==0x7f)return -1;
    char *absolute=copy_span(text,n);
    char *scheme=copy_span(text,colon);
    if(!absolute || !scheme){free(absolute);free(scheme);return -1;}
    for(size_t i=0;i<colon;i++)scheme[i]=(char)tolower((unsigned char)scheme[i]);

    const char *start=text+colon+1;
    const char *end=start+strcspn(start,"?#");
    if(end-start>=2 && start[0]=='/' && start[1]=='/') {
        const char *authority=start+2;
        const char *slash=memchr(authority,'/',(size_t)(end-authority));
        /* Only local file URLs map to paths on the Vita. */
        if(!strcmp(scheme,"file") && slash) {
            size_t hostlen=(size_t)(slash-authority);
            if(hostlen && (hostlen!=9 || strncmp(authority,"localhost",9))) {
                free(absolute);free(scheme);return -1;
            }
        } else if(!strcmp(scheme,"file") && end>authority) {
            free(absolute);free(scheme);return -1;
        }
        start=slash?slash:end;
    }
    char *path=decode_path(start,(size_t)(end-start));
    if(!path){free(absolute);free(scheme);return -1;}
    destroy_url(u);
    u->absolute=absolute;u->scheme=scheme;u->path=path;
    return 0;
}
static int url_assign_file(Im2URL *u,const char *path) {
    if(!path || path[0]!='/')return -1;
    size_t n=strlen(path);
    if(n>URL_LIMIT/3u)return -1;
    char *encoded=malloc(8u+n*3u);
    if(!encoded)return -1;
    memcpy(encoded,"file://",7);
    size_t out=7;
    static const char digits[]="0123456789ABCDEF";
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)path[i];
        if(c<=0x20 || c>=0x7f || c=='%' || c=='?' || c=='#') {
            if(!c){free(encoded);return -1;}
            encoded[out++]='%';encoded[out++]=digits[c>>4];encoded[out++]=digits[c&15];
        }else encoded[out++]=(char)c;
    }
    encoded[out]=0;
    int rc=url_assign(u,encoded);
    free(encoded);
    return rc;
}
static uintptr_t init_string(Im2URL *self,const char *selector,void *string) {
    (void)selector;
    const char *text=url_input(string);
    if(!text || url_assign(self,text)) {im2_objc_release(self);return 0;}
    return (uintptr_t)self;
}
static uintptr_t init_file(Im2URL *self,const char *selector,void *string) {
    (void)selector;
    const char *text=url_input(string);
    if(!text || url_assign_file(self,text)) {im2_objc_release(self);return 0;}
    return (uintptr_t)self;
}
static uintptr_t make_url(void *cls,void *input,int file) {
    if(cls!=url_class)return 0;
    Im2URL *u=im2_objc_alloc(url_class,destroy_url);
    if(!u)return 0;
    const char *text=url_input(input);
    if(!text || (file?url_assign_file(u,text):url_assign(u,text))) {
        im2_objc_release(u);return 0;
    }
    if(!im2_objc_autorelease(u)){im2_objc_release(u);return 0;}
    return (uintptr_t)u;
}
static uintptr_t factory_string(void *cls,const char *selector,void *input) {
    (void)selector;return make_url(cls,input,0);
}
static uintptr_t factory_file(void *cls,const char *selector,void *input) {
    (void)selector;return make_url(cls,input,1);
}
static uintptr_t get_absolute(Im2URL *u,const char *selector) {
    (void)selector;return (uintptr_t)im2_string_create(u->absolute,1);
}
static uintptr_t get_scheme(Im2URL *u,const char *selector) {
    (void)selector;return (uintptr_t)im2_string_create(u->scheme,1);
}
static uintptr_t get_path(Im2URL *u,const char *selector) {
    (void)selector;return (uintptr_t)im2_string_create(u->path,1);
}
static uint32_t is_file(Im2URL *u,const char *selector) {
    (void)selector;return u->scheme && !strcmp(u->scheme,"file");
}
static uintptr_t url_copy(Im2URL *u,const char *selector) {
    (void)selector;return (uintptr_t)im2_objc_retain(u);
}
static void url_dealloc(Im2URL *u,const char *selector) {
    (void)selector;destroy_url(u);
}
#define METHOD(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_url_init(Im2ObjcClass *root,void (*failure)(const char *)) {
    (void)failure; /* URL parsing failures are conventional nil, not fatal exceptions. */
    if(url_class || !root || !im2_objc_class_named("NSString"))return -1;
    const Im2MethodSpec methods[]={
        METHOD("initWithString:","@@:@",init_string),
        METHOD("initFileURLWithPath:","@@:@",init_file),
        METHOD("absoluteString","@@:",get_absolute),
        METHOD("relativeString","@@:",get_absolute),
        METHOD("scheme","@@:",get_scheme),
        METHOD("path","@@:",get_path),
        METHOD("isFileURL","c@:",is_file),
        METHOD("copy","@@:",url_copy),
        METHOD("dealloc","v@:",url_dealloc)
    };
    const Im2MethodSpec factories[]={
        METHOD("URLWithString:","@@:@",factory_string),
        METHOD("fileURLWithPath:","@@:@",factory_file)
    };
    url_class=im2_objc_register_class("NSURL",root,sizeof(Im2URL),
        methods,sizeof(methods)/sizeof(*methods),
        factories,sizeof(factories)/sizeof(*factories));
    return url_class?0:-1;
}
