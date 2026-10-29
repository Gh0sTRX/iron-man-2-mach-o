/* SPDX-License-Identifier: MIT
 * Independently authored, intentionally partial Objective-C URL request types.
 * Networking, cache policy and timeoutInterval ABI are NOT implemented.
 */
#include "url_request.h"
#include "url.h"
#include "foundation.h"
#include "collections.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_HEADERS_LIMIT 32u
typedef struct {void *key,*value;} HttpHeader;
typedef struct {
    Im2ObjcClass *isa;
    void *url,*method,*body;
    size_t count;
    HttpHeader headers[HTTP_HEADERS_LIMIT];
} Im2Request;
static Im2ObjcClass *requests,*mutable_requests;
static void (*on_failure)(const char *);

static void fail(const char *message) {
    if(on_failure)on_failure(message);
    abort();
}
static int object_kind(const void *o,Im2ObjcClass *expected) {
    if(!o || !expected)return 0;
    for(Im2ObjcClass *c=im2_objc_object_class(o);c;) {
        if(c==expected)return 1;
        const Im2ObjcClass *m=im2_objc_class_metadata(c);
        if(!m)return 0;
        c=m->superclass;
    }
    return 0;
}
static int valid_text(const void *o) {
    const char *s=im2_string_utf8(o);
    return s && strlen(s)==im2_string_byte_length(o);
}
static int header_key(const void *o) {
    if(!valid_text(o))return 0;
    const unsigned char *p=(const unsigned char *)im2_string_utf8(o);
    if(!*p)return 0;
    for(;*p;p++)
        if(!( (*p>='A' && *p<='Z') || (*p>='a' && *p<='z') ||
              (*p>='0' && *p<='9') || *p=='-' || *p=='_' ))return 0;
    return 1;
}
static int header_value(const void *o) {
    if(!valid_text(o))return 0;
    const unsigned char *p=(const unsigned char *)im2_string_utf8(o);
    for(;*p;p++)if(*p<0x20 || *p==0x7f)return 0;
    return 1;
}
static int method_token(const void *o) {
    if(!valid_text(o))return 0;
    const unsigned char *p=(const unsigned char *)im2_string_utf8(o);
    if(!*p)return 0;
    for(;*p;p++)if(!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||*p=='-'))return 0;
    return 1;
}
static int key_equal(void *a,void *b) {
    const char *p=im2_string_utf8(a),*q=im2_string_utf8(b);
    size_t n=im2_string_byte_length(a);
    if(n!=im2_string_byte_length(b))return 0;
    for(size_t i=0;i<n;i++) {
        unsigned char x=(unsigned char)p[i],y=(unsigned char)q[i];
        if(x>='A'&&x<='Z')x+=32;
        if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;
    }
    return 1;
}
static void request_destroy(void *object) {
    Im2Request *r=object;
    if(r->url)im2_objc_release(r->url);
    if(r->method)im2_objc_release(r->method);
    if(r->body)im2_objc_release(r->body);
    for(size_t i=0;i<r->count;i++) {
        im2_objc_release(r->headers[i].key);
        im2_objc_release(r->headers[i].value);
        r->headers[i]=(HttpHeader){0};
    }
    r->url=r->method=r->body=NULL;r->count=0;
}
static int request_assign(Im2Request *r,void *url) {
    if(!object_kind(url,im2_objc_class_named("NSURL")))return -1;
    void *held=im2_objc_retain(url);
    void *get=im2_string_create("GET",0);
    if(!held || !get) {
        if(held)im2_objc_release(held);
        if(get)im2_objc_release(get);
        return -1;
    }
    request_destroy(r);
    r->url=held;r->method=get;
    return 0;
}
static void *request_create(void *cls,void *url,int autorelease) {
    if(cls!=requests && cls!=mutable_requests)return NULL;
    Im2Request *r=im2_objc_alloc(cls,request_destroy);
    if(!r)return NULL;
    if(request_assign(r,url)) {im2_objc_release(r);return NULL;}
    if(autorelease && !im2_objc_autorelease(r)) {
        im2_objc_release(r);return NULL;
    }
    return r;
}
static uintptr_t init_url(Im2Request *r,const char *sel,void *url) {
    (void)sel;
    if(request_assign(r,url)) {im2_objc_release(r);return 0;}
    return (uintptr_t)r;
}
static uintptr_t factory_url(void *cls,const char *sel,void *url) {
    (void)sel;return (uintptr_t)request_create(cls,url,1);
}
static uintptr_t request_url(Im2Request *r,const char *sel) {
    (void)sel;return (uintptr_t)r->url;
}
static uintptr_t request_method(Im2Request *r,const char *sel) {
    (void)sel;return (uintptr_t)r->method;
}
static uintptr_t request_body(Im2Request *r,const char *sel) {
    (void)sel;return (uintptr_t)r->body;
}
static uintptr_t request_header(Im2Request *r,const char *sel,void *key) {
    (void)sel;
    if(!header_key(key))return 0;
    for(size_t i=0;i<r->count;i++)
        if(key_equal(r->headers[i].key,key))return (uintptr_t)r->headers[i].value;
    return 0;
}
static uintptr_t all_headers(Im2Request *r,const char *sel) {
    (void)sel;
    void *values[HTTP_HEADERS_LIMIT],*keys[HTTP_HEADERS_LIMIT];
    for(size_t i=0;i<r->count;i++) {
        values[i]=r->headers[i].value;keys[i]=r->headers[i].key;
    }
    return (uintptr_t)im2_dictionary_create(values,keys,r->count,0,1);
}
static uintptr_t request_clone(Im2Request *r,int mutable) {
    Im2Request *copy=request_create(mutable?mutable_requests:requests,r->url,0);
    if(!copy)return 0;
    if(r->method) {
        void *newmethod=im2_objc_retain(r->method);
        if(!newmethod)goto failed;
        im2_objc_release(copy->method);copy->method=newmethod;
    }
    if(r->body && !(copy->body=im2_objc_retain(r->body)))goto failed;
    for(size_t i=0;i<r->count;i++) {
        void *key=im2_objc_retain(r->headers[i].key);
        if(!key)goto failed;
        void *value=im2_objc_retain(r->headers[i].value);
        if(!value){im2_objc_release(key);goto failed;}
        copy->headers[copy->count++]=(HttpHeader){key,value};
    }
    return (uintptr_t)copy;
failed:
    im2_objc_release(copy);return 0;
}
static uintptr_t request_copy(Im2Request *r,const char *sel) {
    (void)sel;
    return object_kind(r,mutable_requests)?
        request_clone(r,0):(uintptr_t)im2_objc_retain(r);
}
static uintptr_t request_mutable_copy(Im2Request *r,const char *sel) {
    (void)sel;return request_clone(r,1);
}
static void request_dealloc(Im2Request *r,const char *sel) {
    (void)sel;request_destroy(r);
}
static void set_method(Im2Request *r,const char *sel,void *method) {
    (void)sel;
    if(!method_token(method))fail("Unsupported HTTP method");
    void *newmethod=im2_objc_retain(method);
    if(!newmethod)fail("Unmanaged HTTP method");
    void *old=r->method;r->method=newmethod;
    if(old)im2_objc_release(old);
}
static void set_body(Im2Request *r,const char *sel,void *body) {
    (void)sel;
    if(body && !object_kind(body,im2_objc_class_named("NSData")))fail("HTTP body must be NSData");
    void *newbody=body?im2_objc_retain(body):NULL;
    if(body && !newbody)fail("Unmanaged HTTP body");
    void *old=r->body;r->body=newbody;
    if(old)im2_objc_release(old);
}
static void set_url(Im2Request *r,const char *sel,void *url) {
    (void)sel;
    if(!object_kind(url,im2_objc_class_named("NSURL")))fail("HTTP request URL must be NSURL");
    void *newurl=im2_objc_retain(url);
    if(!newurl)fail("Unmanaged request URL");
    void *old=r->url;r->url=newurl;
    if(old)im2_objc_release(old);
}
static void set_header(Im2Request *r,const char *sel,void *value,void *key) {
    (void)sel;
    if(!header_key(key) || (value && !header_value(value)))fail("Invalid HTTP header");
    for(size_t i=0;i<r->count;i++)if(key_equal(r->headers[i].key,key)) {
        if(!value) {
            im2_objc_release(r->headers[i].key);
            im2_objc_release(r->headers[i].value);
            memmove(r->headers+i,r->headers+i+1,
                (r->count-i-1)*sizeof(*r->headers));
            r->headers[--r->count]=(HttpHeader){0};
        } else {
            void *held=im2_objc_retain(value);
            if(!held)fail("Unmanaged HTTP header value");
            void *old=r->headers[i].value;r->headers[i].value=held;
            im2_objc_release(old);
        }
        return;
    }
    if(!value)return;
    if(r->count==HTTP_HEADERS_LIMIT)fail("Request header limit reached");
    const char *text=im2_string_utf8(key);
    void *keycopy=im2_string_create_bytes(text,im2_string_byte_length(key),0);
    void *held=im2_objc_retain(value);
    if(!keycopy || !held) {
        if(keycopy)im2_objc_release(keycopy);
        if(held)im2_objc_release(held);
        fail("Unable to retain HTTP header");
    }
    r->headers[r->count++]=(HttpHeader){keycopy,held};
}
#define METHOD(s,t,f) {s,t,(Im2Imp)(f),0}
int im2_url_request_init(Im2ObjcClass *root,void (*failure)(const char *)) {
    if(requests || !root || !im2_objc_class_named("NSURL") ||
       !im2_objc_class_named("NSData") || !im2_objc_class_named("NSDictionary"))
        return -1;
    on_failure=failure;
    size_t checkpoint=im2_objc_registration_checkpoint();
    const Im2MethodSpec methods[]={
        METHOD("initWithURL:","@@:@",init_url),
        METHOD("URL","@@:",request_url),
        METHOD("HTTPMethod","@@:",request_method),
        METHOD("HTTPBody","@@:",request_body),
        METHOD("valueForHTTPHeaderField:","@@:@",request_header),
        METHOD("allHTTPHeaderFields","@@:",all_headers),
        METHOD("copy","@@:",request_copy),
        METHOD("mutableCopy","@@:",request_mutable_copy),
        METHOD("dealloc","v@:",request_dealloc)
    };
    const Im2MethodSpec factories[]={METHOD("requestWithURL:","@@:@",factory_url)};
    requests=im2_objc_register_class("NSURLRequest",root,sizeof(Im2Request),
        methods,sizeof(methods)/sizeof(*methods),
        factories,sizeof(factories)/sizeof(*factories));
    const Im2MethodSpec setters[]={
        METHOD("setHTTPMethod:","v@:@",set_method),
        METHOD("setHTTPBody:","v@:@",set_body),
        METHOD("setURL:","v@:@",set_url),
        METHOD("setValue:forHTTPHeaderField:","v@:@@",set_header)
    };
    mutable_requests=requests?im2_objc_register_class("NSMutableURLRequest",
        requests,sizeof(Im2Request),setters,sizeof(setters)/sizeof(*setters),NULL,0):NULL;
    if(!mutable_requests) {
        (void)im2_objc_registration_rollback(checkpoint);
        requests=mutable_requests=NULL;return -1;
    }
    return 0;
}
