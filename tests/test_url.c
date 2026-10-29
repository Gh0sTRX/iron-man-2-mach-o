/* SPDX-License-Identifier: MIT. Real NSURL IMPs exercised under ASan/UBSan. */
#include "foundation.h"
#include "url.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static Im2Imp method(void *obj,const char *selector) {
    Im2ObjcTarget target;
    assert(!im2_objc_lookup_target(obj,selector,NULL,&target));
    assert(!target.guest && !target.stret_size);
    return target.imp;
}
static void *send0(void *obj,const char *selector) {
    return (void *)((uintptr_t(*)(void *,const char *))method(obj,selector))(obj,selector);
}
static void *send1(void *obj,const char *selector,void *arg) {
    return (void *)((uintptr_t(*)(void *,const char *,void *))method(obj,selector))(obj,selector,arg);
}
static uint32_t boolean(void *obj,const char *selector) {
    return ((uint32_t(*)(void *,const char *))method(obj,selector))(obj,selector);
}
static void destroy(void *o) {
    ((void(*)(void *,const char *))method(o,"dealloc"))(o,"dealloc");
}
static void expect_string(void *obj,const char *selector,const char *expected) {
    void *s=send0(obj,selector);
    assert(s && im2_string_utf8(s) && !strcmp(im2_string_utf8(s),expected));
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    const Im2FoundationConfig config={"/tmp",NULL,NULL,NULL};
    assert(!im2_foundation_init(root,&config));
    assert(!im2_url_init(root,NULL));
    assert(im2_url_init(root,NULL)); /* Duplicate registration must be refused. */
    Im2ObjcClass *cls=im2_objc_class_named("NSURL");
    assert(cls && im2_objc_is_class(cls));

    void *pool=im2_objc_alloc(root,NULL);
    assert(pool && !im2_objc_pool_push(pool));
    void *http=im2_string_create("https://example.com/a%20b?q=7#fragment",0);
    void *url=send1(cls,"URLWithString:",http);
    assert(url && !boolean(url,"isFileURL"));
    expect_string(url,"absoluteString","https://example.com/a%20b?q=7#fragment");
    expect_string(url,"relativeString","https://example.com/a%20b?q=7#fragment");
    expect_string(url,"scheme","https");
    expect_string(url,"path","/a b");
    void *copy=send0(url,"copy");
    assert(copy==url && im2_objc_retain_count(url)==2);
    assert(!im2_objc_release(copy));

    void *file=im2_string_create("/tmp/my data#1%.dat",0);
    void *fileurl=send1(cls,"fileURLWithPath:",file);
    assert(fileurl && boolean(fileurl,"isFileURL"));
    expect_string(fileurl,"absoluteString","file:///tmp/my%20data%231%25.dat");
    expect_string(fileurl,"path","/tmp/my data#1%.dat");
    expect_string(fileurl,"scheme","file");

    void *caps=im2_string_create("FILE:///tmp/test%20two",0);
    void *upper=send1(cls,"URLWithString:",caps);
    assert(upper && boolean(upper,"isFileURL"));
    expect_string(upper,"path","/tmp/test two");
    expect_string(upper,"scheme","file");

    void *relative=im2_string_create("images/cover.png",0);
    void *badhex=im2_string_create("file:///tmp/%GG",0);
    void *badzero=im2_string_create("file:///tmp/%00",0);
    void *remote=im2_string_create("file://remote.example/path",0);
    void *spaces=im2_string_create("http://example.com/bad path",0);
    assert(!send1(cls,"URLWithString:",relative));
    assert(!send1(cls,"fileURLWithPath:",relative));
    assert(!send1(cls,"URLWithString:",badhex));
    assert(!send1(cls,"URLWithString:",badzero));
    assert(!send1(cls,"URLWithString:",remote));
    assert(!send1(cls,"URLWithString:",spaces));
    assert(!send1(cls,"URLWithString:",NULL));

    /* initWithString: owns the same immutable path/scheme representation. */
    void *allocated=im2_objc_alloc(cls,destroy);
    assert(allocated && send1(allocated,"initWithString:",http)==allocated);
    expect_string(allocated,"path","/a b");
    assert(!im2_objc_release(allocated));

    assert(!im2_objc_release(http));
    assert(!im2_objc_release(file));
    assert(!im2_objc_release(caps));
    assert(!im2_objc_release(relative));
    assert(!im2_objc_release(badhex));
    assert(!im2_objc_release(badzero));
    assert(!im2_objc_release(remote));
    assert(!im2_objc_release(spaces));
    assert(!im2_objc_pool_drain(pool));
    puts("PASS: NSURL class imports, URL/path parsing, file encoding, copy ownership and invalid-input rejection");
}
