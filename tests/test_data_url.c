/* SPDX-License-Identifier: MIT. Exercise original NSData selectors with actual NSURLs.
 * The test uses a real temporary file, never game assets or mocked file content.
 */
#define _POSIX_C_SOURCE 200809L
#include "foundation.h"
#include "url.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Im2Imp method(void *receiver,const char *sel) {
    Im2ObjcTarget t;
    assert(!im2_objc_lookup_target(receiver,sel,NULL,&t));
    assert(!t.guest && !t.stret_size);
    return t.imp;
}
static void *send1(void *receiver,const char *sel,void *arg) {
    return (void *)((uintptr_t(*)(void *,const char *,void *))method(receiver,sel))(receiver,sel,arg);
}
static void call_dealloc(void *obj) {
    const char *sel="dealloc";
    ((void(*)(void *,const char *))method(obj,sel))(obj,sel);
}
int main(void) {
    Im2ObjcClass *root=im2_objc_register_class("NSObject",NULL,sizeof(Im2ObjcObject),NULL,0,NULL,0);
    assert(root);
    Im2FoundationConfig config={"/tmp",NULL,NULL,NULL};
    assert(!im2_foundation_init(root,&config));
    assert(!im2_url_init(root,NULL));
    void *pool=im2_objc_alloc(root,NULL);
    assert(pool && !im2_objc_pool_push(pool));
    char tmp[]="/tmp/im2-data-url-XXXXXX";
    int fd=mkstemp(tmp);
    assert(fd>=0);
    const unsigned char expected[]={0,1,0,255,34,3};
    assert(write(fd,expected,sizeof(expected))==(ssize_t)sizeof(expected));
    assert(!close(fd));

    Im2ObjcClass *url_class=im2_objc_class_named("NSURL");
    Im2ObjcClass *data_class=im2_objc_class_named("NSData");
    assert(url_class && data_class);
    void *path=im2_string_create(tmp,0);
    void *url=send1(url_class,"fileURLWithPath:",path);
    assert(url && !strcmp(im2_url_local_file_path(url),tmp));
    void *data=send1(data_class,"dataWithContentsOfURL:",url);
    assert(data && im2_data_length(data)==sizeof(expected));
    assert(!memcmp(im2_data_bytes(data),expected,sizeof(expected)));

    /* -initWithContentsOfURL: must initialize the preallocated receiver. */
    void *preallocated=im2_objc_alloc(data_class,call_dealloc);
    assert(preallocated);
    void *initialized=send1(preallocated,"initWithContentsOfURL:",url);
    assert(initialized==preallocated && im2_data_length(initialized)==sizeof(expected));
    assert(!memcmp(im2_data_bytes(initialized),expected,sizeof(expected)));
    assert(!im2_objc_release(initialized));

    void *remote_text=im2_string_create("https://example.com/asset.bin",0);
    void *remote=send1(url_class,"URLWithString:",remote_text);
    assert(remote && !im2_url_local_file_path(remote));
    assert(!send1(data_class,"dataWithContentsOfURL:",remote));

    const char embedded[]="/tmp/sample\0hidden";
    void *unsafe=im2_string_create_bytes(embedded,sizeof(embedded)-1,0);
    assert(!send1(data_class,"dataWithContentsOfFile:",unsafe));

    assert(!unlink(tmp));
    assert(!send1(data_class,"dataWithContentsOfURL:",url));
    void *missing=im2_objc_alloc(data_class,call_dealloc);
    assert(missing && !send1(missing,"initWithContentsOfURL:",url));

    assert(!im2_objc_release(path));
    assert(!im2_objc_release(remote_text));
    assert(!im2_objc_release(unsafe));
    assert(!im2_objc_pool_drain(pool));
    puts("PASS: NSData file-URL factory/init read actual binary, reject network and missing files, enforce NUL-safe filename");
    return 0;
}
