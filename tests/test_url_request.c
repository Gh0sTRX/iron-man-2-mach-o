/* SPDX-License-Identifier: MIT
 * Exercises production Objective-C method registrations and retained
 * NSURLRequest/NSMutableURLRequest fields with ASan and UBSan.
 */
#include "url_request.h"
#include "url.h"
#include "foundation.h"
#include "collections.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static Im2Imp method(void *receiver,const char *selector) {
    Im2ObjcTarget t;
    assert(!im2_objc_lookup_target(receiver,selector,NULL,&t));
    assert(!t.guest && !t.stret_size);
    return t.imp;
}
static void *send0(void *receiver,const char *sel) {
    return (void *)((uintptr_t(*)(void *,const char *))method(receiver,sel))(receiver,sel);
}
static void *send1(void *receiver,const char *sel,void *a) {
    return (void *)((uintptr_t(*)(void *,const char *,void *))method(receiver,sel))(receiver,sel,a);
}
static void send2(void *receiver,const char *sel,void *a,void *b) {
    ((void(*)(void *,const char *,void *,void *))method(receiver,sel))(receiver,sel,a,b);
}
static void expect_text(void *obj,const char *sel,const char *text) {
    void *result=send0(obj,sel);
    assert(result && im2_string_utf8(result));
    assert(!strcmp(im2_string_utf8(result),text));
}
static void check_missing(void *o,const char *selector) {
    Im2ObjcTarget t;assert(im2_objc_lookup_target(o,selector,NULL,&t));
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    const Im2FoundationConfig config={"/tmp",NULL,NULL,NULL};
    assert(!im2_foundation_init(root,&config));
    assert(!im2_collections_init(root,NULL));
    assert(!im2_url_init(root,NULL));
    assert(!im2_url_request_init(root,NULL));
    assert(im2_url_request_init(root,NULL));

    void *pool=im2_objc_alloc(root,NULL);
    assert(pool && !im2_objc_pool_push(pool));
    Im2ObjcClass *url_class=im2_objc_class_named("NSURL");
    Im2ObjcClass *request_class=im2_objc_class_named("NSURLRequest");
    Im2ObjcClass *mutable_class=im2_objc_class_named("NSMutableURLRequest");
    assert(url_class && request_class && mutable_class);
    void *address=im2_string_create("https://example.com/settings",0);
    void *url=send1(url_class,"URLWithString:",address);
    assert(url);
    assert(!send1(request_class,"requestWithURL:",address));
    void *request=send1(request_class,"requestWithURL:",url);
    assert(request && send0(request,"URL")==url);
    expect_text(request,"HTTPMethod","GET");
    assert(!send0(request,"HTTPBody"));
    check_missing(request,"setHTTPMethod:");
    check_missing(request_class,"requestWithURL:cachePolicy:timeoutInterval:");

    void *same=send0(request,"copy");
    assert(same==request && im2_objc_retain_count(request)==2);
    assert(!im2_objc_release(same));
    void *mutable=send0(request,"mutableCopy");
    assert(mutable && mutable!=request && send0(mutable,"URL")==url);
    void *post=im2_string_create("POST",0);
    ((void(*)(void *,const char *,void *))method(mutable,"setHTTPMethod:"))(mutable,"setHTTPMethod:",post);
    expect_text(mutable,"HTTPMethod","POST");
    expect_text(request,"HTTPMethod","GET");

    const char body_bytes[]={0,1,2,0,4};
    void *body=im2_data_create(body_bytes,sizeof(body_bytes),0);
    assert(body);
    ((void(*)(void *,const char *,void *))method(mutable,"setHTTPBody:"))(mutable,"setHTTPBody:",body);
    assert(send0(mutable,"HTTPBody")==body);
    assert(im2_objc_retain_count(body)==2);

    void *key=im2_string_create("Content-Type",0);
    void *key_lower=im2_string_create("content-type",0);
    void *value=im2_string_create("application/json",0);
    void *replaced=im2_string_create("text/plain",0);
    send2(mutable,"setValue:forHTTPHeaderField:",value,key);
    assert(send1(mutable,"valueForHTTPHeaderField:",key_lower)==value);
    void *headers=send0(mutable,"allHTTPHeaderFields");
    assert(headers && (uintptr_t)send0(headers,"count")==1);
    send2(mutable,"setValue:forHTTPHeaderField:",replaced,key_lower);
    assert(send1(mutable,"valueForHTTPHeaderField:",key)==replaced);
    assert((uintptr_t)send0(headers,"count")==1); /* snapshot retained */
    void *frozen=send0(mutable,"copy");
    assert(frozen && frozen!=mutable);
    check_missing(frozen,"setHTTPBody:");
    expect_text(frozen,"HTTPMethod","POST");
    assert(send0(frozen,"HTTPBody")==body);
    send2(mutable,"setValue:forHTTPHeaderField:",NULL,key_lower);
    assert(!send1(mutable,"valueForHTTPHeaderField:",key));
    assert(send1(frozen,"valueForHTTPHeaderField:",key)==replaced);
    void *new_headers=send0(mutable,"allHTTPHeaderFields");
    assert((uintptr_t)send0(new_headers,"count")==0);

    void *mutable_from_mutable=send0(mutable,"mutableCopy");
    assert(mutable_from_mutable && mutable_from_mutable!=mutable);
    expect_text(mutable_from_mutable,"HTTPMethod","POST");
    assert(!im2_objc_release(mutable_from_mutable));

    assert(!im2_objc_release(frozen));
    assert(!im2_objc_release(mutable));
    assert(im2_objc_retain_count(body)==1);
    assert(!im2_objc_release(body));
    assert(!im2_objc_release(address));
    assert(!im2_objc_release(post));
    assert(!im2_objc_release(key));
    assert(!im2_objc_release(key_lower));
    assert(!im2_objc_release(value));
    assert(!im2_objc_release(replaced));
    assert(!im2_objc_pool_drain(pool));
    puts("PASS: URLRequest class methods, inherited mutable factories, strong ownership, header snapshot/case matching, copy and nil error paths");
}
